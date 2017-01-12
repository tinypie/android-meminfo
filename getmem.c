#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <unistd.h>
#include <fcntl.h>	/* for open etc. system call */
#include <pthread.h>	/* for thread library */
#include <sys/stat.h>	/* for stat */
#include <sys/types.h>	/* for type like int8_t uint32_t etc. */
#include <sys/time.h>	/* for time */

#include "getmem.h"
#include "error.h"
#include "getpss.h"

static int get_meminfo(struct mem_item *mem)
{
    const char* const tags[] = {
        "MemTotal:",
        "MemFree:",
        "Buffers:",
        "Cached:",
        "Active:",
        "Inactive:",
        "SwapTotal:",
        "SwapFree:",
        "AnonPages:",
        "Mapped:",
        "Shmem:",
        "Slab:",
        "PageTables:",
        "KernelStack:",
        "VmallocUsed:",
        "TotalCMA:",
        "UsedCMA:",
        NULL
    };

    static const int tagsLen[] = {
        9,
        8,
        8,
        7,
        7,
        9,
        10,
        9,
        10,
        7,
        6,
        5,
        11,
        12,
        12,
        9,
        8,
        0
    };

    char buffer[1536];
    int num_found = 0;

    int fd = open(PROC_MEMINFO, O_RDONLY);
    if (fd < 0)
        err_sys("unable to open file %s:%s", PROC_MEMINFO, strerror(errno));

    int len = read(fd, buffer, sizeof(buffer)-1);
    close(fd);

    if (len < 0)
        err_quit("file %s is empty\n", PROC_MEMINFO);

    buffer[len] = 0;
    char *p = strstr(buffer, "MemTotal:");

    if (p == NULL)
        err_quit("input file is not /proc/meminfo\n");

    while (*p && num_found < 17) {
        int i = 0;
        while (tags[i]) {
            if (strncmp(p, tags[i], tagsLen[i]) == 0) {
                p += tagsLen[i];
                while (*p == ' ') p++;
                char* num = p;
                while (*p >= '0' && *p <= '9') p++;
                if (*p != 0) {
                    *p = 0;
                    p++;
                }
                mem[i].num = atoi(num);
                strcpy(mem[i].name,  tags[i]);
                num_found++;
                break;
            }
            i++;
        }
        while (*p && *p != '\n') {
            p++;
        }
        if (*p) p++;
    }
    return 0;
}

static int get_zram_mem(int *zram)
{
    int fd, len;
    char buffer[64];

    fd = open(ZRAM_MEM, O_RDONLY);
    if (fd >= 0) {
        len = read(fd, buffer, sizeof(buffer)-1);
        close(fd);
        if (len > 0) {
            buffer[len] = 0;
            *zram = atoi(buffer)/1024;
        }
    } else {
        err_msg("%s open error\n", ZRAM_MEM);
    }

    return 0;
}

static int get_ion_mem(int *buffer, int *ion)
{
    FILE *ion_fp;
    char line[1024];

    char ion_name[128], *p;
    int ion_pid;
    unsigned int ion_size;
    size_t unaccounted_size = 0;
    size_t buffer_size = 0;

    if ((ion_fp = fopen(ION_MEM, "r")) == NULL)
        err_sys("open file %s error %s", ION_MEM, strerror(errno));

    while(fgets(line, sizeof(line), ion_fp) != NULL) {
        if ((p=strstr(line, "="))) {
            p++;
            if(sscanf(p, "%u%s", &ion_size, ion_name) ==2)
                buffer_size += ion_size;
        }

        if (sscanf(line, "%s%d%u", ion_name, &ion_pid, &ion_size) != 3)
            continue;
        else
            unaccounted_size += ion_size;
    }
    fclose(ion_fp);

    //convert to kb
    *ion = unaccounted_size/1024;
    *buffer = buffer_size/1024;

    return 0;
}

int get_gpu_mem(int *gpu)
{
    FILE *gpu_fd;
    char line[1024];

    int gpu_size, flag = 0;

    if ((gpu_fd = fopen(GL_MEM, "r")) == NULL) {
        //err_msg("open file %s error %s", GL_MEM, strerror(errno));
        flag = 1;
        if ((gpu_fd = fopen(GL_MEMTX, "r")) == NULL) {
            err_msg("open file %s error %s", GL_MEMTX, strerror(errno));
            return -errno;
        }
    }

    while(fgets(line, sizeof(line), gpu_fd) != NULL) {
        if (flag == 0) {
            // mali450 (in bytes)
            // Mali mem usage: 42856448
            if (sscanf(line, "Mali mem usage: %d", &gpu_size) != 1)
                continue;
            else
                break;
        } else if (flag == 1) {
            // mali t82x t83x (in pages)
            // mali0                  12282
            if (sscanf(line, "%*s%d", &gpu_size) != 1)
                continue;
            else
                break;
        }
    }

    fclose(gpu_fd);
    if (flag == 0)
        *gpu = gpu_size/1024;
    else if (flag == 1)
        *gpu = gpu_size * 4;

    return 0;
}

