# Author:	Miguel Avalos
# Cruzid:	1704078
# Date:		5.1.2021
# File:		Makefile
# Purpose:	Compliles single file C code 

EXECBIN = httpproxy
FLAGS 	= -Wall -Wextra -Wpedantic -Wshadow

${EXECBIN} : ${EXECBIN}.o
	gcc -o $@ $<
	chmod +x $@

 %.o : %.c
	gcc -c -std=c99 ${FLAGS} $<

clean :
	rm -f ${EXECBIN} ${EXECBIN}.o