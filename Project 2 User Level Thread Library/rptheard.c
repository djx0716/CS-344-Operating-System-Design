// File:	rpthread.c

// List all group member's name: Jianxin Deng, Xuanang Wang
// username of iLab: jd1216, xw272
// iLab Server: ilab2.cs.rutgers.edu

#include "rpthread.h"
#include <ucontext.h>
#include <stdlib.h>
#include <stdio.h>

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE

// timer
struct sigaction sa;
struct itimerval timer;

/* initialize the head of LL and set its next to NULL */

// LL for queue1
node* head1;
int head1_valid = 0;

// LL for queue2
node* head2;
int head2_valid = 0;

// LL for queue3
node* head3;
int head3_valid = 0;

// LL for queue4
node* head4;
int head4_valid = 0;

// LL for unique_id
r_tid* id_head;
int id_valid = 0;
rpthread_t unique_id = 0;

// LL for exited threads that wait to be joined
node* join_thread_head = NULL;

// blocked list head
node* blocked_thread_head = NULL;

// global scheduler context
ucontext_t gs_ctt;
int gs_valid = 0;

// global main context (new)
ucontext_t gm_ctt;
int gm_valid = 0;

//currently running thread & nextRunningThread
/* REMEMBER TO UPDATE THEM WHEN WRRITING SCHDUELER!!! */
node* currentRunning = NULL;
node* currentRLL;

