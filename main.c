#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <num_tasks> <quantum_ms> <algorithm> <verbose>\n", argv[0]);
        fprintf(stderr, "Algorithms: 0=RR, 1=Priority, 2=MLFQ, 3=Lottery, 4=CFS\n");
        return 1;
    }
    
    int n = atoi(argv[1]);
    quantum_ms = atoi(argv[2]);
    sched_algo = atoi(argv[3]);
    verbose_output = (argc > 4) ? atoi(argv[4]) : 0;
    
    if (n < 1 || n > MAX_TASKS) { 
        fprintf(stderr, "num_tasks: 1..%d\n", MAX_TASKS); 
        return 1; 
    }
    if (quantum_ms < 10) { 
        fprintf(stderr, "quantum_ms >= 10\n"); 
        return 1; 
    }
    if (sched_algo < 0 || sched_algo > 4) {
        fprintf(stderr, "algorithm: 0-4\n");
        return 1;
    }
    
    srand(time(NULL));
    
    printf("================================================================================\n");
    printf("         Advanced Time-Sharing Operating System Simulator\n");
    printf("================================================================================\n");
    printf("Configuration:\n");
    printf("  Tasks: %d | Quantum: %dms | Algorithm: %s | Verbose: %s\n",
           n, quantum_ms,
           sched_algo == SCHED_ROUND_ROBIN ? "Round Robin" :
           sched_algo == SCHED_PRIORITY ? "Priority" :
           sched_algo == SCHED_MULTILEVEL_FEEDBACK ? "MLFQ" :
           sched_algo == SCHED_LOTTERY ? "Lottery" : "CFS",
           verbose_output ? "Yes" : "No");
    printf("================================================================================\n\n");
    
    // Initialize resources
    init_resources(3);
    
    // Create diverse task mix
    for (int i = 0; i < n; ++i) {
        task_type_t type;
        int priority = 1 + (rand() % 9);
        int workload;
        
        if (i % 4 == 0) {
            type = TASK_CPU_BOUND;
            workload = 30 + (rand() % 20);
        } else if (i % 4 == 1) {
            type = TASK_IO_BOUND;
            workload = 20 + (rand() % 15);
        } else if (i % 4 == 2) {
            type = TASK_MIXED;
            workload = 25 + (rand() % 20);
        } else {
            type = TASK_INTERACTIVE;
            workload = 40 + (rand() % 30);
            priority += 2;
        }
        
        create_task(type, priority, workload);
        printf("Created Task %2d: Type=%-4s Priority=%d Workload=%d\n",
               i, type_to_string(type), priority, workload);
    }
    
    printf("\nStarting simulation...\n\n");
    
    // Install timer and start
    install_timer(quantum_ms);
    
    current = 0;
    tasks[current].state = RUNNING;
    tasks[current].last_run_start_us = get_time_us();
    
    if (setcontext(&tasks[0].ctx) == -1) {
        perror("setcontext start");
        exit(1);
    }
    
    printf("\nAll tasks completed!\n");
    disable_timer();
    print_statistics();
    cleanup();
    
    return 0;
}