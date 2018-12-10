#include <ucontext.h>
typedef struct green_t
{
  ucontext_t *context;
  void *(*fun)(void*);
  void *arg ;
  struct green_t *next;
  struct green_t *join;
  struct green_t *joinnext;
  struct green_t *condnext;

  int zombie;
} green_t;

//my section 3 struct:
//hold a number of suspended threads. The implementation of the functions should then be
//quite simple.
typedef struct thread_queue
{
  struct green_t *this;
  struct green_t *next;
} thread_queue;
typedef struct green_cond_t
{
  struct green_t *firstt;
  struct green_t *lastt;
} green_cond_t;

//void init();

//static void init() __attribute__ (( constructor ));


int green_create(green_t *thread, void *(*fun)(void*), void *arg);
int green_yield();
int green_join(green_t *thread);

void green_cond_init(green_cond_t* cond);
void green_cond_wait(green_cond_t* cond);
void green_cond_signal(green_cond_t* cond);