/* create a new thread */
int rpthread_create(rpthread_t * thread, pthread_attr_t * attr,
                      void *(*function)(void*), void * arg)
{
    // Create Thread Control Block
    // Create and initialize the context of this thread
    // Allocate space of stack for this thread to run
    // after everything is all set, push this thread int
    // YOUR CODE HERE

    /* initialize timer */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &ring;
    sigaction(SIGPROF, &sa, NULL);

    /* initialize bth */
    if(blocked_thread_head == NULL)
    {
        blocked_thread_head = (node*)malloc(sizeof(node));
        blocked_thread_head->next = NULL;
    }

    /* exit LL initialize */
    if(join_thread_head == NULL)
    {
        join_thread_head = (node*)malloc(sizeof(node));
        join_thread_head->next = NULL;
    }

    /* scheduler set up */
    if (gs_valid == 0)
    {
        gs_valid = 1;

        // stack set up
        // just in case delete this
        getcontext(&(gs_ctt));
        void* gs_stack = malloc(SIGSTKSZ);
        gs_ctt.uc_link = NULL;
        gs_ctt.uc_stack.ss_sp = gs_stack;
        gs_ctt.uc_stack.ss_size = SIGSTKSZ;
        gs_ctt.uc_stack.ss_flags = 0;

        // bind scheduler context with function scheduler
        makecontext(&gs_ctt, (void*)&schedule, 0);
    }

    /* initialize tcb */
    tcb* block = (tcb*)malloc(sizeof(tcb));

    /* unique id set up */
    if (id_valid == 0)
    {
        id_valid = 1; // indicates that head is initialized

        // initialize head
        id_head = (r_tid*)malloc(sizeof(r_tid));

        // initialize new tid
        r_tid* new_tid = (r_tid*)malloc(sizeof(r_tid));
        new_tid->in_use = 1;
        new_tid->id = unique_id;
        unique_id++;
        new_tid->next = NULL;

        // put new tid at the end of LL
        id_head->next = new_tid;
        id_head->in_use = 1;

        block->id = new_tid->id;
        *thread = block->id;
    }
    else
    {
        r_tid* id_ptr = id_head; // initial a pointer for traversal

        /* traverse until the end or found a not in used spot */
        while(id_ptr->next != NULL)
        {
            if(id_ptr->in_use == 0) // if an empty spot found, handle it outside
            {
                break;
            }
            id_ptr = id_ptr->next; //else, keep looping
        }

        if(id_ptr->in_use == 0) // if the loop was out because empty spot found
        {
            id_ptr->in_use == 1; // make it in use
            block->id = id_ptr->id; // dynamically done
            *thread = block->id;
        }

        else //if the loop was out because it reaches the end of LL
        {
            /* set up the new tid */
            r_tid* new_tid = (r_tid*)malloc(sizeof(r_tid));
            new_tid->in_use = 1;
            new_tid->id = unique_id;
            unique_id++;
            new_tid->next = NULL;

            /* append new id at the end of LL */
            id_ptr->next = new_tid;

            block->id = new_tid->id;
            *thread = block->id;
        }
    }

    /* this thread context set up */
    ucontext_t new_ctt;
    getcontext(&(new_ctt));
    void *stack = malloc(SIGSTKSZ);
    new_ctt.uc_link = NULL;
    new_ctt.uc_stack.ss_sp = stack;
    new_ctt.uc_stack.ss_size = SIGSTKSZ;
    new_ctt.uc_stack.ss_flags = 0;
    block->status = 1;
    block->priority = 1;
    block->numberOfTimeSliceUsed = 0;
    block->so_wait = -1;
    block->context = new_ctt;

    // this thread make context
    makecontext(&(block->context),(void*)function, 1, (void*)arg);


    /* set up new node and append at the end of head1 */
    if (head1_valid == 0)
    {// if head has not been initialized
        head1_valid = 1;
        head1 = (node*)malloc(sizeof(node));
        head1->next = NULL;
        /* initialize curentRLL */
        currentRLL = head1;
    }

    if (head2_valid == 0)
    {// if head has not been initialized
        head2_valid = 1;
        head2 = (node*)malloc(sizeof(node));
        head2->next = NULL;
    }

    if (head3_valid == 0)
    {// if head has not been initialized
        head3_valid = 1;
        head3 = (node*)malloc(sizeof(node));
        head3->next = NULL;
    }

    if (head4_valid == 0)
    {// if head has not been initialized
        head4_valid = 1;
        head4 = (node*)malloc(sizeof(node));
        head4->next = NULL;
    }

    // set up the new node
    node* new_node = (node*)malloc(sizeof(node));
    new_node->value = block;

    if(head1->next == NULL)
    {
        head1->next = new_node;
        new_node->next = NULL;
    }
    else
    {
        node* newNode_temp = head1->next;
        head1->next = new_node;
        new_node->next = newNode_temp;
    }

    /* main context is not initialized, initialize it */
    if (gm_valid == 0)
    {
        gm_valid = 1;
        // initialize context for main
        getcontext(&(gm_ctt));
        void* gm_stack = malloc(SIGSTKSZ);
        gm_ctt.uc_link = NULL;
        gm_ctt.uc_stack.ss_sp = gm_stack;
        gm_ctt.uc_stack.ss_size = SIGSTKSZ;
        gm_ctt.uc_stack.ss_flags = 0;

        // set up tcb
        tcb* main_tcb = (tcb*)malloc(sizeof(tcb));
        main_tcb->numberOfTimeSliceUsed = 0;
        main_tcb->status = 1;
        main_tcb->priority = 1;
        main_tcb->so_wait = -1;
        main_tcb->context = gm_ctt;
        // traverse and find a free spot for uniqueID
        r_tid* id_ptr2 = id_head;

        /* traverse until the end or found a not in used spot */
        while(id_ptr2->next != NULL)
        {
            if(id_ptr2->in_use == 0) // if an empty spot found, handle it outside
            {
                break;
            }
            id_ptr2 = id_ptr2->next; //else, keep looping
        }

        if(id_ptr2->in_use == 0) // if the loop was out because empty spot found
        {
            id_ptr2->in_use = 1; // make it in use
            main_tcb->id = id_ptr2->id; // dynamically done
        }

        else //if the loop was out because it reaches the end of LL
        {
            /* set up the new tid */
            r_tid* new_tid2 = (r_tid*)malloc(sizeof(r_tid));
            new_tid2->in_use = 1;
            new_tid2->id = unique_id;
            unique_id++;
            new_tid2->next = NULL;

            /* append new id at the end of LL */
            id_ptr2->next = new_tid2;

            main_tcb->id = new_tid2->id;
        }

        node* main_node = (node*)malloc(sizeof(node)); // main node
        main_node->value = main_tcb;
        currentRunning = main_node;

        node* main_ptr = head1->next;
        head1->next = main_node;
        main_node->next = main_ptr;
        ring();
    }

    /* stop the timer because there is a timer exist and we do not want the
     timer to keep going in schedule*/
    else
    {
        timer.it_interval.tv_usec = 0;
        timer.it_interval.tv_sec = 0;
        timer.it_value.tv_usec = 0;
        timer.it_value.tv_sec = 0;
        setitimer(ITIMER_PROF, &timer, NULL); // timer is stopped here

        ring();
    }

    // ShowHead1();

    return block->id; // return unique thread id to the user
}

