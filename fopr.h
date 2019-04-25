#ifndef FOPR_H
#define FOPR_H

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
* Creates full path of the file based on actual relative path
*/
char * create_full_path(char*, char*);

/*
* Checks if file is regular file (not directory or symlink)
*/
int is_regular_file(const char *);

/*
* Checks if file is directory (not regular file or symlink)
*/
int is_directory(const char *);

#endif
