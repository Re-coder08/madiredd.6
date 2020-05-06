#include <strings.h>
#include "mem.h"

#define ISSET(map, bit)  (map & (1 << bit)) >> bit

static unsigned char frame_bitmap[(FRAME_COUNT / 8) + 1];

void mem_init(){
  bzero(frame_bitmap, sizeof(frame_bitmap));
}

//check which user entry is free, using bitmap
int find_free_frame(){
	int i;
  for(i=0; i < FRAME_COUNT; i++){
    int byte = i / 8;
    int bit = i % 8;
  	if((ISSET(frame_bitmap[byte], bit)) == 0){
      frame_bitmap[byte] |= (1 << bit);  //set the bit
      return i;
    }
  }
  return -1;
}

void frame_reset(struct frame *fr, const int f){
  fr->status = FREE;
  fr->pageid = fr->cid = -1;

  int byte = f / 8;
  int bit = f % 8;

  frame_bitmap[byte] &= ~(1 << bit);  //unset the bit
}

void frame_used(struct frame *fr, const int f){

  int byte = f / 8;
  int bit = f % 8;
  frame_bitmap[byte] |= (1 << bit);  //set the bit

  fr->status = USED;
}

void pagetbl_reset(struct page *pt, struct frame * ft){
  int i;
	for(i=0; i < CHILD_PAGES; i++){
		struct page *p = &pt[i];
		if(p->frameid >= 0){
			frame_reset(&ft[p->frameid], p->frameid);
			p->frameid = -1;   //not in memory
		}
		p->referenced = 0;  //not referenced
	}
}
