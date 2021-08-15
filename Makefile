PROG = ryacc

SRCS = ryacc.c
HEADERS =
OBJS = $(SRCS:.c=.o)

cc = gcc
yacc = yacc
CFLAGS = -Wall -g -O2
#LDFLAGS += -lncurses

all: $(PROG)

clean:
	rm -f $(OBJS) $(PROG)

$(PROG): $(OBJS) $(HEADERS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
