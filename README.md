Unix system service for synchronization of two dirs. Can works with single directories without subdirectories or with their (with -R parameter).

Compile:

gcc main.c comparator.c copy.c fopr.c -o misernd

Usage:

./misernd path/to/source/dir path/to/destination/dir [-R][-t time][-k mb]

-R option allows to synchronize with subdirectories

-t time_value allows to set custom sleeping time of the service in sec

-k mb_value allows to set custom minimal file size to copy it with mapping in MB
