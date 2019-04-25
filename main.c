/* Misern Daemon – linux system service for two directories synchronization
 * @author Sebastian Smoliński, Ernest Stachelski, Michał Fiłończuk
 * @version 1.3
 * @date 2019-04-18
 */

#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "copy.h"
#include "comparator.h"
#include "fopr.h"

#define ARGUMENTS_OK 0
#define ARGUMENTS_WRONG 1
#define FILE_IGNORED -1
#define FILE_IS_DIRECTORY 2 
#define FILE_NOT_AVAIBLE 1
#define FILE_SHOULD_BE_COPIED 1
#define FILE_SHOULD_BE_REMOVED 1
#define DIR_IGNORED -1
#define DIR_SHOULD_BE_REMOVED 1

void sig_handler(int);
int check_arguments(int, char**);
void scan_directory(char *, char *);
int search_for_copy(char*, DIR*, char*); 
int search_for_delete(char*, DIR*, char*); 
int search_for_directory(char*, DIR*, char*);
int remove_directory(const char *);

/*
* sleep_time - default value of daemon's sleeping time (sec)
* div_size - minimum size of the file for copying it with nmap (MB)
*/
int sleep_time = 300;
int div_size = 5; 
int r_flag = 0; 
char * source_path;
char * dest_path; 

/*
* Daemon initialize and create signal handlers
*/
static void skeleton_daemon() {
    pid_t pid;
    pid = fork();

    if (pid < 0) {
			exit(EXIT_FAILURE);
		}

    if (pid > 0) {
    	exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
			exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
		signal(SIGUSR1, sig_handler);

    pid = fork();

    if (pid < 0) { 
    	exit(EXIT_FAILURE);
    }

    if (pid > 0) {
    	exit(EXIT_SUCCESS);
    }

    umask(0);
    chdir("/");

    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--) {
    	close (x);
    }
}

