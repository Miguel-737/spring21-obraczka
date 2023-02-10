# Author:	Miguel Avalos
# Cruzid:	1704078
# Date:		5.1.2021
# File:		Makefile
# Purpose:	Compliles single file C code 

EXECBIN = httpserver
FLAGS 	= -Wall -Wextra -Wpedantic -Wshadow

${EXECBIN} : ${EXECBIN}.o
	gcc -pthread -o $@ $<
	chmod +x $@

 %.o : %.c
	gcc -c -std=c99 ${FLAGS} $<

clean :
	rm -f ${EXECBIN} ${EXECBIN}.o