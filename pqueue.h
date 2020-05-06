struct pitem {
	//int id;									//who is sending messag
	int f;
};

typedef struct circular_pqueue {
	struct pitem *items;
	int front, back;
	int size;
	int len;
} pqueue_t;

int pq_alloc(pqueue_t * q, const int size);
void pq_dealloc(pqueue_t * q);

int pq_push(pqueue_t * q, const struct pitem *);
struct pitem * pq_pop(pqueue_t * q);
void pq_drop(pqueue_t * q, int nth);

struct pitem * pq_front(pqueue_t * q);
struct pitem * pq_at(pqueue_t * q, const int i);
int pq_len(const pqueue_t * q);
