#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pqueue.h"

int pq_alloc(pqueue_t * q, const int qsize){


  q->items = (struct pitem*) malloc(sizeof(struct pitem)*qsize);
  if(q->items == NULL){
    perror("malloc");
    return -1;
  }

  memset(q->items, -1, sizeof(struct pitem)*q->len);
  q->size = qsize;
  q->front = q->back = q->len = 0;

  return 0;
}

void pq_dealloc(pqueue_t * q){
  free(q->items);
}

int pq_push(pqueue_t * q, const struct pitem *item){
  if(q->len >= q->size){
    return -1;
  }

  q->items[q->back++] = *item;
  q->back %= q->size;
  q->len++;

  return 0;
}

struct pitem* pq_pop(pqueue_t * q){
  struct pitem* item = &q->items[q->front++];
  q->len--;
  q->front %= q->size;

  return item;
}

void pq_drop(pqueue_t * q, int nth){
  int i;
  for(i=0; i < q->len; i++){
    struct pitem * msg = pq_pop(q);
    if(i != nth){
      pq_push(q, msg);
    }
  }
}

struct pitem* pq_front(pqueue_t * q){
  return &q->items[q->front];
}

struct pitem * pq_at(pqueue_t * q, const int i){
  if(i > q->len){
    return NULL;
  }
  const int at = (q->front + i) % q->size;
  return &q->items[at];
}

int ppq_len(const pqueue_t * q){
  return q->len;
}
