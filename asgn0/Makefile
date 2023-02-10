# Author:	Miguel Avalos
# Cruzid:	1704078
# Date:		4.11.2021
# File:		Makefile
# Purpose:	Compiles shoulders binary file 
# Note:		adapted from CMPS 12B course

shoulders : shoulders.o
	gcc -o shoulders shoulders.o
	chmod +x shoulders

shoulders.o : shoulders.c
	gcc -c -std=c99 -Wall shoulders.c

clean :
	rm -f shoulders shoulders.o