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

//static green_t *ready_queue_first = NULL;
//static green_t *ready_queue_last = NULL;

static green_t *readyQ = NULL;

static void init() __attribute__ (( constructor ));

// --------Queue Functions----------
void pushReadyQ(green_t *entry) {
    //printf("in push\n");
    if (readyQ == NULL) {
        readyQ = entry;
    } else {
        green_t *temp = readyQ;
        while(temp->next != NULL) {
            temp = temp->next;
            //printf("stuck here");
        }

        temp->next = entry;
    }

    //printf("out of push\n");
}

green_t *popReadyQ() {
    //printf("in pop\n");
    green_t *result = readyQ;
    if (result != NULL) {
        readyQ = result->next;
        result->next = NULL;
    }
    //printf("out of pop\n");
    return result;
}

/*
void ready_enqueue(green_t *entry)
{

  if(ready_queue_last!= NULL && ready_queue_first!=NULL)
  {
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

void cond_enqueue(green_cond_t *cond, green_t *entry)
{
  if(cond->lastt != NULL)
  {
    cond->lastt->next = entry;
    cond->lastt = entry;
  }
  else
  {
    cond->firstt = entry;
    cond->lastt = entry;
  }
}

green_t *cond_dequeue(green_cond_t *cond)
{
  green_t *result = cond->firstt;

  if (cond->firstt != NULL) {
      if (result->next!=NULL) {
          cond->firstt = result->next;
      } else {
          cond->firstt = NULL;
          cond->lastt = NULL;
      }
  }

  if (result != NULL)
    result->next = NULL;

  return result;
}*/

// ---------Timer------------------
void timer_handler(int sig)
{
  //printf("----- Handler reached -----\n");
  green_yield();
}

// ------------Other----------------
/*
void addtojoin(green_t *addTo, green_t *addThis)
{
  if(addTo->join == NULL)
  {
    addTo->join=addThis;
    return;
  }

  green_t *temp = addTo->join;
  while (temp->next != NULL)
      temp = temp->next;

  temp->next = addThis;
}

void placeJoinInQueue(green_t *thread)
{
  if (thread->join == NULL)
      return;

  ready_enqueue(thread->join);
  if (thread->join->next != NULL) {
      green_t *temp = ready_queue_last;
      while (temp->next != NULL)
          temp = temp->next;

      ready_queue_last = temp;
  }
  //thread->join = NULL;
}
*/

void runNextGreen() {
    green_t *next = popReadyQ();
    if (next == NULL) {
        printf("---- Error, no more threads to run. ----\n");
        return;
    }

    running = next;
}
//-------------GREEN--------------


// Initialize the main_cntx so that when we call the cheduling function
// for the first time the running thread will be properly initialized.
void init()
{
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

  getcontext(&main_cntx);
}

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
    green_block_timer();
    //ready_enqueue(new);
    pushReadyQ(new);
    green_unblock_timer();
    return 0;
}

//tart the execution of the real function and, when after re-
//turning from the call, terminate the thread.
void green_thread()
{
    green_unblock_timer();

    green_t *this = running;
    (*this->fun)(this->arg);

    green_block_timer();
    // --place waiting (joining) thread in ready queue--
    //placeJoinInQueue(running);
    if (this->join != NULL)
        pushReadyQ(this->join);

    // --free allocated memory structures--
    free(running->context->uc_stack.ss_sp);
    free(running->context);

    // --we're a zombie--
    running->zombie = TRUE;

    // --find the next thread to run--
    runNextGreen();
    setcontext(running->context);
    green_unblock_timer();
}


// The user will call green_create() and provide: an uninitialized green_t structure,
// the function the thread should execute and pointer to its arguments. We will
// create new context, attach it to the thread structure and add this thread to
// the ready queue.

