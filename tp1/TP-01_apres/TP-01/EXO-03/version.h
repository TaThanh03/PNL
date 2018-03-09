#ifndef VERSION_H
#define VERSION_H

struct version {
	unsigned short major;
	unsigned long minor;
	char flags;
};

int is_unstable(struct version *v);

int is_unstable_bis(struct version *v);

void display_version(struct version *v, int (*function)(struct version*));

int cmp_version(struct version *v, unsigned short major, unsigned long minor);

#endif
