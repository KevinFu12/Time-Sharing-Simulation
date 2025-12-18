# Time-Sharing Operating System Simulator

A comprehensive, feature-rich simulation of time-sharing operating system concepts implemented in C using POSIX user contexts (`ucontext`). This project demonstrates various CPU scheduling algorithms, resource management, and process state management.

## Table of Contents

- [Features]
- [Project Structure]
- [Installation]
- [Usage]
- [Scheduling Algorithms]
- [Task Types]
- [System Architecture]
- [Statistics and Metrics]
- [Examples]
- [Technical Details]
- [Contributing]

## Features

### Core Capabilities
- **5 Scheduling Algorithms**: Round Robin, Priority, Multilevel Feedback Queue (MLFQ), Lottery, and Completely Fair Scheduler (CFS)
- **Multiple Task Types**: CPU-bound, I/O-bound, Mixed, and Interactive workloads
- **Resource Management**: Mutex-style locks with wait queues and deadlock potential
- **Preemptive Multitasking**: Timer-based context switching with configurable quantum
- **Comprehensive Statistics**: Per-task and system-wide performance metrics

### Advanced Features
- Context switching with POSIX `ucontext` API
- Priority-based scheduling with dynamic priority adjustment
- Virtual runtime tracking (CFS implementation)
- Multilevel feedback queue with promotion/demotion
- Lottery scheduling with ticket-based fairness
- Resource contention simulation
- Voluntary yielding and sleeping
- I/O operation simulation with blocking

## Project Structure

```
timeshare/
├── scheduler.h      # Header file with all declarations
├── scheduler.c      # Scheduler implementation and algorithms
├── task.c          # Task management and workload functions
├── main.c          # Main program entry point
├── Makefile        # Build configuration
└── README.md       # This file
```

### File Descriptions

- **scheduler.h**: Contains all type definitions, structs, enums, and function prototypes
- **scheduler.c**: Implements scheduling algorithms, timer management, context switching, and resource management
- **task.c**: Handles task creation, workload execution, and task lifecycle operations
- **main.c**: Parses arguments, initializes the system, and starts the simulation

## Installation

### Prerequisites
- GCC compiler (with C11 support)
- Linux/Unix system (uses POSIX APIs)
- Make utility

### Build Instructions

```bash
# Clone or download the project
cd timeshare

# Build the project
make

# Clean build artifacts
make clean

# Rebuild from scratch
make clean && make
```

## Usage

### Command Syntax
```bash
./timeshare <num_tasks> <quantum_ms> <algorithm> <verbose>
```

### Parameters

| Parameter | Description | Valid Range |
|-----------|-------------|-------------|
| `num_tasks` | Number of tasks to create | 1-64 |
| `quantum_ms` | Time quantum in milliseconds | ≥10 |
| `algorithm` | Scheduling algorithm to use | 0-4 (see below) |
| `verbose` | Enable verbose output | 0 or 1 |

### Algorithm Codes
- `0` - Round Robin (RR)
- `1` - Priority Scheduling
- `2` - Multilevel Feedback Queue (MLFQ)
- `3` - Lottery Scheduling
- `4` - Completely Fair Scheduler (CFS)

### Basic Example
```bash
./timeshare 8 50 2 0
```
This runs 8 tasks with 50ms quantum using MLFQ without verbose output.

## Scheduling Algorithms

### 1. Round Robin (RR)
- **Description**: Classic time-sharing algorithm
- **Behavior**: Each task gets equal time quantum in circular order
- **Best For**: Fair distribution of CPU time
- **Quantum Impact**: High

### 2. Priority Scheduling
- **Description**: Tasks scheduled by priority level (0-9)
- **Behavior**: Higher priority tasks always run first
- **Best For**: Systems with critical/background task separation
- **Risk**: Starvation of low-priority tasks

### 3. Multilevel Feedback Queue (MLFQ)
- **Description**: Adaptive scheduler with 3 priority queues
- **Behavior**: 
  - Tasks start in highest queue
  - Demoted on preemption (CPU-intensive)
  - Promoted on voluntary yield (I/O-intensive)
- **Best For**: Mixed workloads (interactive + batch)
- **Advantage**: Automatically identifies and prioritizes I/O-bound tasks

### 4. Lottery Scheduling
- **Description**: Probabilistic fair-share scheduling
- **Behavior**: Tasks receive tickets; winner selected randomly
- **Tickets**: Base 10 + (priority × 5)
- **Best For**: Proportional CPU sharing
- **Advantage**: Stochastic fairness

### 5. Completely Fair Scheduler (CFS)
- **Description**: Linux-inspired virtual runtime scheduler
- **Behavior**: Tracks virtual runtime; schedules task with smallest vruntime
- **Best For**: General-purpose fair scheduling
- **Advantage**: Excellent fairness with low overhead

## 🎯 Task Types

The simulator creates a diverse mix of tasks with different behaviors:

