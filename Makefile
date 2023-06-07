EXE=exe

OBJS=testcase.o ringbuffer.o

CC=g++

CFLAGS=-Wall -g -std=c++11 -pthread

$(EXE): $(OBJS) Makefile
	$(CC) $(CFLAGS) $(OBJS) -o $@

%.o: %.cpp
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS) exe
