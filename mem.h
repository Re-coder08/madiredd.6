#ifndef MEM_H
#define MEM_H


#define PAGE_SIZE	1
#define CHILD_PAGES	32

#define ADDR_MAX (CHILD_PAGES * PAGE_SIZE * 1024)
#define FRAME_COUNT 256

#define PAGE_ID(addr) addr / (PAGE_SIZE*1024)

enum decision {READ=0, WRITE};
enum oss_decision {ALLOW=0, DELAY, DENY};

enum frame_status { FREE=0, DIRTY, USED};

struct page {
	int frameid;								//index in frames[]
	unsigned char referenced;		//reference bit
};

struct frame {
	int pageid;					//index of page, using the frame
	int cid;
	enum frame_status status;
};

void mem_init();
int find_free_frame();

void frame_reset(struct frame *fr, const int f);
void frame_used(struct frame *fr, const int f);
void pagetbl_reset(struct page *pt, struct frame * ft);

#endif
