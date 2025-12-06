#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>

// Global variable definitions
task_t tasks[MAX_TASKS];
resource_t resources[MAX_RESOURCES];
int num_tasks = 0;
int num_resources = 0;
int current = -1;
volatile sig_atomic_t preempted = 0;
struct itimerval quantum_timer;
int quantum_ms = 50;
sched_algorithm_t sched_algo = SCHED_MULTILEVEL_FEEDBACK;
system_stats_t stats = {0};
history_entry_t history[HISTORY_SIZE];
int history_idx = 0;
unsigned long total_tickets = 0;
bool verbose_output = false;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

unsigned long get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000UL + tv.tv_usec;
}

void log_history(int task_id, state_t old_state, state_t new_state, int reason) {
    if (history_idx >= HISTORY_SIZE) return;
    history[history_idx].task_id = task_id;
    history[history_idx].old_state = old_state;
    history[history_idx].new_state = new_state;
    history[history_idx].timestamp_us = get_time_us();
    history[history_idx].reason = reason;
    history_idx++;
}

const char* state_to_string(state_t state) {
    switch(state) {
        case READY: return "READY";
        case RUNNING: return "RUN";
        case BLOCKED: return "BLOCK";
        case SLEEPING: return "SLEEP";
        case FINISHED: return "DONE";
        default: return "?";
    }
}

const char* type_to_string(task_type_t type) {
    switch(type) {
        case TASK_CPU_BOUND: return "CPU";
        case TASK_IO_BOUND: return "I/O";
        case TASK_MIXED: return "MIX";
        case TASK_INTERACTIVE: return "INT";
        default: return "?";
    }
}

// ============================================================================
// RESOURCE MANAGEMENT
// ============================================================================

void init_resources(int count) {
    num_resources = count;
    for (int i = 0; i < count; i++) {
        resources[i].id = i;
        resources[i].locked = false;
        resources[i].holder_id = -1;
        resources[i].wait_count = 0;
    }
}

bool try_acquire_resource(int task_id, int resource_id) {
    if (resource_id < 0 || resource_id >= num_resources) return false;
    
    resource_t *res = &resources[resource_id];
    if (!res->locked) {
        res->locked = true;
        res->holder_id = task_id;
        tasks[task_id].held_resources[tasks[task_id].num_held_resources++] = resource_id;
        return true;
    }
    
    res->wait_queue[res->wait_count++] = task_id;
    tasks[task_id].waiting_for_resource = resource_id;
    tasks[task_id].state = BLOCKED;
    tasks[task_id].num_blocks++;
    return false;
}

void release_resource(int task_id, int resource_id) {
    if (resource_id < 0 || resource_id >= num_resources) return;
    
    resource_t *res = &resources[resource_id];
    if (res->holder_id != task_id) return;
    
    res->locked = false;
    res->holder_id = -1;
    
    for (int i = 0; i < tasks[task_id].num_held_resources; i++) {
        if (tasks[task_id].held_resources[i] == resource_id) {
            tasks[task_id].held_resources[i] = 
                tasks[task_id].held_resources[--tasks[task_id].num_held_resources];
            break;
        }
    }
    
    if (res->wait_count > 0) {
        int waiter = res->wait_queue[0];
        for (int i = 1; i < res->wait_count; i++) {
            res->wait_queue[i-1] = res->wait_queue[i];
        }
        res->wait_count--;
        
        if (tasks[waiter].state == BLOCKED && tasks[waiter].waiting_for_resource == resource_id) {
            tasks[waiter].state = READY;
            tasks[waiter].waiting_for_resource = -1;
        }
    }
}

// ============================================================================
// SCHEDULING ALGORITHMS
// ============================================================================

int schedule_round_robin() {
    for (int i = 1; i <= num_tasks; ++i) {
        int idx = (current + i) % num_tasks;
        if (tasks[idx].state == READY) return idx;
    }
    return -1;
}

int schedule_priority() {
    int best = -1;
    int best_priority = -1;
    
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state == READY) {
            if (tasks[i].priority > best_priority) {
                best_priority = tasks[i].priority;
                best = i;
            }
        }
    }
    return best;
}

int schedule_multilevel_feedback() {
    for (int level = 0; level <= 2; level++) {
        for (int i = 0; i < num_tasks; i++) {
            if (tasks[i].state == READY && tasks[i].queue_level == level) {
                return i;
            }
        }
    }
    return -1;
}

int schedule_lottery() {
    if (total_tickets == 0) return schedule_round_robin();
    
    unsigned int winner = rand() % total_tickets;
    unsigned int sum = 0;
    
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state == READY) {
            sum += tasks[i].tickets;
            if (sum > winner) return i;
        }
    }
    return -1;
}

int schedule_cfs() {
    int best = -1;
    long min_vruntime = LONG_MAX;
    
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state == READY) {
            if (tasks[i].vruntime < min_vruntime) {
                min_vruntime = tasks[i].vruntime;
                best = i;
            }
        }
    }
    return best;
}

int choose_next_task() {
    switch(sched_algo) {
        case SCHED_ROUND_ROBIN: return schedule_round_robin();
        case SCHED_PRIORITY: return schedule_priority();
        case SCHED_MULTILEVEL_FEEDBACK: return schedule_multilevel_feedback();
        case SCHED_LOTTERY: return schedule_lottery();
        case SCHED_CFS: return schedule_cfs();
        default: return schedule_round_robin();
    }
}

