/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: cooperative multithreading and synchronization
 */

#include <sys/queue.h>
#include "print.c"
#include "thread.h"

/* Student's code goes here (Cooperative Threads). */
/* Define the TCB and helper functions (if needed) for multi-threading. */

/* Student's code ends here. */

/**
 * thread_init(): Initialize the data structures for multi-threading, which includes
 * allocating a TCB entry for the main thread.
 */

int next_yielding_thread(){
    for(int i =1; i <next_thread_id; i++){
        int nytid = (current_thread_id + i) % next_thread_id;
        if(TCB[nytid].status == THREAD_READY){
            return nytid;
        }
    }
    return -1;
}
void clean_up_zombies(){
    for(int i = 1; i < next_thread_id; i++){
        if(TCB[i].status == THREAD_ZOMBIE && TCB[i].stack_bottom != NULL){
            free(TCB[i].stack_bottom);
            TCB[i].stack_bottom = NULL; // need to do this, so no double frees
        }
    }
}

void thread_init() {
    /* Student's code goes here (Cooperative Threads). */    
    // main was not created by threading library, set to NULL? not sure if this is correct lol
    TCB[0].id = 0;
    TCB[0].sp = NULL;
    TCB[0].status = THREAD_RUNNING;
    TCB[0].function_pointer = NULL;
    TCB[0].args = NULL;
    TCB[0].stack_bottom = NULL;
}

/**
 * ctx_entry(): Executing in the context of a newly created thread. The thread will
 * set up its own state (e.g., point its own TCB entry as currently running), and then
 * run its entry function held in its TCB entry. Once the thread finishes its work, it
 * will return to this function and exit by calling thread_exit().
 */
void ctx_entry() {
    TCB[current_thread_id].status = THREAD_RUNNING;
    TCB[current_thread_id].function_pointer(TCB[current_thread_id].args);
    thread_exit();
}

/**
 * thread_create(void (*entry)(void *arg), void *arg): Create a new thread with a
 * stack of STACK_SIZE (defined above) bytes, and schedule it to run.
 *
 * Let T denote the thread that called thread_create().
 * Let T' denote the newly created thread.
 *
 * In thread_create(), T will allocate a TCB entry for T' along with the stack of T',
 * and then T will schedule T' to run using ctx_start().
 *
 * When thread T' context switches back to T, T will return back to thread_create(),
 * and possibly clean up T' if T'' has terminated.
void  ctx_start(void **sp_old, void *sp_new);
void ctx_switch(void **sp_old, void *sp_new);

 */
void thread_create(void (*entry)(void *arg), void *arg) {
    int ntid = next_thread_id++;
    TCB[ntid].id = ntid;
    
    char* stack = malloc(STACK_SIZE);
    void* stack_top = stack + STACK_SIZE;

    TCB[ntid].sp= stack_top; // call malloc for thread stack
    TCB[ntid].stack_bottom = stack;
    TCB[ntid].status = THREAD_READY;
    TCB[ntid].function_pointer = entry;
    TCB[ntid].args= arg;
    int prv_thread_id = current_thread_id;

    if (TCB[prv_thread_id].status == THREAD_RUNNING) {
        // issue was before, we need to set current thread to ready, since we start the context after creating it
        TCB[prv_thread_id].status = THREAD_READY;
    }

    current_thread_id = ntid;
    ctx_start(&TCB[prv_thread_id].sp, TCB[current_thread_id].sp); // we schedule it to run, so it should be YIELDING
    current_thread_id = prv_thread_id;
    //possibly clean up T' if T' terminated
    if(TCB[current_thread_id].status != THREAD_ZOMBIE) TCB[current_thread_id].status = THREAD_RUNNING;
    clean_up_zombies();
}
/**
 * thread_yield(): Switch to another thread using ctx_switch(). If no other thread
 * can be switched to, continue to run the current thread (if it has not exited or
 * been waiting on a semaphore).
 *
 * Once some thread T' context switches back to T, T will return to thread_yield()
 * and cleanup T' if T' has terminated.
 */


void thread_yield() {
    int nytid = next_yielding_thread();
    if(nytid != -1){
        const int prev_thread_id = current_thread_id;
        if (TCB[prev_thread_id].status == THREAD_RUNNING){
            TCB[prev_thread_id].status = THREAD_READY;
        }
        current_thread_id = nytid;
        TCB[nytid].status = THREAD_RUNNING;
        ctx_switch(&TCB[prev_thread_id].sp, TCB[nytid].sp); // if this function call returns, we do cleanup
        // clean up all zombies, since the 
        current_thread_id = prev_thread_id;
        if(TCB[current_thread_id].status != THREAD_ZOMBIE) TCB[current_thread_id].status = THREAD_RUNNING; //dont revive a zombie
        clean_up_zombies();
    }

}

/**
 * thread_exit(): The current thread will set its status ZOMBIE (cannot be scheduled),
 * and yield to another thread. If all the other threads have exited by thread_exit(),
 * call the _end() in thread.s which just infinitely loops.
 */
void thread_exit() {
    TCB[current_thread_id].status = THREAD_ZOMBIE;
    while (1) { //if control is ever given back to this thread, try to yield first
        int nytid = next_yielding_thread();
        if (nytid == -1) {
            _end();
        }
        thread_yield();
    }
}

/* Student's code goes here (Cooperative Threads). */
/* Define helper functions (if needed) for conditional variables. */

/* Student's code ends here. */

/**
 * cv_init(struct cv *condition): Initialize the fields in struct cv.
 */
// when do we free bruh
void cv_init(struct cv *condition){
    condition->head = NULL;
    condition->tail = NULL;
}

