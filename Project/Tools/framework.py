"""
@file: framework.py
@brief: Configuration file generator for project tools.
This script generates configuration files based on predefined templates
and user inputs.
@version: 2.1
@date: 2026-01-26
@author: Member 4 (Davide)

"""
import os
import math
import subprocess
import sys

# --- CONFIGURATION ---
PROJECT_ROOT = os.path.join(os.path.dirname(__file__), "..")
GENERATED_FILE = os.path.join(PROJECT_ROOT, "main_gen.c")

# ==========================================
# 1. USER INPUT 
# ==========================================
def get_user_config():
    print("\n=======================================")
    print("      PTL SYSTEM CONFIGURATOR          ")
    print("=======================================")
    
    config = {}

    # --- 1. GLOBAL DEFAULT POLICY ---
    print("\n[Global Configuration]")
    print("Select Default Overrun Policy: [1] SKIP, [2] KILL, [3] CATCH_UP")
    
    # Map inputs to C strings
    p_map = {'1': "PTL_POLICY_SKIP", '2': "PTL_POLICY_KILL", '3': "PTL_POLICY_CATCH_UP"}
    
    g_choice = input("    Choice (Default 1): ").strip()
    global_policy = p_map.get(g_choice, "PTL_POLICY_SKIP")
    print(f"    -> Global Default set to: {global_policy}")
    config['policy'] = global_policy

    # --- 2. TASK DEFINITIONS ---
    print("\n[Define Tasks] (Type 'done' as name to finish)")
    tasks = []
    idx = 1
    
    while True:
        print(f"\n    --- Task #{idx} ---")
        name = input("    Name: ").strip()
        
        if name.lower() == 'done' or name == "":
            if len(tasks) == 0:
                print("    [!] At least one task is required.")
                continue
            break
            
        try:
            # Timing
            period = int(input(f"    Period (ms): ").strip())
            wcet   = int(input(f"    WCET   (ms): ").strip())
            
            # Deadline (Default = Period)
            dl_in = input(f"    Deadline (Enter for {period}ms): ").strip()
            deadline = int(dl_in) if dl_in else period
            
            # Priority (Standard 1-5)
            prio_in = input(f"    Priority (1-5): ").strip()
            prio = int(prio_in) if prio_in else 1

            # --- POLICY SELECTION (Per-Task) ---
            print(f"    Policy (1=SKIP, 2=KILL, 3=CATCH_UP)")
            pol_in = input(f"    Select (Enter for Global): ").strip()
            
            # Logic: If Enter, use 'PTL_POLICY_USE_GLOBAL'. 
            # If 1/2/3, use specific.
            if not pol_in:
                task_policy = "PTL_POLICY_USE_GLOBAL"
            else:
                task_policy = p_map.get(pol_in, "PTL_POLICY_USE_GLOBAL")

            tasks.append({
                "name": name,
                "period": period,
                "wcet": wcet,
                "deadline": deadline, 
                "priority": prio,
                "policy": task_policy
            })
            idx += 1
        except ValueError:
            print("    [!] Invalid number. Try again.")

    config['tasks'] = tasks
    return config

# ==========================================
# 2. ANALYSIS & SIMULATION
# ==========================================
def perform_analysis(cfg):
    tasks = cfg['tasks']
    print(f"\n=== Schedulability Report ({len(tasks)} tasks) ===")
    
    # 2.1 Calculate Cycles
    periods = [t['period'] for t in tasks]
    
    # Calculate GCD (Minor Cycle)
    minor_cycle = periods[0]
    for p in periods[1:]:
        minor_cycle = math.gcd(minor_cycle, p)
        
    # Calculate LCM (Major Cycle / Hyperperiod)
    major_cycle = periods[0]
    for p in periods[1:]:
        major_cycle = (major_cycle * p) // math.gcd(major_cycle, p)

    print(f"MINOR CYCLE (GCD):         {minor_cycle} ms")
    print(f"MAJOR CYCLE (Hyperperiod): {major_cycle} ms")
    
    # 2.2 Utilization Check
    utilization = sum(t['wcet'] / t['period'] for t in tasks)
    print(f"TOTAL CPU LOAD:            {utilization*100:.2f}%")
    
    if utilization > 1.0:
        print("[FAIL] System is OVERLOADED (>100%).")
        return False
        
    # 2.3 SIMULATE SCHEDULE (Timeline)
    print(f"\n=== Simulated Schedule (0 to {major_cycle} ms) ===")
    print("Note: Simulates Fixed Priority Preemptive Scheduling.")
    
    # -- Simulation Logic: Tick by Tick --
    active_jobs = [] # List of {'task': t, 'rem': wcet, 'abs_deadline': d}
    history = []     # For printing
    
    for t in range(major_cycle):
        # A. Release new jobs
        for task in tasks:
            if t % task['period'] == 0:
                active_jobs.append({
                    'name': task['name'],
                    'prio': task['priority'],
                    'rem': task['wcet'],
                    'deadline': t + task['deadline']
                })
        
        # B. Pick Highest Priority Job (Preemptive)
        # Filter out finished jobs
        active_jobs = [j for j in active_jobs if j['rem'] > 0]
        
        if not active_jobs:
            history.append("IDLE")
            continue
            
        # Sort: Highest Priority First. 
        # Python Sort is stable. We sort by Priority (Descending).
        active_jobs.sort(key=lambda x: x['prio'], reverse=True)
        running_job = active_jobs[0]
        
        # C. Execute
        running_job['rem'] -= 1
        history.append(running_job['name'])
        
        # D. Check Deadline Miss
        # Note: If time is t, we are executing during [t, t+1).
        # Completion is at t+1.
        if (t + 1) > running_job['deadline'] and running_job['rem'] > 0:
            print(f"  [!] DEADLINE MISS: {running_job['name']} at {t+1}ms")

    # 2.4 Print Compact Timeline
    if not history: return True

    start_t = 0
    current_task = history[0]
    
    print(f"{'Time (ms)':<15} {'Running Task'}")
    print("-" * 30)
    
    for t in range(1, len(history)):
        if history[t] != current_task:
            print(f"{start_t:03d} - {t:03d}        {current_task}")
            current_task = history[t]
            start_t = t
            
    # Print final segment
    print(f"{start_t:03d} - {len(history):03d}        {current_task}")
    print("-" * 30)

    return True

