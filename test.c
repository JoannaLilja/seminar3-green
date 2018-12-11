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
}


int flag = 0;
green_cond_t cond;

void *test2 (void *arg)
{
  int id = *(int*) arg;
  int loop = 10000;
  while ( loop > 0)
  {

    if ( flag == id )
    {

      printf (" thread %d: %d\n" , id, loop);
      loop--;
      flag = ( id + 1) % 2;
      green_cond_signal(&cond);
    }
    else {
      //green_cond_signal(&cond);
      printf("id: %d | flag: %d\n", id, flag);
      green_cond_wait(&cond);
    }
  }
}

void test()
{
  green_cond_init(&cond);

  green_t g0, g1;
  int a0 = 0;
  int a1 = 1;

  green_create(&g0, test2, &a0);
  printf("0 created\n");
  green_create(&g1, test2, &a1);
  printf("1 created\n");

  green_join(&g0);
  printf("0 joined\n");
  green_join(&g1);
  printf("1 joined\n");

  printf("done\n");

}


int main()
{
  green_cond_init(&cond);
  test();

  return 0;
}
