/*************************************************************************
*	 File Name: epoll_server.h
*	 Author: LiChun
*	 Mail: trainlee@gmail.com
*	 Created Time: 2020年11月21日 星期六 21时09分59秒
 ************************************************************************/

#ifndef _EPOLL_SERVER_H
#define _EPOLL_SERVER_H


#ifdef D

#define _perror(a) perror(a)
#define _printf(format, ...)     printf(format, ##__VA_ARGS__)
#define DAMON(a) 1

#else

#define DAMON(a) creat_daemon(a) 

#define _perror(a)
#define _printf(format, ...)

#endif


typedef struct advace_data{
    int port;
    char *html_404;
    char *work_dir;
    pthread_mutex_t mutex;
} advace_data;

//advace_data advace_t;

int creat_daemon(int); // 创建守护进程
void advace_init(const char *); // 初始化advace_t
void advace_dis();
int init_listen_fd(int port, int epfd);
void epoll_run(int port);
void do_accept(int lfd, int epfd);
void do_read(void *);
int get_line(int sock, char *buf, int size);
void disconnect(int cfd, int epf);
void http_request(const char* request, int cfd);
void send_respond_head(int cfd, int no, const char* desp, const char* type, long len);
void send_file(int cfd, const char* filename);
void send_dir(int cfd, const char* dirname);
void encode_str(char* to, int tosize, const char* from);
void decode_str(char *to, char *from);
const char *get_file_type(const char *name);

#endif
