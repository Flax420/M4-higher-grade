/* On Mac OS (aka OS X) the ucontext.h functions are deprecated and requires the
   following define.
*/
#define _XOPEN_SOURCE 700

/* On Mac OS when compiling with gcc (clang) the -Wno-deprecated-declarations
   flag must also be used to suppress compiler warnings.
*/

#include <signal.h>   /* SIGSTKSZ (default stack size), MINDIGSTKSZ (minimal
                         stack size) */
#include <stdbool.h>  /* true, false */
#include <stdio.h>    /* puts(), printf(), fprintf(), perror(), setvbuf(), _IOLBF,
                         stdout, stderr */
#include <stdlib.h>   /* exit(), EXIT_SUCCESS, EXIT_FAILURE, malloc(), free() */
#include <string.h>   /* memset() */
#include <sys/time.h> /* ITIMER_REAL, struct itimerval, setitimer() */
#include <ucontext.h> /* ucontext_t, getcontext(), makecontext(),
                         setcontext(), swapcontext() */

#include "sthreads.h"

/* Stack size for each context. */
#define STACK_SIZE SIGSTKSZ*100

/*******************************************************************************
                             Global data structures

                Add data structures to manage the threads here.
********************************************************************************/

typedef struct {
  thread_t *first;
  thread_t *last;
}thread_list_t;

typedef struct {
  thread_list_t *ready_list;
  thread_list_t *terminated;
} pool_t;

int tid_counter = 1;
pool_t *pool;
ucontext_t main_context;

/*******************************************************************************
                             Auxiliary functions

                      Add internal helper functions here.
********************************************************************************/

void thread_list_push(thread_list_t *list , thread_t *thread) {
  if(list->first == NULL) {
    list->first = thread;
    list->last = thread;
    return;
  }
  list->last->next = thread;
  list->last = thread;
}

thread_t *thread_list_pop(thread_list_t *list) {
  if(list->first == NULL) { // List empty
    return NULL;
  } else if(list->first->next == NULL) { // only element
    thread_t *first = list->first;
    list->first = NULL;
    list->last = NULL;
    return first;
  }
  thread_t *first = list->first;
  list->first = first->next;
  return first;
}

void create_context(ucontext_t *ctx) {
  ctx->uc_stack.ss_sp = calloc(STACK_SIZE, sizeof(char));
  ctx->uc_stack.ss_size = STACK_SIZE;
  ctx->uc_link = &main_context;
}

thread_t *create_thread(void (*start)()){
  thread_t *thread = calloc(1, sizeof(thread_t));
  thread->tid = tid_counter;
  thread->state = waiting;
  getcontext(&thread->ctx);
  create_context(&thread->ctx);
  makecontext(&thread->ctx, start, -1); 
  tid_counter++;
  return thread;
}


void signal_handler(int s) {
  switch (s) {
    case SIGALRM:
      puts("alarm");
      break;
  }
}


void set_timer(void (*handler) (int)) {
  struct itimerval timer;
  struct sigaction sa;

  memset (&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigaction(SIGALRM, &sa, NULL);

  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 50*1000;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;

  if (setitimer (ITIMER_REAL, &timer, NULL) < 0) {
    perror("Error setting timer");
    exit(EXIT_FAILURE);
  }
}


/*******************************************************************************
                    Implementation of the Simple Threads API
********************************************************************************/


int init(){
  pool = calloc(1, sizeof(pool_t));
  pool->ready_list = calloc(1, sizeof(thread_list_t));
  pool->terminated = calloc(1, sizeof(thread_list_t));
  pool->ready_list->first = NULL;
  pool->ready_list->last = NULL;
  pool->terminated->first = NULL;
  pool->terminated->last = NULL;
  getcontext(&main_context);
  return 1;
}


tid_t spawn(void (*start)()){
  thread_t *thread = create_thread(start);
  thread_list_push(pool->ready_list, thread);
  return thread->tid;
}


void yield(){
  thread_t *job = pool->ready_list->first;
  if (job == NULL) { 
    puts("There is no job to yield!");
    return;
  }
  // if in the main job
  if (job->state != running) {
    job->state = running;
    swapcontext(&main_context, &job->ctx);
    return;
  }

  job->state = ready;
  thread_list_push(pool->ready_list, thread_list_pop(pool->ready_list));
  thread_t *new_job = pool->ready_list->first;
  new_job->state = running;
  set_timer(signal_handler);
  swapcontext(&job->ctx, &new_job->ctx);
}


void done(){
}


tid_t join(tid_t thread_t) {
  return thread_t;
}
