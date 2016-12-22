#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include <errno.h>
#include <ctype.h>
#include <time.h>

#include "getpss.h"
#include "error.h"

char * heap_name(int which)
{
    switch (which) {
        case 0: return "Unknown";
        case 1: return "Dalvik";
        case 2: return "Native";
        case 3: return "Dalvik other";
        case 4: return "Stack";
        case 5: return "Cursor";
        case 6: return "Ashmem";
        case 7: return "UnKnow dev";
        case 8: return ".so mmap";
        case 9: return ".jar mmap";
        case 10: return ".apk mmap";
        case 11: return ".ttf mmap";
        case 12: return ".dex mmap";
        case 13: return ".oat mmap";
        case 14: return ".art mmap";
        case 15: return "Unknown map";
        case 16: return "GL";
        default: return "????";
    }
}


#define INIT_PIDS 20
static int get_pids(pid_t **pids_out, int *len) {
    DIR *proc;
    struct dirent *dir;
    pid_t pid, *pids, *new_pids;
    size_t pids_count, pids_size;
    int error;

    proc = opendir(PROCDIR);
    if (!proc)
        return errno;

    pids = malloc(INIT_PIDS * sizeof(pid_t));
    if (!pids) {
        closedir(proc);
        return errno;
    }

    pids_count = 0; pids_size = INIT_PIDS;

    while ((dir = readdir(proc))) {
        if (sscanf(dir->d_name, "%d", &pid) < 1)
            continue;

        if (pids_count >= pids_size) {
            new_pids = realloc(pids, 2 * pids_size * sizeof(pid_t));
            if (!new_pids) {
                error = errno;
                free(pids);
                closedir(proc);
                return error;
            }
            pids = new_pids;
            pids_size = 2 * pids_size;
        }

        pids[pids_count] = pid;
        pids_count++;
    }

    closedir(proc);
    new_pids = realloc(pids, pids_count * sizeof(pid_t));

    if (!new_pids) {
        error = errno;
        free(pids);
        return error;
    }

    *pids_out = new_pids;
    *len = pids_count;
    return 0;
}

