#ifndef MEMINFO_HASH_H
#define MEMINFO_HASH_H

#define HASH_SIZE 200
#define GAP_SIZE 50
#define SHRINK_SIZE 60*4

#include "getpss.h"

struct proc {
    struct proc *next;
    int pid;
    int pss;
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
