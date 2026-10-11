# Intelligent Airport Runway Operation and Scheduling System

**Alliance University**  
**Alliance School of Advanced Computing**  
*Micro-Project Submission for Data Structures & Algorithms / Object-Oriented C++ Programming*

---

### Student Team Members
1. **Diyah B**
2. **Raju ram**
3. **Ajay Krishna S**
4. **Shazeb**
5. **Gaurav**

---

## 1. Project Overview & Rationale

International airports face intense congestion, managing hundreds of arrivals and departures daily. Unpredictable disturbances such as adverse weather, emergency landings, mechanical issues, and maintenance closures disrupt planned sequences. Without automated decision support, airports suffer from increased holding times, fuel wastage, and hazardous runway conflicts.

This project delivers an **Intelligent Airport Runway Operation and Scheduling System** implemented in standard C++17. The system models multi-runway allocation using a **custom Max-Heap Priority Queue**, dynamic conflict detection, greedy time-slot allocation, and automated safety-separation management under varying weather conditions.

### Course Outcomes Addressed
* **CO2**: *Apply appropriate data structures and algorithmic techniques to solve computational problems.*  
  $\rightarrow$ Implemented a custom Max-Heap Priority Queue (`CustomPriorityQueue`) from scratch using dynamic arrays with $O(\log N)$ push and pop operations, custom comparators, and chronological sorting.
* **CO3**: *Analyze and develop efficient solutions using suitable algorithms and programming techniques.*  
  $\rightarrow$ Implemented greedy interval scheduling with safe separation constraints, preemption for emergency landings, and dynamic rescheduling for delays and runway maintenance.
* **CO4**: *Design and implement an efficient C++ solution for a real-world operational problem.*  
  $\rightarrow$ Built a modular, object-oriented, interactive Air Traffic Control (ATC) decision-support software with comprehensive error handling and analytics reporting.

---

## 2. Menu Structure

The system strictly adheres to the required operational specification:

```text
============================================================
          INTELLIGENT AIRPORT RUNWAY SYSTEM
        Alliance School of Advanced Computing
============================================================
========== AIRPORT RUNWAY SCHEDULING SYSTEM ==========

1. Add Aircraft
2. Display Waiting Aircraft
3. Schedule Aircraft
4. Handle Emergency Aircraft
5. Update Flight Delay
6. Detect Runway Conflicts
7. Display Runway Schedule
8. Generate Report
9. Exit
------------------------------------------------------
(Tip: Enter '0' at menu to preload sample demo flights)
```

---

## 3. Core Modules & Functionality

### 1. Add Aircraft
* Registers aircraft with Flight ID, Type (`Arrival`/`Departure`), Scheduled Time (`HH:MM`), Priority Level (`1` to `5`), Emergency Status (`Y`/`N`), and Estimated Runway Occupation Time (minutes).
* Includes validation against duplicate IDs and malformed inputs.

### 2. Display Waiting Aircraft
* Displays all unscheduled aircraft in tabular format ordered by priority queue urgency.
* Shows queue ranking, flight type, scheduled time, priority, and emergency tags.

### 3. Schedule Aircraft (Core Algorithm)
* Clears previous allocations and processes flights using the **Custom Max-Heap Priority Queue**.
* Evaluates all operational runways to identify the earliest available time window satisfying:
  $$\text{StartTime}(B) \ge \text{EndTime}(A) + S_{\text{weather}}$$
  where $S_{\text{weather}}$ is the safety separation buffer.
* Assigns aircraft to the runway that minimizes waiting time:
  $$\min (\text{AllocatedTime} - \text{ScheduledTime})$$

### 4. Handle Emergency Aircraft
* Supports elevating an existing flight to Emergency status or immediately injecting a new inbound emergency flight.
* Emergency flights are assigned Priority 5 and trigger **immediate preemptive rescheduling**, reorganizing downstream flights while maintaining separation.

### 5. Update Flight Delay
* Adjusts scheduled times for delayed aircraft.
* Triggers automatic ripple-free rescheduling across active runways.

### 6. Detect Runway Conflicts
* Audits every operational runway for:
  * Direct interval overlaps ($\text{StartTime}_B < \text{EndTime}_A$)
  * Safety separation violations ($\text{Gap} < S_{\text{weather}}$)
  * Operations planned on closed/maintenance runways
* Includes an interactive environmental simulator:
  * Weather modes: **Clear** (3 min), **Rain** (5 min), **Heavy Rain/Fog** (7 min), **Severe Storm** (10 min).
  * Runway maintenance toggle (Close/Open runways dynamically).

### 7. Display Runway Schedule
* Generates master chronological timetables per runway:
  * Slot index, Aircraft ID, Operation Type, Allocated Window (`HH:MM - HH:MM`), Scheduled Time, Delay/Wait Time, Priority, and Emergency status.

### 8. Generate Report
* Computes real-time ATC analytics:
  * Total registered, scheduled, waiting, and emergency aircraft.
  * Active runway count and environmental conditions.
  * Total & Average Aircraft Waiting Time ($\min$).
  * Maximum Waiting Time and identity of the most delayed flight.
  * Per-runway and airport-wide Capacity Utilization:
    $$\text{Utilization} = \left(\frac{\sum \text{OccupiedMinutes}}{\text{ActiveRunways} \times 720}\right) \times 100\%$$
  * Total conflicts auto-resolved by the algorithm.
  * Automated ATC Decision Support advisory.

