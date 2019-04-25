#ifndef COMPARATOR_H
#define COMPARATOR_H

#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

/*
* Compares last modify time of two files given as arguments.
*/
int compare_mtime(char* p1, char* p2);

/*
* Compares size of two files given as arguments.
*/
int compare_size(char * p1, int div_size);

#endif
