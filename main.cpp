#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace std;

// ============================================================
// 1. DATA STRUCTURES
// ============================================================

/**
 * Structure representing an Aircraft operating at the airport.
 * Course Outcome CO2: Structured representation of flight entities.
 */
struct Aircraft
{
    int id;                     // Unique Flight / Aircraft ID
    string type;                // "Arrival" or "Departure"
    int scheduledTime;          // Scheduled time in minutes from 00:00 (0 - 1439)
    int priority;               // Operational priority: 1 (Lowest) to 5 (Highest)
    bool emergency;             // True if flight has emergency status
    int runwayTime;             // Required runway occupation duration (minutes)

    // Dynamic scheduling attributes
    bool scheduled;             // True if assigned to a runway slot
    int assignedRunway;         // Runway ID assigned (-1 if unassigned)
    int actualTime;             // Allocated start time in minutes (-1 if unassigned)
    int waitingTime;            // Waiting / Delay duration: (actualTime - scheduledTime)
};

/**
 * Structure representing a single Runway Operation slot.
 */
struct Operation
{
    int aircraftID;             // ID of the aircraft occupying the runway
    string type;                // "Arrival" or "Departure"
    int startTime;              // Start of runway occupation (in minutes)
    int endTime;                // End of runway occupation (in minutes)
    bool isEmergency;           // Indicates if this operation was an emergency
};

/**
 * Structure representing an Airport Runway and its scheduled operations.
 */
struct Runway
{
    int id;                     // Runway identifier (e.g., 1, 2, 3)
    string code;                // ICAO/IATA designator (e.g., "09L", "09R", "27")
    bool active;                // Operational availability (true = OPEN, false = CLOSED/MAINTENANCE)
    vector<Operation> operations; // Chronological sequence of scheduled operations
};

/**
 * Structure to record detected schedule conflicts.
 */
struct ConflictRecord
{
    int runwayId;
    int aircraftA;
    int aircraftB;
    int startA, endA;
    int startB, endB;
    string reason;
    bool resolved;
};

// ============================================================
// 2. GLOBAL SYSTEM STATE
// ============================================================

vector<Aircraft> aircraftList;
vector<Runway> runways;
vector<ConflictRecord> conflictHistory;

string weatherCondition = "Clear";
int safetySeparation = 3;       // Minimum required gap between consecutive runway ops (minutes)
int conflictsResolvedCount = 0; // Total count of conflicts detected and resolved

// ============================================================
// 3. TIME CONVERSION & FORMATTING UTILITIES
// ============================================================

/**
 * Convert "HH:MM" string format to total minutes from 00:00.
 * Returns -1 on invalid format.
 */
int convertToMinutes(const string &timeStr)
{
    if (timeStr.length() != 5 || timeStr[2] != ':')
        return -1;

    for (int i = 0; i < 5; ++i)
    {
        if (i == 2) continue;
        if (!isdigit(timeStr[i])) return -1;
    }

    int hour = (timeStr[0] - '0') * 10 + (timeStr[1] - '0');
    int minute = (timeStr[3] - '0') * 10 + (timeStr[4] - '0');

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
        return -1;

    return hour * 60 + minute;
}

/**
 * Convert minutes from 00:00 into formatted "HH:MM" 24-hour string.
 */
string convertToTime(int totalMinutes)
{
    if (totalMinutes < 0) return "--:--";
    int hour = (totalMinutes / 60) % 24;
    int minute = totalMinutes % 60;

    ostringstream oss;
    oss << setw(2) << setfill('0') << hour << ":"
        << setw(2) << setfill('0') << minute;
    return oss.str();
}

/**
 * Helper to safely read an integer with stream validation.
 */
