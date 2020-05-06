#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <signal.h>
#include <fcntl.h>

#include "lock.h"
#include "child.h"
#include "oss.h"
#include "pqueue.h"


static pqueue_t pq;     //for pages
static queue_t  device; //for device memory requests

static unsigned int child_count = 0;
static unsigned int child_max = 100;
static unsigned int child_done = 0;  //child_done children

static unsigned int mem_refs = 0;
static unsigned int mem_rd = 0;
static unsigned int mem_wr = 0;
static unsigned int mem_faults = 0;

static unsigned int line_count = 0;

static struct DATA *data = NULL;
static int qid = -1, shmid = -1, semid = -1;
static const char * logfile = "output.txt";

static int is_signaled = 0; /* 1 if we were signaled */
static int m = 0;  /* 1 if verbose */

static int attach(){

 	key_t k = ftok("oss.c", 1);

 	shmid = shmget(k, sizeof(struct DATA), IPC_CREAT | S_IRWXU);
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
	qid = msgget(k, IPC_CREAT | S_IRWXU);
	if(qid == -1){
		perror("msgget");
		return -1;
	}

  k = ftok("oss.c", 3);
  semid = semget(k, 1, IPC_CREAT | S_IRWXU);
  if(semid == -1){
    perror("semget");
    return -1;
  }

  union semun un;
  un.val = 1;
  if(semctl(semid, 0, SETVAL, un) == -1){
    perror("semctl");
    return -1;
  }

	return 0;
}

static int detach(){

  shmdt(data);
  shmctl(shmid, IPC_RMID,NULL);
  semctl(semid, 0, IPC_RMID, NULL);
	msgctl(qid, IPC_RMID, NULL);

  pq_dealloc(&pq);
  q_dealloc(&device);

  return 0;
}

static void list_memory(){
	int i;
	printf("[%li:%li] Master: Current memory layout is:\n", data->clock.sec, data->clock.nsec);
  printf("\t\t\t\tOccupied\tRefByte\tDirtyBit\n");
	for(i=0; i < FRAME_COUNT; i++){

    struct frame * fr = &data->frames[i];        //frame

    char * occupied = (fr->pageid >= 0) ? "Yes" : "No";
    int ref = 0, dirty = 0;
    if(fr->pageid > 0){
      ref = data->children[fr->cid].pages[fr->pageid].referenced;
      dirty = (data->frames[i].status == DIRTY) ? 1 : 0;
    }
    printf("Frame %2d\t\t%s\t\t%7d\t%8d\n", i, occupied, ref, dirty);
	}
	putchar('\n');
  line_count += FRAME_COUNT + 2;
}

static void pq_remove(pqueue_t * q, int id){
  int i;
  line_count++;
  printf("[%li:%li] Master: Removing P%d pages from memory from frames ", data->clock.sec, data->clock.nsec, data->children[id].cid);

  for(i=0; i < q->len; i++){
    struct pitem * pb = pq_pop(q);
    struct frame * fr = &data->frames[pb->f];
    if(fr->cid != id){
      pq_push(q, pb); //return to q
    }else{
      printf("%d ", pb->f);
    }
  }
  printf("\n");
}


static int pq_pop_CLOCK(){
  static int hand = 0;
  int evicted = 0;
  struct pitem * pb;

  while(!evicted){

    pb = pq_at(&pq, hand);  //page pointed by hand
    struct frame * fr = &data->frames[pb->f];    //frame that holds the excluded page
    struct page * p = &data->children[fr->cid].pages[fr->pageid]; //get page

    if(p->referenced){
      p->referenced = 0;
    }else{
      evicted = 1;
      line_count++;
      printf("[%li:%li] Master: Evicting P%d page %d, hand = %d\n",
        data->clock.sec, data->clock.nsec, fr->cid, fr->pageid, hand);
    }
    hand = (hand + 1) % FRAME_COUNT;
  }

  return pb->f; //return frame holding evicted page
}

static int mm_page_fault(struct qitem * msg){

	mem_faults++;
  struct child * child = &data->children[msg->id];
  const int pageid = PAGE_ID(msg->addr);

  //search for a free frame to load page in
  int frameid = find_free_frame();
  if(frameid >= 0){

    if(data->frames[frameid].pageid == -1){  //if frame is not used
      line_count++;
      printf("[%li:%li] Master: Using free frame %d for P%d page %d\n",
        data->clock.sec, data->clock.nsec, frameid, child->cid, pageid);
      return frameid;

    }else{
      fprintf(stderr, "Error: frame bitmap is wrong. Frame %d is used\n", frameid);
      fflush(stdout);
      detach();
      exit(EXIT_FAILURE);
    }
  }

  //have to evict a page
  frameid = pq_pop_CLOCK(); //choose a frame, using CLOCK
  struct frame * fr = &data->frames[frameid]; //frame that holds the evicted page
  struct page * p = &data->children[fr->cid].pages[fr->pageid];

  if(p->frameid < 0){
	  fprintf(stderr, "Error: Evicted page %d of P%d is empty\n", fr->pageid, fr->cid);
	  fflush(stdout);
    detach();
	  exit(EXIT_FAILURE);
  }

  struct child * oldchild = &data->children[fr->cid];

  line_count++;
  printf("[%li:%li] Master: Clearing frame %d and swapping P%d page %d for P%d page %d\n",
    data->clock.sec, data->clock.nsec, p->frameid, oldchild->cid, fr->pageid, child->cid, pageid);

  frameid = p->frameid;
  frame_reset(&data->frames[p->frameid], p->frameid);
  p->frameid = -1;

  return frameid;  //return index of free frame
}