static void read_mapinfo(FILE *fp, struct stats_t *stats)
{
    char line[1024];
    int len, nameLen;
    int skip, done = 0;

    unsigned size = 0, rss = 0, pss = 0, swappable_pss = 0;
    float sharing_proportion = 0.0;
    unsigned shared_clean = 0, shared_dirty = 0;
    unsigned private_clean = 0, private_dirty = 0;
    int is_swappable = 0;
    unsigned referenced = 0;
    unsigned temp;

    uint64_t start;
    uint64_t end = 0;
    uint64_t prevEnd = 0;
    char* name;
    int name_pos;

    int whichHeap = HEAP_UNKNOWN;
    int prevHeap = HEAP_UNKNOWN;

    if(fgets(line, sizeof(line), fp) == 0) return;

    while (!done) {
        prevHeap = whichHeap;
        prevEnd = end;
        whichHeap = HEAP_UNKNOWN;
        skip = 0;
        is_swappable = 0;

        len = strlen(line);
        if (len < 1) return;
        line[--len] = 0;

        if (sscanf(line, "%"SCNx64 "-%"SCNx64 " %*s %*x %*x:%*x %*d%n", &start, &end, &name_pos) != 2) {
            skip = 1;
        } else {
            while (isspace(line[name_pos])) {
                name_pos += 1;
            }
            name = line + name_pos;
            nameLen = strlen(name);

            if ((strstr(name, "[heap]") == name)) {
                whichHeap = HEAP_NATIVE;
            } else if (strncmp(name, "/dev/ashmem", 11) == 0) {
                if (strncmp(name, "/dev/ashmem/dalvik-", 19) == 0) {
                    whichHeap = HEAP_DALVIK_OTHER;
                    if ((strstr(name, "/dev/ashmem/dalvik-alloc space") == name) ||
                               (strstr(name, "/dev/ashmem/dalvik-main space") == name)) {
                        // This is the regular Dalvik heap.
                        whichHeap = HEAP_DALVIK;
                    } else if (strstr(name, "/dev/ashmem/dalvik-large object space") == name) {
                        whichHeap = HEAP_DALVIK;
                    } else if (strstr(name, "/dev/ashmem/dalvik-non moving space") == name) {
                        whichHeap = HEAP_DALVIK;
                    } else if (strstr(name, "/dev/ashmem/dalvik-zygote space") == name) {
                        whichHeap = HEAP_DALVIK;
                    }
                } else if (strncmp(name, "/dev/ashmem/CursorWindow", 24) == 0) {
                    whichHeap = HEAP_CURSOR;
                } else if (strncmp(name, "/dev/ashmem/libc malloc", 23) == 0) {
                    whichHeap = HEAP_NATIVE;
                } else {
                    whichHeap = HEAP_ASHMEM;
                }
            } else if (strncmp(name, "[anon:libc_malloc]", 18) == 0) {
                whichHeap = HEAP_NATIVE;
            } else if (strncmp(name, "[stack", 6) == 0) {
                whichHeap = HEAP_STACK;
            } else if (strncmp(name, "/dev/", 5) == 0) {
                if (!strncmp(name, "/dev/mali", 6) || !strncmp(name, "/dev/ump", 6))
                    whichHeap = HEAP_GL;
                else
                    whichHeap = HEAP_UNKNOWN_DEV;
            } else if (nameLen > 3 && strcmp(name+nameLen-3, ".so") == 0) {
                whichHeap = HEAP_SO;
                is_swappable = 1;
            } else if (nameLen > 4 && strcmp(name+nameLen-4, ".jar") == 0) {
                whichHeap = HEAP_JAR;
                is_swappable = 1;
            } else if (nameLen > 4 && strcmp(name+nameLen-4, ".apk") == 0) {
                whichHeap = HEAP_APK;
                is_swappable = 1;
            } else if (nameLen > 4 && strcmp(name+nameLen-4, ".ttf") == 0) {
                whichHeap = HEAP_TTF;
                is_swappable = 1;
            } else if ((nameLen > 4 && strcmp(name+nameLen-4, ".dex") == 0) ||
                       (nameLen > 5 && strcmp(name+nameLen-5, ".odex") == 0)) {
                whichHeap = HEAP_DEX;
                is_swappable = 1;
            } else if (nameLen > 4 && strcmp(name+nameLen-4, ".oat") == 0) {
                whichHeap = HEAP_OAT;
                is_swappable = 1;
            } else if (nameLen > 4 && strcmp(name+nameLen-4, ".art") == 0) {
                whichHeap = HEAP_ART;
                is_swappable = 1;
            } else if (strncmp(name, "[anon:", 6) == 0) {
                whichHeap = HEAP_UNKNOWN;
            } else if (nameLen > 0) {
                whichHeap = HEAP_UNKNOWN_MAP;
            } else if (start == prevEnd && prevHeap == HEAP_SO) {
                // bss section of a shared library.
                whichHeap = HEAP_SO;
            }
        }

        shared_clean = 0;
        shared_dirty = 0;
        private_clean = 0;
        private_dirty = 0;

        while (1) {
            if (fgets(line, 1024, fp) == 0) {
                done = 1;
                break;
            }

            if (line[0] == 'S' && sscanf(line, "Size: %d kB", &temp) == 1) {
                size = temp;
            } else if (line[0] == 'R' && sscanf(line, "Rss: %d kB", &temp) == 1) {
                rss = temp;
            } else if (line[0] == 'P' && sscanf(line, "Pss: %d kB", &temp) == 1) {
                pss = temp;
            } else if (line[0] == 'S' && sscanf(line, "Shared_Clean: %d kB", &temp) == 1) {
                shared_clean = temp;
            } else if (line[0] == 'S' && sscanf(line, "Shared_Dirty: %d kB", &temp) == 1) {
                shared_dirty = temp;
            } else if (line[0] == 'P' && sscanf(line, "Private_Clean: %d kB", &temp) == 1) {
                private_clean = temp;
            } else if (line[0] == 'P' && sscanf(line, "Private_Dirty: %d kB", &temp) == 1) {
                private_dirty = temp;
            } else if (line[0] == 'R' && sscanf(line, "Referenced: %d kB", &temp) == 1) {
                referenced = temp;
            } else if (sscanf(line, "%" SCNx64 "-%" SCNx64 " %*s %*x %*x:%*x %*d", &start, &end) == 2) {
                // looks like a new mapping
                // example: "10000000-10001000 ---p 10000000 00:00 0"
                break;
            }
        }

        if (!skip) {
            if (is_swappable && (pss > 0)) {
                sharing_proportion = 0.0;
                if ((shared_clean > 0) || (shared_dirty > 0)) {
                    sharing_proportion = (pss - private_clean
                            - private_dirty)/(shared_clean+shared_dirty);
                }
                swappable_pss = (sharing_proportion*shared_clean) + private_clean;
            } else
                swappable_pss = 0;

            stats[whichHeap].pss += pss;

            stats[whichHeap].privateDirty += private_dirty;
            stats[whichHeap].sharedDirty += shared_dirty;
            stats[whichHeap].privateClean += private_clean;
            stats[whichHeap].sharedClean += shared_clean;

        }
    }
}