// ============================================================================
// CONTEXT SWITCHING
// ============================================================================

void do_context_switch(int prev, int next) {
    unsigned long now = get_time_us();
    
    if (prev >= 0 && tasks[prev].state != FINISHED) {
        unsigned long run_time = now - tasks[prev].last_run_start_us;
        tasks[prev].total_cpu_time_us += run_time;
        tasks[prev].vruntime += run_time;
        
        tasks[prev].state = READY;
        stats.context_switches++;
    }
    
    current = next;
    tasks[current].state = RUNNING;
    tasks[current].last_run_start_us = now;
    
    if (tasks[current].start_time_us == 0) {
        tasks[current].start_time_us = now;
    }
    
    log_history(current, READY, RUNNING, 0);
}

void schedule_next() {
    unsigned long now = get_time_us();
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state == SLEEPING && now >= tasks[i].sleep_until_us) {
            tasks[i].state = READY;
            tasks[i].sleep_until_us = 0;
        }
    }
    
    int next = choose_next_task();
    if (next == -1) return;
    
    int prev = current;
    do_context_switch(prev, next);
    
    if (prev >= 0 && tasks[prev].state != FINISHED && tasks[prev].state != BLOCKED) {
        if (swapcontext(&tasks[prev].ctx, &tasks[current].ctx) == -1) {
            perror("swapcontext");
            exit(1);
        }
    } else {
        if (setcontext(&tasks[current].ctx) == -1) {
            perror("setcontext");
            exit(1);
        }
    }
}

// ============================================================================
// TIMER MANAGEMENT
// ============================================================================

void install_timer(int ms) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = scheduler_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    quantum_timer.it_value.tv_sec = ms / 1000;
    quantum_timer.it_value.tv_usec = (ms % 1000) * 1000;
    quantum_timer.it_interval = quantum_timer.it_value;
    
    if (setitimer(ITIMER_REAL, &quantum_timer, NULL) == -1) {
        perror("setitimer");
        exit(1);
    }
}

void disable_timer() {
    struct itimerval zero = {0};
    setitimer(ITIMER_REAL, &zero, NULL);
}

void scheduler_handler(int signum) {
    (void)signum;
    if (current < 0) return;
    
    preempted = 1;
    tasks[current].num_preemptions++;
    stats.preemptions++;
    
    if (sched_algo == SCHED_MULTILEVEL_FEEDBACK) {
        if (tasks[current].queue_level < 2) {
            tasks[current].queue_level++;
        }
    }
    
    log_history(current, RUNNING, READY, 0);
    
    int next = choose_next_task();
    if (next == -1 || next == current) {
        preempted = 0;
        return;
    }
    
    int prev = current;
    do_context_switch(prev, next);
    preempted = 0;
    
    if (swapcontext(&tasks[prev].ctx, &tasks[current].ctx) == -1) {
        perror("swapcontext in handler");
        exit(1);
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

void print_statistics() {
    printf("\n");
    printf("================================================================================\n");
    printf("                        SIMULATION STATISTICS\n");
    printf("================================================================================\n");
    printf("Scheduling Algorithm:    %s\n", 
           sched_algo == SCHED_ROUND_ROBIN ? "Round Robin" :
           sched_algo == SCHED_PRIORITY ? "Priority" :
           sched_algo == SCHED_MULTILEVEL_FEEDBACK ? "Multilevel Feedback Queue" :
           sched_algo == SCHED_LOTTERY ? "Lottery" : "CFS");
    printf("Quantum:                 %d ms\n", quantum_ms);
    printf("Total Tasks:             %d\n", num_tasks);
    printf("Completed Tasks:         %lu\n", stats.completed_tasks);
    printf("Context Switches:        %lu\n", stats.context_switches);
    printf("Preemptions:             %lu\n", stats.preemptions);
    printf("Voluntary Yields:        %lu\n", stats.voluntary_yields);
    
    if (stats.completed_tasks > 0) {
        printf("Avg Turnaround Time:     %.2f ms\n", 
               stats.total_turnaround_time_ms / stats.completed_tasks);
        printf("Avg CPU Time:            %.2f ms\n",
               stats.total_run_time_ms / stats.completed_tasks);
    }
    
    printf("\n");
    printf("Per-Task Statistics:\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("ID  Type  Pri  State CPU(ms) Turn(ms) Preempt Yield Block Queue/VRT\n");
    printf("--------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < num_tasks; i++) {
        task_t *t = &tasks[i];
        double turnaround = (t->finish_time_us - t->creation_time_us) / 1000.0;
        printf("%2d  %-4s  %2d   %-5s %7.2f %8.2f %7lu %5lu %5lu ",
               t->id,
               type_to_string(t->type),
               t->priority,
               state_to_string(t->state),
               t->total_cpu_time_us / 1000.0,
               turnaround,
               t->num_preemptions,
               t->num_yields,
               t->num_blocks);
        
        if (sched_algo == SCHED_MULTILEVEL_FEEDBACK) {
            printf("Q%d\n", t->queue_level);
        } else if (sched_algo == SCHED_CFS) {
            printf("%ld\n", t->vruntime);
        } else {
            printf("-\n");
        }
    }
    printf("================================================================================\n");
}

void cleanup() {
    for (int i = 0; i < num_tasks; ++i) {
        if (tasks[i].stack) {
            free(tasks[i].stack);
            tasks[i].stack = NULL;
        }
    }
}