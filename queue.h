#include "oss_time.h"

struct qitem {
	int id;									//who is sending messag
	int rw;	//receive or return
	int addr;	//value
	int result;

	struct oss_time t;	//time this request will be loaded from device
};

typedef struct circular_queue {
	struct qitem *items;
	int front, back;
	int size;
	int len;
} queue_t;

int q_alloc(queue_t * q, const int size);
void q_dealloc(queue_t * q);

int q_push(queue_t * q, const struct qitem *);
struct qitem * q_pop(queue_t * q);
struct qitem * q_pop_time(queue_t * q, struct oss_time *clock);

void q_drop(queue_t * q, int nth);
void q_remove(queue_t * q, int id);

struct qitem * q_front(queue_t * q);
struct qitem * q_at(queue_t * q, const int i);
int q_len(const queue_t * q);
