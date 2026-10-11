#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

using namespace std;

// ============================================================
// 1. DATA STRUCTURES (CO2, CO3, CO4)
// ============================================================

struct Aircraft
{
    int id;
    string type;                // "Arrival" or "Departure"
    int scheduledTime;          // Minutes from 00:00
    int priority;               // 1 to 5
    bool emergency;
    int runwayTime;             // Minutes
    bool scheduled;
    int assignedRunway;         // -1 if unassigned
    int actualTime;             // Minutes
    int waitingTime;            // Minutes
};

struct Operation
{
    int aircraftID;
    string type;
    int startTime;
    int endTime;
    bool isEmergency;
};

struct Runway
{
    int id;
    string code;
    bool active;
    vector<Operation> operations;
};

struct ConflictItem
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
// 2. GLOBAL ENGINE STATE
// ============================================================

vector<Aircraft> aircraftList;
vector<Runway> runways;
string weatherCondition = "Clear";
int safetySeparation = 3;
int conflictsResolvedCount = 0;
const string STATE_FILE = "airport_state.json";

// ============================================================
// 3. TIME HELPERS
// ============================================================

int convertToMinutes(const string &timeStr)
{
    if (timeStr.length() != 5 || timeStr[2] != ':')
        return -1;
    int hour = (timeStr[0] - '0') * 10 + (timeStr[1] - '0');
    int minute = (timeStr[3] - '0') * 10 + (timeStr[4] - '0');
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
        return -1;
    return hour * 60 + minute;
}

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

// ============================================================
// 4. CUSTOM MAX-HEAP PRIORITY QUEUE (CO2)
// ============================================================

bool higherPriority(const Aircraft &a, const Aircraft &b)
{
    if (a.emergency != b.emergency)
        return a.emergency;
    if (a.priority != b.priority)
        return a.priority > b.priority;
    if (a.scheduledTime != b.scheduledTime)
        return a.scheduledTime < b.scheduledTime;
    return a.runwayTime < b.runwayTime;
}

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
            else break;
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
            else break;
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
        if (heap.empty()) throw runtime_error("Empty priority queue");
        Aircraft top = heap[0];
        heap[0] = heap.back();
        heap.pop_back();
        if (!heap.empty()) heapifyDown(0);
        return top;
    }

    bool empty() const { return heap.empty(); }
    int size() const { return heap.size(); }
};

// ============================================================
// 5. RUNWAY SCHEDULING & CONFLICT LOGIC (CO3)
// ============================================================

void sortOperations(Runway &runway)
{
    sort(runway.operations.begin(), runway.operations.end(),
         [](const Operation &a, const Operation &b) {
             return a.startTime < b.startTime;
         });
}

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
                proposedTime = op.endTime + safetySeparation;
                found = false;
                break;
            }
        }
    }
    return proposedTime;
}

void scheduleAircraft()
{
    for (auto &r : runways)
    {
        r.operations.clear();
    }

    for (auto &p : aircraftList)
    {
        p.scheduled = false;
        p.assignedRunway = -1;
        p.actualTime = -1;
        p.waitingTime = 0;
    }

    CustomPriorityQueue pq;
    for (const auto &p : aircraftList)
    {
        pq.push(p);
    }

    int detectedConflictCount = 0;

    while (!pq.empty())
    {
        Aircraft current = pq.pop();
        int bestRunwayIdx = -1;
        int bestStartTime = -1;
        int smallestWaiting = 999999;

        for (size_t r = 0; r < runways.size(); ++r)
        {
            if (!runways[r].active) continue;

            int availableTime = findAvailableTime(runways[r], current.scheduledTime, current.runwayTime);
            int waiting = availableTime - current.scheduledTime;

            if (waiting < smallestWaiting)
            {
                smallestWaiting = waiting;
                bestRunwayIdx = r;
                bestStartTime = availableTime;
            }
        }

        if (bestRunwayIdx != -1)
        {
            if (smallestWaiting > 0) detectedConflictCount++;
            Operation op;
            op.aircraftID = current.id;
            op.type = current.type;
            op.startTime = bestStartTime;
            op.endTime = bestStartTime + current.runwayTime;
            op.isEmergency = current.emergency;

            runways[bestRunwayIdx].operations.push_back(op);
            sortOperations(runways[bestRunwayIdx]);

            for (auto &p : aircraftList)
            {
                if (p.id == current.id)
                {
                    p.scheduled = true;
                    p.assignedRunway = runways[bestRunwayIdx].id;
                    p.actualTime = bestStartTime;
                    p.waitingTime = smallestWaiting;
                    break;
                }
            }
        }
    }

    conflictsResolvedCount += detectedConflictCount;
}