/*
* SIGUSR1 handler for waking up deamon on demand 
*/
void sig_handler(int signum) {
	signal(SIGUSR1, sig_handler);
    if (signum == SIGUSR1) {
			setlogmask(LOG_UPTO (LOG_NOTICE));
			openlog("misernd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1); 
			syslog(LOG_NOTICE, "Service waken up by signal");
			closelog();
    }
}

/**
* Starting point of the service
*/
int main(int argc, char **argv) {
	if(check_arguments(argc, argv) != ARGUMENTS_OK) {
		printf("Wrong arguments!\n"); 
		printf("Usage: ./misernd source/path dest/path [-R] [-t number] [-k number]\n"); 
		return EXIT_FAILURE; 
	}

	char * relative_source = argv[1];
	char source_buffer[PATH_MAX+1]; 
	source_path = realpath(relative_source, source_buffer); 
	
	char * relative_dest = argv[2];
	char dest_buffer[PATH_MAX+1]; 
	dest_path = realpath(relative_dest, dest_buffer);
	
	skeleton_daemon();
	
	while(1) {
		sleep(sleep_time);

		setlogmask(LOG_UPTO (LOG_NOTICE));
		openlog("misernd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1); 
		syslog(LOG_NOTICE, "Service session start");

		scan_directory(source_path, dest_path); 

		syslog(LOG_NOTICE, "Service session end");
		closelog();
	}
	
	return EXIT_SUCCESS;
}

/*
* Checks if arguments are correct – minimal arguments are different destination and source dir
* return: ARGUMENTS_OK if success or ARGUMENTS_WRONG if fail. 
*/
int check_arguments(int argc, char **argv) {
	if(argc < 3) return ARGUMENTS_WRONG; 
	
	struct stat source_stat, dest_stat;
	char *source_path = argv[1];
	char *dest_path = argv[2];

	if(strcmp(argv[1], argv[2]) == 0)
		return ARGUMENTS_WRONG; 

	stat(source_path, &source_stat);
	stat(dest_path, &dest_stat);

	if(S_ISDIR(source_stat.st_mode) == 0 || S_ISDIR(dest_stat.st_mode) == 0) {
		return ARGUMENTS_WRONG; 
	}

	if(argc > 3) {
		int r_counter = 4; 
		while(r_counter <= argc) {
			if(strcmp(argv[r_counter-1], "-t") == 0) {
				if((r_counter + 1) <= argc) {
					if(atoi(argv[r_counter]) <= 0) {
						return ARGUMENTS_WRONG; 
					} else {
						sleep_time = atoi(argv[r_counter]); 
						r_counter += 1; 
					}
				} else {
					return ARGUMENTS_WRONG;
				}
			} else if (strcmp(argv[r_counter-1], "-k") == 0) {
				if((r_counter + 1) <= argc) {
					if(atoi(argv[r_counter]) <= 0) {
						return ARGUMENTS_WRONG; 
					} else {
						div_size = atoi(argv[r_counter]); 
						r_counter += 1; 
					}
				} else {
					return ARGUMENTS_WRONG;
				}
			}  else if (strcmp(argv[r_counter-1], "-R") == 0) {
				r_flag = 1;
			} else {
				return ARGUMENTS_WRONG; 
			}

			r_counter += 1; 
		}
	}

	return ARGUMENTS_OK; 
}

/*
* Scans directories and checks if there's files to copy or delete (for same content of source and destination directories). 
*/
void scan_directory(char * src, char * dst) {
	DIR *source_dir = opendir(src);
	DIR *dest_dir = opendir(dst); 
	
	struct dirent *source; 
	while((source = readdir(source_dir)) != NULL) {
		int result = search_for_copy(source->d_name, dest_dir, src);
		if(result == 1) {
			char * source_file = create_full_path(src, source->d_name);
			char * dest_file = create_full_path(dst, source->d_name);

			if(compare_size(source_file, div_size) == 1) {
				char * strings[4] = { "copy", source_file, dest_file, "512" }; 
				copy_normal(4, strings);
			} else {
				char * strings[4] = { "mapping", source_file, dest_file, "512" }; 
				copy_with_mapping(4, strings);
			}

			syslog(LOG_NOTICE, "Copied file %s to %s", source_file, dest_file);
		} else if(result == 2) {
			struct stat st = {0};

			if (stat(create_full_path(dst, source->d_name), &st) == -1) {
				mkdir(create_full_path(dst, source->d_name), 0700);
			}
 			
			scan_directory(create_full_path(src, source->d_name), create_full_path(dst, source->d_name));
		}
	}
	
	closedir(source_dir);
	closedir(dest_dir);
	
	source_dir = opendir(src);
	dest_dir = opendir(dst); 

	struct dirent *dest; 
	while((dest = readdir(dest_dir)) != NULL) {
		int result = search_for_delete(dest->d_name, source_dir, dst);
		if(result == 1) {
			char * file_path = create_full_path(dst, dest->d_name); 
			remove(file_path);
			syslog(LOG_NOTICE, "Removed file %s", file_path);
		} else if(result == 2) {
			if(search_for_directory(dest->d_name, source_dir, dst) == DIR_SHOULD_BE_REMOVED) {
				remove_directory(create_full_path(dst, dest->d_name));
			}
		}
	}

	closedir(source_dir);
	closedir(dest_dir);
}

/*
* Searchs file with given name in given directory for copying action
* return: FILE_IGNORED, FILE_IS_DIRECTORY, FILE_NOT_AVAIBLE when file shouldn't be copied 
*/
int search_for_copy(char* file_name, DIR* directory, char * actual_directory) {
	char * full_path = create_full_path(actual_directory, file_name); 
	
	if(strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
		return FILE_IGNORED; 
	}

	if(is_directory(full_path) != 0 && r_flag == 1) {
		return FILE_IS_DIRECTORY;
	}

	if(access(full_path, F_OK) == -1) {
  	return FILE_NOT_AVAIBLE;
	}
	
	if(is_regular_file(full_path) == 0) {
		return FILE_IGNORED; 
	}
	
	struct dirent *dest;
	while((dest = readdir(directory)) != NULL) {
		char * potential_path = create_full_path(dest_path, dest->d_name); 
		
		if(strcmp(dest->d_name, ".") == 0 || strcmp(dest->d_name, "..") == 0) 
			continue;
		
		if(is_regular_file(full_path) == 0) {
			continue; 
		}
		
		if(strcmp(file_name, dest->d_name) == 0) {
			if(compare_mtime(full_path, potential_path) == 0) {
				return FILE_IGNORED; 
			} else { 
				return FILE_SHOULD_BE_COPIED; 
			}
		}
	}

	return FILE_SHOULD_BE_COPIED;
}

/*
* Searchs file with given name in given directory for deleting action
* return: FILE_IGNORED, FILE_IS_DIRECTORY, FILE_NOT_AVAIBLE when file shouldn't be removed. 
*/
int search_for_delete(char* file_name, DIR* directory, char * actual_directory) {
	char * full_path = create_full_path(actual_directory, file_name); 
	
	if(strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
		return FILE_IGNORED; 
	}

	if(is_directory(full_path) != 0 && r_flag == 1) {
		return FILE_IS_DIRECTORY;
	}

	if(access(full_path, F_OK) == -1) {
  	return FILE_NOT_AVAIBLE;
	}
	
	struct dirent *dest;
	while((dest = readdir(directory)) != NULL) {
		char * potential_path = create_full_path(dest_path, dest->d_name); 
		
		if(strcmp(dest->d_name, ".") == 0 || strcmp(dest->d_name, "..") == 0) 
			continue;
		
		if(is_regular_file(full_path) == 0) {
			continue; 
		}
		
		if(strcmp(file_name, dest->d_name) == 0) {
			return FILE_IGNORED;
		}
	}

	return FILE_SHOULD_BE_REMOVED;
}

/*
* Searches for directory inside given directory to delete 
* return DIR_IGNORED if directory shouldn't be removed.
*/
int search_for_directory(char* file_name, DIR* directory, char * actual_directory) {
	char * full_path = create_full_path(actual_directory, file_name); 

	struct dirent *dest;
	while((dest = readdir(directory)) != NULL) {
		char * potential_path = create_full_path(dest_path, dest->d_name); 
		
		if(strcmp(dest->d_name, ".") == 0 || strcmp(dest->d_name, "..") == 0) 
			continue;
		
		if(strcmp(file_name, dest->d_name) == 0) {
			return DIR_IGNORED;
		}
	}

	return DIR_SHOULD_BE_REMOVED;
}

/*
* Removes directory with given path
*/
int remove_directory(const char *path) {
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d) {
      struct dirent *p;

      r = 0;
      while (!r && (p=readdir(d))) {
          int r2 = -1;
          char *buf;
          size_t len;

          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
             continue;
          }

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          if (buf) {
             struct stat statbuf;
             snprintf(buf, len, "%s/%s", path, p->d_name);

             if (!stat(buf, &statbuf)) {
                if (S_ISDIR(statbuf.st_mode)) {
                   r2 = remove_directory(buf);
                } else {
                   r2 = unlink(buf);
                }
             }

             free(buf);
          }

          r = r2;
      }

      closedir(d);
   }

   if (!r) {
      r = rmdir(path);
   }
}


