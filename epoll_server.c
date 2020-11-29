/*************************************************************************
*    File Name: epoll_server.c
*    Author: LiChun
*    Mail: trainlee@gmail.com
*    Created Time: 2020年11月21日 星期六 21时08分38秒
*************************************************************************/

#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <sys/sendfile.h>
#include <sys/mman.h>
#include "epoll_server.h"
#include "threadpool/threadpool.h"
#include "common.h"
#include "cjson/cJSON.h"

#define MAXSIZE 2000
#define _MAXLENGTH 4096
#define _LENGTH 1024                
#define _THREADPOOLRUN_SIZE 20 

pthread_mutex_t mute;
advace_data advace_t;

typedef struct epoll_transmit_data{
    int fd;
    int epfd;
}epoll_event_t;


void *reactor(void *arg) {
    //signal(SIGABRT, SIG_IGN);
    int epfd = *((int*)arg);

    // 委托内核检测添加到树上的节点
    struct epoll_event all[MAXSIZE];

    threadpool_t *pool = threadpool_init(_THREADPOOLRUN_SIZE);
    
    // 初始化锁
    pthread_mutex_init(&mute, NULL);
    while(1) {
        int ret = epoll_wait(epfd, all, MAXSIZE, -1);
        if(ret == -1) {
            _perror("epoll_wait error");
            exit(1);
        }

        // 遍历发生变化的节点
        int i = 0;
        for(i = 0; i < ret; ++i) {
            // 只处理读事件, 其他事件默认不处理
            struct epoll_event *pev = &all[i];
            if(!(pev->events & EPOLLIN)) {
                // 不是读事件
                continue;
            }

            // 进入线程读写数据
            // 申请空间，在do_read中释放
            epoll_event_t *p = (void *)malloc(sizeof(epoll_event_t));
            p->fd = pev->data.fd;
            p->epfd = epfd;
            threadpool_add(pool, do_read, (void *)p);
        }
    }
    // 销毁互斥锁
    pthread_mutex_destroy(&mute);
    threadpool_destroy(pool, graceful_shutdown);
    return NULL;
}

void epoll_run(int port) {

    _printf("port = %d\nworf_dir = %s\n404 = %s\n", advace_t.port, advace_t.work_dir, advace_t.html_404);
    // 创建一个从epoll树的根节点
    int sub_fd = epoll_create(MAXSIZE);
    if(sub_fd == -1) {
        _perror("epoll_create error");
        exit(1);
    }

    // 创建主epoll树根节点
    int master_fd = epoll_create(1); 
    // 创建要监听的套接字
    int lfd = init_listen_fd(port, master_fd);

    // 创建处理线程
    pthread_t pth_t;
    pthread_create(&pth_t, NULL, reactor, (void *)&sub_fd);
    pthread_detach(pth_t);

    // 委托内核检测添加到树上的节点
    struct epoll_event all[MAXSIZE];
    while(1) {
        int ret = epoll_wait(master_fd, all, MAXSIZE, -1);
        if(ret == -1) {
            _perror("epoll_wait error");
            exit(1);
        }
        _printf("有%d个连接\n", ret);

        // 遍历发生变化的节点
        int i = 0;
        for(i = 0; i < ret; ++i) {
            // 只处理读事件, 其他事件默认不处理
            struct epoll_event *pev = &all[i];
            // 如果是读事件
            if(pev->events & EPOLLIN) {
                // 处理新连接并加到epoll树
                do_accept(lfd, sub_fd);
            }
        }
    }
}

