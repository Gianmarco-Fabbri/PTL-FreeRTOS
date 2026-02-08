# PTL Timing Model

> Technical documentation for the Periodic Task Layer scheduling behavior

---

## Overview

The PTL (Periodic Task Layer) implements a **Supervisor Model** where a high-priority supervisor task manages the release, deadline checking, and overrun handling of periodic worker tasks.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                       PTL Supervisor                             │
│                   Priority: configMAX_PRIORITIES - 1             │
│                      Wakes every 1 ms (tick)                     │
│                                                                   │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│   │ Check       │  │ Release     │  │ Apply       │             │
│   │ Deadlines   │──│ Ready Tasks │──│ Policies    │             │
│   └─────────────┘  └─────────────┘  └─────────────┘             │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Worker Tasks                                │
│            (Generic Wrapper + User Job Function)                 │
│                                                                   │
│   ┌──────────┐     ┌──────────┐     ┌──────────┐               │
│   │ Task A   │     │ Task B   │     │ Task C   │               │
│   │ Prio: 3  │     │ Prio: 2  │     │ Prio: 1  │               │
│   └──────────┘     └──────────┘     └──────────┘               │
└─────────────────────────────────────────────────────────────────┘
```

---

## Timing Definitions

| Term                 | Symbol | Description                                                   |
| -------------------- | ------ | ------------------------------------------------------------- |
| **Period**     | T      | Time between consecutive job releases                         |
| **Deadline**   | D      | Maximum time allowed for job completion (relative to release) |
| **WCET**       | C      | Worst-Case Execution Time (maximum job duration)              |
| **Release**    | R_k    | Absolute time when job k becomes ready                        |
| **Completion** | F_k    | Absolute time when job k finishes                             |

### Relationships

- **Implicit Deadline**: If `D = 0`, then `D = T` (deadline equals period)
- **Constrained Deadline**: `D ≤ T` (required for correct operation)
- **Schedulability**: Sum of `C_i / T_i` for all tasks should be < 100%

---

## Supervisor Task Behavior

The supervisor task runs at tick granularity (every 1 ms by default) and performs:

### 1. Release Check (at t = R_k)

```
for each task i:
    if current_tick == next_release_time[i]:
        if task[i].is_active:
            → OVERRUN detected, apply policy
        else:
            → xTaskNotifyGive(task[i])  // Release new job
            → Log PTL_EVENT_RELEASE
```

### 2. Deadline Check (continuous)

```
for each active task i:
    if current_tick > absolute_deadline[i]:
        → Log PTL_EVENT_DEADLINE_MISS
        → Apply per-task or global overrun policy
```

### 3. Overrun Policy Application

| Policy             | Behavior                                  |
| ------------------ | ----------------------------------------- |
| **SKIP**     | Let current job finish, skip next release |
| **KILL**     | Delete task, recreate, start fresh        |
| **CATCH_UP** | Mark missed, release new job immediately  |

---

## Timing Diagrams

### Normal Execution

```
Time:   0    10    20    30    40    50    60    70    80    90    100
        │     │     │     │     │     │     │     │     │     │     │
TaskA   ├─────┤     │     │     ├─────┤     │     │     ├─────┤     │
        │ R₀  │ F₀  │     │     │ R₁  │ F₁  │     │     │ R₂  │ F₂  │
        │     │     │     │     │     │     │     │     │     │     │
Period: 50ms, WCET: 10ms, Deadline: 50ms
Status: ✅ All jobs complete before deadline
```

### SKIP Policy (Overrun)

```
Time:   0    10    20    30    40    50    60    70    80    90    100
        │     │     │     │     │     │     │     │     │     │     │
TaskA   ├───────────────────────┤     │     ├─────┤     │     │     │
        │ R₀ (overruns D₀=50)   │ F₀  │     │ R₂  │ F₂  │     │     │
        │                   ▲   │     │     │     │     │     │     │
                            │
                    SKIP: R₁ not released (job 0 still active)
```

### KILL Policy (Overrun)

```
Time:   0    10    20    30    40    50    60    70    80    90    100
        │     │     │     │     │     │     │     │     │     │     │
