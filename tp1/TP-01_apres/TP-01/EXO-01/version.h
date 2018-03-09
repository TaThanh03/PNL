#ifndef VERSION_H
#define VERSION_H

struct version {
	short int major;
	short int minor;
	char flags;
};

int is_unstable(struct version *v);

int is_unstable_bis(struct version *v);

void display_version(struct version *v, int (*function)(struct version*));

int cmp_version(struct version *v, unsigned short major, unsigned long minor);

struct commit *commit_of(struct version *version);

struct commit {
	unsigned long id;
	struct version version;
	char *comment;
	struct commit *next;
	struct commit *prev;
};

#endif