### CPU-Bound Tasks
- Heavy computation (square root calculations)
- Minimal I/O operations
- Long time slices before yielding

### I/O-Bound Tasks
- Frequent I/O operations (simulated)
- Short CPU bursts between I/O
- Voluntary blocking and sleeping

### Mixed Tasks
- Balanced CPU and I/O work
- Resource acquisition/release
- Periodic yielding

### Interactive Tasks
- Short CPU bursts
- Frequent voluntary yields
- Simulated user interaction delays
- Higher initial priority

## System Architecture

### State Machine
Tasks transition through multiple states:

```
READY → RUNNING → [BLOCKED/SLEEPING/FINISHED]
   ↑        ↓
   └────────┘
```

- **READY**: Waiting in run queue
- **RUNNING**: Currently executing
- **BLOCKED**: Waiting for resource
- **SLEEPING**: Voluntary sleep
- **FINISHED**: Completed execution

### Context Switching
1. Timer interrupt (SIGALRM) triggers scheduler
2. Save current task context
3. Select next task via scheduling algorithm
4. Update statistics (CPU time, vruntime, etc.)
5. Restore next task context

### Resource Management
- 3 system resources (configurable)
- Mutex-style locking
- FIFO wait queues
- Automatic release on task exit
- Deadlock potential (for demonstration)

## Statistics and Metrics

The simulator tracks comprehensive statistics:

### System-Wide Metrics
- Total context switches
- Number of preemptions
- Voluntary yields
- Average turnaround time
- Average CPU time
- Completed tasks

### Per-Task Metrics
- Task ID and type
- Priority level
- Final state
- Total CPU time (ms)
- Turnaround time (ms)
- Number of preemptions
- Number of yields
- Number of blocks
- Queue level (MLFQ) or Virtual runtime (CFS)

### Sample Output
```
================================================================================
                        SIMULATION STATISTICS
================================================================================
Scheduling Algorithm:    Multilevel Feedback Queue
Quantum:                 50 ms
Total Tasks:             8
Completed Tasks:         8
Context Switches:        156
Preemptions:             142
Voluntary Yields:        89
Avg Turnaround Time:     2847.32 ms
Avg CPU Time:            1523.45 ms

Per-Task Statistics:
--------------------------------------------------------------------------------
ID  Type  Pri  State CPU(ms) Turn(ms) Preempt Yield Block Queue/VRT
--------------------------------------------------------------------------------
 0  CPU    3   DONE   1847.23  2956.78     45     12     0  Q2
 1  I/O    7   DONE    456.12  2134.56     15     28     8  Q0
 2  MIX    5   DONE   1234.56  2789.34     38     24     3  Q1
...
```

## Examples

### Example 1: Compare Algorithms
```bash
# Round Robin - fair but may not optimize for I/O
./timeshare 12 50 0 0

# MLFQ - adapts to workload patterns
./timeshare 12 50 2 0

# CFS - Linux-style fairness
./timeshare 12 50 4 0
```

### Example 2: Quantum Size Impact
```bash
# Small quantum (high overhead, better interactivity)
./timeshare 10 10 0 0

# Large quantum (lower overhead, worse interactivity)
./timeshare 10 200 0 0
```

### Example 3: Verbose Debugging
```bash
# Watch detailed task execution
./timeshare 6 30 2 1
```

### Example 4: Stress Test
```bash
# Maximum tasks
./timeshare 64 25 2 0
```

### Example 5: Priority Scheduling
```bash
# See how priority affects execution
./timeshare 15 50 1 0
```

## Technical Details

### Key Technologies
- **POSIX ucontext API**: User-level context switching
- **POSIX Signals**: Timer interrupts (SIGALRM)
- **setitimer()**: Periodic timer for quantum enforcement
- **makecontext()/swapcontext()**: Context creation and switching

### Design Patterns
- **State Pattern**: Task state management
- **Strategy Pattern**: Pluggable scheduling algorithms
- **Observer Pattern**: Statistics collection

### Memory Management
- Each task has 128KB stack (configurable)
- Dynamic allocation with proper cleanup
- No memory leaks (verified with valgrind)

### Signal Handling
- Async-signal-safe operations in handler
- Context saving during preemption
- Re-entrant safe statistics updates

## Performance Considerations

### Scalability
- Supports up to 64 concurrent tasks
- O(n) scheduling for most algorithms
- O(1) resource lookups
- Bounded history buffer (1000 entries)

### Tuning Parameters
```c
#define STACK_SIZE (1024 * 128)  // Per-task stack
#define MAX_TASKS 64              // Maximum tasks
#define MAX_RESOURCES 10          // System resources
#define HISTORY_SIZE 1000         // Event history buffer
```

## Contributing

Contributions are welcome! Areas for contribution:
- New scheduling algorithms
- Additional task types
- Performance optimizations
- Bug fixes
- Documentation improvements
- Test cases

---