vector<ConflictItem> auditConflicts()
{
    vector<ConflictItem> list;
    for (const auto &runway : runways)
    {
        if (!runway.active && !runway.operations.empty())
        {
            for (const auto &op : runway.operations)
            {
                ConflictItem ci;
                ci.runwayId = runway.id;
                ci.aircraftA = op.aircraftID;
                ci.aircraftB = -1;
                ci.startA = op.startTime;
                ci.endA = op.endTime;
                ci.startB = -1;
                ci.endB = -1;
                ci.reason = "Operation assigned on CLOSED/MAINTENANCE runway";
                ci.resolved = false;
                list.push_back(ci);
            }
        }

        if (runway.operations.size() < 2) continue;

        for (size_t i = 0; i < runway.operations.size() - 1; ++i)
        {
            const auto &curr = runway.operations[i];
            const auto &next = runway.operations[i + 1];

            if (next.startTime < curr.endTime)
            {
                ConflictItem ci;
                ci.runwayId = runway.id;
                ci.aircraftA = curr.aircraftID;
                ci.aircraftB = next.aircraftID;
                ci.startA = curr.startTime;
                ci.endA = curr.endTime;
                ci.startB = next.startTime;
                ci.endB = next.endTime;
                ci.reason = "Direct Time Overlap (Runway collision risk)";
                ci.resolved = false;
                list.push_back(ci);
            }
            else if (next.startTime < curr.endTime + safetySeparation)
            {
                ConflictItem ci;
                ci.runwayId = runway.id;
                ci.aircraftA = curr.aircraftID;
                ci.aircraftB = next.aircraftID;
                ci.startA = curr.startTime;
                ci.endA = curr.endTime;
                ci.startB = next.startTime;
                ci.endB = next.endTime;
                ci.reason = "Safety separation gap violated (" +
                            to_string(next.startTime - curr.endTime) + " min < " +
                            to_string(safetySeparation) + " min required)";
                ci.resolved = false;
                list.push_back(ci);
            }
        }
    }
    return list;
}

// ============================================================
// 6. DEFAULT DEMO INITIALIZATION
// ============================================================

void initializeDefaultData()
{
    runways.clear();
    Runway r1{1, "09L (Primary)", true, {}};
    Runway r2{2, "09R (Secondary)", true, {}};
    Runway r3{3, "27 (Crosswind)", true, {}};
    runways.push_back(r1);
    runways.push_back(r2);
    runways.push_back(r3);

    aircraftList.clear();
    aircraftList.push_back({101, "Arrival",   convertToMinutes("08:00"), 3, false, 5, false, -1, -1, 0});
    aircraftList.push_back({102, "Departure", convertToMinutes("08:05"), 2, false, 4, false, -1, -1, 0});
    aircraftList.push_back({103, "Arrival",   convertToMinutes("08:10"), 4, false, 5, false, -1, -1, 0});
    aircraftList.push_back({104, "Departure", convertToMinutes("08:10"), 2, false, 4, false, -1, -1, 0});
    aircraftList.push_back({105, "Arrival",   convertToMinutes("08:15"), 5, true,  6, false, -1, -1, 0}); // Emergency
    aircraftList.push_back({106, "Departure", convertToMinutes("08:20"), 3, false, 4, false, -1, -1, 0});
    aircraftList.push_back({107, "Arrival",   convertToMinutes("08:25"), 1, false, 5, false, -1, -1, 0});
    aircraftList.push_back({108, "Departure", convertToMinutes("08:30"), 4, false, 4, false, -1, -1, 0});

    weatherCondition = "Clear";
    safetySeparation = 3;
    conflictsResolvedCount = 0;

    scheduleAircraft();
}

