#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#define IOCTL_HELLO_TYPE 6969
#define HELLO _IOR(IOCTL_HELLO_TYPE, 3, char*)
#define WHO   _IOW(IOCTL_HELLO_TYPE, 4, char*)
 
int main()
{
        int fd;
	char my_string[32];
        printf("*********************************\n");
        printf("\nOpening Driver\n");
        fd = open("/dev/hello", O_RDONLY);
        if(fd < 0) {
                printf("Cannot open device file...\n");
                return 0;
        }

	printf("Writing Value to Driver\n");
        ioctl(fd, WHO, "BLABLA"); 

        printf("Reading Value from Driver\n");
        ioctl(fd, HELLO, (char*) &my_string);
        printf("Value is %s\n", my_string);        
 
        printf("Closing Driver\n");
        close(fd);
}