// Put the running thread last in the ready queue and then select the first
// thread from the queue as the next thread to run.
int green_yield()
{
  green_block_timer();
  green_t *susp = running;

  // add susp to ready queue
//  ready_enqueue(susp);
  pushReadyQ(susp);

  // select the next thread for execution
//  green_t *next = ready_dequeue();
  // -------
//  running = next;
  runNextGreen();
  swapcontext(susp->context, running->context);
  green_unblock_timer();
  return 0;
}


//Wait for a thread to terminate. We therefore add the thread
//to the join field and select another thread for execution.
int green_join (green_t *thread) {

  if (thread->zombie)
  return 0;

  green_t *susp = running;
  green_block_timer();
  // --add to waiting threads--
  //addtojoin(thread, susp);
  if (thread->join != NULL) {
      green_t *tmp = thread->join;
      while (tmp->next != NULL)
          tmp = tmp->next;
      tmp->next = susp;
  } else
      thread->join = susp;

  // --select the next thread for execution--
//  green_t *next = ready_dequeue();
  runNextGreen();

  swapcontext(susp->context, running->context);
  green_unblock_timer();
  return 0;
}


// initialize a green condition variable
void green_cond_init(green_cond_t* cond) {
  //cond = (green_cond_t *) malloc(sizeof(green_cond_t));
  cond->firstt = NULL;
  //cond->lastt = NULL;
}

void pushCondQ(green_t * entry, green_cond_t *cond) {
    if (cond->firstt == NULL) {
        cond->firstt = entry;
    } else {
        green_t *temp = cond->firstt;
        while(temp->next != NULL)
            temp = temp->next;
        temp->next = entry;
    }
}

// suspend the current thread on the condition
void green_cond_wait(green_cond_t* cond)
{
  green_block_timer();
  green_t *susp = running;
  pushCondQ(susp, cond);
  runNextGreen();
  swapcontext(susp->context, running->context);
  green_unblock_timer();
}

green_t *popCondQ(green_cond_t *cond) {
    green_t *result = cond->firstt;
    if (result != NULL) {
        cond->firstt = result->next;
        result->next = NULL;
    }

    return result;
}
// move the first suspended thread to the ready queue
void green_cond_signal(green_cond_t* cond)
{
  green_block_timer();
  green_t *res = popCondQ(cond);

  if (res != NULL)
      pushReadyQ(res);

  green_unblock_timer();
}

int green_mutex_init(green_mutex_t *mutex) {
    mutex->taken = FALSE;
    mutex->susp = NULL;
    return 0;
}

void pushMutexQ(green_t *entry, green_mutex_t *mutex) {
    if (mutex->susp == NULL) {
        mutex->susp = entry;
    } else {
        green_t *temp = mutex->susp;
        while(temp->next != NULL) {
            temp = temp->next;
        }

        temp->next = entry;
    }
}

int green_mutex_lock(green_mutex_t *mutex) {
    green_block_timer();

    green_t *susp = running;
    while (mutex->taken) {
        pushMutexQ(susp, mutex);
        runNextGreen();
        swapcontext(susp->context, running->context);
    }

    mutex->taken = TRUE;
    green_unblock_timer();
    return 0;
}

green_t *popMutexQ(green_mutex_t *mutex) {
    green_t *result = mutex->susp;
    mutex->susp = NULL;
    return result;
}

int green_mutex_unlock(green_mutex_t *mutex) {
    green_block_timer();
    green_t *suspList = popMutexQ(mutex);
    if (suspList != NULL)
        pushReadyQ(suspList);
    mutex->taken = FALSE;
    green_unblock_timer();
    return 0;
}

void green_block_timer() {
    if (sigprocmask(SIG_BLOCK, &block, NULL) == 0) {
      //printf("Blocked timer.\n");
      return;
    }

    printf("Error blocking timer.\n");
}

void green_unblock_timer() {
  if (sigprocmask(SIG_UNBLOCK, &block, NULL) == 0) {
    //printf("Unblocked timer.\n");
    return;
  }

  printf("Error unblocking timer.\n");
}
