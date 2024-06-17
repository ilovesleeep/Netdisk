Srcs   := $(wildcard *.c)
CC     := gcc
CFLAGS := -Wall -g -lpthread

# all in one
Objs   := $(patsubst %.c, %.o, $(Srcs))
Out := main
ONE: $(Out)
$(Out): $(Objs)
	$(CC) $^ -o $@
%.o: %.c
	$(CC) -c $< $(CFLAGS)

# one by one
Outs := $(patsubst %.c, %, $(Srcs))
ALL: $(Outs)
%: %.c
	$(CC) $< -o $@ $(CFLAGS)

.PHONY: clean rebuild ONE ALL
clean:
	$(RM) $(Objs) $(Out) $(Outs)
rebuild: clean $(Out)
