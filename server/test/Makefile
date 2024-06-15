SRCS:=$(wildcard *.c)
OBJS:=$(patsubst %.c, %, $(SRCS))
CC:=gcc
FLAGS:=-g
LIBS:=-lcrypt

ALL:$(OBJS)

%:%.c
	$(CC)  $^ -o $@ $(FLAGS) $(LIBS)

clean:
	rm -rf $(OBJS)
