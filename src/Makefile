
CC=gcc
CFLAGS=-g 
CPPFLAGS=-Wall -Werror

PROGS=shell

.PHONY: all
all: $(PROGS)

shell: shell.o shell-utils.o

%.o: %.c
	$(CC) -MMD -MF .$*.d $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

%: %.o
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

DEPENDS=$(wildcard .*.d)
include $(DEPENDS)

.PHONY: clean
clean:
	$(RM) $(PROGS) *.o .*.d