static int get_codec_mem(int *codec)
{
    FILE *codec_fd;
    char line[1024], *p;

    int codec_size;

    if ((codec_fd = fopen(CODEC_MEM, "r")) == NULL) {
        err_msg("open file %s error %s", CODEC_MEM, strerror(errno));
        return -errno;
    }

    while(fgets(line, sizeof(line), codec_fd) != NULL) {
        if ((p=strstr(line, "CMA size:"))) {
            p = strstr(line, "alloced:");
            p += sizeof("alloced");

            while (*p == ' ') p++;
            char* num = p;
            while (*p >= '0' && *p <= '9') p++;
            if (*p != 0) {
                *p = 0;
                p++;
            }
            codec_size = atoi(num);
            *codec = codec_size * 1024;
            break;
        }
    }

    fclose(codec_fd);
    return 0;
}

static int get_codec_mem_scatter(int *codec)
{
    FILE *codec_fd;
    char line[1024], *p;

    int codec_size;
    int total = 0;

    if ((codec_fd = fopen(CODEC_MEM_SCATTER, "r")) == NULL) {
        err_msg("open file %s error %s", CODEC_MEM_SCATTER, strerror(errno));
        return -errno;
    }

    while(fgets(line, sizeof(line), codec_fd) != NULL) {
        // alloc from sys pages cnt:
        if ((p=strstr(line, "alloc from sys pages cnt:"))) {
            p += sizeof("alloc from sys pages cnt");

            while (*p == ' ') p++;
            char* num = p;
            while (*p >= '0' && *p <= '9') p++;
            if (*p != 0) {
                *p = 0;
                p++;
            }
            codec_size = atoi(num);
            total += codec_size * 4;
        } else if ((p=strstr(line, "one_page_cnt:"))) {
            p += sizeof("one_page_cnt");

            while (*p == ' ') p++;
            char* num = p;
            while (*p >= '0' && *p <= '9') p++;
            if (*p != 0) {
                *p = 0;
                p++;
            }
            codec_size = atoi(num);
            total += codec_size * 4;
        }
    }

    fclose(codec_fd);
    *codec = total;
    return 0;
}

static int get_vmalloc_mem(int *vmalloc)
{
    FILE *vmalloc_fd;
    char line[1024], *p;

    int vmalloc_size = 0;
    int vmap_size;

    if ((vmalloc_fd = fopen(VMALLOC_INFO, "r")) == NULL) {
        err_msg("open file %s error %s", VMALLOC_INFO, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), vmalloc_fd) != NULL) {
        if (strstr(line, "ioremap")) {
            continue;
        } else if ((p=strstr(line, "pages=")) != NULL) {
            p += sizeof("pages");
            char* num = p;
            while (*p >= '0' && *p <= '9') p++;
            if (*p != 0)
                *p = 0;
            // convert to KB
            vmalloc_size += atoi(num) * 4;
        } else if (strstr(line, "vmap")) {
            // skip ion vmap
            if (strstr(line, "ion"))
                continue;
            if(sscanf(line, "%*s%d%*s%*s", &vmap_size) == 1)
                vmalloc_size += vmap_size/1024;
        }
    }

    fclose(vmalloc_fd);
    *vmalloc = vmalloc_size;
    return 0;
}

