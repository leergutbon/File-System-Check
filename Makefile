#
# Makefile for File-System-Check
#

CC=gcc
CFLAGS=-Wall -g -pedantic -ansi

all:		prog

prog: main.c
		$(CC) $(CFLAGS) -o checkfs main.c

clean:
		rm checkfs

debug: main
		rm checkfs
		$(CC) $(CFLAGS) -o fsc_debug main.c
		gdb fsc_debug
