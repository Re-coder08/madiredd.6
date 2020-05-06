CC=gcc
CFLAGS=-Wall -g -pedantic -D_XOPEN_SOURCE

sim: oss user

oss: oss.c oss_time.o queue.o pqueue.o mem.o child.o lock.o oss.h
	$(CC) $(CFLAGS) oss.c oss_time.o queue.o pqueue.o mem.o child.o lock.o -o oss -lrt

user: user.o oss_time.o mem.o child.o lock.o oss.h
	$(CC) $(CFLAGS) user.o oss_time.o mem.o child.o lock.o -o user

user.o: user.c child.h
	$(CC) $(CFLAGS) -c user.c

oss_time.o: oss_time.h oss_time.c
	$(CC) $(CFLAGS) -c oss_time.c

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) -c queue.c

pqueue.o: pqueue.c pqueue.h
	$(CC) $(CFLAGS) -c pqueue.c

mem.o: mem.c mem.h
	$(CC) $(CFLAGS) -c mem.c

clean:
	rm -f ./oss ./user *.log *.o output.txt
