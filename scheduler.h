#ifndef SCHEDULER_H
#define SCHEDULER_H

#define _XOPEN_SOURCE 700
#include <ucontext.h>
#include <signal.h>
#include <stdbool.h>

#define STACK_SIZE (1024 * 128)
#define MAX_TASKS 64
#define MAX_RESOURCES 10
#define HISTORY_SIZE 1000

// Scheduling algorithms
typedef enum {
    SCHED_ROUND_ROBIN,
    SCHED_PRIORITY,
    SCHED_MULTILEVEL_FEEDBACK,
    SCHED_LOTTERY,
    SCHED_CFS
} sched_algorithm_t;

typedef enum { 
    READY, 
    RUNNING, 
    BLOCKED,
    SLEEPING,
    FINISHED 
} state_t;

typedef enum {
    TASK_CPU_BOUND,
    TASK_IO_BOUND,
    TASK_MIXED,
    TASK_INTERACTIVE
} task_type_t;

// Resource management
typedef struct {
    int id;
    bool locked;
    int holder_id;
    int wait_queue[MAX_TASKS];
    int wait_count;
} resource_t;

// Statistics tracking
typedef struct {
    unsigned long context_switches;
    unsigned long preemptions;
    unsigned long voluntary_yields;
    double total_wait_time_ms;
    double total_run_time_ms;
    double total_turnaround_time_ms;
    unsigned long completed_tasks;
} system_stats_t;

typedef struct {
    int task_id;
    state_t old_state;
    state_t new_state;
    unsigned long timestamp_us;
    int reason;
} history_entry_t;

typedef struct {
    ucontext_t ctx;
    void *stack;
    int id;
    state_t state;
    task_type_t type;
    
    // Scheduling info
    int priority;
    int original_priority;
    int queue_level;
    unsigned int tickets;
    long vruntime;
    
    // Timing info
    unsigned long creation_time_us;
    unsigned long start_time_us;
    unsigned long finish_time_us;
    unsigned long total_cpu_time_us;
    unsigned long last_run_start_us;
    unsigned long wait_time_us;
    unsigned long sleep_until_us;
    
    // Work tracking
    unsigned long work_counter;
    int workload;
    int io_operations;
    int remaining_io;
    
    // Resource management
    int held_resources[MAX_RESOURCES];
    int num_held_resources;
    int waiting_for_resource;
    
    // Statistics
    unsigned long num_preemptions;
    unsigned long num_yields;
    unsigned long num_blocks;
} task_t;

// Global variables (extern declarations)
extern task_t tasks[MAX_TASKS];
extern resource_t resources[MAX_RESOURCES];
extern int num_tasks;
extern int num_resources;
extern int current;
extern volatile sig_atomic_t preempted;
extern struct itimerval quantum_timer;
extern int quantum_ms;
extern sched_algorithm_t sched_algo;
extern system_stats_t stats;
extern history_entry_t history[HISTORY_SIZE];
extern int history_idx;
extern unsigned long total_tickets;
extern bool verbose_output;

// Scheduler functions
void install_timer(int ms);
void disable_timer();
void scheduler_handler(int signum);
void schedule_next();
void do_context_switch(int prev, int next);
int choose_next_task();
int schedule_round_robin();
int schedule_priority();
int schedule_multilevel_feedback();
int schedule_lottery();
int schedule_cfs();

// Resource management
void init_resources(int count);
bool try_acquire_resource(int task_id, int resource_id);
void release_resource(int task_id, int resource_id);

// Task functions
void create_task(task_type_t type, int priority, int workload);
void task_yield();
void task_sleep(unsigned long us);
void task_exit();
void simulate_io_operation();

// Task workload functions
void cpu_bound_work();
void io_bound_work();
void mixed_work();
void interactive_work();

// Utility functions
unsigned long get_time_us();
void log_history(int task_id, state_t old_state, state_t new_state, int reason);
const char* state_to_string(state_t state);
const char* type_to_string(task_type_t type);
void print_statistics();
void cleanup();

#endif // SCHEDULER_H