# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your test from sources.

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: test

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

test.o: test.c csapp.h
	$(CC) $(CFLAGS) -c test.c

test: test.o csapp.o
	$(CC) $(CFLAGS) test.o csapp.o -o test $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude test --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o test core *.tar *.zip *.gzip *.bzip *.gz