### 9. Exit
* Gracefully terminates the application.

*(Bonus: Enter `0` at the main menu to preload 8 realistic demonstration flights for instant testing during project defense).*

---

## 4. Algorithmic Design & Complexity Analysis

| Operation / Module | Data Structure / Algorithm | Time Complexity | Space Complexity |
| :--- | :--- | :--- | :--- |
| Priority Queue Insert (`push`) | Max-Heap (`heapifyUp`) | $O(\log N)$ | $O(1)$ |
| Priority Queue Extract (`pop`) | Max-Heap (`heapifyDown`) | $O(\log N)$ | $O(1)$ |
| Runway Slot Search | Greedy Interval Search + Sort | $O(M \cdot K \log K)$ | $O(K)$ |
| Full Airport Scheduling | Heap Extraction + Multi-Runway Search | $O(N \log N + N \cdot M \cdot K)$ | $O(N)$ |
| Conflict Detection Engine | Pairwise Interval Verification | $O(M \cdot K)$ | $O(C)$ |
| Report Generation | Linear Scan & Aggregation | $O(N + M \cdot K)$ | $O(1)$ |

*(Where $N$ = total aircraft, $M$ = number of active runways, $K$ = operations per runway, $C$ = conflicts detected).*

---

## 5. Web Interface Architecture (Interactive ATC Dashboard)

In addition to the standard terminal menu interface, an interactive **Air Traffic Control (ATC) Web Dashboard** is provided.

```
 [ Modern Web Browser (HTML5 / Tailwind CSS / Vanilla JS) ]
                              ▲
                              │ HTTP GET / POST (JSON API)
                              ▼
        [ Python Lightweight Server: server.py ]
        (Zero external pip dependencies, uses standard library)
                              ▲
                              │ Subprocess CLI Dispatch
                              ▼
       [ Native Compiled C++ Core Engine: runway_engine ]
        - Custom Max-Heap Priority Queue (CO2)
        - Greedy Interval Slot Allocation (CO3)
        - Dynamic Conflict Audit Engine (CO3)
        - Weather Separation & Capacity Analytics (CO4)
```

### Key Web Dashboard Features
1. **Live Air Traffic Control Timetable**: Real-time visual timeline across Runways `09L`, `09R`, and `27` with color-coded flights (Arrivals in Blue, Departures in Green, Emergencies in pulsing Red).
2. **Interactive Weather Controls**: Instant toggling between **Clear** ($3\text{ min}$ buffer), **Rain** ($5\text{ min}$ buffer), **Fog** ($7\text{ min}$ buffer), and **Severe Storm** ($10\text{ min}$ buffer). The C++ engine immediately recalculates and redistributes all flight slots safely.
3. **One-Click Emergency Injection**: Declare an emergency on any flight or register a new inbound emergency aircraft with Priority 5 preemption.
4. **Runway Maintenance Toggle**: Close or reopen any runway for maintenance; the C++ scheduler automatically diverts traffic to the remaining active runways.
5. **Real-Time Conflict Audit Panel**: Displays real-time audit reports confirming zero runway collisions.
6. **ATC Decision Support & Performance Gauges**: Displays live metrics for Average Delay, Maximum Delay, and Runway Capacity Utilization %.

---

## 6. Compilation & Execution Guide

### Prerequisites
* Linux OS (Ubuntu / Debian or any standard POSIX shell)
* `g++` compiler supporting C++17
* `python3` (built-in standard library)
* `make` build utility

### Build & Run Commands

```bash
# Navigate to project folder
cd /home/girish/Documents/raju

# 1. Compile both the Terminal CLI and the Native C++ Engine:
make

# 2. Run the Terminal Menu Interface (Options 1 to 9):
make run
# Or directly: ./runway_system

# 3. Start the Interactive Web Dashboard on http://localhost:5000:
make web
# Or directly: python3 server.py 5000
```

---

## 7. Demonstration & Test Walkthrough

### Terminal CLI Demonstration
1. Run `./runway_system`.
2. Enter **`0`** to load the demonstration dataset (8 flights).
3. Enter **`2`** to inspect the Priority Queue waiting order.
4. Enter **`3`** to execute the multi-runway scheduling algorithm.
5. Enter **`7`** to display the generated conflict-free runway timelines.
6. Enter **`6`** to run the Runway Conflict Detector, then select `1` $\rightarrow$ `4` to simulate a Severe Storm ($10\text{ min}$ separation).
7. Enter **`8`** to inspect the comprehensive ATC Operational Report.
8. Enter **`9`** to exit.

### Web Dashboard Demonstration
1. Start the web server: `make web` (or `python3 server.py 5000`).
2. Open your browser and navigate to: **`http://localhost:5000`**.
3. Click **"Run C++ Scheduler"** to trigger the C++ Priority Queue engine.
4. Try changing the weather to **"Severe Storm"** or clicking **"Close for Maintenance"** on Runway 1, and watch the C++ scheduler instantly re-optimize the flights in real time.