// 读数据
void do_read(void *void_p) {
    // 忽略SIGPIPE信号
    sigset_t signal_mask;
    sigemptyset (&signal_mask);
    sigaddset (&signal_mask, SIGPIPE);
    int rc = pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
    //if (rc != 0) {
    //    _printf("block sigpipe error/n");
    //}

    // 将浏览器发过来的数据, 读到buf中 
    //char line[1024] = {0};
    int cfd = ((epoll_event_t *)void_p)->fd;
    int epfd = ((epoll_event_t *)void_p)->epfd;
    // 释放void，在reactor中申请
    if (void_p)free(void_p);

    char *line = (char *)calloc(sizeof(char), _MAXLENGTH);
    // 读请求行
    //int len = get_line(cfd, line, sizeof(line));
    int len = get_line(cfd, line, _MAXLENGTH);
    if(len == 0) {
        _printf("客户端断开了连接...\n");
    }
    else {
        _printf("请求行数据: %s", line);
        _printf("============= 请求头 ============\n");
        // 还有数据没读完
        char *buf = (char *)calloc(sizeof(char), _MAXLENGTH);
        // 继续读
        while(len) {
            //char buf[1024] = {0};
            //len = get_line(cfd, buf, sizeof(buf));
            len = get_line(cfd, buf, _MAXLENGTH);
            _printf("-----: %s", buf);
        }
        free(buf);
        _printf("============= The End ============\n");
    }

    // 请求行: get /xxx http/1.1
    // 判断是不是get请求
    if(strncasecmp("get", line, 3) == 0) {
        // 处理http请求
        http_request(line, cfd);
    }
    // 关闭套接字, cfd从epoll上del
    disconnect(cfd, epfd);         
    // 释放line空间
    free(line);
}

// 断开连接的函数
void disconnect(int cfd, int epfd) {

    // 加锁
    pthread_mutex_lock(&mute);
    
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    if(ret == -1) {
        _perror("epoll_ctl del cfd error");
        //exit(1);
    }
    _printf("用户已经断开\n");
    close(cfd);

    // 解锁
    pthread_mutex_unlock(&mute);

}

// http请求处理
void http_request(const char* request, int cfd) {
    // 拆分http请求行
    // get /xxx http/1.1
    char method[12], path[1024], protocol[12];
    sscanf(request, "%[^ ] %[^ ] %[^ ]", method, path, protocol);

    _printf("method = %s, path = %s, protocol = %s\n", method, path, protocol);

    // 转码 将不能识别的中文乱码 - > 中文
    // 解码 %23 %34 %5f
    decode_str(path, path);
    // 处理path  /xx
    // 去掉path中的/
    char* file = path+1;
    // 如果没有指定访问的资源, 默认显示资源目录中的内容
    if(strcmp(path, "/") == 0) {
        // file的值, 资源目录的当前位置
        file = ".";
    }

    // 获取文件属性
    struct stat st;
    int ret = stat(file, &st);
    if(ret == -1) {
        // show 404
        _perror("No file");
        send_respond_head(cfd, 404, "File Not Found", ".html", -1);
        // 发送 404.html
        _printf("%s\n", advace_t.html_404);
        send_file(cfd, advace_t.html_404);
    }
    // 判断是目录还是文件
    // 如果是目录
    else if(S_ISDIR(st.st_mode)) {
        // 发送头信息
        send_respond_head(cfd, 200, "OK", get_file_type(".html"), -1);
        // 发送目录信息
        send_dir(cfd, file);
    } else if(S_ISREG(st.st_mode)) {
        // 文件
        // 发送消息报头
        send_respond_head(cfd, 200, "OK", get_file_type(file), st.st_size);
        // 发送文件内容
        send_file(cfd, file);
    }
}

// 发送目录内容
void send_dir(int cfd, const char* dirname) {
    // 拼一个html页面<table></table>
    char buf[4094] = {0};

    sprintf(buf, "<html><head><title>目录名: %s</title></head>", dirname);
    sprintf(buf+strlen(buf), "<body><h1>当前目录: %s</h1><table>", dirname);

    char enstr[1024] = {0};
    char path[1024] = {0};
    // 目录项二级指针
    struct dirent** ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort);
    // 遍历
    int i = 0;
    for(i = 0; i < num; ++i) {
        char* name = ptr[i]->d_name;

        // 拼接文件的完整路径
        sprintf(path, "%s/%s", dirname, name);
        _printf("path = %s ===================\n", path);
        struct stat st;
        stat(path, &st);

        encode_str(enstr, sizeof(enstr), name);
        // 如果是文件
        if(S_ISREG(st.st_mode)) {
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        }
        // 如果是目录
        else if(S_ISDIR(st.st_mode)) {
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        }
        send(cfd, buf, strlen(buf), 0);
        memset(buf, 0, sizeof(buf));
        // 字符串拼接
    }

    sprintf(buf+strlen(buf), "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);

    _printf("dir message send OK!!!!\n");
#if 0
    // 打开目录
    DIR* dir = opendir(dirname);
    if(dir == NULL) {
        _perror("opendir error");
        exit(1);
    }

    // 读目录
    struct dirent* ptr = NULL;
    while( (ptr = readdir(dir)) != NULL ) {
        char* name = ptr->d_name;
    }
    closedir(dir);
#endif
}

