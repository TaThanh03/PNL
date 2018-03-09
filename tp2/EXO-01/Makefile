.PHONY : all clean check
.ONESHELL:

CHECK_PATCH=./checkpatch.pl --no-tree

all : testHistory

%.o : %.c
	gcc -Wall -g -c $<

testHistory : testHistory.c version.o history.o commit.o
	gcc -Wall $^ -o $@

check : 
	for f in *.c *.h ; do
		$(CHECK_PATCH) -f $$f
	done

clean :
	rm -rf *.o testVersion
