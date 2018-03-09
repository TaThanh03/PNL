#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
typedef int (*orig_read_type)(int fd, void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count){
	char my_buff = 0;
  orig_read_type orig_read;
  orig_read = (orig_read_type)dlsym(RTLD_NEXT,"read");
	if(orig_read(fd, &my_buff, count) == sizeof(char) && my_buff == 'r'){
		*(char *)buf = 'i';
		return 1;
	}
  return orig_read(fd, buf, count);
}
