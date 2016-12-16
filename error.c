/*
 * this source file include the error functon
 */
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAXLINE 1024

static void err_doit(int,int,const char *,va_list);

/*
 * Nonfatal error related to a system call.
 * Print a message and return.
 */
void err_ret(const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	err_doit(1,errno,fmt,ap);
	va_end(ap);
}

/*
 * fatal error related to a system call.
 * print a message and terminate.
 */
void err_sys(const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	err_doit(1,errno,fmt,ap);
	va_end(ap);
	exit(1);
}

/*
 * fatal error unrelated to a system call
 * Error code passed as explict parameter.
 * print a message and terminate.
 */
void err_exit(int error,const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	err_doit(1,error,fmt,ap);
	va_end(ap);
	exit(1);
}

/*
 * fatal error related to a system call
 * print a message, dump core, and terminate
 */
void err_dump(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	err_doit(1, errno, fmt, ap);
	va_end(ap);
	abort();
	exit(1);	/*shouldn't be here */
}

/*
 * Nonfatal error unrelated to system call.
 * Print a message and return.
 */
void err_msg(const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	err_doit(0,0,fmt,ap);
	va_end(ap);
}

/* fatal error unrelated to a system call.
 * print a message and terminate.
 */
void err_quit(const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	err_doit(0,0,fmt,ap);
	va_end(ap);
	exit(1);
}

/*
 * print a message and return to caller.
 * caller specifies "errnoflag".
 */
static void err_doit(int errnoflag,int error,const char *fmt,va_list ap)
{
	char buf[MAXLINE];
	vsnprintf(buf,MAXLINE,fmt,ap);
	if(errnoflag)
		snprintf(buf+strlen(buf),MAXLINE-strlen(buf),":%s",strerror(error));
	strcat(buf,"\n");
	fflush(stdout);		/*in case stdout and stderr are the same */
	fputs(buf,stderr);
	fflush(NULL);		/* flushes all stdio ouput streams */
}
