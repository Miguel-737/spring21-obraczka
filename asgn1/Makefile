# Author:	Miguel Avalos
# Cruzid:	1704078
# Date:		4.11.2021
# File:		Makefile
# Purpose:	Compiles shoulders binary file 
# Note:		adapted from CMPS 12B course

EXECBIN = httpserver
FLAGS 	= -Wall -Wextra -Wpedantic -Wshadow

${EXECBIN} : ${EXECBIN}.o
	gcc -o $@ $<
	chmod +x $@

 %.o : %.c
	gcc -c -std=c99 ${FLAGS} $<

clean :
	rm -f ${EXECBIN} ${EXECBIN}.o