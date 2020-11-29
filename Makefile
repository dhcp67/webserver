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
SrcFiles = $(wildcard *.c)

ifeq ($(v), debug)
ALL: debug
# 内容替换
ObjectFiles = $(patsubst %.c,d%.o,$(SrcFiles))
CxxArgs = -g -DD -Wall -c -I./include
else
ALL: webserver
ObjectFiles = $(patsubst %.c,%.o,$(SrcFiles))
CxxArgs = -c -O3 -I./include
endif

#$^ 全部依赖文件
server : $(ObjectFiles)
	${CC} -o $@ $^ -lpthread 
	@echo -e "\033[32m make relese version OK\033[0m"

debug : $(ObjectFiles)
	${CC} -o $@ $^ -lpthread 
	@echo -e "\033[32m  make debug version OK\033[0m"

#$< 第一个依赖文件
%.o : %.c
	${CC} ${CxxArgs} $< -o $@ 

d%.o : %.c
	${CC} ${CxxArgs} $< -o $@ 

#.PHONY: clear
clear: 
	-@rm -f *.o debug a.out server
	@echo "clear OK"

#.PHONY:run
run :
	./debug

