#
# Makefile for File-System-Check
#

CC=gcc
CFLAGS=-Wall -g -pedantic -ansi

all:		prog

prog: main.c
		$(CC) $(CFLAGS) -o fsc main.c

clean:
		rm fsc

debug: main
		rm fsc
		$(CC) $(CFLAGS) -o fsc_debug main.c
		gdb fsc_debug