// 发送响应头
void send_respond_head(int cfd, int no, const char* desp, const char* type, long len) {
    _printf("file type == %s\n", type);
    char *buf = (char *)calloc(sizeof(char), _MAXLENGTH);
    // 状态行
    sprintf(buf, "http/1.1 %d %s\r\n", no, desp);
    send(cfd, buf, strlen(buf), 0);
    // 消息报头
    sprintf(buf, "Content-Type:%s\r\n", type);
    sprintf(buf+strlen(buf), "Content-Length:%ld\r\n", len);
    send(cfd, buf, strlen(buf), 0);
    // 空行
    send(cfd, "\r\n", 2, 0);
    free(buf);
}

// 发送
int socket_send(int fd, const char* usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = (char *)usrbuf;

    while(nleft > 0){
        if((nwritten = write(fd, bufp, nleft)) <= 0){
            if (errno == EINTR)
                nwritten = 0;
            else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                nwritten = 0;
                //usleep(100);
            }
            else{
                return -1;
            }
        } else {
            nleft -= nwritten;
            bufp += nwritten;
            _printf("总 = %lu 剩 = %lu\n", n, nleft);
        }
    }
    return 0;
}



// 发送文件
void send_file(int cfd, const char* filename) {
    // 打开文件
    _printf("filename = %s\n" , filename);
    int fd = open(filename, O_RDWR);
    if(fd == -1) {
        // show 404
        close(fd);
        return;
    }

    // 循环读文件
    //char buf[_MAXLENGTH * 10] = {0};
    struct stat st;
    stat(filename, &st);

    size_t len = st.st_size;

    char *fileadd = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    int ret = socket_send(cfd, fileadd, st.st_size);
    if (ret < 0) {
        _perror("send file error");
    }
    munmap(fileadd, st.st_size);
    _printf("this ----------------------------\n");
    if(len == -1) {
        _perror("read file error");
        close(fd);
        exit(1);
    }

    close(fd);
    // 释放buf空间
}

// 解析http请求消息的每一行内容
int get_line(int sock, char *buf, int size) {
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n')) {
        n = recv(sock, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n')) {
                    recv(sock, &c, 1, 0);
                }
                else {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        }
        else {
            c = '\n';
        }
    }
    buf[i] = '\0';

    return i;
}

// 接受新连接处理并放入epoll树
void do_accept(int lfd, int epfd) {
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    int cfd = accept(lfd, (struct sockaddr*)&client, &len);
    if(cfd == -1) {
        _perror("accept error");
        exit(1);
    }

    // 打印客户端信息
    char ip[64] = {0};
    _printf("New Client IP: %s, Port: %d, cfd = %d\n",
           inet_ntop(AF_INET, &client.sin_addr.s_addr, ip, sizeof(ip)),
           ntohs(client.sin_port), cfd);

    // 设置cfd为非阻塞
    int flag = fcntl(cfd, F_GETFL);

    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    // 得到的新节点挂到epoll树上
    struct epoll_event ev;
    ev.data.fd = cfd;

    // 边沿非阻塞模式

    ev.events = EPOLLIN | EPOLLET;
    pthread_mutex_lock(&mute);
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
    pthread_mutex_unlock(&mute);
    if(ret == -1) {
        _perror("epoll_ctl add cfd error");
        exit(1);
    }
}

int init_listen_fd(int port, int epfd) {
    //　创建监听的套接字
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if(lfd == -1) {
        _perror("socket error");
        exit(1);
    }

    // lfd绑定本地IP和port
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);

    // 端口复用
    int flag = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    int ep_flag = fcntl(epfd, F_GETFL);

    ep_flag |= O_NONBLOCK;
    fcntl(epfd, F_SETFL, ep_flag);

    int ret = bind(lfd, (struct sockaddr*)&serv, sizeof(serv));
    if(ret == -1) {
        _perror("bind error");
        exit(1);
    }

    // 设置监听
    ret = listen(lfd, MAXSIZE);
    if(ret == -1) {
        _perror("listen error");
        exit(1);
    }

    // lfd添加到epoll树上
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if(ret == -1) {
        _perror("epoll_ctl add lfd error");
        exit(1);
    }

    return lfd;
}

