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
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "common.h"
#include "cjson/cJSON.h"
#include "epoll_server.h"

#define _CONF_FILE "conf.json"
#define HTML_404 "~/code/webserver/404.html"

extern advace_data advace_t;

int main(int argc, const char* argv[]) {
    //signal(SIGABRT, SIG_IGN);

    // 初始化advace_t
    advace_init(_CONF_FILE);
    int flag = 0;
    flag = DAMON(flag);
    if (flag == 1) {
        _printf("debug\n");
    } else if (flag < 0) {
        perror("daemon error");
        return -1;
    }
    
    _printf("port = %d\nworf_dir = %s\n404 = %s\n", advace_t.port, advace_t.work_dir, advace_t.html_404);


    // 执行核心逻辑
    // 端口
    int port = advace_t.port;
    // 修改进程的工作目录, 方便后续操作
    // 启动epoll模型 
    epoll_run(port);

    // 销毁advace_t
    advace_dis();

    return 0;
}
