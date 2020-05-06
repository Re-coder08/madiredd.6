#ifndef CHILD_H
#define CHILD_H

#include "oss_time.h"
#include <sys/types.h>
#include "mem.h"

#define CHILDREN 18

enum state { CHILD_READY=1, CHILD_WAIT, CHILD_DONE };

struct child {
	pid_t	pid;	/* process ID */
	int cid;		/* child   ID */

	enum state state;	/* process state */
	struct page	  pages[CHILD_PAGES];		//page table
	double weights[CHILD_PAGES];					//weight for each page in child page table
};

void child_reset(struct child * children, const unsigned int i, struct frame* frames);
int child_fork(struct child * children, const int id, const int m);

int clear_terminated(struct child * children);

#endif
