// File:	rpthread_t.h

// List all group member's name: Jianxin Deng, Xuanang Wang
// username of iLab: jd1216, xw272
// iLab Server: ilab2.cs.rutgers.edu

#ifndef RTHREAD_T_H
#define RTHREAD_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_RTHREAD macro */
#define USE_RTHREAD 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>

typedef uint rpthread_t;


typedef struct dynamic_threadID
{
    rpthread_t id;
    int in_use;// 0 not in use 1 is in use
    struct dynamic_threadID* next;
}r_tid;


typedef struct threadControlBlock {
	/* add important states in a thread control block */
	// thread Id
	// thread status
	// thread context
	// thread stack
	// thread priority
	// And more ...

	// YOUR CODE HERE
	rpthread_t id;
	int status; // 0 for blocked, 1 for ready, 2 for running, 3 for exit, 4 for yield
	ucontext_t context;
	int priority;
	int numberOfTimeSliceUsed;
	void* ret_val;
	rpthread_t so_wait;

} tcb;

/* LL */
typedef struct node
{
    tcb* value;
    struct node *next;
}node;

/* mutex struct definition */
typedef struct rpthread_mutex_t {
	/* add something here */

	// YOUR CODE HERE
} rpthread_mutex_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE


/* Function Declarations: */

/* create a new thread */
int rpthread_create(rpthread_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int rpthread_yield();

/* terminate a thread */
void rpthread_exit(void *value_ptr);

/* wait for thread termination */
int rpthread_join(rpthread_t thread, void **value_ptr);

/* initial the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex);

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex);

/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex);

static void schedule();

static void sched_stcf();

static void sched_mlfq();

// Your Code here

void stcf_insertProperPosi(int index);

void mlfq_scheduleNextJob();
void mlfq_insertProperPosi(int notsu);
void mlfq_boost();

node* findWaiter(rpthread_t target_tid);


//helper fucntion
void ShowHead1();
void ShowHead2();
void ShowHead3();
void ShowHead4();
void ShowRetLL();
void ShowBlockLL();
void setTime();
void stopTimer();
void ring();

/* pop the first node of a given LL */
//struct *node pop_node(struct *node head);

#ifdef USE_RTHREAD
#define pthread_t rpthread_t
#define pthread_mutex_t rpthread_mutex_t
#define pthread_create rpthread_create
#define pthread_exit rpthread_exit
#define pthread_join rpthread_join
#define pthread_mutex_init rpthread_mutex_init
#define pthread_mutex_lock rpthread_mutex_lock
#define pthread_mutex_unlock rpthread_mutex_unlock
#define pthread_mutex_destroy rpthread_mutex_destroy

#define pthread_yield rpthread_yield
#endif

#endif