// ============================================================
// 7. JSON STATE SERIALIZATION & FILE PERSISTENCE
// ============================================================

string escapeJson(const string &s)
{
    ostringstream o;
    for (char c : s)
    {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else o << c;
    }
    return o.str();
}

string serializeToJson()
{
    ostringstream oss;
    oss << "{\n";
    oss << "  \"weather\": \"" << escapeJson(weatherCondition) << "\",\n";
    oss << "  \"safetySeparation\": " << safetySeparation << ",\n";
    oss << "  \"conflictsResolved\": " << conflictsResolvedCount << ",\n";

    // Runways
    oss << "  \"runways\": [\n";
    for (size_t i = 0; i < runways.size(); ++i)
    {
        const auto &r = runways[i];
        oss << "    {\n";
        oss << "      \"id\": " << r.id << ",\n";
        oss << "      \"code\": \"" << escapeJson(r.code) << "\",\n";
        oss << "      \"active\": " << (r.active ? "true" : "false") << ",\n";
        oss << "      \"operations\": [\n";
        for (size_t j = 0; j < r.operations.size(); ++j)
        {
            const auto &op = r.operations[j];
            oss << "        {\n";
            oss << "          \"aircraftID\": " << op.aircraftID << ",\n";
            oss << "          \"type\": \"" << escapeJson(op.type) << "\",\n";
            oss << "          \"startTime\": " << op.startTime << ",\n";
            oss << "          \"endTime\": " << op.endTime << ",\n";
            oss << "          \"startTimeStr\": \"" << convertToTime(op.startTime) << "\",\n";
            oss << "          \"endTimeStr\": \"" << convertToTime(op.endTime) << "\",\n";
            oss << "          \"isEmergency\": " << (op.isEmergency ? "true" : "false") << "\n";
            oss << "        }" << (j + 1 < r.operations.size() ? "," : "") << "\n";
        }
        oss << "      ]\n";
        oss << "    }" << (i + 1 < runways.size() ? "," : "") << "\n";
    }
    oss << "  ],\n";

    // Aircraft List
    oss << "  \"aircraft\": [\n";
    for (size_t i = 0; i < aircraftList.size(); ++i)
    {
        const auto &p = aircraftList[i];
        oss << "    {\n";
        oss << "      \"id\": " << p.id << ",\n";
        oss << "      \"type\": \"" << escapeJson(p.type) << "\",\n";
        oss << "      \"scheduledTime\": " << p.scheduledTime << ",\n";
        oss << "      \"scheduledTimeStr\": \"" << convertToTime(p.scheduledTime) << "\",\n";
        oss << "      \"priority\": " << p.priority << ",\n";
        oss << "      \"emergency\": " << (p.emergency ? "true" : "false") << ",\n";
        oss << "      \"runwayTime\": " << p.runwayTime << ",\n";
        oss << "      \"scheduled\": " << (p.scheduled ? "true" : "false") << ",\n";
        oss << "      \"assignedRunway\": " << p.assignedRunway << ",\n";
        oss << "      \"actualTime\": " << p.actualTime << ",\n";
        oss << "      \"actualTimeStr\": \"" << convertToTime(p.actualTime) << "\",\n";
        oss << "      \"waitingTime\": " << p.waitingTime << "\n";
        oss << "    }" << (i + 1 < aircraftList.size() ? "," : "") << "\n";
    }
    oss << "  ],\n";

    // Detected Conflicts
    vector<ConflictItem> conflicts = auditConflicts();
    oss << "  \"conflicts\": [\n";
    for (size_t i = 0; i < conflicts.size(); ++i)
    {
        const auto &c = conflicts[i];
        oss << "    {\n";
        oss << "      \"runwayId\": " << c.runwayId << ",\n";
        oss << "      \"aircraftA\": " << c.aircraftA << ",\n";
        oss << "      \"aircraftB\": " << c.aircraftB << ",\n";
        oss << "      \"startAStr\": \"" << convertToTime(c.startA) << "\",\n";
        oss << "      \"endAStr\": \"" << convertToTime(c.endA) << "\",\n";
        oss << "      \"startBStr\": \"" << convertToTime(c.startB) << "\",\n";
        oss << "      \"endBStr\": \"" << convertToTime(c.endB) << "\",\n";
        oss << "      \"reason\": \"" << escapeJson(c.reason) << "\"\n";
        oss << "    }" << (i + 1 < conflicts.size() ? "," : "") << "\n";
    }
    oss << "  ],\n";

    // Operational Analytics Metrics
    int activeRunwaysCount = 0;
    for (const auto &r : runways) if (r.active) activeRunwaysCount++;

    int scheduledCount = 0;
    int waitingCount = 0;
    int emergencyCount = 0;
    int totalWaitTime = 0;
    int maxWaitTime = 0;
    int maxWaitFlightId = -1;
    int totalRunwayOccMins = 0;

    for (const auto &p : aircraftList)
    {
        if (p.emergency) emergencyCount++;
        if (p.scheduled)
        {
            scheduledCount++;
            totalWaitTime += p.waitingTime;
            totalRunwayOccMins += p.runwayTime;
            if (p.waitingTime > maxWaitTime)
            {
                maxWaitTime = p.waitingTime;
                maxWaitFlightId = p.id;
            }
        }
        else waitingCount++;
    }

    double avgWait = (scheduledCount > 0) ? ((double)totalWaitTime / scheduledCount) : 0.0;
    int operatingCapMins = activeRunwaysCount * 720;
    double utilization = (operatingCapMins > 0) ? (((double)totalRunwayOccMins / operatingCapMins) * 100.0) : 0.0;

    oss << "  \"metrics\": {\n";
    oss << "    \"totalAircraft\": " << aircraftList.size() << ",\n";
    oss << "    \"scheduledAircraft\": " << scheduledCount << ",\n";
    oss << "    \"waitingAircraft\": " << waitingCount << ",\n";
    oss << "    \"emergencyCount\": " << emergencyCount << ",\n";
    oss << "    \"activeRunways\": " << activeRunwaysCount << ",\n";
    oss << "    \"totalWaitTime\": " << totalWaitTime << ",\n";
    oss << "    \"avgWaitTime\": " << fixed << setprecision(2) << avgWait << ",\n";
    oss << "    \"maxWaitTime\": " << maxWaitTime << ",\n";
    oss << "    \"maxWaitFlightId\": " << maxWaitFlightId << ",\n";
    oss << "    \"runwayUtilization\": " << fixed << setprecision(2) << utilization << ",\n";
    oss << "    \"conflictsDetected\": " << conflicts.size() << "\n";
    oss << "  }\n";
    oss << "}\n";

    return oss.str();
}

