#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

//#include "taskmonitor.h"
#define IOCTL_TASKMONITOR 6969
#define GET_SAMPLE _IOR(IOCTL_TASKMONITOR, 3, char*)
#define TASKMON_STOP _IOR(IOCTL_TASKMONITOR, 4, char*)
#define TASKMON_START _IOR(IOCTL_TASKMONITOR, 5, char*)

int main()
{
        int fd;
	char my_string[256];
        printf("*********************************\n");
        printf("\nOpening Driver\n");
        fd = open("/dev/ex_06", O_RDONLY);
        if(fd < 0) {
                printf("Cannot open device file...\n");
                return 0;
        }
        printf("Reading Value from Driver\n");
        ioctl(fd, GET_SAMPLE, (char*) &my_string);
        printf("Value is %s\n", my_string);        
 
        printf("stop thread\n");
        ioctl(fd, TASKMON_STOP, (char*) &my_string);
	
	sleep(10);
	
        printf("start thread\n");
        ioctl(fd, TASKMON_START, (char*) &my_string);

        printf("Closing Driver\n");
        close(fd);
}
