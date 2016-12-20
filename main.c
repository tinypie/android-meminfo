/*
 * the basis headers for C
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <getopt.h>

/*
 * headers for unix like programming environment
 */
#include <unistd.h>
#include <fcntl.h>  	/* for open etc. system call */
#include <pthread.h>	/* for thread library */
#include <sys/stat.h>	/* for stat */
#include <sys/types.h>	/* for type like int8_t uint32_t etc. */
#include <sys/time.h>	/* for time */

#include "getmem.h"
#include "getpss.h"
#include "error.h"
#include "hash.h"

extern char *optarg;
extern int optind;

static void usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s [options] [pid or proc name]\n", cmd);
    fprintf(stderr, "Options include:\n"
//            "  -f <filename>   Log to file. Default to stdout\n"
            "  -t <time>       dump meminfo every specific time in second\n"
            "  -l              detect leak\n"
            "  -h              show help\n");
}

/*
 * Associate an incoming signal (e.g. HUP) to a specific function
 */
static int catch_sig(int signo, void(*handler)())
{
    struct sigaction action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(signo, &action, NULL) == -1) {
        return -1;
    }
    return 1;
}

static void clean_quit(int signo)
{
    printf("Terminating early on signal %d\n", signo);
    hash_clear();
    exit(-1);
}

static int get_time(struct meminfo *minfo)
{
    time_t rawtime;

    time(&rawtime);
    minfo->timestap = *localtime(&rawtime);
    //printf("Current local time and date: %s", asctime(minfo->timestap));
    return 0;
}

static void clear_minfo(struct meminfo *minfo)
{
    int i;
    if (minfo == NULL)
        return;
    if (minfo->pss == NULL) {
        free(minfo);
        return;
    }

    for(i = 0; i < minfo->num_procs; i++)
        if (minfo->pss[i])
            free(minfo->pss[i]);

    free(minfo->pss);
    free(minfo);
}

int main(int argc, char *argv[])
{
    int c, index = 0, time = 0, count = 1;
    int pid = -1, ret, leak = 0;
    char *procn = NULL;
    char *outfile;

    /* option_name, has_arg(0: none, 1:recquired, 2 optional), flag, return_value) */
    static struct option long_opts[] = {
        {"help", 0, NULL, 'h'},
        {"version", 0, NULL, 'v'},
        {0, 0, NULL, 0}
    };

    while ((c=getopt_long(argc, argv, "f:t:lhv", long_opts, &index)) != EOF) {
        switch (c) {
        case 'f':
            count += 2;
            outfile = strdup(optarg);
            break;
        case 't':
            count += 2;
            if (isdigit(optarg[0]))
                time = atoi(optarg);
            else
                err_quit("time should be number\n");
            break;
        case 'l':
            count += 1;
            leak = 1;
            break;
        case 'v':
            printf("version 0.1\n");
            exit(0);
        case 'h':    /* fall through to default */
        default:
            usage(argv[0]);
            exit(0);
        }
    }

    if (argc - count == 1) {
        if (isdigit(argv[argc-1][0]))
            pid = atoi(argv[argc-1]);
        else
            procn = strdup(argv[argc-1]);
    } else if (argc -count > 1) {
            usage(argv[0]);
            exit(0);
    }

    if (leak == 1 && time == 0)
        time = 60;

    /*
     *  We want to catch the interrupt signal
     *  We should probably clean up memory
     *  and free up the hashtable before we go.
     */
    if (catch_sig(SIGINT, clean_quit) == -1) {
        err_quit("can't catch SIGINT signal.\n");
    }

    count = 0;
    do {
        if (pid != -1 || procn != NULL) {
            struct proc_info procs;
            if (procn != NULL)
                if ((pid = get_pid(procn)) == -1)
                    err_quit("process %s not running\n", procn);

            if (getprocname(pid, procs.cmdline, sizeof(procs.cmdline)) != 0)
                err_quit("count not find process of pid %d\n", pid);

            procs.pid = pid;
            if ((ret = get_pss(&procs)) == -1) {
                err_msg("get pss of pid %d error\n", pid);
                continue;
            }

            print_pss(&procs);
            if (leak) {
                hash_insert_item(&procs);
                count++;
            }
        } else {
            struct meminfo *minfo;
            minfo = calloc(1, sizeof(struct meminfo));
            if (minfo == NULL)
                err_sys("calloc meminfo error\n");

            get_time(minfo);
            get_procmem(minfo);
            get_mem(minfo);
            print_procmem(minfo);
            print_meminfo(minfo->item);

            if (leak) {
                hash_insert(minfo);
                count++;
            }
            clear_minfo(minfo);
        }

        if (leak) {
            detect_leak();
            if (!count % SHRINK_SIZE)
                hash_shrink();
        }
        if (time > 0) {
            printf("---------------------------------------------------------\n");
            sleep(time);
        }
    } while(time);

    return 0;
}