/* give CPU possession to other user-level threads voluntarily */
int rpthread_yield() {
	// Change thread state from Running to Ready
	// Save context of this thread to its thread control block
	// switch from thread context to scheduler context
	// YOUR CODE HERE

    stopTimer();
	// no threads can be run OR no thread is running


	/* now there is a thread running, yiled it and add it to the
	end of runQueue, swapcontext to the schduler */
	tcb* temp = currentRunning->value;
    //change tcb status to 4 (yield)
    temp->status = 4;
	swapcontext(&(temp->context),&(gs_ctt));

	return 0;
};

/* terminate a thread */
void rpthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread

    // YOUR CODE HERE
    // after user call exit, throw this node to the wait_join LL
    stopTimer();

    // handle the thread that is waitting for the caller thread
    rpthread_t waiter_id = currentRunning->value->so_wait; //  waiter is the thr thread waitting for caller thread

    if(waiter_id != -1){
        // someone is waitting the current thread, let the waiter thread to be ready for run
        node* waiter_node;

        waiter_node = findWaiter(waiter_id);
        // find waiter node from blocked list
        if(waiter_node == NULL) printf("djx fucked up in exit for handle block!!!!");
        waiter_node->value->status = 1; // 1 for ready
        int exit_flag = 0;
        node* ptr = head1->next;
        while(ptr!=NULL){
            // determine which scheduler user us using, 1 for schf, 0 for mlfq
            if(ptr->value->numberOfTimeSliceUsed > 0){
                exit_flag = 1;
            }
            ptr = ptr->next;
        }
        int waiter_notsu = waiter_node->value->numberOfTimeSliceUsed;
        node* LLptr;
        if(exit_flag == 0){
            //we are in mlfq
            if(waiter_notsu == 0){
                LLptr = head1->next;
            }
            else if(waiter_notsu == 1){
                LLptr = head2->next;
            }
            else if(waiter_notsu == 2){
                LLptr = head3->next;
            }
            else{
                LLptr = head4->next;
            }
            while(LLptr->next!=NULL){
                LLptr = LLptr->next;
            }
            LLptr->next = waiter_node;
        }
        else{
            // we are in schf
            node* LLptr_prev = head1;
            LLptr = head1->next;

            if(LLptr == NULL){
                // no thread in head1
                LLptr_prev->next = waiter_node;
            }
            else{
                while(LLptr->value->numberOfTimeSliceUsed < waiter_notsu){
                    LLptr_prev = LLptr;
                    LLptr = LLptr->next;
                }
                LLptr_prev->next = waiter_node;
                waiter_node->next = LLptr;
            }
        }

        currentRunning->value->so_wait = -1;

        /* handle waiter node, delink it from blocked_lsit
           reinsert it to proper position (deicde which scheduler user called) */

    }

    //update ret_val info, 3 means thread is exited
    currentRunning->value->ret_val = value_ptr;
    currentRunning->value->status = 3;
    // delink this thread from the LL it belongs to
    node* ptr = currentRLL;
    while(ptr->next != currentRunning){
        //for debug
        if(ptr == NULL){
            printf("fucked up in exit\n");
        }
        ptr = ptr->next;
    }
    ptr->next = currentRunning->next;

    // no thread has called exit yet
    if(join_thread_head == NULL){
        join_thread_head = (node*)malloc(sizeof(node));
        join_thread_head->next = currentRunning;
        currentRunning->next = NULL;
        setcontext(&(gs_ctt));
        return;
    }

    node* wait_ptr = join_thread_head;
    while(wait_ptr->next != NULL){
        wait_ptr = wait_ptr->next;
    }
    currentRunning->next = NULL;
    wait_ptr->next = currentRunning;
    setcontext(&(gs_ctt));
};


