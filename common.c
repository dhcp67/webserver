/*************************************************************************
	> File Name: common.c
	> Author: LiChun
	> Mail: 318082789@qq.com
	> Created Time: 2019年06月21日 星期五 19时14分04秒
 ************************************************************************/

#include "common.h"

//创建套接字
int socket_create(int port) {
    int sockfd;
    struct sockaddr_in addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    //协议族
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int on = 1;
    //io复用
    if((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))<0) {  
        perror("setsockopt failed");  
        exit(1);  
    }  

    //绑定
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        return -1;

    }

    //监听
    if (listen(sockfd, 20) < 0) {
        perror("listen ERROR\n");
        return -1;
    }

    return sockfd;
}


//non_block非阻塞连接
int socket_connect_nonblock(char *host, int port, struct timeval *tv) {
    int sockfd, ret;
    struct sockaddr_in server;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(host);
    
    //printf("Connect to %s : %d \n", host, port);

    unsigned long ul = 1;
    ioctl(sockfd, FIONBIO, &ul);
    
    fd_set write;
    FD_ZERO(&write);
    FD_SET(sockfd, &write);

    int error = -1;
    int len;
    len = sizeof(error);
    ret = sockfd;

    if ((connect(sockfd, (struct sockaddr *)&server, sizeof(server))) < 0) {
        if ((select(sockfd + 1, NULL, &write, NULL, tv)) > 0) {
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len) < 0) {
                ret = -1;
            }
            if (error == 0) {
                ret = 0;
            } else {
                ret = -1;
            }
        }
    } 

    return ret;
}

int socket_connect(char *host, int port) {
    int sockfd;
    struct sockaddr_in server;
        
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }
    //test
    //ioctl(sockfd, FIONBIO, &ul);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(host);
    if ((connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)) {
        //perror("connect()");
        return -1;
    } 
    return sockfd;
}
int socket_accept(int server_listen) {
    int sockfd = accept(server_listen, NULL, NULL);
    return sockfd;
}

void _write_log(const char *errorfile, const int line, const char *Logfile,  const char *fmt, ...) {
    //打开日志文件
    FILE *fp = fopen(Logfile, "a+");
    //文件锁
    flock(fp->_fileno, LOCK_EX);

    //变参列表
    va_list arg;
    va_start(arg, fmt);
    //时间
    time_t time_log = time(NULL);
    struct tm *tm_log =localtime(&time_log);
    fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] [%s: %d] ", tm_log->tm_year + 1900, tm_log->tm_mon + 1, tm_log->tm_mday, tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec,errorfile, line);
    //成功返回总字符数，失败返回-1
    vfprintf(fp, fmt, arg);
    va_end(arg);
    fflush(fp);
    fclose(fp);
    return ;
}


//获取配置文件
int get_conf_value(char *file, char *key, char *val) {
    int ret = -1;                               //定义一个返回值
    FILE *fp = NULL;                            //定义文件指针
    char *line = NULL, *substr = NULL;          //定义行指针,子字符串指针
    size_t n = 0, len = 0;                      //定义n，长度len
    ssize_t read;                               //定义一个记录返回长度的变量

    if (key == NULL) {                          //如果key为空
        write_log(LOG_FILE, "%s", "获取配置文件key错误");                      //报错
        return ret;
    }
    fp = fopen(file, "r");                      //以只读打开文件

    if (fp == NULL) {                           //打开失败
        write_log(LOG_FILE, "%s", "配置文件未能成功打开");    //报错
        return ret;
    }

    while ((read = getline(&line, &n, fp))) {   //循环读取行
        substr = strstr(line, key);             //查找key是否在
        if (substr == NULL) {                   //如果没在
            continue;                           //跳过本次循环
        } else { 
            len = strlen(key);                  //求取长度
            if (line[len] == '=') {             //Key后面是否为=
                strncpy(val, line + len + 1, (int)read - len - 2); //找到了配置赋值给val
                ret = 1;
                break;
            } else {
                continue;                       //否则跳过本次循环
            }
        }
    }
    if (substr == NULL) {                       //如果没找到 
        write_log(LOG_FILE, "%s", "配置文件未正确填写");
        ret = -1;
    }
    fclose(fp);                                 //关闭文件
    free(line);                                 //释放line
    return 0;
}

