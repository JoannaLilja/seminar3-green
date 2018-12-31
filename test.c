#include <stdio.h>
#include "green.h"

void *test1 (void *arg)
{
  int i = *(int*) arg;
  int loop = 4;
  while (loop > 0)
  {
    printf ( "thread %d: %d\n" , i, loop );
    loop--;
    green_yield();
  }

  return NULL;
}


int flag = 0;
green_cond_t cond;

void *test2(void *arg)
{
  int id = *(int*) arg;
  int loop = 10000;
  while ( loop > 0)
  {

    if ( flag == id )
    {
     /* for(int i = 0; i < 1000000; i++) {
        if(i < 1)
          i++;
      }*/
      loop--;
      flag = ( id + 1) % 4;
      printf("Thread %d: %d\n" , id, loop);
      green_cond_signal(&cond);
    }
    else {
      green_cond_signal(&cond);
      green_cond_wait(&cond);
    }
  }

  return NULL;
}

int count = 0;
green_mutex_t mutex;
void *testCounter(void *arg) {
  int i = *(int*)arg;
  int loop = 1000000;
  while(loop > 0) {
    green_mutex_lock(&mutex);
    count++;
    loop--;
    green_mutex_unlock(&mutex);
  }
}

int cell = 0;
int productions = 0;
void *producer(void *arg) {
  for (int i = 0; i < 10000; i++) {
    while (cell == 1)
        green_cond_wait_lock(&cond, &mutex);

    cell = 1;
    productions++;
    //printf("Produced!\n");
    green_cond_signal(&cond);
    green_mutex_unlock(&mutex);
  }
}

int consumptions = 0;
void *consumer(void *arg) {
  for (int i = 0; i < 10000; i++) {
    while (cell == 0)
      green_cond_wait_lock(&cond, &mutex);

    cell = 0;
    consumptions++;
   // printf("Consumed!\n");
    green_cond_signal(&cond);
    green_mutex_unlock(&mutex);
  }
}

void test()
{
  green_t g0, g1, g2, g3;
  int a0 = 0;
  int a1 = 1;
  int a2 = 2;
  int a3 = 3;

  green_create(&g0, test2, &a0);
  printf("0 created\n");
  green_create(&g1, test2, &a1);
  printf("1 created\n");
  green_create(&g2, test2, &a2);
  green_create(&g3, test2, &a3);

  green_join(&g0);
  printf("0 joined\n");
  green_join(&g1);
  printf("1 joined\n");
  green_join(&g2);
  printf("2 joined\n");
  green_join(&g3);
  printf("3 joined\n");

  printf("Count: %d\n", count);
  printf("done\n");
}

void testProdConsumer() {
  green_t g0, g1;
  green_create(&g0, producer, NULL);
  green_create(&g1, consumer, NULL);

  green_join(&g0);
  green_join(&g1);

  printf("Produced: %d | Consumed %d\n", productions, consumptions);
}

int main()
{
    green_cond_init(&cond);
    green_mutex_init(&mutex);
    //test();
    testProdConsumer();
    return 0;
}