static int mm_load_addr(struct qitem * msg){
  int rv;
  struct oss_time tload;
  tload.sec = 0;

  const int pageid = PAGE_ID(msg->addr);
  if(pageid > CHILD_PAGES){
		fprintf(stderr, "Error: Page index > %d\n", CHILD_PAGES);
		return DENY;
	}
  struct child * child = &data->children[msg->id];
  struct page * p = &child->pages[pageid];

  if(p->frameid < 0){	//if page is not in memory
  	line_count++;
    printf("[%li:%li] Master: Address %d is not in a frame, pagefault\n",
      data->clock.sec, data->clock.nsec, msg->addr);

  	p->frameid = mm_page_fault(msg); //get frame for page, so we can load it into memory
  	p->referenced = 1;  //after page is loaded into memory, raise the ref bit

    frame_used(&data->frames[p->frameid], p->frameid);
  	data->frames[p->frameid].pageid = pageid;
  	data->frames[p->frameid].cid = msg->id;

    //14 ms load time for device
    msg->t.sec = 0; msg->t.nsec = 14 * 1000;
    oss_time_update(&msg->t, &data->clock);

  	rv = (q_push(&device, msg) == -1) ? DENY : DELAY;

  }else{ //no page fault
    tload.nsec = 10;
    oss_time_update(&data->clock, &tload);	//add 10ns to clock
    rv = ALLOW;
  }

  return rv;
}

static int mm_reference(struct qitem * msg){
  int rv = DENY;

  mem_refs++;

  struct child * child = &data->children[msg->id];

  if(msg->rw == READ){
    mem_rd++;
    printf("[%li:%li] Master: P%d requesting read of address %d\n", data->clock.sec, data->clock.nsec, child->cid, msg->addr);
  }else{
    mem_wr++;
    printf("[%li:%li] Master: P%d requesting write of address %d\n", data->clock.sec, data->clock.nsec, child->cid, msg->addr);
  }
  line_count++;

	rv = mm_load_addr(msg);

  if(rv == ALLOW){
    const int pageid = PAGE_ID(msg->addr);
    struct page * p = &child->pages[pageid];

    if(msg->rw == READ){
      line_count++;
  		printf("[%li:%li] Master: Address %d in frame %d, giving data to P%d\n", data->clock.sec, data->clock.nsec,
  			msg->addr, p->frameid, child->cid);
    }else{
      line_count++;
  		printf("[%li:%li] Master: Address %d in frame %d, writing data to frame\n",
  			data->clock.sec, data->clock.nsec, msg->addr, p->frameid);

  		struct frame * fr = &data->frames[p->frameid];
  		if(fr->status != DIRTY){
  			line_count++;
        printf("[%li:%li] Master: Dirty bit of frame %d set, adding additional time to the clock\n",
          data->clock.sec, data->clock.nsec, p->frameid);
  		}
    }
  }

  return rv;
}

static int check_ready(){

  struct oss_time t;
  struct msgbuf buf;

  t.sec = 0;
  memset(&buf, 0, sizeof(struct msgbuf));

  if(msgrcv(qid, (void*)&buf, MSG_LEN, getpid(), 0) == -1){
    if(errno == EINTR){
      return 0;
    }else{
      perror("msgrcv");
  		return -1;
    }
	}

  struct qitem * msg = &buf.msg;
  struct child * child = &data->children[msg->id];

	if(buf.msg.addr == -1){
    line_count++;
    printf("[%li:%li] Master has detected P%d quits\n", data->clock.sec, data->clock.nsec, child->cid);
    pq_remove(&pq, msg->id);
    child_reset(data->children, msg->id, data->frames);
    q_remove(&device, msg->id);
    child_done++;
	}else{

  	msg->result = mm_reference(msg);

  	if(msg->result != DELAY){
  		buf.mtype = child->pid;
      if(msgsnd(qid, (void*)&buf, MSG_LEN, 0) == -1){
        perror("msgsnd");
        return -1;
      }
  	}
  }

  //add request processing to clock
  t.nsec = rand() % 100;
  oss_time_update(&data->clock, &t);

  return 0;
}

