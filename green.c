#include <stdlib.h>
#include <ucontext.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include "green.h"

#define FALSE 0
#define TRUE 1
#define STACK_SIZE 4096

#define PERIOD 100

static sigset_t block;
void timer_handler(int);

static ucontext_t main_cntx = {0};
static green_t main_green= {&main_cntx, NULL, NULL, NULL, NULL, FALSE};
static green_t *running = &main_green;

static green_t *ready_queue_first = NULL;
static green_t *ready_queue_last = NULL;

static green_t *waiting_queue_first;
static green_t *waiting_queue_last;



static void init() __attribute__ (( constructor ));

// --------Queue Functions----------
void ready_enqueue(green_t *entry)
{
  if(ready_queue_last!= NULL && ready_queue_first!=NULL)
  {
    //printf("%p\n", ready_queue_last->next);
    green_t *p = ready_queue_last->next;
    ready_queue_last->next = entry;
    ready_queue_last = entry;
  }
  else
  {
    ready_queue_first = entry;
    ready_queue_last = entry;
  }
}

green_t *ready_dequeue()
{
  green_t *result = ready_queue_first;
  if (ready_queue_first != NULL) {
      if (result->next!=NULL) {
          ready_queue_first = result->next;
      } else {
          ready_queue_first = NULL;
          ready_queue_last = NULL;
      }
  }

  if (result != NULL)
    result->next = NULL;
  return result;
}

// ---------Timer------------------
void timer_handler(int sig)
{

  printf("handelr reached");
  /*
  // --add the running to the ready queue--
  ready_enqueue(running);
  // --------------------------------------

  // --find the next thread for execution--
  green_t *next = ready_dequeue();
  // --------------------------------------

  running = next;
  swapcontext(running->context, next->context);
  //swapcontext(susp->context, next->context);*/
  green_yield();

}

// ------------Other----------------

void addtojoin(green_t *addTo, green_t *addThis)
{
  if(addTo->join == NULL)
  {
    addTo->join=addThis;
    return;
  }
  if(addTo->join->joinnext==NULL)
  {
    addTo->join->joinnext = addThis;
    return;
  }
  //green_t *holder = addTo->join->joinnext;
  addThis->joinnext = addTo->join->joinnext;
  addTo->join->joinnext;

}

void placeJoinInQueue(green_t *thread)
{
  if (thread->join == NULL)
      return;

  ready_enqueue(thread->join);
  thread->join = NULL;
}

//-------------GREEN--------------


// Initialize the main_cntx so that when we call the cheduling function
// for the first time the running thread will be properly initialized.
void init()
{
  getcontext(&main_cntx);

  printf("init\n");

  sigemptyset(&block);
  sigaddset(&block, SIGVTALRM);

  struct sigaction act = {0};
  struct timeval interval;
  struct itimerval period;

  act.sa_handler = timer_handler;
  assert(sigaction(SIGVTALRM, &act, NULL) == 0 );
  interval.tv_sec = 0;
  interval.tv_usec = PERIOD;
  period.it_interval = interval;
  period.it_value = interval;
  setitimer(ITIMER_VIRTUAL, &period, NULL);

}


//tart the execution of the real function and, when after re-
//turning from the call, terminate the thread.
void green_thread()
{

    green_t *this = running;

    (*this->fun)(this->arg);

    // --place waiting (joining) thread in ready queue--
    placeJoinInQueue(running);
    // -------------------------------------------------


    // --free allocated memory structures--
    free(running->context->uc_stack.ss_sp);
    free(running->context);
    // ------------------------------------


    // --we're a zombie--
    running->zombie = TRUE;
    // ------------------


    // --find the next thread to run--
    green_t *next = ready_dequeue();
    // -------------------------------

    if(next==NULL)
      next=&main_green;
    running = next;
    setcontext(next->context);

}


// The user will call green_create() and provide: an uninitialized green_t structure,
// the function the thread should execute and pointer to its arguments. We will
// create new context, attach it to the thread structure and add this thread to
// the ready queue.

int green_create(green_t *new, void *(*fun) (void*) , void *arg)
{
  ucontext_t* cntx = ( ucontext_t*) malloc (sizeof( ucontext_t ));
  getcontext(cntx);

  void *stack = malloc(STACK_SIZE);

  cntx->uc_stack . ss_sp = stack;
  cntx->uc_stack . ss_size = STACK_SIZE ;

  makecontext ( cntx, green_thread, 0); //green_thread
  new->context = cntx;
  new->fun = fun;
  new->arg = arg;
  new->next = NULL;
  new->join = NULL;
  new->zombie = FALSE;

  // --add new to the ready queue--
  ready_enqueue(new);
  // ------------------------------

  return 0;

}

// Put the running thread last in the ready queue and then select the first
// thread from the queue as the next thread to run.
int green_yield()
{
  green_t *susp = running;

  // add susp to ready queue
  ready_enqueue(susp);
  // -------


  // select the next thread for execution
  green_t *next = ready_dequeue();
  // -------

  running = next;

  swapcontext(susp->context, next->context);
  return 0;
}


//Wait for a thread to terminate. We therefore add the thread
//to the join field and select another thread for execution.
int green_join (green_t *thread) {

  if (thread->zombie)
  return 0;


  green_t *susp = running ;
  // --add to waiting threads--
  addtojoin(thread, running);
  // --------------------------

  // --select the next thread for execution--
  green_t *next = ready_dequeue();
  // ----------------------------------------


  running = next;
  swapcontext(susp->context, next->context);

  return 0;
}


// initialize a green condition variable
void green_cond_init(green_cond_t* cond)
{
  cond = (green_cond_t* )malloc(sizeof(green_cond_t));
  //cond->threadqueue = (thread_queue*)malloc(sizeof(thread_queue));
  //cond->threadqueue = (thread_queue*)malloc(sizeof(thread_queue));

  //cond->threadqueue->this = (green_t*)malloc(sizeof(green_t*));
  //cond->threadqueue->next = (green_t*)malloc(sizeof(green_t*));

  //cond->lastelement->this = (green_t*)malloc(sizeof(green_t*));
  //cond->lastelement->next = (green_t*)malloc(sizeof(green_t*));

  //*cond = (green_cond_t){NULL,NULL};



  //cond->threadqueue-thia = (thread_queue*)malloc(sizeof(thread_queue));

}

// suspend the current thread on the condition
void green_cond_wait(green_cond_t* cond)
{
  if(cond->lastt != running)
  {
    if(cond->firstt==NULL)
      cond->firstt = running;//(thread_queue*)1;
    else
      cond->lastt->condnext = running;

    cond->lastt = running;
  }
  //green_cond_signal(cond);//

}

// move the first suspended thread to the ready queue
void green_cond_signal(green_cond_t* cond)
{
  if(cond->firstt==NULL)
  {
    //printf("tried to signal but there were no suspended threads");
    return;
  }

  ready_enqueue(cond->firstt);

  cond->firstt = cond->firstt->condnext;

}