static int load_maps(int pid, struct stats_t *stats)
{
    char tmp[128];
    FILE *fp;

    sprintf(tmp, PROCDIR"/%d/smaps", pid);
    fp = fopen(tmp, "r");
    if (fp == NULL) return -1;
    read_mapinfo(fp, stats);
    fclose(fp);

    return 0;
}

void get_cmdline(int pid, char *cmd, int len)
{
    char path[256];
    sprintf(path, PROCDIR"/%d/cmdline", pid);

    FILE *fp = fopen(path, "r");
    if (fp != NULL) {
        fgets(cmd, len, fp);
        fclose(fp);
    }
}

int getprocname(pid_t pid, char *buf, int len) {
    char filename[128];
    FILE *f;
    int rc = 0;
    static const char* unknown_cmdline = "<unknown>";

    if (len <= 0) {
        return -1;
    }

    if (sprintf(filename, PROCDIR"/%d/cmdline", pid) < 0) {
        rc = 1;
        goto exit;
    }

    f = fopen(filename, "r");
    if (f == NULL) {
        rc = 2;
        goto exit;
    }

    if (fgets(buf, len, f) == NULL) {
        rc = 3;
        goto closefile;
    }

closefile:
    fclose(f);
exit:
    if (rc != 0) {
        /*
         * The process went away before we could read its process name. Try
         * to give the user "<unknown>" here, but otherwise they get to look
         *  at a blank.
         */
        if (strcpy(buf, unknown_cmdline)) {
            rc = 4;
        }
    }
    return rc;
}

int get_pss(struct proc_info *proc)
{
    int ret;
    if (proc == NULL) return -1;
    memset(proc->stats, 0, sizeof(proc->stats));
    ret = load_maps(proc->pid, proc->stats);
    return ret;
}

static void print_line(struct stats_t *tmp, char *name)
{
    if (tmp->pss > 0)
        printf("%15s%7d%7d%7d\n", name,
                tmp->pss, tmp->privateDirty, tmp->privateClean);
}

static void pss_detail_add(struct stats_t *a, struct stats_t *b, struct stats_t *sum)
{
        sum->pss = a->pss + b->pss;
        sum->privateDirty = a->privateDirty + b->privateDirty ;
        sum->sharedDirty = a->sharedDirty + b->sharedDirty ;
        sum->privateClean = a->privateClean + b->privateClean ;
        sum->sharedClean = a->sharedClean + b->sharedClean ;
}

int print_pss(struct proc_info *proc)
{
    int i;
    struct stats_t total, dalvik, othermap, unknown;
    struct stats_t *tmp = proc->stats;

    if (proc == NULL) return -1;

    memset(&total, 0, sizeof(struct stats_t));
    memset(&dalvik, 0, sizeof(struct stats_t));
    memset(&othermap, 0, sizeof(struct stats_t));
    memset(&unknown, 0, sizeof(struct stats_t));

    for (i = 0; i < _NUM_HEAP; i++) {
        pss_detail_add(&tmp[i], &total, &total);
        if (i == HEAP_DALVIK || i == HEAP_DALVIK_OTHER)
            pss_detail_add(&tmp[i], &dalvik, &dalvik);
        else if (i > HEAP_SO && i <= HEAP_UNKNOWN_MAP)
            pss_detail_add(&tmp[i], &othermap, &othermap);
        else if (i == HEAP_UNKNOWN ||
                (i >= HEAP_CURSOR && i <= HEAP_UNKNOWN_DEV))
            pss_detail_add(&tmp[i], &unknown, &unknown);
    }

    printf("Applications Memory Usage %s(kB):\n", proc->cmdline);
    printf("                  pss  Private  Private\n");
    printf("                 Total    Dirty    Clean\n");
    printf("                 -----   ------   ------\n");

    print_line(&tmp[HEAP_NATIVE], heap_name(HEAP_NATIVE));
    print_line(&dalvik, heap_name(HEAP_DALVIK));
    print_line(&tmp[HEAP_STACK], heap_name(HEAP_STACK));
    print_line(&tmp[HEAP_SO], heap_name(HEAP_SO));
    print_line(&othermap,"other map");
    print_line(&tmp[HEAP_GL], heap_name(HEAP_GL));
    print_line(&unknown,"unkonw");
    print_line(&total,"total");

    return 0;
}

