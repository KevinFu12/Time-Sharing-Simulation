#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ============================================================================
// TASK OPERATIONS
// ============================================================================

void task_yield() {
    if (current < 0) return;
    tasks[current].num_yields++;
    stats.voluntary_yields++;
    
    if (sched_algo == SCHED_MULTILEVEL_FEEDBACK) {
        if (tasks[current].queue_level > 0) {
            tasks[current].queue_level--;
        }
    }
    
    log_history(current, RUNNING, READY, 1);
    schedule_next();
}

void task_sleep(unsigned long us) {
    if (current < 0) return;
    unsigned long now = get_time_us();
    tasks[current].sleep_until_us = now + us;
    tasks[current].state = SLEEPING;
    
    log_history(current, RUNNING, SLEEPING, 2);
    schedule_next();
}

void simulate_io_operation() {
    if (current < 0) return;
    
    if (verbose_output) {
        printf("  [T%02d] Starting I/O operation\n", current);
    }
    task_sleep(5000 + (rand() % 10000));
}

void task_exit() {
    if (current < 0) return;
    
    unsigned long now = get_time_us();
    tasks[current].state = FINISHED;
    tasks[current].finish_time_us = now;
    
    unsigned long turnaround = now - tasks[current].creation_time_us;
    stats.total_turnaround_time_ms += turnaround / 1000.0;
    stats.total_run_time_ms += tasks[current].total_cpu_time_us / 1000.0;
    stats.completed_tasks++;
    
    while (tasks[current].num_held_resources > 0) {
        release_resource(current, tasks[current].held_resources[0]);
    }
    
    if (sched_algo == SCHED_LOTTERY) {
        total_tickets -= tasks[current].tickets;
    }
    
    log_history(current, RUNNING, FINISHED, 3);
    
    printf("[T%02d] FINISHED | CPU: %.2fms | Turnaround: %.2fms | Preemptions: %lu\n",
           current,
           tasks[current].total_cpu_time_us / 1000.0,
           turnaround / 1000.0,
           tasks[current].num_preemptions);
    fflush(stdout);
    
    int all_done = 1;
    for (int i = 0; i < num_tasks; ++i) {
        if (tasks[i].state != FINISHED) { all_done = 0; break; }
    }
    
    if (all_done) {
        disable_timer();
        return;
    }
    
    schedule_next();
}

// ============================================================================
// TASK WORKLOAD FUNCTIONS
// ============================================================================

void cpu_bound_work() {
    int id = current;
    for (int iter = 0; iter < tasks[id].workload; ++iter) {
        tasks[id].work_counter++;
        
        double result = 0;
        for (int k = 0; k < 100000; ++k) {
            result += sqrt((double)k);
        }
        
        if ((iter % (tasks[id].workload/5 + 1)) == 0 && verbose_output) {
            printf("  [T%02d|CPU] Progress: %d/%d (%.1f%%)\n", 
                   id, iter, tasks[id].workload, 100.0*iter/tasks[id].workload);
        }
    }
    task_exit();
}

void io_bound_work() {
    int id = current;
    tasks[id].remaining_io = tasks[id].io_operations;
    
    while (tasks[id].remaining_io > 0) {
        tasks[id].work_counter++;
        
        for (volatile int k = 0; k < 50000; ++k) {
            __asm__ __volatile__("");
        }
        
        simulate_io_operation();
        tasks[id].remaining_io--;
        
        if (verbose_output && tasks[id].remaining_io % 3 == 0) {
            printf("  [T%02d|I/O] Remaining I/O ops: %d\n", id, tasks[id].remaining_io);
        }
    }
    task_exit();
}

void mixed_work() {
    int id = current;
    for (int iter = 0; iter < tasks[id].workload; ++iter) {
        tasks[id].work_counter++;
        
        double result = 0;
        for (int k = 0; k < 80000; ++k) {
            result += sqrt((double)k);
        }
        
        if (iter % 5 == 0 && tasks[id].remaining_io > 0) {
            simulate_io_operation();
            tasks[id].remaining_io--;
        }
        
        if (iter % 7 == 0 && num_resources > 0) {
            int res_id = rand() % num_resources;
            if (try_acquire_resource(id, res_id)) {
                if (verbose_output) printf("  [T%02d] Acquired resource %d\n", id, res_id);
                for (volatile int k = 0; k < 10000; ++k) {}
                release_resource(id, res_id);
            } else {
                if (verbose_output) printf("  [T%02d] Blocked on resource %d\n", id, res_id);
                schedule_next();
                if (verbose_output) printf("  [T%02d] Resumed after resource wait\n", id);
            }
        }
        
        if (iter % 10 == 0) {
            task_yield();
        }
    }
    task_exit();
}

void interactive_work() {
    int id = current;
    for (int iter = 0; iter < tasks[id].workload; ++iter) {
        tasks[id].work_counter++;
        
        for (volatile int k = 0; k < 30000; ++k) {
            __asm__ __volatile__("");
        }
        
        if (iter % 2 == 0) {
            task_yield();
        }
        
        if (iter % 5 == 0) {
            task_sleep(8000);
        }
    }
    task_exit();
}

// ============================================================================
// TASK CREATION
// ============================================================================

void create_task(task_type_t type, int priority, int workload) {
    if (num_tasks >= MAX_TASKS) {
        fprintf(stderr, "Max tasks exceeded\n");
        exit(1);
    }
    
    task_t *t = &tasks[num_tasks];
    memset(t, 0, sizeof(*t));
    t->id = num_tasks;
    t->state = READY;
    t->type = type;
    t->priority = priority;
    t->original_priority = priority;
    t->queue_level = 0;
    t->tickets = 10 + priority * 5;
    t->vruntime = 0;
    t->workload = workload;
    t->io_operations = (type == TASK_IO_BOUND) ? 15 : 5;
    t->remaining_io = t->io_operations;
    t->creation_time_us = get_time_us();
    t->waiting_for_resource = -1;
    
    total_tickets += t->tickets;
    
    t->stack = malloc(STACK_SIZE);
    if (!t->stack) { perror("malloc stack"); exit(1); }
    
    if (getcontext(&t->ctx) == -1) { perror("getcontext"); exit(1); }
    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = STACK_SIZE;
    t->ctx.uc_link = NULL;
    
    void (*func)() = NULL;
    switch(type) {
        case TASK_CPU_BOUND: func = cpu_bound_work; break;
        case TASK_IO_BOUND: func = io_bound_work; break;
        case TASK_MIXED: func = mixed_work; break;
        case TASK_INTERACTIVE: func = interactive_work; break;
    }
    
    makecontext(&t->ctx, func, 0);
    num_tasks++;
}