void insert_thread(struct cv * condition){
    //just always append to tail, keep dummy node
    LLNode* tail = malloc(sizeof(LLNode));
    tail->thread = &TCB[current_thread_id];
    tail->next = NULL;
    if(condition->head == NULL){
        condition->head = tail;
        condition->tail = tail;
        return;
    }
    condition->tail->next = tail;
    condition->tail = tail;
}
Thread* remove_thread(struct cv * condition){
    LLNode* to_remove = condition->head;
    if(to_remove != NULL){
        condition->head = to_remove->next;
        if(condition->tail == to_remove){
            condition->tail = NULL;
        }
        Thread* thread_to_return = to_remove->thread;
        free(to_remove);
        return thread_to_return;
    }
    return NULL;
}

/**
 * cv_wait(struct cv *condition): Remove the current thread from the TCB, and add it
 * to the conditional variable. Try to yield to another thread in the TCB.
 */
void cv_wait(struct cv *condition){
    int ctid = current_thread_id;
    if(TCB[ctid].status != THREAD_ZOMBIE) TCB[ctid].status = THREAD_BLOCKED;
    insert_thread(condition);
    while (TCB[ctid].status == THREAD_BLOCKED) {
        thread_yield();
    }
}

/**
 * cv_signal(struct cv *condition): Remove a thread (if exists) from the conditional
 * variable, and add it back to the TCB so that it can be scheduled later. However,
 * cv_signal should not switch the CPU context to another thread (i.e., the current
 * thread should continue to run).
 */
void cv_signal(struct cv *condition){
    Thread* thread_to_signal = remove_thread(condition);
    if(thread_to_signal != NULL){
        if(TCB[thread_to_signal->id].status != THREAD_ZOMBIE){
            TCB[thread_to_signal->id].status = THREAD_READY;
        }
    }
}
/*
cv should maintain a queue of threads that are waiting on the condition variable. When cv_wait is called, the current thread should be added to this queue and marked as blocked. When cv_signal is called, one thread from the queue should be removed and marked as ready, so that it can be scheduled to run later.
    - remove from TCB when waiting, add to CV queue
    - add back to TCB when signaled, remove from CV queue
cv_broadcast does this for all condition variables

*/

#define BUF_SIZE 3
void* buffer[BUF_SIZE];
int count = 0;
int head = 0, tail = 0;
struct cv nonempty, nonfull;

void produce(void* arg) {
    while (1) {
        while (count == BUF_SIZE) cv_wait(&nonfull);
        /* At this point, the buffer is not full. */

        /* Student's code goes here (Cooperative Threads). */
        /* Print out the producer ID with the arg pointer. */
        // cast void* to int* in memory, then dereference to get the int value
        printf("Producer %d produced item %d\n\r", current_thread_id, *((int *)(arg)));
        /* Student's code ends here. */
        buffer[tail] = arg;
        tail = (tail + 1) % BUF_SIZE;
        count += 1;
        cv_signal(&nonempty);
    }
}

void consume(void *arg) {
    while (1) {
        while (count == 0) cv_wait(&nonempty);
        /* At this point, the buffer is not empty. */

        /* Student's code goes here (Cooperative Threads). */
        /* Print out the consumer ID with the arg pointer. */
        printf("Consumer %d consumed item %d\n\r", current_thread_id, *((int *)(arg)));
        /* Student's code ends here. */
        void* result = buffer[head];
        head = (head + 1) % BUF_SIZE;
        count -= 1;
        cv_signal(&nonfull);
    }
}

void child(void* arg) {
    for (int i = 0; i < 10; i++) {
        printf("%s is in for loop i=%d\n\r", arg, i);
        thread_yield();
    }
}

// int main() {
//     thread_init(); //main thread not initialized properly?? so cant return?? hmm
//     thread_create(child, "Child thread");
//     for (int i = 0; i < 10; i++) {
//         printf("Main thread is in for loop i=%d\n\r", i);
//         thread_yield();
//     }
//     printf("main done\n");
//     thread_exit();
// }
int main() {
    thread_init();
    cv_init(&nonempty);
    cv_init(&nonfull);
    int ID[100];
    for (int i = 0; i < 100; i++) ID[i] = i;

    for (int i = 0; i < 100; i++)
        thread_create(consume, ID + i);

    for (int i = 0; i < 100; i++)
        thread_create(produce, ID + i);

    printf("main thread exits\n\r");
    thread_exit();

    /* The control flow should NEVER get here. If the main thread is the last to
     * call thread_exit(), thread_exit() should terminate the program by calling
     * the _end() in thread.s.
     * If the main thread is not the last, thread_exit() will switch the context
     * to another thread. Later, when all the threads have called thread_exit(),
     * the last one calling it should then call _end() within thread_exit(). */
}
/*
When we context switch:
- stack pointer goes to stack of new thread
- program counter goes to corresponding PC of thread
- we also need to SAVE_ALL_REGISTERS in stack memory, of the parent thread
    - so, when we switch back, we can restore execution
    - also need to remember the current stack pointer of current thread, AS THE FIRST entry (doesnt have to be first, but it has to be known)

Thread Control Block:
- data structure that contains metadata of every thread:
    - ThreadID
    - status: thread's current execution status; running, ready, blocked
    - stack pointer: saved value of SP, before switching to a diff thread
    - entry function pointer: the "main" function for each thread
    - entry function arguments
    etc.


char* child_stack = malloc(STACK_SIZE);
ctx_start(&TCB[current_idx].sp, child_stack + STACK_SIZE);
    - since stack grows upwards; will return highest address, that was malloced
    - malloc returns lowest address, hence why we need to add STACK_SIZE to get the end

Thread_Yield:
- yield the CPU to another thread

Implement the following:
void ctx_entry();
void thread_init();
void thread_create(void (*entry)(void *arg), void *arg);
void thread_yield();
void thread_exit();

*/