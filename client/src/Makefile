Srcs   := $(wildcard *.c)
CC     := gcc
CFLAGS := -g
LIBS   := -lpthread -lmysqlclient -lcrypt -lssl -lcrypto -ll8w8jwt
 
# all in one
Objs   := $(patsubst %.c, %.o, $(Srcs))
Out    := main
ONE: $(Out)
$(Out): $(Objs)
	$(CC) $^ -o $@ $(CFLAGS) $(LIBS)
%.o: %.c
	$(CC) -c $< $(CFLAGS) $(LIBS)

# one by one
Outs   := $(patsubst %.c, %, $(Srcs))
ALL: $(Outs)
%: %.c
	$(CC) $< -o $@ $(CFLAGS) $(LIBS)

.PHONY: clean rebuild ONE ALL
clean:
	$(RM) $(Objs) $(Out) $(Outs)
rebuild: clean $(Out)
