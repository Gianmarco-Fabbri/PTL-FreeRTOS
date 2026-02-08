"""
@file: config_gen.py
@brief: Configuration file generator for project tools.
This script generates configuration files based on predefined templates
and user inputs.
@version: 1.0
@date: 2024-06-15
@author: Member 4 (Davide)

"""
import os
import math


def get_user_inputs():
    print("\n=== PTL CONFIGURATION FRAMEWORK ===")
    config = {}

    # --- 1. GLOBAL DEFAULT POLICY ---
    print("\n[Global Configuration]")
    print("Select Default Overrun Policy: [1] SKIP, [2] KILL, [3] CATCH_UP")
    
    # Map inputs '1','2','3' to the actual C strings
    p_map = {'1': "PTL_POLICY_SKIP", '2': "PTL_POLICY_KILL", '3': "PTL_POLICY_CATCH_UP"}
    
    # Get Global Choice
    g_choice = input("Enter choice (1-3): ").strip()
    
    # If invalid or empty, default to SKIP
    global_policy = p_map.get(g_choice, "PTL_POLICY_SKIP")
    print(f"-> Global Default set to: {global_policy}")

    # --- 2. TASK DEFINITIONS ---
    print("\n[Define Tasks]")
    tasks = []
    num_tasks = 0
    
    while True:
        print(f"\n--- Task #{num_tasks + 1} ---")
        task_name = input(f"  Name (or 'done'): ").strip()
        
        if task_name.lower() == 'done':
            if not tasks:
                print("  [ERROR] Define at least one task.")
                continue
            break
        if not task_name: task_name = f"Task_{num_tasks + 1}"

        try:
            # Timing
            period = int(input(f"  Period (ms): ").strip())
            wcet = int(input(f"  WCET (ms): ").strip())
            
            # Deadline (Default = Period)
            dl_in = input(f"  Deadline (Enter for {period}ms): ").strip()
            deadline = int(dl_in) if dl_in else period
            
            # Priority (Standard 1-5)
            prio_in = input(f"  Priority (1-5): ").strip()
            priority = int(prio_in) if prio_in else 1

            # --- POLICY SELECTION (The part you wanted) ---
            print(f"  Policy (1=SKIP, 2=KILL, 3=CATCH_UP)")
            pol_in = input(f"  Select (Press Enter for Global {global_policy}): ").strip()
            
            # Logic: If they type nothing, use Global. 
            # If they type 1/2/3, use that. If garbage, use Global.
            if not pol_in:
                task_policy = global_policy
            else:
                task_policy = p_map.get(pol_in, global_policy)

            # Store it
            tasks.append({
                "name": task_name, "period": period, "wcet": wcet,
                "deadline": deadline, "priority": priority,
                "policy": task_policy  # <--- Saves the specific string
            })
            num_tasks += 1

        except ValueError:
            print("  [ERROR] Invalid input. Please use numbers.")

    config['tasks'] = tasks
    return config


# ==========================================
# 2. ANALYSIS & SIMULATION (Updated)
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
    print("Note: This assumes Rate Monotonic / Fixed Priority behavior.")
    
    # Initialize Simulation State
    # timeline[time] = TaskName or None
    timeline = [None] * major_cycle 

            
    # -- Better Simulation Logic: Tick by Tick --
    active_jobs = [] # List of {'task': t, 'rem': wcet, 'abs_deadline': d}
    
    history = [] # For printing
    
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
        
        # B. Pick Highest Priority Job
        # Filter out finished jobs
        active_jobs = [j for j in active_jobs if j['rem'] > 0]
        
        if not active_jobs:
            history.append("IDLE")
            continue
            
        # Sort: Highest Priority First. Tie-breaker: Earliest Deadline (optional)
        # FreeRTOS: Higher number = Higher Priority
        active_jobs.sort(key=lambda x: x['prio'], reverse=True)
        running_job = active_jobs[0]
        
        # C. Execute
        running_job['rem'] -= 1
        history.append(running_job['name'])
        
        # D. Check Deadline Miss
        if t + 1 > running_job['deadline'] and running_job['rem'] > 0:
            print(f"  [!] DEADLINE MISS: {running_job['name']} at {t+1}ms")

    # 2.4 Print Compact Timeline
    # Compress "A, A, A, B, B" into "0-3: A, 3-5: B"
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


# TO DO: Add Comments and Code Generation Functions.


# ==========================================
# MAIN EXECUTION (Test Mode)
# ==========================================
if __name__ == "__main__":
    # 1. Get Input from User
    user_config = get_user_inputs()
    
    # 2. Run Analysis & Simulation (No Code Gen)
    perform_analysis(user_config)