//* Wait for thread termination */
int rpthread_join(rpthread_t thread, void **value_ptr) {

	// Wait for a specific thread to terminate
	// De-allocate any dynamic memory created by the joining thread

	// YOUR CODE HERE
    stopTimer();
    int join_flag = 0;
    node* thread_ptr = NULL;

	/* first check the exit list to see whether the thread represented
     by the parameter "thread" is finished*/
    if(join_thread_head->next == NULL)
    {
        int join_flag = 1; // means not finished yet
    }
    else
    {
        // ptr to the next of head of finished queue because head might be NULL
        node* join_ptr = join_thread_head->next;
        // loop until the end of finished queue to see whether thread with id
        // "thread" is finished
        while(join_ptr != NULL)
        {
            if(join_ptr->value->id == thread)
            {
                join_flag = 2;
                thread_ptr = join_ptr;
                break;
            }
            join_ptr = join_ptr->next;
        }

        if (join_flag == 0)
        {
            join_flag = 1;
        }
    }

        /* as this point, we are done traversing the finished list 0 should no longer exist,
           1 means not found and 2 means found*/


        if(join_flag == 2)
        {
           setTime();
        }

        /* not in the exit list, start traversing the job queue */
        else
        {
            /* set currentRunning to blocked */
            currentRunning->value->status = 0;

            /* delink this blocked thing in the job queue */
            if(blocked_thread_head == NULL)
            {
                blocked_thread_head = (node*)malloc(sizeof(node));
                blocked_thread_head->next = NULL;
            }

            node* crPre_ptr = currentRLL;

            while(crPre_ptr->next != currentRunning)
            {
                crPre_ptr = crPre_ptr->next;
            }

            crPre_ptr->next = currentRunning->next;

            /* put it into the blocked queue */
            node* bt_ptr = blocked_thread_head;

            while(bt_ptr->next != NULL)
            {
                bt_ptr = bt_ptr->next;
            }

            bt_ptr->next = currentRunning;
            currentRunning->next = NULL;

            /* find the node that has id "thread" */
            node* join_ptr2 = head1->next;

            while(join_ptr2 != NULL)
            {
                if(join_ptr2->value->id == thread) // found!
                {
                    thread_ptr = join_ptr2;
                    break;
                }

                join_ptr2 = join_ptr2->next;
            }


            /* note that join_ptr2 has not to be NULL besides MLFQ */
            if(thread_ptr == NULL)
            {
                //MLFQ
                node* join_ptr3 = head2->next;
                int MLFQ_flag = 0;

                while(join_ptr3 != NULL)
                {
                    if(join_ptr3->value->id == thread) // found!
                    {
                        thread_ptr = join_ptr3;
                        MLFQ_flag = 1;
                        break;
                    }

                    join_ptr3 = join_ptr3->next;
                }

                join_ptr3 = head3->next;
                while(join_ptr3 != NULL && MLFQ_flag == 0)
                {
                    if(join_ptr3->value->id == thread) // found!
                    {
                        thread_ptr = join_ptr3;
                        MLFQ_flag = 1;
                    }

                    join_ptr3 = join_ptr3->next;
                }

                join_ptr3 = head4->next;
                while(join_ptr3 != NULL && MLFQ_flag == 0)
                {
                    if(join_ptr3->value->id == thread) // found!
                    {
                        thread_ptr = join_ptr3;
                        MLFQ_flag = 1;
                    }

                    join_ptr3 = join_ptr3->next;
                }
            }

            /* someone is waiting */
            thread_ptr->value->so_wait = currentRunning->value->id;

            /* while the thread current is waiting has not finished */
            while(thread_ptr->value->so_wait != -1)
            {
                stopTimer();
                swapcontext(&(currentRunning->value->context), &gs_ctt);
            }
        }

        //if(thread_ptr == NULL) printf("BUGGGGGGGGGGGGGG\n");
        /* store ret_val */
        if(value_ptr != NULL)
        {
            *value_ptr = thread_ptr->value->ret_val; // bug?
        }

        /* delink */

        // delink from finished list
        node* delink_prev = join_thread_head;

        // find the previous node of the one that would be delinked
        while(delink_prev->next != thread_ptr)
        {
             delink_prev = delink_prev->next;
        }

         delink_prev->next = thread_ptr->next;

         /* free */

        // make this id free to use
        r_tid* reuse_ptr = id_head->next;

        while(reuse_ptr->id != thread_ptr->value->id)
        {
            reuse_ptr = reuse_ptr->next;
        }

        reuse_ptr->in_use = 0;

        // free its tcb
        free(thread_ptr->value);

        // free the node
        free(thread_ptr);

	return 0;
};

/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex,
                          const pthread_mutexattr_t *mutexattr) {
	//Initialize data structures for this mutex

	// YOUR CODE HERE
	return 0;
};

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex) {
        // use the built-in test-and-set atomic function to test the mutex
        // When the mutex is acquired successfully, enter the critical section
        // If acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
        return 0;
};

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	// Release mutex and make it available again.
	// Put threads in block list to run queue
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	return 0;
};