// 16进制数转化为10进制
int hexit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

/*
 *  这里的内容是处理%20之类的东西！是"解码"过程。
 *  %20 URL编码中的‘ ’(space)
 *  %21 '!' %22 '"' %23 '#' %24 '$'
 *  %25 '%' %26 '&' %27 ''' %28 '('......
 *  相关知识html中的‘ ’(space)是&nbsp
 */
//编码，用作回写浏览器的时候，将字母数字及/_.-~以外的字符串转义后回写
void encode_str(char* to, int tosize, const char* from) {
    int tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0)  {
            *to = *from;
            ++to;
            ++tolen;
        } 
        else  {
            sprintf(to, "%%%02x", (int) *from & 0xff);
            to += 3;
            tolen += 3;
        }

    }
    *to = '\0';
}


void decode_str(char *to, char *from) {
    for ( ; *from != '\0'; ++to, ++from  )  {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))  { 

            *to = hexit(from[1])*16 + hexit(from[2]);

            from += 2;                      
        } 
        else {
            *to = *from;

        }

    }
    *to = '\0';

}

// 通过文件名获取文件的类型
const char *get_file_type(const char *name) {
    char* dot;

    // 自右向左查找‘.’字符, 如不存在返回NULL
    dot = strrchr(name, '.');   
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav" ) == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}

int creat_daemon(int flag) {
    // 创建子进程，父进程退出
    int pid = fork();
    if (pid == -1) {
        return -1;
    } else if (pid > 0) {
        exit(0);
    }
    // 当会长
    setsid();
    // 设置掩码
    umask(0);
    // 切换目录
    int ret;
    if (advace_t.work_dir != NULL) {
        ret = chdir(advace_t.work_dir); // 切换到家目录
    } else {
        ret = chdir(getenv("HOME")); // 切换到家目录
    }
    _printf("%s\n", getenv("HOME"));
    _printf("-------------- (%s)\n", advace_t.work_dir);
    if(ret == -1) {
        return -1;
    }
    
    // 关闭文件描述符
    close(0);
    close(1);
    //close(2);
    return 0;
}

// advace_t初始化
void advace_init(const char *conf_file) {
    _printf("config file %s\n", conf_file);
    int fd = open(conf_file, O_RDONLY);
    if (fd < 1) {
        _perror("open file error");
        exit(1);
    }
    struct stat st;
    stat(conf_file, &st);
    // 文件映射
    char *m = (char *)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (m == NULL) {
        _perror("perror");
        exit(1);
    }

    cJSON *js = cJSON_Parse(m);
    // 读取端口
    cJSON *port_js = cJSON_GetObjectItem(js, "port");
    // 读取工作目录
    cJSON *work_dir_js = cJSON_GetObjectItem(js, "work_dir");
    // 读取404.html目录
    cJSON *htm_404 = cJSON_GetObjectItem(js, "html_404");
    
    // 取出值并给advace_t
    char *tmp = work_dir_js->valuestring;
    advace_t.work_dir = (char *)malloc(strlen(tmp) + 1);
    strcpy(advace_t.work_dir, tmp);

    tmp = htm_404->valuestring;
    advace_t.html_404 = (char *)malloc(strlen(tmp) + 1);
    strcpy(advace_t.html_404, tmp);

    advace_t.port = port_js->valueint;

    if (m) munmap(m, st.st_size);
    if (port_js) cJSON_DeleteItemFromObject(port_js, "port");
    if (work_dir_js) cJSON_DeleteItemFromObject(work_dir_js, "work_dir");
    if (htm_404) cJSON_DeleteItemFromObject(htm_404, "html_404");
    if (js) cJSON_Delete(js);
    close(fd);
}

// advace_t销毁
void advace_dis() {
    if (advace_t.html_404) free(advace_t.html_404);
    if (advace_t.work_dir) free(advace_t.work_dir);
}

