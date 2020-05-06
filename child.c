#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "child.h"

#define TOGGLE(map, bit) map ^= (1 << bit);
#define ISSET(map, bit)  (map & (1 << bit)) >> bit

static unsigned int user_bitmap = 0;

//check which user entry is free, using bitmap
static int find_free_index(){
	int i;
  for(i=0; i < CHILDREN; i++){
  	if((ISSET(user_bitmap, i)) == 0){
      TOGGLE(user_bitmap, i);
      return i;
    }
  }
  return -1;
}

//Clear a user entry
void child_reset(struct child * children, const unsigned int i, struct frame * frames){

	user_bitmap &= ~(1 << i);  //unset the bit

	children[i].pid = 0;
	children[i].cid = 0;
	children[i].state = 0;

	pagetbl_reset(children[i].pages, frames);

	//set weights for -m 1
	int j;
	double * weights = children[i].weights;
  const double w = 1.0;
	for(j=0; j < CHILD_PAGES; j++){
		 weights[i] =  w / (double)(j + 1);
	}
}

int child_fork(struct child * children, const int id, const int m){

  struct child *pe;
	char arg[10], arg2[10];

  const int ci = find_free_index();
	if(ci == -1){
		return -2;  //no space for a new child
	}

  pe =  &children[ci];

  pe->cid	= id;
	snprintf(arg, 10, "%d", ci);
	snprintf(arg2, 10, "%d", m);

	const pid_t pid = fork();
	switch(pid){
		case -1:
			perror("fork");
      return -1;
			break;

		case 0:
			execl("./user", "./user", arg, arg2, NULL);
			perror("execl");
			exit(EXIT_FAILURE);

		default:
			pe->pid = pid;
			pe->state = CHILD_READY;
			break;
  }

  return ci;
}