/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in rpthread_mutex_init

	return 0;
};

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)
	// YOUR CODE HERE

    // caller thread is exited, scheule a new thread
    if(currentRunning->value->status == 3){
        mlfq_scheduleNextJob();
        currentRunning->value->status = 2; // 2 for running
        setTime();
        setcontext(&(currentRunning->value->context));
        return;
    }

    // caller thread is yilded, we don't increment notsu this case
    // modify thread tcb values
    int curr_notsu = currentRunning->value->numberOfTimeSliceUsed;
    if(currentRunning->value->status != 4){
        currentRunning->value->numberOfTimeSliceUsed++;
        curr_notsu++;
    }

    if(currentRunning->value->status != 0)
    {
         mlfq_insertProperPosi(curr_notsu);
         currentRunning->value->status = 1; // 1 for ready
    }

    mlfq_scheduleNextJob();

    currentRunning->value->status = 2; // 2 for running
    setTime();
    setcontext(&(currentRunning->value->context));
    return;

    // privority boost?

}

/* Preemptive SJF (STCF) scheduling algorithm */
static void sched_stcf() {
	// Your own implementation of STCF
	// (feel free to modify arguments and return types)
	// YOUR CODE HERE
    // check thread status
    if(currentRunning->value->status == 3){
        currentRunning = head1->next;
        currentRunning->value->status = 2; // 2 for running
        setTime();
        setcontext(&(currentRunning->value->context));
        return;
    }

    tcb* curr_tcb = currentRunning->value;
    // update # of times that this thread has used assigned timt slice
    int curr_notsu = curr_tcb->numberOfTimeSliceUsed;
    currentRunning->value->numberOfTimeSliceUsed++;
    curr_notsu++;
    //reinsert the currentRunning Thread into the queue
    stcf_insertProperPosi(curr_notsu);
    curr_tcb->status = 1; // 1 for ready

    // run the next job
    currentRunning = head1->next;
    currentRunning->value->status = 2; // 2 for running
    //printf("current running AFTER switch is %d\n",currentRunning->value->id);

    setTime();
    setcontext(&(currentRunning->value->context));

}

/* scheduler */
static void schedule() {
	// Every time when timer interrup happens, your thread library
	// should be contexted switched from thread context to this
	// schedule function

	// Invoke different actual scheduling algorithms
	// according to policy (STCF or MLFQ)

	// if (sched == STCF)
	//		sched_stcf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

	// sched_fifo();
    // sched_stcf();
    sched_mlfq();

    // schedule policy
    #ifndef MLFQ
	    // Choose STCF
    #else
	    // Choose MLFQ
    #endif

}

// Feel free to add any other functions you need

// YOUR CODE HERE

// MLFQ: schdule a new thread
void mlfq_scheduleNextJob(){
    // traverse 4 list to find a job

    node* ptr1 = head1->next;
    node* ptr2 = head2->next;
    node* ptr3 = head3->next;
    node* ptr4 = head4->next;

    if(ptr1 != NULL){
        currentRunning = ptr1;
        currentRLL = head1;
        return;
    }
    // now head1->next is NULL, which means no threads in head1
    if(ptr2 != NULL){
        currentRunning = ptr2;
        currentRLL = head2;
        return;
    }
    // now head2->next is NULL, which means no threads in head1 and head2
    if(ptr3 != NULL){
        currentRunning = ptr3;
        currentRLL = head3;
        return;
    }
    /* now head3->next is NULL, which means no threads in head1, head2 and head3
    which means that all threads are in head4
    */
    if(ptr4 != NULL){
        // we should run RoundRobin here
        currentRunning = ptr4;
        currentRLL = head4;
        return;
    }

    // no threads can be scheduled
    currentRunning = NULL;
    printf("djx fucked up in mlfq_schedule next job\n");
}