static int get_cma_mem(int *cma)
{
    FILE *file;
    char line[1024], *p;

    int cma_free = 0, flag = 0;

    if ((file = fopen(PAGETYPE, "r")) == NULL) {
        err_msg("open file %s error %s", PAGETYPE, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (flag == 0 && strstr(line, "total")) {
            flag = 1;
            continue;
        } else if ((strstr(line, "CMA"))) {
            if (flag == 1)
                if((p=strrchr(line, ' '))) {
                    p++;
                    if (isdigit(*p)) {
                        cma_free = atoi(p) * 4;
                        break;
                    }
                }
        }
    }

    fclose(file);
    *cma = cma_free;
    return 0;
}

int print_meminfo(struct mem_item *mem)
{
    int total, kernel, kernel_cached;
    int pss, free_ram, unknown, ion;

    total = mem[MEMINFO_TOTAL].num;
    pss = mem[MEMINFO_ANONPAGES].num + mem[MEMINFO_MAPPED].num;

    printf("\nmemory information in kernel's view\n");
    printf("%15s%7d KB\n", mem[MEMINFO_TOTAL].name, total);

    printf("%15s%7d KB\n", "PSS:", pss);
    printf("             +---");
    printf("%15s%7d KB\n", mem[MEMINFO_ANONPAGES].name, mem[MEMINFO_ANONPAGES].num);
    printf("             |---");
    printf("%15s%7d KB\n", mem[MEMINFO_MAPPED].name, mem[MEMINFO_MAPPED].num);

    kernel_cached = mem[MEMINFO_CACHED].num - mem[MEMINFO_MAPPED].num;
    free_ram = kernel_cached + mem[MEMINFO_FREE].num;
    printf("%15s%7d KB\n", "Free Ram:", free_ram);
    printf("             +---");
    printf("%15s%7d KB", mem[MEMINFO_FREE].name, mem[MEMINFO_FREE].num);
    if (mem[MEMINFO_FREE_CMA].num)
        printf(" (free cma:%d KB)", mem[MEMINFO_FREE_CMA].num);

    printf("\n             |---");
    printf("%15s%7d KB\n", "Kernel cached:", kernel_cached);

    ion = mem[MEMINFO_ION].num + mem[MEMINFO_ION_BUFFER].num;

    kernel = mem[MEMINFO_BUFFERS].num
        + mem[MEMINFO_SLAB].num
        + mem[MEMINFO_PAGE_TABLES].num
        + mem[MEMINFO_KERNEL_STACK].num
        + mem[MEMINFO_SHMEM].num
        + mem[MEMINFO_VMALLOC_INFO].num
        + mem[MEMINFO_ZRAM_TOTAL].num
        + ion
        + mem[MEMINFO_GPU_USED].num
        + mem[MEMINFO_CODEC_USED].num;

    unknown = total - pss - free_ram - kernel;
    printf("%15s%7d KB\n", "kernel used:", kernel+unknown);
    printf("             +---");
    printf("%15s%7d KB\n", mem[MEMINFO_BUFFERS].name, mem[MEMINFO_BUFFERS].num);
    printf("             |---");
    printf("%15s%7d KB\n", mem[MEMINFO_SLAB].name, mem[MEMINFO_SLAB].num);
    printf("             |---");
    printf("%15s%7d KB\n", mem[MEMINFO_PAGE_TABLES].name, mem[MEMINFO_PAGE_TABLES].num);
    printf("             |---");
    printf("%15s%7d KB\n", mem[MEMINFO_KERNEL_STACK].name, mem[MEMINFO_KERNEL_STACK].num);
    printf("             |---");
    printf("%15s%7d KB\n", mem[MEMINFO_SHMEM].name, mem[MEMINFO_SHMEM].num);
    printf("             |---");
    printf("%15s%7d KB\n", "vmalloc:", mem[MEMINFO_VMALLOC_INFO].num);
    printf("             |---");
    printf("%15s%7d KB\n", "zram:", mem[MEMINFO_ZRAM_TOTAL].num);
    printf("             |---");
    printf("%15s%7d KB (graphic:%d + buffer:%d)\n", "ion:", ion,
            mem[MEMINFO_ION].num, mem[MEMINFO_ION_BUFFER].num);
    printf("             |---");
    printf("%15s%7d KB\n", "gpu:", mem[MEMINFO_GPU_USED].num);
    printf("             |---");
    printf("%15s%7d KB\n", "codecMem:", mem[MEMINFO_CODEC_USED].num);
    printf("             |---");
    printf("%15s%7d KB\n", "unknown:", unknown);

    printf("\ncma memory information:\n");
    printf("%15s%d KB %15s%d KB\n", "Total CMA:", mem[MEMINFO_TOTAL_CMA].num,
            "driver used:", mem[MEMINFO_DUSED_CMA].num);

    return 0;
}

void print_mem(struct mem_item *mem)
{
    int i;
    for (i = 0; i < 15; i++) {
        printf("%s\t\t%d\n", mem[i].name, mem[i].num);
    }
}

int get_mem(struct meminfo *mem)
{
    int codec_scatter = 0;

    get_meminfo(mem->item);
    get_zram_mem(&(mem->item[MEMINFO_ZRAM_TOTAL].num));
    get_ion_mem(&(mem->item[MEMINFO_ION_BUFFER].num),
            &(mem->item[MEMINFO_ION].num));
    get_gpu_mem(&(mem->item[MEMINFO_GPU_USED].num));
    get_vmalloc_mem(&(mem->item[MEMINFO_VMALLOC_INFO].num));

    get_codec_mem(&(mem->item[MEMINFO_CODEC_USED].num));
    get_codec_mem_scatter(&codec_scatter);
    mem->item[MEMINFO_CODEC_USED].num += codec_scatter;

    //get cma information
    get_cma_mem(&(mem->item[MEMINFO_FREE_CMA].num));

    return 0;
}