static int check_device(){
	struct msgbuf mb;
  struct qitem * msg;

	while((msg = q_pop_time(&device, &data->clock)) != NULL){

    struct child * child = &data->children[msg->id];

		struct pitem pb;
    int pageid = PAGE_ID(msg->addr);
    pb.f = child->pages[pageid].frameid;

		//page is set up. Just send reply to unblock process
		msg->result = (pq_push(&pq, &pb) == -1) ? DENY : ALLOW;

    mb.msg = *msg;
    mb.mtype = child->pid;
		if(msgsnd(qid, (void*)&mb, MSG_LEN, 0) == -1){
			perror("msgsnd");
			return -1;
		}

		line_count++;
    printf("[%li:%li] Master: Indicating to P%d that write has happened to address %d\n",
      data->clock.sec, data->clock.nsec, child->cid, msg->addr);
	}

	return 0;
}

static void check_lines(){
  if(line_count >= 100000){
    printf("[%li:%li] OSS: Too many lines ...\n", data->clock.sec, data->clock.nsec);
    stdout = freopen("/dev/null", "w", stdout);
  }
}

static void alarm_children(){
  int i;
  struct msgbuf mb;

  bzero(&mb, sizeof(mb));
	mb.msg.result = DENY;

  for(i=0; i < CHILDREN; i++){
    if(data->children[i].pid > 0){
      mb.mtype = data->children[i].pid;
      if(msgsnd(qid, (void*)&mb, MSG_LEN, 0) == -1){
        perror("msgsnd");
      }
    }
  }
}

static void output_stat(){

  //reopen output, since it may be closed, if too many lines
  stdout = freopen(logfile, "a", stdout);

	printf("Runtime: %li:%li\n", data->clock.sec, data->clock.nsec);
  printf("Child count: %d/%d\n", child_count, child_done);
  printf("Memory references: %u\n", mem_refs);
  printf("Memory rd/wr: %u/%u\n", mem_rd, mem_wr);
	printf("Memory accesses/s: %.2f\n", (float) mem_refs / data->clock.sec);

	printf("Page fauls per memory access: %.2f\n", (float)mem_faults / mem_refs);
}


static int user_fork(struct oss_time * fork_time){

  if(child_count >= child_max){  //if we can fork more
    return 0;
  }

  //if its fork time
  if(oss_time_compare(&data->clock, fork_time) == -1){
    return 0;
  }

  //next fork time
  fork_time->sec = data->clock.sec + 1;
  fork_time->nsec = 0;

  const int ci = child_fork(data->children, child_count, m);
  if(ci < 0){
    //child_reset(data->children, ci, data->frames);
    return 0;
  }else if(ci >= 0){
    printf("[%li:%li] OSS: Generating process with PID %u\n", data->clock.sec, data->clock.nsec, data->children[ci].cid);
  }
  child_count++;

  return 1;
}

static int oss_init(){

  srand(getpid());

  stdout = freopen(logfile, "w", stdout);
  if(stdout == NULL){
		perror("freopen");
		return -1;
	}
  bzero(data, sizeof(struct DATA));
  mem_init();

  pq_alloc(&pq, FRAME_COUNT);   /* queue of pages */
  q_alloc(&device, CHILDREN);  /* queue of blocked requests */

  //clear page table
  int i;
	for(i=0; i < CHILDREN; i++)
		child_reset(data->children, i, data->frames);

	//clear frame table
	for(i=0; i < FRAME_COUNT; i++)
		frame_reset(&data->frames[i], i);

  return 0;
}

static void move_time(){
  struct oss_time t;		//clock increment
  t.sec = 1;
  t.nsec = rand() % 10000;

  lock(semid);
  oss_time_update(&data->clock, &t);
  unlock(semid);
}

static void signal_handler(const int signal){
  is_signaled = 1;
  printf("[%li:%li] OSS: Signaled with %d\n", data->clock.sec, data->clock.nsec, signal);
}

int main(const int argc, const char * argv[]){

  signal(SIGINT,  signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGALRM, signal_handler);
  signal(SIGCHLD, SIG_IGN);

  alarm(2);

  if(argc == 3){
    if(argv[1][1] == 'm'){
      m = atoi(argv[2]);
    }else{
      fprintf(stderr, "Error: Invalid opageidon %s\n", argv[1]);
      return -1;
    }
  }

  if((attach(1) == -1) || (oss_init() == -1)){
    return -1;
  }

  struct oss_time fork_time;	//when to fork another process
	bzero(&fork_time, sizeof(struct oss_time));

  int last_sec = data->clock.sec;

  while(!is_signaled && (child_done < child_max)){

    move_time();

		//if we are ready to fork, start a process
    user_fork(&fork_time);

		check_ready();
    check_device();

    //show memory layout every second
    if(data->clock.sec > (last_sec + 10)){  //changed to 10s, since 1 produces too much output

      list_memory();
      last_sec = data->clock.sec;
    }

  	check_lines();
  }

	output_stat();

  alarm_children();
  detach();

  return 0;
}
