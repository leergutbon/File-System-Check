#
# Makefile for File-System-Check
#

CC=gcc
CFLAGS=-Wall -m32 -g -pedantic -ansi

all:		prog

prog: checkfs.c
		$(CC) $(CFLAGS) -o checkfs checkfs.c

clean:
		rm checkfs

debug: checkfs
		rm checkfs
		$(CC) $(CFLAGS) -o checkfs checkfs.c
		gdb checkfs
