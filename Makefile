
CC=gcc
CFLAGS=-O0 -g -Wall -std=gnu11
INCLUDES=-I.
LIBS=-lm

TESTS=tests

SRCS=$(TESTS)/app.c dhc.c
OBJS = $(SRCS:.c=.o)
MAIN = app
MAIN_LIB = dhc_lib.so

SRCS_AD=$(TESTS)/audio_demo.c dhc.c
OBJS_AD = $(SRCS_AD:.c=.o)
MAIN_AD = audio_demo

SRCS_ABD=$(TESTS)/audio_block_demo.c dhc.c
OBJS_ABD = $(SRCS_ABD:.c=.o)
MAIN_ABD = audio_block_demo

SRCS_TB=$(TESTS)/test_block.c dhc.c
OBJS_TB = $(SRCS_TB:.c=.o)
MAIN_TB = test_block

.PHONY: depend clean test

all: $(MAIN) $(MAIN_AD) $(MAIN_ABD)
	@echo  Done

$(MAIN_AD): $(OBJS_AD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN_AD) $(OBJS_AD) $(LIBS)

$(MAIN_ABD): $(OBJS_ABD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN_ABD) $(OBJS_ABD) $(LIBS)

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LIBS)

$(MAIN_TB): $(OBJS_TB)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN_TB) $(OBJS_TB) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

lib:
	$(CC) $(CFLAGS) $(INCLUDES) -shared -fPIC -o $(MAIN_LIB) dhc.c
	cp $(MAIN_LIB) ./python_bindings/

test: $(MAIN_TB)
	./$(MAIN_TB)

clean:
	$(RM) *.o *~ $(TESTS)/*.o $(MAIN) $(MAIN_AD) $(MAIN_ABD) $(MAIN_TB) $(MAIN_LIB)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

