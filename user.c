#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include "lock.h"
#include "oss.h"

static struct DATA * data = NULL;
static struct child * child = NULL;
static int semid = -1;

int attach(){

 key_t k = ftok("oss.c", 1);

 const int shmid = shmget(k, sizeof(struct DATA), 0);
 if (shmid == -1) {
	 perror("shmget");
	 return -1;
 }

 data = (struct DATA*) shmat(shmid, (void *)0, 0);
 if (data == (void *)-1) {
	 perror("shmat");
	 shmctl(shmid, IPC_RMID, NULL);
	 return -1;
 }

 k = ftok("oss.c", 2);
 const int qid = msgget(k, 0);
 if(qid == -1){
	 perror("msgget");
	 return -1;
 }

 k = ftok("oss.c", 3);
 semid = semget(k, 1, 0);
 if(semid == -1){
   perror("semget");
   return -1;
 }

 return qid;
}

int weighted_rand(){
  //update weights
  int i;
  for(i=1; i < CHILD_PAGES; i++){
     child->weights[i] += child->weights[i-1];
  }

  const double r = ((double)rand() / (double)RAND_MAX) * child->weights[CHILD_PAGES-1];

  for(i=0; i < CHILD_PAGES; i++){
    if(child->weights[i] > r){
      break;
    }
  }

  return i;
}

int main(const int argc, const char * argv[]){

  struct msgbuf buf;

  const int id = atoi(argv[1]);
  const int m  = atoi(argv[2]);

	const int qid = attach();
	if(qid < 0){
    fprintf(stderr, "User can't attach\n");
		return -1;
	}

	srand(getpid());

  child = &data->children[id];

  int nref = 0;

  while(1){

    int nref_rand = 900 + (rand() % 200);  //1000 +/- 100
    //int nref_rand = 9 + (rand() % 2);  //10 +/- 1
    if(nref > nref_rand){
      //lock(semid);
      //TODO: check if we should terminate
      //unlock(semid);
      nref = 0; //reset reference counter
      break;
    }

		bzero(&buf, sizeof(struct msgbuf));

		//what decision the user will take - take or release
    buf.msg.id = id;
    if(m == 0){ //random request
      buf.msg.addr = rand() % ADDR_MAX;
    }else{  //weight[] request
      buf.msg.addr = weighted_rand();
    }
		buf.msg.rw = ((rand() % 100) < 70) ? 0 : 1;  //70 % read chance
    buf.msg.result = DENY;
		buf.mtype = getppid();

		if( (msgsnd(qid, (void*)&buf, MSG_LEN, 0) == -1) ||
        (msgrcv(qid, (void*)&buf, MSG_LEN, getpid(), 0) == -1) ){
	    perror("msgsnd");
	    break;
	  }

    if(buf.msg.result == DENY){
      break;  //quit
		}
    nref++;
  }

  //return all resources
  buf.mtype = getppid();
  buf.msg.id = id;
  buf.msg.rw = 1;
  buf.msg.addr = -1; //all
  buf.msg.result = 0;
  msgsnd(qid, (void*)&buf, MSG_LEN, 0);

	shmdt(data);

  return 0;
}
