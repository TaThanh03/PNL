#include<stdlib.h>
#include<stdio.h>

#include"version.h"

int is_unstable(struct version *v)
{
	/*return 1 & ((char *)v)[sizeof(unsigned short) + sizeof(unsigned long) - 2];*/
	return 1 & ((char *)v)[sizeof(unsigned short)];
}

int is_unstable_bis(struct version *v)
{
	return 1 & v->minor;
}

void display_version(struct version *v, int (*function)(struct version*))
{
	printf("%2u.%d %s", v->major, v->minor,
			     function(v) ? "(unstable)" : "(stable)  ");
}

int cmp_version(struct version *v, unsigned short major, unsigned long minor)
{
	return v->major == major && v->minor == minor;
}