void saveStateToFile()
{
    ofstream outFile(STATE_FILE);
    if (outFile.is_open())
    {
        outFile << serializeToJson();
        outFile.close();
    }
}

// Simple key extraction from JSON file
string extractStringValue(const string &content, const string &key)
{
    size_t pos = content.find("\"" + key + "\"");
    if (pos == string::npos) return "";
    size_t colon = content.find(":", pos);
    size_t quoteStart = content.find("\"", colon);
    if (quoteStart == string::npos) return "";
    size_t quoteEnd = content.find("\"", quoteStart + 1);
    if (quoteEnd == string::npos) return "";
    return content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

int extractIntValue(const string &content, const string &key, int defaultVal = 0)
{
    size_t pos = content.find("\"" + key + "\"");
    if (pos == string::npos) return defaultVal;
    size_t colon = content.find(":", pos);
    size_t comma = content.find_first_of(",}\n", colon);
    string sub = content.substr(colon + 1, comma - colon - 1);
    stringstream ss(sub);
    int val;
    if (ss >> val) return val;
    return defaultVal;
}

bool loadStateFromFile()
{
    ifstream inFile(STATE_FILE);
    if (!inFile.is_open()) return false;

    stringstream buffer;
    buffer << inFile.rdbuf();
    string content = buffer.str();
    inFile.close();

    if (content.empty()) return false;

    weatherCondition = extractStringValue(content, "weather");
    if (weatherCondition.empty()) weatherCondition = "Clear";
    safetySeparation = extractIntValue(content, "safetySeparation", 3);
    conflictsResolvedCount = extractIntValue(content, "conflictsResolved", 0);

    // Parse Aircraft array
    aircraftList.clear();
    size_t acArrayStart = content.find("\"aircraft\"");
    if (acArrayStart != string::npos)
    {
        size_t acOpen = content.find("[", acArrayStart);
        size_t acClose = content.find("]", acOpen);
        string acSection = content.substr(acOpen, acClose - acOpen + 1);

        size_t cur = 0;
        while (true)
        {
            size_t objStart = acSection.find("{", cur);
            if (objStart == string::npos) break;
            size_t objEnd = acSection.find("}", objStart);
            if (objEnd == string::npos) break;

            string objStr = acSection.substr(objStart, objEnd - objStart + 1);
            Aircraft p;
            p.id = extractIntValue(objStr, "id", 0);
            p.type = extractStringValue(objStr, "type");
            p.scheduledTime = extractIntValue(objStr, "scheduledTime", 0);
            p.priority = extractIntValue(objStr, "priority", 1);
            p.emergency = (objStr.find("\"emergency\": true") != string::npos);
            p.runwayTime = extractIntValue(objStr, "runwayTime", 5);
            p.scheduled = (objStr.find("\"scheduled\": true") != string::npos);
            p.assignedRunway = extractIntValue(objStr, "assignedRunway", -1);
            p.actualTime = extractIntValue(objStr, "actualTime", -1);
            p.waitingTime = extractIntValue(objStr, "waitingTime", 0);

            if (p.id > 0) aircraftList.push_back(p);
            cur = objEnd + 1;
        }
    }

    // Parse Runways array
    runways.clear();
    size_t rwArrayStart = content.find("\"runways\"");
    if (rwArrayStart != string::npos)
    {
        size_t rwOpen = content.find("[", rwArrayStart);
        size_t rwClose = content.find("\"aircraft\"", rwOpen);
        string rwSection = content.substr(rwOpen, rwClose - rwOpen);

        // Standard 3 runways if not found
        Runway r1{1, "09L (Primary)", true, {}};
        Runway r2{2, "09R (Secondary)", true, {}};
        Runway r3{3, "27 (Crosswind)", true, {}};

        if (rwSection.find("\"id\": 1") != string::npos && rwSection.find("\"active\": false") != string::npos)
            r1.active = false;
        if (rwSection.find("\"id\": 2") != string::npos && rwSection.find("\"active\": false") != string::npos)
            r2.active = false;
        if (rwSection.find("\"id\": 3") != string::npos && rwSection.find("\"active\": false") != string::npos)
            r3.active = false;

        runways.push_back(r1);
        runways.push_back(r2);
        runways.push_back(r3);
    }
    else
    {
        Runway r1{1, "09L (Primary)", true, {}};
        Runway r2{2, "09R (Secondary)", true, {}};
        Runway r3{3, "27 (Crosswind)", true, {}};
        runways.push_back(r1);
        runways.push_back(r2);
        runways.push_back(r3);
    }

    return true;
}

// ============================================================
// 8. MAIN CLI DISPATCHER
// ============================================================

int main(int argc, char *argv[])
{
    if (!loadStateFromFile())
    {
        initializeDefaultData();
        saveStateToFile();
    }

    if (argc < 2)
    {
        cout << serializeToJson();
        return 0;
    }

    string command = argv[1];

    if (command == "get_state")
    {
        cout << serializeToJson();
    }
    else if (command == "reset_demo")
    {
        initializeDefaultData();
        saveStateToFile();
        cout << serializeToJson();
    }
    else if (command == "schedule")
    {
        scheduleAircraft();
        saveStateToFile();
        cout << serializeToJson();
    }
    else if (command == "add_aircraft")
    {
        // args: id, type, timeStr, priority, emergency (0/1), runwayTime
        if (argc >= 8)
        {
            Aircraft p;
            p.id = stoi(argv[2]);
            p.type = argv[3];
            p.scheduledTime = convertToMinutes(argv[4]);
            p.priority = stoi(argv[5]);
            p.emergency = (stoi(argv[6]) == 1);
            p.runwayTime = stoi(argv[7]);
            p.scheduled = false;
            p.assignedRunway = -1;
            p.actualTime = -1;
            p.waitingTime = 0;

            if (p.emergency) p.priority = 5;

            // Remove existing duplicate if any
            aircraftList.erase(remove_if(aircraftList.begin(), aircraftList.end(),
                                         [&](const Aircraft &x) { return x.id == p.id; }),
                               aircraftList.end());

            aircraftList.push_back(p);
            scheduleAircraft();
            saveStateToFile();
        }
        cout << serializeToJson();
    }
    else if (command == "emergency")
    {
        if (argc >= 3)
        {
            int targetId = stoi(argv[2]);
            for (auto &p : aircraftList)
            {
                if (p.id == targetId)
                {
                    p.emergency = true;
                    p.priority = 5;
                    break;
                }
            }
            scheduleAircraft();
            saveStateToFile();
        }
        cout << serializeToJson();
    }
    else if (command == "delay")
    {
        if (argc >= 4)
        {
            int targetId = stoi(argv[2]);
            int delayMins = stoi(argv[3]);
            for (auto &p : aircraftList)
            {
                if (p.id == targetId)
                {
                    p.scheduledTime += delayMins;
                    break;
                }
            }
            scheduleAircraft();
            saveStateToFile();
        }
        cout << serializeToJson();
    }
    else if (command == "weather")
    {
        if (argc >= 3)
        {
            string w = argv[2];
            if (w == "Clear") { weatherCondition = "Clear"; safetySeparation = 3; }
            else if (w == "Rain") { weatherCondition = "Rain"; safetySeparation = 5; }
            else if (w == "HeavyRain") { weatherCondition = "Heavy Rain / Fog"; safetySeparation = 7; }
            else if (w == "Storm") { weatherCondition = "Severe Storm"; safetySeparation = 10; }
            scheduleAircraft();
            saveStateToFile();
        }
        cout << serializeToJson();
    }
    else if (command == "toggle_runway")
    {
        if (argc >= 3)
        {
            int rId = stoi(argv[2]);
            for (auto &r : runways)
            {
                if (r.id == rId)
                {
                    r.active = !r.active;
                    break;
                }
            }
            scheduleAircraft();
            saveStateToFile();
        }
        cout << serializeToJson();
    }
    else if (command == "delete_aircraft")
    {
        if (argc >= 3)
        {
            int targetId = stoi(argv[2]);
            aircraftList.erase(remove_if(aircraftList.begin(), aircraftList.end(),
                                         [&](const Aircraft &x) { return x.id == targetId; }),
                               aircraftList.end());
            scheduleAircraft();
            saveStateToFile();
        }
        cout << serializeToJson();
    }
    else
    {
        cout << serializeToJson();
    }

    return 0;
}