static void stat_procmem(struct meminfo *meminfo)
{
    int i, j;
    struct mem_item *stats = meminfo->pss_detail;
    struct proc_info **procs = meminfo->pss;

    memset(stats, 0, sizeof(struct mem_item) * _NUM_HEAP);

    for (i = 0; i < meminfo->num_procs; i++) {
        struct stats_t *tmp = procs[i]->stats;

        procs[i]->dalvikpss = tmp[HEAP_DALVIK].pss + tmp[HEAP_DALVIK_OTHER].pss;
        procs[i]->nativepss = tmp[HEAP_NATIVE].pss;
        procs[i]->totalpss = 0;

        for (j = 0; j < _NUM_HEAP; j++) {
            stats[j].num += tmp[j].pss;
            strcpy(stats[j].name, heap_name(j));

            // skip GL
            if (j == HEAP_GL)
                continue;
            procs[i]->totalpss += tmp[j].pss;
        }
    }
}

static int cmppss(const void *a, const void *b)
{
    return  (*((struct proc_info**)b))->totalpss - (*((struct proc_info**)a))->totalpss;
}

static int cmpcat(const void *a, const void *b)
{
    return (*(struct mem_item *)b).num - (*(struct mem_item *)a).num;
}

void print_procmem(struct meminfo *meminfo)
{
    int i, total = 0;
    struct proc_info *tmp;
    struct tm *tm = &(meminfo->timestap);

    qsort(meminfo->pss, meminfo->num_procs, sizeof(meminfo->pss[0]), cmppss);

    printf("Total PSS by process");
    printf("(%02d-%02d-%02d %02d:%02d:%02d):\n", tm->tm_year + 1900, tm->tm_mon + 1,
            tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

    for (i = 0; i < meminfo->num_procs; i++) {
        tmp = meminfo->pss[i];
        if (tmp == NULL)
            continue;

        if (tmp->totalpss == 0)
            continue;

        if (getprocname(tmp->pid, tmp->cmdline, sizeof(tmp->cmdline)) < 0) {
            free(meminfo->pss[i]);
            meminfo->pss[i] = NULL;
            continue;
        }
        total += tmp->totalpss;

        printf("%7d KB: %s (%d)\n", tmp->totalpss, tmp->cmdline, tmp->pid);
    }
    printf("%10s: %7d KB\n", "total pss", total);

    qsort(meminfo->pss_detail, _NUM_HEAP, sizeof(meminfo->pss_detail[0]), cmpcat);

    printf("\nTotal PSS by category:\n");
    for (i = 0; i < _NUM_HEAP; i++)
        printf("%7d KB: %s\n",
                meminfo->pss_detail[i].num, meminfo->pss_detail[i].name);

}

int get_procmem(struct meminfo *meminfo)
{
    pid_t *pids;
    int i, num_procs = -1;
    struct proc_info **procs;

    get_pids(&pids, &num_procs);
    if (num_procs == -1)
        err_quit("no process find\n");
    meminfo->pss = calloc(num_procs, sizeof(struct proc_info *));
    if (meminfo->pss == NULL)
        err_quit("calloc pss error\n");
    meminfo->num_procs = num_procs;

    procs = meminfo->pss;

    for (i = 0; i < num_procs; i++) {
        procs[i] = malloc(sizeof(struct proc_info));
        if (procs[i] == NULL) continue;
        procs[i]->pid = pids[i];
        get_pss(procs[i]);
    }

    stat_procmem(meminfo);

    return 0;
}

int get_pid(char *procn)
{
    DIR *proc;
    struct dirent *dir;
    pid_t pid;
    char cmdline[128];

    proc = opendir(PROCDIR);
    if (!proc)
        return -errno;

    while ((dir = readdir(proc))) {
        if (sscanf(dir->d_name, "%d", &pid) < 1)
            continue;

        if (getprocname(pid, cmdline, sizeof(cmdline)) < 0)
            return -1;
        if (strstr(cmdline, procn)) {
            closedir(proc);
            return pid;
        }
    }

    closedir(proc);
    return -1;
}
