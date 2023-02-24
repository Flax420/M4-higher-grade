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
  size_t    length;
}thread_list_t;

typedef struct {
  thread_list_t *ready_list;
  thread_list_t *terminated;
  thread_list_t *waiting;
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
  printf("The list length before inc. is: %lu\n", list->length);
  list->length++;
  printf("The list length after inc. is: %lu\n", list->length);
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
  list->length--;
  return first;
}

thread_t *thread_list_remove(thread_list_t *list, int index) {
  if (index == 0) {
    return thread_list_pop(list);
  }

  if (index >= list->length) {
    perror("Can't remove from index not in list!");
  }

  // Find thread to remove
  int thread_pos = 0;
  thread_t *prev_thread;
  thread_t *thread = list->first;
  while (thread_pos < index) {
    prev_thread = thread;
    thread = thread->next;
    thread_pos++;
  }

  prev_thread->next = thread->next;
  if (thread->next == NULL) {
    list->last = prev_thread;
  }

  list->length--;
  return thread;
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
      yield();
      break;
  }
}


void set_timer(void (*handler) (int)) {
  struct itimerval timer;
  struct sigaction sa;

  memset (&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigaction(SIGALRM, &sa, NULL);

  memset (&timer, 0, sizeof(timer));
  timer.it_value.tv_usec = 50*1000;

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
  pool->waiting = calloc(1, sizeof(thread_list_t));

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
    set_timer(signal_handler);
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
  thread_t *job = pool->ready_list->first;

  // Terminate job
  job->state = terminated;
  thread_list_push(pool->terminated, thread_list_pop(pool->ready_list));

  // Check for waiting jobs
  thread_t *waiting_thread;
  int len = pool->waiting->length;
  printf("There are %d waiting jobs\n", len);
  for (int i = 0; i < len; i++) {
    waiting_thread = thread_list_pop(pool->waiting);

    // If waiting for the current job
    if (waiting_thread->waiting_for == job->tid) {
      puts("There was a waiting job");
      waiting_thread->state = ready;
      waiting_thread->waiting_for = 0;
      thread_list_push(pool->ready_list, waiting_thread);
      continue;
    }

    puts("There was a job waiting for another thread");
    thread_list_push(pool->waiting, waiting_thread);
  }

  // Run next job
  thread_t *new_job = pool->ready_list->first;
  new_job->state = running;
  set_timer(signal_handler);
  swapcontext(&job->ctx, &new_job->ctx);
}


tid_t join(tid_t thread) {
  thread_t *job = pool->ready_list->first;

  // Check terminated jobs
  thread_t *terminated_thread;
  int len = pool->terminated->length;
  for (int i = 0; i < len; i++) {
    terminated_thread = thread_list_pop(pool->terminated);

    // If found the thread to join
    if (terminated_thread->tid == thread) {
      thread_list_push(pool->terminated, terminated_thread);
      puts("returned immediately");
      return thread;
    }

    thread_list_push(pool->terminated, terminated_thread);
  }

  // If the thread is not already terminated
  puts("Adding job to waiting list");
  job->state = waiting;
  printf("The length of the waiting list before adding a job is: %lu\n", pool->waiting->length);
  thread_t *t = thread_list_pop(pool->ready_list);
  printf("removed the thread");
  thread_list_push(pool->waiting, t);
  printf("The length of the waiting list after adding a job is: %lu\n", pool->waiting->length);
  job->waiting_for = thread;

  // Run next job
  thread_t *new_job = pool->ready_list->first;
  new_job->state = running;
  set_timer(signal_handler);
  puts("running another job");
  swapcontext(&job->ctx, &new_job->ctx);

  puts("returning after waiting");
  return thread;
}
