#ifndef MEMINFO_HASH_H
#define MEMINFO_HASH_H

#define HASH_SIZE 200
#define GAP_SIZE 100

#include "getpss.h"

struct proc {
    struct proc *next;
    int pid;
    int pss;
//   int native_heap;
//   int dalvik_heap;
};

struct hash {
    struct hash *next;
    struct proc *head;
    char *cmdline;
    int init_pss;
    int max_pss;
    int min_pss;
    int count;
};

void hash_clear();
void hash_shrink();
int detect_leak();
int hash_insert(struct meminfo *minfo);
int hash_insert_item(struct proc_info *item);

#endif
