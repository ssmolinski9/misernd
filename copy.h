#ifndef COPY_H
#define COPY_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

/*
* Copies file with unix system call like open or wirte
*/
int copy_normal(int, char **);

/*
* Copies file with mapping on memory (nmap) for bigger files 
*/
int copy_with_mapping(int, char **);

#endif
