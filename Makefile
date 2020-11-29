#------------------------------------------------------#
#- Author: Li Chun
#- Mail: trainlee1024@gmail.com
#------------------------------------------------------#
# make_file

# v=debug,默认为debug版本
# make 为debug版
# make v=[任意] 为正式版

v = debug
CC = gcc
#wildcard 文件匹配
SrcFiles = $(wildcard *.c) $(wildcard threadpool/*.c) $(wildcard cjson/*.c)
ObjectFiles = $(patsubst %.c,%.o,$(SrcFiles))

ifeq ($(v), debug)
ALL: debug
# 内容替换
CxxArgs = -g -DD -Wall -c -I./include 
else
ALL: webserver
CxxArgs = -c -O3 -I./include
endif

#$^ 全部依赖文件
webserver : $(ObjectFiles)
	${CC} -o $@ $^ -lpthread 
	@echo -e "\033[32m make relese version OK\033[0m"

debug : $(ObjectFiles)
	${CC} -o $@ $^ -lpthread 
	@echo -e "\033[32m  make debug version OK\033[0m"

#$< 第一个依赖文件
%.o : %.c
	${CC} ${CxxArgs} $< -o $@ 

#.PHONY: clean
clean: 
	-@rm -f *.o debug webserver */*.o
	@echo "clear OK"

#.PHONY:run
run : webserver
	./webserver

#.PHONY:kill
kill:
	pkill webserver
