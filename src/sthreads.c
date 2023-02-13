/* On Mac OS (aka OS X) the ucontext.h functions are deprecated and requires the
   following define.
*/
#define _XOPEN_SOURCE 700

/* On Mac OS when compiling with gcc (clang) the -Wno-deprecated-declarations
   flag must also be used to suppress compiler warnings.
*/

#include <signal.h>   /* SIGSTKSZ (default stack size), MINDIGSTKSZ (minimal
                         stack size) */
#include <stdio.h>    /* puts(), printf(), fprintf(), perror(), setvbuf(), _IOLBF,
                         stdout, stderr */
#include <stdlib.h>   /* exit(), EXIT_SUCCESS, EXIT_FAILURE, malloc(), free() */
#include <ucontext.h> /* ucontext_t, getcontext(), makecontext(),
                         setcontext(), swapcontext() */
#include <stdbool.h>  /* true, false */

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

int tid_counter = 1;
thread_list_t *pool;
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


/*******************************************************************************
                    Implementation of the Simple Threads API
********************************************************************************/


int init(){
  pool = calloc(1, sizeof(pool));
  pool->first = NULL;
  pool->last = NULL;
  getcontext(&main_context);
  return 1;
}


tid_t spawn(void (*start)()){
  thread_t *thread = create_thread(start);
  thread_list_push(pool, thread);
  pool->first->state = running;
  if(swapcontext(&main_context, &pool->first->ctx)) { //if fail
    return -1;
  }
  return thread->tid;
}

void yield(){
}

void  done(){
}

tid_t join(tid_t thread_t) {
  return -1;
}
