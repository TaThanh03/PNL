#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "version.h"

struct commit *commit_of(struct version *version)
{
	return (struct commit *)( (void *)version   -  (void *)&((struct commit *)0)->version );  	
}

int main(int argc, char const *argv[])
{
	struct version v = {.major = 3,
			    .minor = 5,
			    .flags = 0};

	struct commit c = {.version = v};
	c.comment = malloc(sizeof(char));
	c.next = malloc(sizeof(struct commit));
	c.prev = malloc(sizeof(struct commit));

	printf("les adresses des différents champs \n");
	printf("id:       %p \n", &c.id);
	printf("version:  %p \n", c.version);
	printf("comment:  %p \n", &c.comment);
	printf("next:     %p \n", c.next);
	printf("prev:     %p \n", c.prev);
	
	commit_of(&v);
	printf("commit_of \n");
	printf("%p \n", &c);
	printf("%p \n", commit_of(&v));

	return 0;
}
