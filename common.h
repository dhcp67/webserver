/*************************************************************************
	> File Name: common.h
	> Author: LiChun
	> Mail: 318082789@qq.com
	> Created Time: 2019年06月20日 星期四 18时49分06秒
 ************************************************************************/

#ifndef _COMMON_H
#define _COMMON_H

#ifdef D

#define DBG(format, ...) printf(format, ##__VA_ARGS__)

#else

#define DBG(a,b)
#endif

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <sys/file.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>

#define __MAX_N 1024

#define P(sem) sem_wait(sem)

#define V(sem) sem_post(sem)

#define swap(a, b) {\
    __typeof(a) tmp = a;\
    *a = *b;\
    *b = *tmp;\
}

#define _FALSE "错误"

#define LOG_FILE ".common.log"

#define CONF_FILE "conf.server"

//创建套接字
int socket_create(int);


int socket_connect(char *, int );

//非阻塞连接
int socket_connect_nonblock(char *, int, struct timeval *);

void *stop_out(int);

//接受连接信息
int socket_accept(int );

//等待函数
void my_sleep(int);

int get_conf_value(char *file, char *key, char *val); 

//替换字符串
void strreplace(char *s, char *a, char *b);

void _write_log(const char *errorfile, const int line, const char *Logfile,  const char *fmt, ...);

//快速排序
void quick_sort(int *data, int left, int right);

//选择排序
void select_sort(int *data, int length);

#define write_log(Logfile, fmt,...) _write_log(__FILE__, __LINE__, Logfile, ##__VA_ARGS__)

#endif
