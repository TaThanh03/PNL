.PHONY : all clean check
.ONESHELL:

CHECK_PATCH=../../../../biblio/eudyptula/torvalds/scripts/checkpatch.pl

all : cron_func

%.o : %.c
	gcc -shared -fPIC -shared -Wall -c $<

cron_func : cron_func.o 
	gcc -L. -Wall $^ -o $@ -lfunc

check :
	for f in *.c *.h ; do
		$(CHECK_PATCH) -f $$f
	done

clean :
	rm -rf *.o cron_func
