#ifndef MEMINFO_GETMEMINFO_H
#define MEMINFO_GETMEMINFO_H

#include "getpss.h"

#ifdef ANDROID
#define PROC_MEMINFO "/proc/meminfo"
#define VMALLOC_INFO "/proc/vmallocinfo"
#define ION_MEM "/proc/ion/vmalloc_ion"
#define ZRAM_MEM "/sys/block/zram0/mem_used_total"
#define GL_MEM "/sys/kernel/debug/mali/gpu_memory"
#define GL_MEMTX "/sys/kernel/debug/mali0/gpu_memory"
#define CODEC_MEM "/sys/class/codec_mm/codec_mm_dump"
#define CODEC_MEM_SCATTER "/sys/class/codec_mm/codec_mm_scatter_dump"
#else
#define PROC_MEMINFO "test/meminfo"
//#define VMALLOC_INFO "test/1.txt"
#define VMALLOC_INFO "test/vmallocinfo"
#define ION_MEM "test/vmalloc_ion"
#define ZRAM_MEM "test/mem_used_total"
#define GL_MEMTX "test/gpu_memory_tx"
#define GL_MEM "test/gpu_memory"
#define CODEC_MEM "test/codec_mm_dump"
#define CODEC_MEM_SCATTER "test/codec_mm_scatter_dump"
#endif

int get_mem(struct meminfo *mem);
int print_meminfo(struct mem_item *mem);

#endif // MEMCOM_GETMEMINFO_H