TaskA   ├─────────────────╳     ├─────┤     │     ├─────┤     │     │
        │ R₀ (killed at D₀)     │ R₁  │ F₁  │     │ R₂  │ F₂  │     │
        │                 ▲     │     │     │     │     │     │     │
                          │
                    KILL: Task deleted and recreated
```

### CATCH_UP Policy (Overrun)

```
Time:   0    10    20    30    40    50    60    70    80    90    100
        │     │     │     │     │     │     │     │     │     │     │
TaskA   ├───────────────────────┼─────┤     ├─────┤     │     │     │
        │ R₀ (overruns D₀=50)   │ R₁* │ F₁  │ R₂  │ F₂  │     │     │
        │                   ▲   │     │     │     │     │     │     │
                            │
                    CATCH_UP: R₁ released immediately after F₀
                    *Job 0 marked as missed
```

---

## Release Jitter

The PTL guarantees release jitter ≤ 1 tick (1 ms at 1000 Hz):

- Supervisor wakes exactly at tick boundaries via `vTaskDelayUntil()`
- All tasks released immediately via `xTaskNotifyGive()`
- Higher priority supervisor ensures timely scheduling check

---

## Performance Considerations

### Overhead Sources

1. **Supervisor wake-up**: 1x per tick
2. **Task iteration**: O(n) where n = number of tasks
3. **Trace logging**: Circular buffer, O(1)

### Recommended Limits

| Parameter           | Recommended | Maximum |
| ------------------- | ----------- | ------- |
| Tasks               | ≤ 8        | 16      |
| Minimum period      | 5 ms        | 2 ms    |
| Supervisor overhead | < 5%        | 10%     |

---

## API Timing Sequence

```
main()
  │
  ├── UART_init()
  ├── PTL_TraceInit()
  ├── PTL_Init(config, tasks, count)
  │     │
  │     ├── Validate configurations
  │     ├── Create worker tasks (suspended)
  │     └── Initialize task states
  │
  └── PTL_Start()
        │
        ├── Create supervisor task
        ├── Burn_Calibrate()  // Calibrate CPU timing
        └── vTaskStartScheduler()  // Never returns
              │
              ├── t=0: All tasks released simultaneously
              ├── t=1: Supervisor checks deadlines
              ├── t=T₁: Task 1 next release
              └── ...
```

---

## Trace Events

| Event               | Description    | When Logged                |
| ------------------- | -------------- | -------------------------- |
| `RELEASE`         | Job released   | Supervisor releases task   |
| `START`           | Job begins     | Worker wrapper entry       |
| `COMPLETE`        | Job finished   | Worker wrapper exit        |
| `DEADLINE_MISS`   | D exceeded     | Supervisor check           |
| `OVERRUN_SKIP`    | Policy applied | Supervisor detects overrun |
| `OVERRUN_KILL`    | Policy applied | Supervisor detects overrun |
| `OVERRUN_CATCHUP` | Policy applied | Supervisor detects overrun |
| `SWITCH_IN`       | Context switch | FreeRTOS trace hook        |
| `SWITCH_OUT`      | Context switch | FreeRTOS trace hook        |

---

## Configuration Example

```c
// Task: 50ms period, 40ms deadline, 10ms work
PTL_TaskConfig_t sensor = {
    .pcName        = "Sensor",
    .xPeriod       = pdMS_TO_TICKS(50),   // T = 50ms
    .xDeadline     = pdMS_TO_TICKS(40),   // D = 40ms (< T)
    .uxPriority    = 3,
    .usStackDepth  = 256,
    .pvEntryFunction = Job_Sensor,        // WCET ≈ 10ms
    .pvParameters  = NULL,
    .eOverrunPolicy = PTL_POLICY_KILL
};
```

### Schedulability Check

For Rate Monotonic scheduling:

- U = Σ(C_i / T_i) ≤ n(2^(1/n) - 1)
- For n=3 tasks: U ≤ 0.78 (78%)

---

## Author

Group 38 - Real-Time Systems Project 2026
