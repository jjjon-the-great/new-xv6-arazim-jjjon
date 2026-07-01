#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
//#include "kernel/proc.h"


/* Possible states of a thread: */
#define FREE        0x0
#define RUNNING     0x1
#define RUNNABLE    0x2

#define STACK_SIZE  8192
#define MAX_THREAD  4

struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};
struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  //void (*func)();               /* the thread's function*/
  //int   is_first_run;           /* 1 if this is the thread's first run*/
  struct context * context;      /* context. */
};

struct thread all_thread[MAX_THREAD];
struct thread *current_thread;
extern void thread_switch(uint64, uint64);
struct thread *threadb;
              
void 
thread_init(void)
{
  // main() is thread 0, which will make the first invocation to
  // thread_schedule().  it needs a stack so that the first thread_switch() can
  // save thread 0's state.  thread_schedule() won't run the main thread ever
  // again, because its state is set to RUNNING, and thread_schedule() selects
  // a RUNNABLE thread.
  /*for (int i = 0; i < MAX_THREAD, i++){
    all_thread[i] = (struct thread *)malloc(sizeof(struct thread));
  }*/
  current_thread = &all_thread[0];
  current_thread->state = RUNNING;
  current_thread->context = (struct context *)malloc(sizeof(struct context));

}

void 
thread_schedule(void)
{
  struct thread *t, *next_thread;
  //printf("current %p status %p\n", current_thread, current_thread->state);
  /* Find another runnable thread. */
  next_thread = 0;
  t = current_thread + 1;
  for(int i = 0; i < MAX_THREAD; i++){
    //printf("checking %p stats %d\n", t, t->state);
    if(t >= all_thread + MAX_THREAD)
      t = all_thread+1;
    if(t->state == RUNNABLE) {
      next_thread = t;
      break;
    }
    t = t + 1;
  }
  //printf("selected thread %p\n", t);

  if (next_thread == 0) {
    printf("thread_schedule: no runnable threads\n");
    exit(-1);
  }

  if (current_thread != next_thread) {   
    //printf("current thread %p, current status %p\n", current_thread, current_thread->state);      /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    if (current_thread->state == RUNNING){
      current_thread->state = RUNNABLE;
    } else {
      free(current_thread->context);
    }
    //current_thread->state = RUNNABLE;
    current_thread = next_thread;
    //printf("current thread %p, current status %p\n", t, t->state);
    //printf("status located at %p\n", &(t->state));
    //printf("were editing %p\n",(uint64)(t->context));
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     * 
     */
    //thread_switch();
    //printf("from %p to %p\n", t, current_thread);
    //printf("trying to switch - next program is %p\n", next_thread->context->ra);
    //printf("well %d\n", (next_thread->context->ra));
    //printf("first before: %p\n", t->context->ra);
    //printf("threads: %p, %p\n", t, next_thread);
    //printf("contexts: %p, %p\n", t->context, next_thread->context);
    //printf("specific addresses: %p, %p\n", &(t->context->ra), &(next_thread->context->ra));
    //printf("args: %p, %p\n", (uint64)(t->context), (uint64)(next_thread->context));
    thread_switch((uint64)(t->context), (uint64)(current_thread->context));
    //printf("first after: %p\n", t->context->ra);
    //printf("nothing happened?\n");
  } else{
    //printf("weird\n");
    next_thread = 0; }
}

void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  //t->is_first_run = 1;
  //t->func = func;
  t->context = (struct context *)malloc(sizeof(struct context));
  t->context->ra = (uint64)func;
  t->context->sp = ((uint64)t->stack)+STACK_SIZE;
  //*(uint64 *)&(t->stack) = (uint64)func;
  //printf("thread %p with f %p s %p\n", t ,t->context->ra, t->context->sp );
  // YOUR CODE HERE
}

void 
thread_yield(void)
{ 
  //printf("current thread is %p\n", current_thread);
  //current_thread->state = RUNNABLE;
  //printf("status is %d\n", current_thread->state);
  thread_schedule();
}

volatile int a_started, b_started, c_started;
volatile int a_n, b_n, c_n;

void 
thread_a(void)
{
  int i;
  printf("thread_a started\n");
  a_started = 1;
  while(b_started == 0 || c_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_a %d\n", i);
    a_n += 1;
    thread_yield();
  }
  printf("thread_a: exit after %d\n", a_n);

  current_thread->state = FREE;
  thread_schedule();
}

void 
thread_b(void)
{
  int i;
  printf("thread_b started\n");
  b_started = 1;
  threadb = current_thread;
  while(a_started == 0 || c_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_b %d\n", i);
    b_n += 1;
    thread_yield();
  }
  printf("thread_b: exit after %d\n", b_n);

  current_thread->state = FREE;
  thread_schedule();
}

void 
thread_c(void)
{
  int i;
  printf("thread_c started\n");
  //printf("looking at %p,  status %p\n", threadb, threadb->state);
  c_started = 1;
  while(a_started == 0 || b_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_c %d\n", i);
    c_n += 1;
    thread_yield();
  }
  printf("thread_c: exit after %d\n", c_n);

  current_thread->state = FREE;
  thread_schedule();
}

int 
main(int argc, char *argv[]) 
{
  a_started = b_started = c_started = 0;
  a_n = b_n = c_n = 0;
  thread_init();
  thread_create(thread_a);
  thread_create(thread_b);
  thread_create(thread_c);
  printf("created threads\n");
  thread_schedule();
  exit(0);
}
