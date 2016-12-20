#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "hash.h"


struct hash htable[HASH_SIZE];

static int list_size(struct proc *head)
{
    int size = 0;
    struct proc *tmp = head;

    while (tmp != NULL) {
        size++;
        tmp = tmp->next;
    }

    return size;
}

static int leak_check_process(struct hash *htable)
{
    int *sample, count, i = 0, size;
    int tp, gap, ret = 0;
    float hit;
    struct proc *tmp, *head = htable->head;

    if (head == NULL)
        return -1;
    // not enouph samples
    if ((size = list_size(head)) < 4)
        return -1;

    gap = (head->pss - htable->init_pss)/1024;
    if (gap > GAP_SIZE)
        ret = 2;

    sample = calloc(size, sizeof(int));
    while(head) {
        tmp = head->next;
        tp = head->pss;
        count = 0;
        hit = 0;

        while(tmp) {
            count++;
            if (tp > tmp->pss)
                hit++;
            tmp = tmp->next;
        }
        if (count <= 3) break;

        if (hit/count > 0.8) {
            if (i >= size) break;
            sample[i] = 1;
        }
        i++;

        head = head->next;
    }

    hit = 0;
    for (i = 0; i < size; i++)
        if (sample[i] == 1)
            hit++;
    free(sample);

    if (hit/size > 0.8)
        ret++;
    return ret;
}

static void print_hash(struct hash *hit)
{
    struct proc *head;
    int i = 0;

    if (hit == NULL) return;
    head = hit->head;

    printf("process %s (%d) may have memory leak:\n",
            hit->cmdline, head->pid);
    printf("init %d, min %d, max %d samples(%d):",
            hit->init_pss, hit->min_pss, hit->max_pss, hit->count);

    while(head) {
        if ((++i)%10 == 0) printf("\n");
        printf("\t%d", head->pss);
        head = head->next;
    }
    printf("\n");
}

static unsigned int hash_index(const char *str)
{
    unsigned int hash = 5381;
    int c;

    while ((c = *str++) != '\0')
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

int hash_insert_item(struct proc_info *item)
{
    int i;
    struct proc *pinfo, **head;
    struct hash *hit = NULL;

    if (item == NULL) return -1;
    if (item->totalpss == 0) {
        for (i = 0; i < _NUM_HEAP; i++) {
            // skip GL
            if (i == HEAP_GL)
                continue;
            item->totalpss += item->stats[i].pss;
        }

        if (item->totalpss == 0) {
            err_msg("hash insert %s total pss is 0, skip\n", item->cmdline);
            return -1;
        }
    }

    pinfo = (struct proc *) malloc(sizeof(struct proc));
    if (pinfo == NULL)
        err_sys("malloc error \n");

    pinfo->pid = item->pid;
    pinfo->pss = item->totalpss;

    i = hash_index(item->cmdline) % HASH_SIZE;
    // first insert
    if (htable[i].cmdline == NULL) {
        htable[i].cmdline = strdup(item->cmdline);
        head = &htable[i].head;
        hit = &htable[i];
        hit->init_pss = hit->min_pss = hit->max_pss = pinfo->pss;
    } else {
        if (!strcmp(item->cmdline, htable[i].cmdline)) {
            head = &htable[i].head;
            hit = &htable[i];
        } else { //conflict
            struct hash  *tmp = htable[i].next;

            while(tmp != NULL) {
                if (!strcmp(item->cmdline, tmp->cmdline)) {
                    hit = tmp;
                    break;
                }
                tmp = tmp->next;
            }

            // not found
            if (hit == NULL) {
                hit = malloc(sizeof(struct hash));
                if (hit == NULL) err_sys("malloc hit error\n");
                hit->head = NULL;
                hit->cmdline = strdup(item->cmdline);
                hit->init_pss = hit->min_pss = hit->max_pss = pinfo->pss;
                // insert into the hash table
                hit->next = htable[i].next;
                htable[i].next = hit;
            }

            head = &(hit->head);
        }
    }
    hit->count++;

    if (*head != NULL) {
        // pss not change skip
        if ((*head)->pss == pinfo->pss)
            return 0;

        if (pinfo->pss > hit->max_pss)
            hit->max_pss = pinfo->pss;
        else if (pinfo->pss < hit->min_pss)
            hit->min_pss = pinfo->pss;

        // oops pid changes
        if (pinfo->pid != (*head)->pid) {
            struct proc *tmp, *pn = *head;
            int leak = leak_check_process(hit);
            if (leak == 1)
                print_hash(hit);
    
            while(pn) {
                tmp = pn;
                pn = pn->next;
                free(tmp);
            }
            *head = NULL;
        }
    }

    pinfo->next = *head;
    *head= pinfo;

    return 0;
}

int hash_insert(struct meminfo *minfo)
{
    int i;
    for (i = 0; i < minfo->num_procs; i++) {
        if (minfo->pss[i] == NULL)
            continue;
        if (minfo->pss[i]->totalpss == 0)
            continue;

        if (minfo->pss[i]->cmdline[0] == '\0')
            if (getprocname(minfo->pss[i]->pid, minfo->pss[i]->cmdline,
                        sizeof(minfo->pss[i]->cmdline)) != 0)
                continue;

        hash_insert_item(minfo->pss[i]);
    }
    return 0;
}

static void shrink_link(struct hash *h)
{
    int size;
    struct proc *pnext, *tmp;
    if (h == NULL)
        return;
    if (h->head == NULL)
        return;

    pnext = h->head;
    size = list_size(pnext);
    if (size < SHRINK_SIZE)
        return;

    while(pnext) {
        tmp = pnext;
        pnext = pnext->next;
        free(tmp);
    }
    h->head = NULL;

}

void hash_shrink()
{
    int i;
    struct hash *hnext;

    for (i = 0; i < HASH_SIZE; i++) {
        if (htable[i].cmdline == NULL)
            continue;
        if (htable[i].head == NULL)
            continue;

        shrink_link(&htable[i]);

        hnext = htable[i].next;
        while(hnext) {
            shrink_link(hnext);
            hnext = hnext->next;
        }
    }
}

int detect_leak()
{
    int i;
    struct hash *hnext;

    for (i = 0; i < HASH_SIZE; i++) {
        if (htable[i].cmdline == NULL)
            continue;
        if (htable[i].head == NULL)
            continue;

        if (leak_check_process(&htable[i]) == 1)
                print_hash(&htable[i]);

        hnext = htable[i].next;
        while(hnext) {
            if (leak_check_process(hnext) == 1)
                print_hash(hnext);
            hnext = hnext->next;
        }
    }

    return 0;
}

static void list_clear(struct proc *head)
{
    struct proc *pnext;
    while(head) {
        pnext = head;
        head = head->next;
        free(pnext);
    }
}

void hash_clear()
{
    int i;
    struct hash *hnext, *tmp;

    for (i = 0; i < HASH_SIZE; i++) {
        if (htable[i].cmdline == NULL)
            continue;

        if (htable[i].head == NULL)
            continue;

        hnext = htable[i].next;
        while(hnext) {
            list_clear(hnext->head);
            tmp = hnext;
            hnext = hnext->next;
            free(tmp);
        }

        list_clear(htable[i].head);
    }
}