int readInteger(const string &prompt, int minVal = -999999, int maxVal = 999999)
{
    int val;
    while (true)
    {
        cout << prompt;
        if (cin >> val)
        {
            if (val >= minVal && val <= maxVal)
                return val;
            cout << " [Error] Value must be between " << minVal << " and " << maxVal << ". Try again.\n";
        }
        else
        {
            cout << " [Error] Invalid input. Please enter a valid number.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }
}

// ============================================================
// 4. PRIORITY COMPARATOR & CUSTOM MAX-HEAP PRIORITY QUEUE
// ============================================================

/**
 * Determines relative priority between two aircraft.
 * Criteria:
 * 1. Emergency Status (Emergency flight takes absolute precedence).
 * 2. Explicit Priority Score (5 = Highest, down to 1).
 * 3. Scheduled Time (Earlier scheduled time served first - FIFO).
 * 4. Runway Occupation Time (Shortest runway occupancy as tie-breaker).
 */
bool higherPriority(const Aircraft &a, const Aircraft &b)
{
    // Emergency aircraft always have highest priority
    if (a.emergency != b.emergency)
        return a.emergency;

    // Higher numerical priority first
    if (a.priority != b.priority)
        return a.priority > b.priority;

    // Earlier scheduled time first
    if (a.scheduledTime != b.scheduledTime)
        return a.scheduledTime < b.scheduledTime;

    // Tiebreaker: shorter runway occupancy
    return a.runwayTime < b.runwayTime;
}

/**
 * Custom Max-Heap Priority Queue implementation.
 * Satisfies Course Outcome CO2: Custom implementation of fundamental data structures.
 */
class CustomPriorityQueue
{
private:
    vector<Aircraft> heap;

    void heapifyUp(int index)
    {
        while (index > 0)
        {
            int parent = (index - 1) / 2;
            if (higherPriority(heap[index], heap[parent]))
            {
                swap(heap[index], heap[parent]);
                index = parent;
            }
            else
            {
                break;
            }
        }
    }

    void heapifyDown(int index)
    {
        int size = heap.size();
        while (true)
        {
            int left = 2 * index + 1;
            int right = 2 * index + 2;
            int highest = index;

            if (left < size && higherPriority(heap[left], heap[highest]))
                highest = left;

            if (right < size && higherPriority(heap[right], heap[highest]))
                highest = right;

            if (highest != index)
            {
                swap(heap[index], heap[highest]);
                index = highest;
            }
            else
            {
                break;
            }
        }
    }

public:
    void push(const Aircraft &plane)
    {
        heap.push_back(plane);
        heapifyUp(heap.size() - 1);
    }

    Aircraft pop()
    {
        if (heap.empty())
            throw runtime_error("Attempted pop from empty priority queue.");

        Aircraft topPlane = heap[0];
        heap[0] = heap.back();
        heap.pop_back();

        if (!heap.empty())
            heapifyDown(0);

        return topPlane;
    }

    Aircraft top() const
    {
        if (heap.empty())
            throw runtime_error("Attempted top on empty priority queue.");
        return heap[0];
    }

    bool empty() const
    {
        return heap.empty();
    }

    int size() const
    {
        return heap.size();
    }

    vector<Aircraft> toSortedVector() const
    {
        CustomPriorityQueue copy = *this;
        vector<Aircraft> sortedList;
        while (!copy.empty())
        {
            sortedList.push_back(copy.pop());
        }
        return sortedList;
    }
};

// ============================================================
// 5. RUNWAY OPERATION SORTING & CONFLICT DETECTION ENGINE
// ============================================================

/**
 * Sorts operations on a given runway in chronological order by startTime.
 */
void sortOperations(Runway &runway)
{
    sort(runway.operations.begin(), runway.operations.end(),
         [](const Operation &a, const Operation &b) {
             return a.startTime < b.startTime;
         });
}

/**
 * Checks if a proposed operation interval [startTime, endTime] on a given runway
 * conflicts with any existing operation under current safety separation.
 */
bool hasConflict(const Runway &runway, int startTime, int endTime)
{
    for (const auto &op : runway.operations)
    {
        // Conflict condition: Interval overlaps or violates minimum safety separation
        if (startTime < op.endTime + safetySeparation &&
            endTime + safetySeparation > op.startTime)
        {
            return true;
        }
    }
    return false;
}

/**
 * Greedily searches for the earliest conflict-free start time on a runway
 * on or after the requested start time.
 */
int findAvailableTime(Runway &runway, int requestedTime, int occupationTime)
{
    int proposedTime = requestedTime;
    sortOperations(runway);

    bool found = false;
    while (!found)
    {
        found = true;
        int proposedEnd = proposedTime + occupationTime;

        for (const auto &op : runway.operations)
        {
            if (proposedTime < op.endTime + safetySeparation &&
                proposedEnd + safetySeparation > op.startTime)
            {
                // Advance proposed time to right after the conflicting operation + safety separation
                proposedTime = op.endTime + safetySeparation;
                found = false;
                break;
            }
        }
    }

    return proposedTime;
}

// ============================================================
// 6. CORE SCHEDULING ALGORITHM (CO2, CO3, CO4)
// ============================================================

/**
 * Schedules all waiting aircraft across active runways using Priority Queue.
 * Resolves runway conflicts dynamically while maintaining safety separations.
 */
void scheduleAircraft()
{
    if (aircraftList.empty())
    {
        cout << "\n [Info] No aircraft registered in the system to schedule.\n";
        return;
    }

    // Reset current schedules across all runways
    for (auto &r : runways)
    {
        r.operations.clear();
    }

    // Reset scheduling status for all aircraft
    for (auto &plane : aircraftList)
    {
        plane.scheduled = false;
        plane.assignedRunway = -1;
        plane.actualTime = -1;
        plane.waitingTime = 0;
    }

    // Insert all aircraft into the Custom Priority Queue
    CustomPriorityQueue pq;
    for (const auto &plane : aircraftList)
    {
        pq.push(plane);
    }

    int scheduledCount = 0;
    int detectedConflictCount = 0;

    // Process aircraft strictly according to priority queue order
    while (!pq.empty())
    {
        Aircraft current = pq.pop();

        int bestRunwayIdx = -1;
        int bestStartTime = -1;
        int smallestWaiting = 999999;

        // Evaluate all active runways to find optimal slot that minimizes waiting time
        for (size_t r = 0; r < runways.size(); ++r)
        {
            if (!runways[r].active)
                continue;

            int availableTime = findAvailableTime(runways[r], current.scheduledTime, current.runwayTime);
            int waiting = availableTime - current.scheduledTime;

            if (waiting < smallestWaiting)
            {
                smallestWaiting = waiting;
                bestRunwayIdx = r;
                bestStartTime = availableTime;
            }
        }

        // Assign to optimal runway
        if (bestRunwayIdx != -1)
        {
            // Flight had to be moved from its requested time to avoid a clash
            if (smallestWaiting > 0) detectedConflictCount++;
            Operation op;
            op.aircraftID = current.id;
            op.type = current.type;
            op.startTime = bestStartTime;
            op.endTime = bestStartTime + current.runwayTime;
            op.isEmergency = current.emergency;

            runways[bestRunwayIdx].operations.push_back(op);
            sortOperations(runways[bestRunwayIdx]);

            // Update master aircraft registry
            for (auto &plane : aircraftList)
            {
                if (plane.id == current.id)
                {
                    plane.scheduled = true;
                    plane.assignedRunway = runways[bestRunwayIdx].id;
                    plane.actualTime = bestStartTime;
                    plane.waitingTime = smallestWaiting;
                    break;
                }
            }
            scheduledCount++;
        }
    }

    conflictsResolvedCount += detectedConflictCount;

    cout << "\n ============================================================\n";
    cout << "  SCHEDULING SUMMARY\n";
    cout << " ============================================================\n";
    cout << "  Total Aircraft Processed : " << aircraftList.size() << "\n";
    cout << "  Successfully Scheduled   : " << scheduledCount << "\n";
    cout << "  Waiting / Unassigned     : " << (aircraftList.size() - scheduledCount) << "\n";
    cout << "  Separation Buffer Used   : " << safetySeparation << " mins (" << weatherCondition << " Weather)\n";
    cout << "  Conflicts Auto-Resolved  : " << detectedConflictCount << "\n";
    cout << " ============================================================\n";
}

// ============================================================
// 7. MENU OPTION HANDLERS (1 to 9)
// ============================================================

/**
 * 1. Add Aircraft
 */
void addAircraft()
{
    cout << "\n--- ADD AIRCRAFT REGISTRATION ---\n";
    Aircraft plane;

    plane.id = readInteger("Enter Aircraft ID (e.g., 101): ", 1, 99999);

    // Check duplicate ID
    for (const auto &p : aircraftList)
    {
        if (p.id == plane.id)
        {
            cout << " [Error] Aircraft ID " << plane.id << " already exists in the registry.\n";
            return;
        }
    }

    // Type selection
    int typeChoice;
    cout << "Select Operation Type:\n";
    cout << "  1. Arrival (Landing)\n";
    cout << "  2. Departure (Takeoff)\n";
    typeChoice = readInteger("Enter choice (1/2): ", 1, 2);
    plane.type = (typeChoice == 1) ? "Arrival" : "Departure";

    // Scheduled time
    string timeStr;
    while (true)
    {
        cout << "Enter Scheduled Time (HH:MM 24-hr format): ";
        cin >> timeStr;
        int mins = convertToMinutes(timeStr);
        if (mins != -1)
        {
            plane.scheduledTime = mins;
            break;
        }
        cout << " [Error] Invalid time format. Please use HH:MM (e.g., 08:30, 14:15).\n";
    }

    // Priority
    cout << "Priority Levels: 1=Low (Cargo), 2=Normal, 3=VIP/Scheduled, 4=Urgent, 5=Emergency\n";
    plane.priority = readInteger("Enter Priority (1 - 5): ", 1, 5);

    // Emergency status
    char emChoice;
    cout << "Is this an Emergency flight? (Y/N): ";
    cin >> emChoice;
    plane.emergency = (emChoice == 'Y' || emChoice == 'y');

    if (plane.emergency)
    {
        plane.priority = 5;
        cout << " [Notice] Emergency declared! Priority automatically elevated to 5 (Critical).\n";
    }

    // Runway occupation time
    plane.runwayTime = readInteger("Enter Estimated Runway Occupation Time in minutes (e.g., 4): ", 1, 60);

    // Initialize state
    plane.scheduled = false;
    plane.assignedRunway = -1;
    plane.actualTime = -1;
    plane.waitingTime = 0;

    aircraftList.push_back(plane);
    cout << " [Success] Aircraft " << plane.id << " (" << plane.type << ") registered successfully.\n";

    // Auto-prompt to schedule
    cout << " Run option 3 (Schedule Aircraft) to assign optimal runway slots.\n";
}

/**
 * 2. Display Waiting Aircraft
 */
void displayWaitingAircraft()
{
    cout << "\n========================================================================================\n";
    cout << "                          CURRENT WAITING AIRCRAFT QUEUE\n";
    cout << "========================================================================================\n";

    // Build priority queue of unassigned aircraft
    CustomPriorityQueue waitQueue;
    for (const auto &p : aircraftList)
    {
        if (!p.scheduled)
            waitQueue.push(p);
    }

    if (waitQueue.empty())
    {
        cout << " [Info] No aircraft currently in waiting state. All registered aircraft are scheduled.\n";
        cout << "========================================================================================\n";
        return;
    }

    cout << left << setw(8)  << "Queue#"
         << setw(10) << "Flight ID"
         << setw(14) << "Type"
         << setw(12) << "Sched Time"
         << setw(10) << "Priority"
         << setw(12) << "Emergency"
         << setw(14) << "Occupancy"
         << setw(12) << "Status" << "\n";
    cout << "----------------------------------------------------------------------------------------\n";

    int rank = 1;
    vector<Aircraft> sortedWaiting = waitQueue.toSortedVector();
    for (const auto &p : sortedWaiting)
    {
        cout << left << setw(8)  << rank++
             << setw(10) << p.id
             << setw(14) << p.type
             << setw(12) << convertToTime(p.scheduledTime)
             << setw(10) << p.priority
             << setw(12) << (p.emergency ? "YES [!]" : "NO")
             << setw(14) << (to_string(p.runwayTime) + " mins")
             << setw(12) << "WAITING" << "\n";
    }
    cout << "========================================================================================\n";
    cout << " Total waiting for runway assignment: " << sortedWaiting.size() << " aircraft.\n";
}

/**
 * 4. Handle Emergency Aircraft
 */
void handleEmergencyAircraft()
{
    cout << "\n--- HANDLE EMERGENCY AIRCRAFT ---\n";
    cout << "1. Declare Emergency for an existing Aircraft\n";
    cout << "2. Register and Immediately Insert a NEW Emergency Aircraft\n";
    int choice = readInteger("Select Option (1/2): ", 1, 2);

    if (choice == 1)
    {
        if (aircraftList.empty())
        {
            cout << " [Info] No registered aircraft found.\n";
            return;
        }

        int targetId = readInteger("Enter Aircraft ID to declare Emergency: ");
        bool found = false;

        for (auto &plane : aircraftList)
        {
            if (plane.id == targetId)
            {
                plane.emergency = true;
                plane.priority = 5;
                found = true;
                cout << "\n [ALERT] Aircraft " << targetId << " marked as CRITICAL EMERGENCY!\n";
                cout << " Immediate preemptive rescheduling initiated...\n";
                scheduleAircraft();
                break;
            }
        }

        if (!found)
            cout << " [Error] Aircraft ID " << targetId << " not found in registry.\n";
    }
    else
    {
        Aircraft plane;
        plane.id = readInteger("Enter New Emergency Aircraft ID: ", 1, 99999);

        for (const auto &p : aircraftList)
        {
            if (p.id == plane.id)
            {
                cout << " [Error] Aircraft ID already exists.\n";
                return;
            }
        }

        int typeChoice = readInteger("Select Operation Type (1. Arrival / 2. Departure): ", 1, 2);
        plane.type = (typeChoice == 1) ? "Arrival" : "Departure";

        string timeStr;
        while (true)
        {
            cout << "Enter Scheduled / Inbound Time (HH:MM): ";
            cin >> timeStr;
            int mins = convertToMinutes(timeStr);
            if (mins != -1)
            {
                plane.scheduledTime = mins;
                break;
            }
            cout << " [Error] Invalid format. Use HH:MM.\n";
        }

        plane.priority = 5;
        plane.emergency = true;
        plane.runwayTime = readInteger("Enter Runway Occupation Duration (minutes): ", 1, 60);
        plane.scheduled = false;
        plane.assignedRunway = -1;
        plane.actualTime = -1;
        plane.waitingTime = 0;

        aircraftList.push_back(plane);
        cout << "\n [ALERT] Emergency Aircraft " << plane.id << " registered with Priority 5!\n";
        cout << " Recalculating airport schedule with maximum preemption...\n";
        scheduleAircraft();
    }
}

/**
 * 5. Update Flight Delay
 */
void updateFlightDelay()
{
    cout << "\n--- UPDATE FLIGHT DELAY ---\n";
    if (aircraftList.empty())
    {
        cout << " [Info] No aircraft registered in the system.\n";
        return;
    }

    int targetId = readInteger("Enter Aircraft ID to update delay: ");
    Aircraft *targetPlane = nullptr;

    for (auto &plane : aircraftList)
    {
        if (plane.id == targetId)
        {
            targetPlane = &plane;
            break;
        }
    }

    if (!targetPlane)
    {
        cout << " [Error] Aircraft ID " << targetId << " not found.\n";
        return;
    }

    cout << " Current Scheduled Time : " << convertToTime(targetPlane->scheduledTime) << "\n";
    int delayMins = readInteger("Enter Delay in minutes (positive number): ", 1, 720);

    int oldTime = targetPlane->scheduledTime;
    targetPlane->scheduledTime += delayMins;

    cout << "\n [Success] Aircraft " << targetId << " delayed by " << delayMins << " minutes.\n";
    cout << " Old Time: " << convertToTime(oldTime) << " --> New Scheduled Time: " << convertToTime(targetPlane->scheduledTime) << "\n";
    cout << " Rescheduling operations to avoid ripple conflicts...\n";

    scheduleAircraft();
}

/**
 * 6. Detect Runway Conflicts
 * Comprehensive conflict detection engine auditing all runways.
 */
void detectRunwayConflicts()
{
    cout << "\n========================================================================================\n";
    cout << "                        RUNWAY OPERATIONAL CONFLICT DETECTOR\n";
    cout << "========================================================================================\n";
    cout << " Active Safety Separation Rule : " << safetySeparation << " minutes (" << weatherCondition << " Weather)\n";
    cout << " Auditing planned schedules across all runways...\n\n";

    vector<ConflictRecord> detectedList;

    for (auto &runway : runways)
    {
        cout << "--- Runway " << runway.id << " (" << runway.code << ") - Status: "
             << (runway.active ? "ACTIVE" : "CLOSED / MAINTENANCE") << " ---\n";

        if (!runway.active && !runway.operations.empty())
        {
            for (const auto &op : runway.operations)
            {
                ConflictRecord cr;
                cr.runwayId = runway.id;
                cr.aircraftA = op.aircraftID;
                cr.aircraftB = -1;
                cr.startA = op.startTime;
                cr.endA = op.endTime;
                cr.startB = -1;
                cr.endB = -1;
                cr.reason = "Operation assigned on CLOSED/MAINTENANCE runway";
                cr.resolved = false;
                detectedList.push_back(cr);
            }
        }

        if (runway.operations.size() < 2)
        {
            cout << " No inter-aircraft timing conflicts detected on this runway.\n\n";
            continue;
        }

        // Check consecutive operations for separation violation or time overlap
        for (size_t i = 0; i < runway.operations.size() - 1; ++i)
        {
            const auto &curr = runway.operations[i];
            const auto &next = runway.operations[i + 1];

            // Direct overlap: next starts before curr finishes
            if (next.startTime < curr.endTime)
            {
                ConflictRecord cr;
                cr.runwayId = runway.id;
                cr.aircraftA = curr.aircraftID;
                cr.aircraftB = next.aircraftID;
                cr.startA = curr.startTime;
                cr.endA = curr.endTime;
                cr.startB = next.startTime;
                cr.endB = next.endTime;
                cr.reason = "Direct Time Overlap (Runway collision hazard)";
                cr.resolved = false;
                detectedList.push_back(cr);
            }
            // Separation violation: gap is less than safety separation
            else if (next.startTime < curr.endTime + safetySeparation)
            {
                ConflictRecord cr;
                cr.runwayId = runway.id;
                cr.aircraftA = curr.aircraftID;
                cr.aircraftB = next.aircraftID;
                cr.startA = curr.startTime;
                cr.endA = curr.endTime;
                cr.startB = next.startTime;
                cr.endB = next.endTime;
                cr.reason = "Safety Separation Violation (Gap: " +
                            to_string(next.startTime - curr.endTime) + " min < Required: " +
                            to_string(safetySeparation) + " min)";
                cr.resolved = false;
                detectedList.push_back(cr);
            }
        }

        if (detectedList.empty())
        {
            cout << " Schedule is safe: Safe separation maintained for all operations.\n\n";
        }
    }

    if (!detectedList.empty())
    {
        cout << "\n [WARNING] " << detectedList.size() << " CONFLICT(S) DETECTED IN SCHEDULE:\n";
        for (const auto &cr : detectedList)
        {
            cout << " * Runway " << cr.runwayId << " Conflict: Aircraft " << cr.aircraftA
                 << " [" << convertToTime(cr.startA) << "-" << convertToTime(cr.endA) << "]";
            if (cr.aircraftB != -1)
            {
                cout << " vs Aircraft " << cr.aircraftB
                     << " [" << convertToTime(cr.startB) << "-" << convertToTime(cr.endB) << "]";
            }
            cout << " -> " << cr.reason << "\n";
        }
        cout << "\n [Recommendation] Execute Option 3 (Schedule Aircraft) to automatically resolve these conflicts.\n";
    }
    else
    {
        cout << " [Status] 0 conflicts detected in current runway operations.\n";
        cout << " All scheduled flights satisfy minimum safety separation of " << safetySeparation << " minutes.\n";
    }

    // Environmental / Runway Simulation Submenu
    cout << "\n--- CONFLICT SIMULATION & ENVIRONMENTAL SETTINGS ---\n";
    cout << "1. Simulate Weather Change (Alters Safety Separation)\n";
    cout << "2. Toggle Runway Maintenance (Close/Reopen Runway)\n";
    cout << "3. Return to Main Menu\n";
    int subChoice = readInteger("Select Option (1-3): ", 1, 3);

    if (subChoice == 1)
    {
        cout << "\nSelect Weather Condition:\n";
        cout << " 1. Clear (3 min separation)\n";
        cout << " 2. Rain (5 min separation)\n";
        cout << " 3. Heavy Rain / Fog (7 min separation)\n";
        cout << " 4. Severe Storm (10 min separation)\n";
        int wChoice = readInteger("Choose weather (1-4): ", 1, 4);

        if (wChoice == 1) { weatherCondition = "Clear"; safetySeparation = 3; }
        else if (wChoice == 2) { weatherCondition = "Rain"; safetySeparation = 5; }
        else if (wChoice == 3) { weatherCondition = "Heavy Rain / Fog"; safetySeparation = 7; }
        else if (wChoice == 4) { weatherCondition = "Severe Storm"; safetySeparation = 10; }

        cout << "\n [Update] Weather changed to: " << weatherCondition << "\n";
        cout << " Required safety separation updated to: " << safetySeparation << " minutes.\n";
        cout << " Automatically recalculating runway schedules to preserve safety...\n";
        scheduleAircraft();
    }
    else if (subChoice == 2)
    {
        cout << "\nAvailable Runways:\n";
        for (const auto &r : runways)
        {
            cout << " Runway " << r.id << " (" << r.code << ") - Current Status: "
                 << (r.active ? "ACTIVE" : "CLOSED") << "\n";
        }
        int rId = readInteger("Enter Runway ID to toggle status: ", 1, runways.size());
        for (auto &r : runways)
        {
            if (r.id == rId)
            {
                r.active = !r.active;
                cout << " Runway " << r.id << " is now " << (r.active ? "OPEN (ACTIVE)" : "CLOSED (MAINTENANCE)") << ".\n";
                cout << " Rescheduling affected flights to available runways...\n";
                scheduleAircraft();
                break;
            }
        }
    }
}

/**
 * 7. Display Runway Schedule
 */
void displayRunwaySchedule()
{
    cout << "\n========================================================================================\n";
    cout << "                          MASTER AIRPORT RUNWAY SCHEDULE\n";
    cout << "========================================================================================\n";
    cout << " Current Weather: " << weatherCondition << " | Safety Separation: " << safetySeparation << " mins\n";

    for (auto &runway : runways)
    {
        cout << "\n========================================================================================\n";
        cout << " RUNWAY " << runway.id << " (" << runway.code << ")  -  Status: "
             << (runway.active ? "[ACTIVE - OPERATIONAL]" : "[CLOSED - MAINTENANCE]") << "\n";
        cout << "========================================================================================\n";

        if (runway.operations.empty())
        {
            cout << "  No scheduled operations on this runway.\n";
            continue;
        }

        sortOperations(runway);

        cout << left << setw(6)  << "Slot#"
             << setw(12) << "Flight ID"
             << setw(14) << "Type"
             << setw(16) << "Runway Window"
             << setw(14) << "Sched Time"
             << setw(12) << "Wait Time"
             << setw(14) << "Priority"
             << setw(12) << "Emergency" << "\n";
        cout << "----------------------------------------------------------------------------------------\n";

        int slot = 1;
        for (const auto &op : runway.operations)
        {
            // Find corresponding aircraft details
            int origSched = -1;
            int priorityVal = 0;
            for (const auto &p : aircraftList)
            {
                if (p.id == op.aircraftID)
                {
                    origSched = p.scheduledTime;
                    priorityVal = p.priority;
                    break;
                }
            }

            int wait = (origSched != -1) ? (op.startTime - origSched) : 0;
            string windowStr = convertToTime(op.startTime) + " - " + convertToTime(op.endTime);

            cout << left << setw(6)  << slot++
                 << setw(12) << op.aircraftID
                 << setw(14) << op.type
                 << setw(16) << windowStr
                 << setw(14) << convertToTime(origSched)
                 << setw(12) << (to_string(wait) + " mins")
                 << setw(14) << priorityVal
                 << setw(12) << (op.isEmergency ? "YES [!]" : "NO") << "\n";
        }
    }
    cout << "\n========================================================================================\n";
}

/**
 * 8. Generate Report
 * Comprehensive ATC Decision Support, Efficiency Metrics and Analytics.
 */
void generateReport()
{
    cout << "\n========================================================================================\n";
    cout << "                    ALLIANCE AIRPORT ATC OPERATIONAL REPORT\n";
    cout << "========================================================================================\n";

    int activeRunwayCount = 0;
    for (const auto &r : runways)
    {
        if (r.active) activeRunwayCount++;
    }

    int scheduledCount = 0;
    int waitingCount = 0;
    int emergencyCount = 0;
    int totalWaitTime = 0;
    int maxWaitTime = 0;
    int maxWaitFlightId = -1;
    int totalRunwayOccupationTime = 0;

    for (const auto &p : aircraftList)
    {
        if (p.emergency) emergencyCount++;

        if (p.scheduled)
        {
            scheduledCount++;
            totalWaitTime += p.waitingTime;
            totalRunwayOccupationTime += p.runwayTime;

            if (p.waitingTime > maxWaitTime)
            {
                maxWaitTime = p.waitingTime;
                maxWaitFlightId = p.id;
            }
        }
        else
        {
            waitingCount++;
        }
    }

    double avgWaitTime = (scheduledCount > 0) ? ((double)totalWaitTime / scheduledCount) : 0.0;

    // Runway utilization calculation over an operational day (12 active peak hours = 720 minutes per active runway)
    int dailyOperatingMinutes = activeRunwayCount * 720;
    double runwayUtilization = (dailyOperatingMinutes > 0) ?
        (((double)totalRunwayOccupationTime / dailyOperatingMinutes) * 100.0) : 0.0;

    cout << left << setw(35) << "System Status Metric" << "Value\n";
    cout << "----------------------------------------------------------------------------------------\n";
    cout << left << setw(35) << "Current Weather Condition" << ": " << weatherCondition << "\n";
    cout << left << setw(35) << "Mandatory Safety Separation" << ": " << safetySeparation << " minutes\n";
    cout << left << setw(35) << "Total Configured Runways" << ": " << runways.size() << "\n";
    cout << left << setw(35) << "Active Runways" << ": " << activeRunwayCount << " / " << runways.size() << "\n";
    cout << left << setw(35) << "Total Registered Aircraft" << ": " << aircraftList.size() << "\n";
    cout << left << setw(35) << "Successfully Scheduled Aircraft" << ": " << scheduledCount << "\n";
    cout << left << setw(35) << "Pending / Waiting Aircraft" << ": " << waitingCount << "\n";
    cout << left << setw(35) << "Emergency Flights Handled" << ": " << emergencyCount << "\n";
    cout << "----------------------------------------------------------------------------------------\n";
    cout << " EFFICIENCY & PERFORMANCE INDICATORS\n";
    cout << "----------------------------------------------------------------------------------------\n";
    cout << left << setw(35) << "Total Operational Delay" << ": " << totalWaitTime << " minutes\n";
    cout << left << setw(35) << "Average Aircraft Waiting Time" << ": " << fixed << setprecision(2) << avgWaitTime << " minutes\n";
    cout << left << setw(35) << "Maximum Waiting Time" << ": " << maxWaitTime << " minutes";
    if (maxWaitFlightId != -1) cout << " (Flight ID: " << maxWaitFlightId << ")";
    cout << "\n";
    cout << left << setw(35) << "Runway Capacity Utilization" << ": " << fixed << setprecision(2) << runwayUtilization << " %\n";
    cout << left << setw(35) << "Conflicts Resolved by Algorithm" << ": " << conflictsResolvedCount << "\n";
    cout << "----------------------------------------------------------------------------------------\n";

    cout << " PER-RUNWAY BREAKDOWN\n";
    cout << "----------------------------------------------------------------------------------------\n";
    for (const auto &r : runways)
    {
        int runwayMins = 0;
        for (const auto &op : r.operations)
        {
            runwayMins += (op.endTime - op.startTime);
        }
        double rUtil = (720 > 0) ? (((double)runwayMins / 720.0) * 100.0) : 0.0;
        cout << " Runway " << r.id << " (" << r.code << ") : "
             << setw(2) << r.operations.size() << " operations | "
             << setw(3) << runwayMins << " mins occupied | Utilization: "
             << fixed << setprecision(1) << rUtil << " %\n";
    }

    cout << "----------------------------------------------------------------------------------------\n";
    cout << " ATC DECISION SUPPORT RECOMMENDATION:\n";
    if (waitingCount > 0)
    {
        cout << " [!] High Traffic Congestion: Open closed runways or reduce separation if weather permits.\n";
    }
    else if (avgWaitTime > 15.0)
    {
        cout << " [!] Elevated Waiting Times: Consider staggered slot allocation for departures.\n";
    }
    else
    {
        cout << " [OK] Runway Operations Optimal: Safe separation maintained and delays within acceptable threshold.\n";
    }
    cout << "========================================================================================\n";
}

// ============================================================
// 8. SAMPLE DATA INITIALIZATION (DEMO SUITE)
// ============================================================

/**
 * Loads realistic flight benchmark data for fast demonstration and testing.
 */
void loadSampleDemonstrationData()
{
    aircraftList.clear();

    // ID, Type, Scheduled Time (HH:MM), Priority (1-5), Emergency, Occupation Time (mins)
    aircraftList.push_back({101, "Arrival",   convertToMinutes("08:00"), 3, false, 5, false, -1, -1, 0});
    aircraftList.push_back({102, "Departure", convertToMinutes("08:05"), 2, false, 4, false, -1, -1, 0});
    aircraftList.push_back({103, "Arrival",   convertToMinutes("08:10"), 4, false, 5, false, -1, -1, 0});
    aircraftList.push_back({104, "Departure", convertToMinutes("08:10"), 2, false, 4, false, -1, -1, 0}); // Simultaneous clash
    aircraftList.push_back({105, "Arrival",   convertToMinutes("08:15"), 5, true,  6, false, -1, -1, 0}); // Emergency flight
    aircraftList.push_back({106, "Departure", convertToMinutes("08:20"), 3, false, 4, false, -1, -1, 0});
    aircraftList.push_back({107, "Arrival",   convertToMinutes("08:25"), 1, false, 5, false, -1, -1, 0}); // Low priority cargo
    aircraftList.push_back({108, "Departure", convertToMinutes("08:30"), 4, false, 4, false, -1, -1, 0});

    cout << "\n [Success] Loaded 8 realistic demonstration aircraft flights into the system:\n";
    cout << " - Flight 105: Critical Emergency Inbound (08:15)\n";
    cout << " - Flight 103 & 104: Simultaneous Slot Request Conflict (08:10)\n";
    cout << " - Mixture of Arrivals and Departures across priority levels (1 - 5)\n";
    cout << " Schedule ready for testing.\n";
}

// ============================================================
// 9. SYSTEM INITIALIZATION & MAIN MENU LOOP
// ============================================================

void initializeSystem()
{
    runways.clear();

    Runway r1;
    r1.id = 1;
    r1.code = "09L (Primary)";
    r1.active = true;

    Runway r2;
    r2.id = 2;
    r2.code = "09R (Secondary)";
    r2.active = true;

    Runway r3;
    r3.id = 3;
    r3.code = "27  (Crosswind)";
    r3.active = true;

    runways.push_back(r1);
    runways.push_back(r2);
    runways.push_back(r3);
}

void displayMenu()
{
    cout << "\n";
    cout << "============================================================\n";
    cout << "          INTELLIGENT AIRPORT RUNWAY SYSTEM\n";
    cout << "        Alliance School of Advanced Computing\n";
    cout << "============================================================\n";
    cout << "========== AIRPORT RUNWAY SCHEDULING SYSTEM ==========\n";
    cout << "\n";
    cout << "1. Add Aircraft\n";
    cout << "2. Display Waiting Aircraft\n";
    cout << "3. Schedule Aircraft\n";
    cout << "4. Handle Emergency Aircraft\n";
    cout << "5. Update Flight Delay\n";
    cout << "6. Detect Runway Conflicts\n";
    cout << "7. Display Runway Schedule\n";
    cout << "8. Generate Report\n";
    cout << "9. Exit\n";
    cout << "------------------------------------------------------\n";
    cout << "(Tip: Enter '0' at menu to preload sample demo flights)\n";
}

int main()
{
    initializeSystem();

    cout << "\n============================================================\n";
    cout << "  WELCOME TO THE AIRPORT RUNWAY OPERATION & SCHEDULING SYSTEM\n";
    cout << "  Alliance University - Alliance School of Advanced Computing\n";
    cout << "  Team: Diyah B, Raju ram, Ajay Krishna S, Shazeb, Gaurav\n";
    cout << "============================================================\n";

    int choice;
    do
    {
        displayMenu();
        choice = readInteger("Enter your choice (1 - 9): ", 0, 9);

        switch (choice)
        {
            case 0:
                loadSampleDemonstrationData();
                break;

            case 1:
                addAircraft();
                break;

            case 2:
                displayWaitingAircraft();
                break;

            case 3:
                scheduleAircraft();
                break;

            case 4:
                handleEmergencyAircraft();
                break;

            case 5:
                updateFlightDelay();
                break;

            case 6:
                detectRunwayConflicts();
                break;

            case 7:
                displayRunwaySchedule();
                break;

            case 8:
                generateReport();
                break;

            case 9:
                cout << "\n============================================================\n";
                cout << "  Thank you for using the Airport Runway Scheduling System!\n";
                cout << "  Alliance School of Advanced Computing.\n";
                cout << "============================================================\n\n";
                break;

            default:
                cout << " [Error] Invalid choice. Please choose from 1 to 9.\n";
                break;
        }

    } while (choice != 9);

    return 0;
}

