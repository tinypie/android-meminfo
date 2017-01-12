#ifndef MEMINFO_GETPSS_H
#define MEMINFO_GETPSS_H

#ifdef ANDROID
#define PROCDIR "/proc"
#else
#define PROCDIR "test"
#endif

#include <sys/types.h>
#include <time.h>

enum enum_meminfo {
    MEMINFO_TOTAL,
    MEMINFO_FREE,
    MEMINFO_BUFFERS,
    MEMINFO_CACHED,
    MEMINFO_ACTIVE,
    MEMINFO_INACTIVE,
    MEMINFO_SWAP_TOTAL,
    MEMINFO_SWAP_FREE,
    MEMINFO_ANONPAGES,
    MEMINFO_MAPPED,
    MEMINFO_SHMEM,
    MEMINFO_SLAB,
    MEMINFO_PAGE_TABLES,
    MEMINFO_KERNEL_STACK,
    MEMINFO_VMALLOC_USED,
    MEMINFO_TOTAL_CMA,
    MEMINFO_DUSED_CMA,
    MEMINFO_VMALLOC_INFO,
    MEMINFO_ZRAM_TOTAL,
    MEMINFO_ION,
    MEMINFO_ION_BUFFER,
    MEMINFO_GPU_USED,
    MEMINFO_CODEC_USED,
    MEMINFO_FREE_CMA,
    MEMINFO_COUNT
};

struct mem_item {
    char name[64];
    int num;
};

enum enum_heap {
    HEAP_UNKNOWN,
    HEAP_DALVIK,
    HEAP_NATIVE,
    HEAP_DALVIK_OTHER,
    HEAP_STACK,
    HEAP_CURSOR,
    HEAP_ASHMEM,
    HEAP_UNKNOWN_DEV,
    HEAP_SO,
    HEAP_JAR,
    HEAP_APK,
    HEAP_TTF,
    HEAP_DEX,
    HEAP_OAT,
    HEAP_ART,
    HEAP_UNKNOWN_MAP,
    HEAP_GL,
    _NUM_HEAP,
    _NUM_CORE_HEAP = HEAP_NATIVE+1
};

struct stats_t {
    int pss;
    int rss;
    int privateDirty;
    int sharedDirty;
    int privateClean;
    int sharedClean;
};

struct proc_info {
    struct stats_t stats[_NUM_HEAP];
    int dalvikpss;
    int nativepss;
    int otherpss;
    int totalpss;
    int pid;
    char cmdline[96];
};

struct meminfo {
    struct tm timestap;
    struct proc_info **pss;
    int num_procs;
    struct mem_item pss_detail[_NUM_HEAP];
    struct mem_item item[MEMINFO_COUNT];
};

int get_procmem(struct meminfo *minfo);
void print_procmem(struct meminfo *minfo);
int print_pss(struct proc_info *proc);
int get_pss(struct proc_info *proc);
int get_pid(char *procn);
int getprocname(pid_t pid, char *buf, int len);

#endif