// MLFQ: insert currentRunning to the list it should be based on notsu
void mlfq_insertProperPosi(int notsu){

    // main thread's tid must be 1!
    node* ptr;
    // currentRunning should be insert at the end of head2
    if(notsu == 1){
        if(currentRunning->value->status != 0) {
            currentRLL->next = currentRunning->next;
        }
        ptr = head2;
        while(ptr->next != NULL)    ptr = ptr->next;
        ptr->next = currentRunning;
        currentRunning->next = NULL;
        return;
    }

    // currentRunning should be insert at the end of head3
    if(notsu == 2){
        // handle special case for main
        if(currentRunning->value->status != 0) {
            currentRLL->next = currentRunning->next;
        }
        ptr = head3;
        while(ptr->next != NULL)    ptr = ptr->next;
        ptr->next = currentRunning;
        currentRunning->next = NULL;
        return;
    }

    if(notsu > 2){
        // all other threads should be here now
        if(currentRunning->value->status != 0) {
            currentRLL->next = currentRunning->next;
        }
        ptr = head4;
        while(ptr->next != NULL)    ptr = ptr->next;
        ptr->next = currentRunning;
        currentRunning->next = NULL;
        return;
    }
}

// find the thread that has the input tid
node* findWaiter(rpthread_t target_tid){
    // this function should work for both scheduler
   node* blocked_ptr = blocked_thread_head->next;
   node* blockedPrev_ptr = blocked_thread_head;
   while(blocked_ptr != NULL){
       if(blocked_ptr->value->id == target_tid){
           // get this thread, delink it from the blocked_list
           blockedPrev_ptr->next = blocked_ptr->next;
           blocked_ptr->next = NULL;
           return blocked_ptr;
       }
       blockedPrev_ptr = blocked_ptr;
       blocked_ptr = blocked_ptr -> next;
   }
   return NULL;
}

// insert currentRunning Thread to the proper position in the queue
void stcf_insertProperPosi(int index){

    // if this node is the last node, no need to modify the position
    if(currentRunning->next == NULL)
    {
        return;
    }

    // if this node has the same numOfTimeRun as the one after it, no need to modify it
    if(index <= currentRunning->next->value->numberOfTimeSliceUsed)
    {
        return;
    }

    // modify it if its next run less time interval than it
    node* CR_prev = head1;

    // traverse until its before the currentRunning node
    while(CR_prev->next != currentRunning)
    {
        CR_prev = CR_prev->next;
    }

    node* insertF_temp = currentRunning->next;
    currentRunning->next = insertF_temp->next;
    insertF_temp->next = currentRunning;
    CR_prev->next = insertF_temp;
}

void ShowHead1(){
    node* ptr = head1->next;
    while(ptr!=NULL){
        tcb* tcb_ptr = ptr->value;
        printf("I am thread#%d ---->  ",tcb_ptr->id);
        ptr = ptr->next;
    }
}

void ShowHead2(){
    node* ptr = head2->next;
    while(ptr!=NULL){
        tcb* tcb_ptr = ptr->value;
        printf("I am thread#%d ---->  ",tcb_ptr->id);
        ptr = ptr->next;
    }
}

void ShowHead3(){
    node* ptr = head3->next;
    while(ptr!=NULL){
        tcb* tcb_ptr = ptr->value;
        printf("I am thread#%d ---->  ",tcb_ptr->id);
        ptr = ptr->next;
    }
}

void ShowHead4(){
    node* ptr = head4->next;
    while(ptr!=NULL){
        tcb* tcb_ptr = ptr->value;
        printf("I am thread#%d ---->  ",tcb_ptr->id);
        ptr = ptr->next;
    }
}

void ShowRetLL(){
    node* ptr = join_thread_head->next;
    while(ptr!=NULL){
        tcb* tcb_ptr = ptr->value;
        printf("I am exited thread#%d ---->  \n",tcb_ptr->id);
        ptr = ptr->next;
    }
}

void ShowBlockLL(){
    node* ptr = blocked_thread_head->next;
    while(ptr!=NULL){
        tcb* tcb_ptr = ptr->value;
        printf("I am blocked thread#%d ---->  \n",tcb_ptr->id);
        ptr = ptr->next;
    }
}

void setTime(){
    // 0 means we dont want to repeat timer
    timer.it_interval.tv_usec = 0;
	timer.it_interval.tv_sec = 0;

	timer.it_value.tv_usec = 5;
	timer.it_value.tv_sec = 0;

	setitimer(ITIMER_PROF, &timer, NULL);
}

void stopTimer(){
    timer.it_interval.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    timer.it_value.tv_sec = 0;
    setitimer(ITIMER_PROF, &timer, NULL); // timer is stopped here
}

void ring(){
    // swap context when timer ends
    swapcontext(&(currentRunning->value->context),&(gs_ctt));
}
