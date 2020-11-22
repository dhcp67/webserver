/*************************************************************************
*    File Name: main.c
*    Author: LiChun
*    Mail: trainlee@gmail.com
*    Created Time: 2020年11月21日 星期六 21时08分38秒
*************************************************************************/


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include "epoll_server.h"
#include "threadpool.h" 

inline void creat_daemon(int a) {
    // 创建子进程，父进程退出
    int pid = fork();
    if (pid == -1) {
        exit(-1);
    } else if (pid > 0) {
        exit(0);
    }
    // 当会长
    setsid();
    // 设置掩码
    umask(0);
    // 切换目录
    chdir(getenv("HOME")); // 切换到家目录
    // 关闭文件描述符
    close(0);
    close(1);
    close(2);
}

int main(int argc, const char* argv[]) {
    //signal(SIGABRT, SIG_IGN);

    int a = 0;
    DAMON(a);
    // 执行核心逻辑

    if(argc < 3) {
        printf("eg: ./a.out port path\n");
        exit(1);
    }

    // 端口
    int port = atoi(argv[1]);
    // 修改进程的工作目录, 方便后续操作
    int ret = chdir(argv[2]);
    if(ret == -1) {
        perror("chdir error");
        exit(1);
    }
    
    // 启动epoll模型 
    epoll_run(port);

    return 0;
}