# ==========================================
# 3. CODE GENERATION
# ==========================================
def generate_code(cfg):
    print(f"\n[*] Generating code to: {GENERATED_FILE}...")
    
    c_code = f"""/* AUTO-GENERATED BY config_gen.py */
#include "ptl.h"
#include "ptl_trace.h"
#include "uart.h"
#include "burner.h"


/* --- JOB FUNCTIONS --- */
"""
    # Generate Jobs
    for t in cfg['tasks']:
        c_code += f"""
void Job_{t['name']}(void *p) {{
    (void)p;
    UART_printf("[{t['name']}] Start (Run: {t['wcet']}ms)\\n");
    Burn({t['wcet']});
    UART_printf("[{t['name']}] End\\n");
}}
"""

    # Generate Task List
    c_code += f"""
/* --- CONFIGURATION --- */
int main(void) {{
    UART_init();
    UART_printf("\\n=== AUTO-GENERATED CONFIGURATION ===\\n");
    UART_printf("Global Policy: {cfg['policy']}\\n");

    static PTL_TaskConfig_t t[] = {{"""
    
    for t in cfg['tasks']:
        c_code += f"""
        /* {t['name']}: {t['policy']} */
        {{ "{t['name']}", pdMS_TO_TICKS({t['period']}), pdMS_TO_TICKS({t['deadline']}), {t['priority']}, 256, Job_{t['name']}, NULL, {t['policy']} }},"""
    
    c_code += f"""
    }};

    static PTL_GlobalConfig_t c = {{ {cfg['policy']}, pdTRUE, {len(cfg['tasks'])} }};

    if (PTL_Init(&c, t, {len(cfg['tasks'])}) == pdPASS) {{
        PTL_Start();
    }} else {{
        UART_printf("[FATAL] PTL Init Failed!\\n");
    }}
    
    /* SAFETY LOOP: Should never reach here unless scheduler fails */
    for(;;) {{}}
    
    return 0; /* Unreachable, but keeps compiler happy */
}}
"""

    with open(GENERATED_FILE, "w") as f:
        f.write(c_code)
    print("    [OK] Code Generated.")

# ==========================================
# 4. AUTO-RUN (MAKEFILE INTEGRATION)
# ==========================================
def run_simulation():
    print("\n=======================================")
    print("           LAUNCHING SIMULATION        ")
    print("=======================================")
    
    choice = input("Build and Run now? (y/n): ").strip().lower()
    if choice != 'y':
        return

    # 1. Build the ELF using the new entry point
    # 2. Start QEMU
    cmd = ["make", "all", "qemu_start", "ENTRY_POINT=main_gen.c"]
    
    try:
        # Run make from the Project root
        subprocess.run(cmd, cwd=PROJECT_ROOT, check=True)
    except subprocess.CalledProcessError:
        print("\n[ERROR] Simulation failed or crashed.")
        subprocess.run({"make","clean"})
    except KeyboardInterrupt:
        print("\n[STOP] Simulation aborted by user.")
        subprocess.run({"make","clean"})


# ==========================================
# MAIN
# ==========================================
if __name__ == "__main__":
    try:
        config = get_user_config()
        perform_analysis(config)
        generate_code(config)
        run_simulation()
    except KeyboardInterrupt:
        print("\n\n[EXIT] Tool closed.")