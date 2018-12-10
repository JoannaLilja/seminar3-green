#include <stdlib.h>
#include <ucontext.h>
#include <assert.h>
#include <stdio.h>
#include "green.h"

#define FALSE 0
#define TRUE 1
#define STACK_SIZE 4096

static ucontext_t main_cntx = {0};
static green_t main_green= {&main_cntx, NULL, NULL, NULL, NULL, FALSE};
static green_t *running = &main_green;

static green_t *ready_queue_first;
static green_t *ready_queue_last;

static green_t *waiting_queue_first;
static green_t *waiting_queue_last;



static void init() __attribute__ (( constructor ));

// --------Queue Functions----------
void ready_enqueue(green_t *entry)
{
  if(ready_queue_last!= NULL)
  {
    ready_queue_last->next = entry;
    ready_queue_last = entry;
    return;
  }
  ready_queue_first = entry;
  ready_queue_last = entry;
}

// To do: if list has only one item, remove it and set first and last to null.
// if queue is empty, we return null -> is that okay?
// dont we need to set next to null
green_t *ready_dequeue()
{
  green_t *result = ready_queue_first;
  if(ready_queue_first!= NULL && ready_queue_first->next!=NULL)
    ready_queue_first = ready_queue_first->next;
  return result;
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

// To do: cancel the link in thread->join after adding to queue
void placeJoinInQueue(green_t *thread)
{
  if(thread->join!=NULL)
  ready_enqueue(thread->join);
}

//-------------GREEN--------------


// Initialize the main_cntx so that when we call the cheduling function
// for the first time the running thread will be properly initialized.
void init()
{
  getcontext(&main_cntx);
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


    // To do free running->context->uc_stack.ss_sp then running->context
    // --free allocated memory structures--
    free(running); //<<<<< double free or corrupt (out)
                //Program received signal SIGABRT, Aborted.
                //__GI_raise (sig=sig@entry=6) at ../sysdeps/unix/sysv/linux/raise.c:5151
                //	../sysdeps/unix/sysv/linux/raise.c: No such file or directory.
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
