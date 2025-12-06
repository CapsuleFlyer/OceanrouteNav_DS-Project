#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>

//extern bool showOnlyActiveRoutes;
//extern string activeCompanyFilter;

// ==== BEGIN: Custom STL Replacements ====
template<typename T, int N = 256> struct SimpleVector {
    T data[N];
    int sz;
    SimpleVector() : sz(0) {}
    void push_back(const T& v) { if (sz < N) data[sz++] = v; }
    bool empty() {
        if (sz == 0) {
            return true;
        }

        if (N == 0) {
            return true;
        }

        return false;
    }
    void clear() { sz = 0; }
    int size() const { return sz; }
    T& operator[](int i) { return data[i]; }
    const T& operator[](int i) const { return data[i]; }
    T* begin() { return data; }
    T* end() { return data + sz; }
};

template<typename T, int N = 256> struct SimpleQueue {
    T data[N];
    int front, back;
    SimpleQueue() : front(0), back(0) {}
    void push(const T& v) { if ((back + 1) % N != front) { data[back] = v; back = (back + 1) % N; } }
    void pop() { if (!empty()) front = (front + 1) % N; }
    T& top() { return data[front]; }
    bool empty() const { return front == back; }
};

template<typename T, int N = 256> struct SimplePriorityQueue {
    T data[N];
    int sz;
    SimplePriorityQueue() : sz(0) {}
    void push(const T& v) {
        int i = sz++;
        data[i] = v;
        while (i > 0 && data[(i - 1) / 2] > data[i]) {
            T tmp = data[i]; data[i] = data[(i - 1) / 2]; data[(i - 1) / 2] = tmp;
            i = (i - 1) / 2;
        }
    }
    void pop() {
        if (sz == 0) return;
        data[0] = data[--sz];
        int i = 0;
        while (2 * i + 1 < sz) {
            int j = 2 * i + 1;
            if (j + 1 < sz && data[j + 1] < data[j]) j++;
            if (data[i] < data[j]) break;
            T tmp = data[i]; data[i] = data[j]; data[j] = tmp;
            i = j;
        }
    }
    T& top() { return data[0]; }
    bool empty() const { return sz == 0; }
};

template<typename K, typename V, int N = 256> struct SimpleMap {
    K keys[N];
    V values[N];
    int sz;
    SimpleMap() : sz(0) {}
    V& operator[](const K& k) {
        for (int i = 0; i < sz; ++i) if (keys[i] == k) return values[i];
        keys[sz] = k; values[sz] = V(); return values[sz++];
    }
    struct iterator {
        K* k; V* v;
        iterator(K* kk, V* vv) : k(kk), v(vv) {}
        iterator& operator++() { ++k; ++v; return *this; }
        bool operator!=(const iterator& o) const { return k != o.k; }
        std::pair<K, V> operator*() const { return { *k, *v }; }
    };
    iterator begin() { return iterator(keys, values); }
    iterator end() { return iterator(keys + sz, values + sz); }
    int size() const { return sz; }
    bool contains(const K& k) const { for (int i = 0; i < sz; ++i) if (keys[i] == k) return true; return false; }
    int find(const K& k) const { for (int i = 0; i < sz; ++i) if (keys[i] == k) return i; return -1; }
};

inline int my_min(int a, int b) { return a < b ? a : b; }
inline int my_max(int a, int b) { return a > b ? a : b; }
inline float my_min(float a, float b) { return a < b ? a : b; }
inline float my_max(float a, float b) { return a > b ? a : b; }
inline char my_tolower(char c) { return (c >= 'A' && c <= 'Z') ? (c + 32) : c; }
inline bool my_isalnum(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }
// ==== END: Custom STL Replacements ====
#define TOTALNUMBEROFPORTS 40
#define MAX_COST 999999999

using namespace std;



struct graphEdgeRoute {
    string destinationName;
    string date;
    string departureTime;
    string arrivalTime;
    int voyageCost;
    string shippingCompanyName;
    graphEdgeRoute* next;

    graphEdgeRoute() { // TODO: figure out if i need to call the default constructor on destinationNode over here
        this->destinationName = "";
        this->date = "";
        this->departureTime = "";
        this->arrivalTime = "";
        this->voyageCost = 0;
        this->shippingCompanyName = "";
        this->next = nullptr;
    }

    graphEdgeRoute(string destinationName, string date, string departureTime, string arrivalTime, int voyageCost, string shippingCompanyName) {
        this->destinationName = destinationName;
        this->date = date;
        this->departureTime = departureTime;
        this->arrivalTime = arrivalTime;
        this->voyageCost = voyageCost;
        this->shippingCompanyName = shippingCompanyName;
        this->next = nullptr;
    }
};


/*
struct WaitingShip {
    string company;
    long long arrivalTime;      // minutes since simulation start
    long long serviceEndTime;   // when it finishes docking
    WaitingShip() : arrivalTime(0), serviceEndTime(0) {}
};
*/
// DELETE THESE TWO STRUCTS:
// struct WaitingShip { ... }
// struct WaitingShipGraph { ... }

// REPLACE WITH THIS ONE â€” ONLY ONE STRUCT
struct WaitingShip {
    string company;
    long long arrivalTime = 0;
    long long serviceStartTime = -1;
    long long serviceEndTime = -1;
    int dockSlot = -1;  // -1 = waiting, 0-2 = docked

    WaitingShip() = default;
    WaitingShip(string c, long long at) : company(c), arrivalTime(at), serviceStartTime(-1), serviceEndTime(-1), dockSlot(-1) {}
};


struct graphVerticePort {
    string portName; // can be both source and destination
    int dailyPortCharge;
    // Queue shipManagement; // TODO> add this as a queue for ship management by making your own priority queue(heap)
    graphEdgeRoute* routeHead;
    sf::Vector2f position; // For SFML rendering
    float latitude; // optional
    float longitude; // optional
    bool hasCoordinates;
    SimpleVector<WaitingShip, 32> dockingQueue;   // â† QUEUE OF WAITING SHIPS
    int maxDocks = 3;                             // â† MAX DOCKING SLOTS
    int currentDocked = 0;

    graphVerticePort() {
        this->portName = "";
        this->dailyPortCharge = 0;
        this->routeHead = nullptr;
        this->latitude = 0.0f;
        this->longitude = 0.0f;
        this->hasCoordinates = false;
    }

    graphVerticePort(string portName, int dailyPortCharge) {
        this->portName = portName;
        this->dailyPortCharge = dailyPortCharge;
        this->routeHead = nullptr;
        this->latitude = 0.0f;
        this->longitude = 0.0f;
        this->hasCoordinates = false;
    }

};

struct PathResult { // just stores the pathing, need to test out different things for now
    int* path;
    int pathSize;
    int totalCost;
    long long totalTimeMinutes;
    bool found;

    PathResult() : path(nullptr), pathSize(0), totalCost(MAX_COST), found(false) {}

    ~PathResult() {
        if (path != nullptr) {
            delete[] path;
            path = nullptr;
        }
    }

    PathResult(const PathResult& other) {
        pathSize = other.pathSize;
        totalCost = other.totalCost;
        found = other.found;
        if (other.path != nullptr && pathSize > 0) {
            path = new int[pathSize];
            for (int i = 0; i < pathSize; i++) {
                path[i] = other.path[i];
            }
        }
        else {
            path = nullptr;
        }
    }

    PathResult& operator=(const PathResult& other) {
        if (this != &other) {
            if (path != nullptr) {
                delete[] path;
            }
            pathSize = other.pathSize;
            totalCost = other.totalCost;
            found = other.found;
            if (other.path != nullptr && pathSize > 0) {
                path = new int[pathSize];
                for (int i = 0; i < pathSize; i++) {
                    path[i] = other.path[i];
                }
            }
            else {
                path = nullptr;
            }
        }
        return *this;
    }
};

struct PQNode { // priority queue node for dijikstra n a*
    int portIndex;
    int cost;
    int heuristic;

    PQNode() : portIndex(0), cost(0), heuristic(0) {} // Default constructor for SimplePriorityQueue
    PQNode(int idx, int c, int h = 0) : portIndex(idx), cost(c), heuristic(h) {}

    bool operator>(const PQNode& other) const {
        return (cost + heuristic) > (other.cost + other.heuristic);
    }
    bool operator<(const PQNode& other) const {
        return (cost + heuristic) < (other.cost + other.heuristic);
    }
};

struct JourneyLeg {
    int portIndex;
    string company;
    long long departureTime;
    long long arrivalTime;
    JourneyLeg* next;
    JourneyLeg() : portIndex(-1), next(nullptr) {}
};

/*

class MultiLegJourney {
public:
    JourneyLeg* head;
    JourneyLeg* tail;
    int legCount;
    MultiLegJourney() : head(nullptr), tail(nullptr), legCount(0) {}
    void addLeg(int portIdx, const string& comp = "") {
        JourneyLeg* newLeg = new JourneyLeg();
        newLeg->portIndex = portIdx;
        newLeg->company = comp;
        if (!head) head = tail = newLeg;
        else { tail->next = newLeg; tail = newLeg; }
        legCount++;
    } }
};
*/
class MultiLegJourney {
public:
    JourneyLeg* head;
    JourneyLeg* tail;
    int legCount;

    MultiLegJourney() : head(nullptr), tail(nullptr), legCount(0) {}

    void addLeg(int portIdx, const string& comp = "") {
        JourneyLeg* newLeg = new JourneyLeg();
        newLeg->portIndex = portIdx;
        newLeg->company = comp;
        newLeg->next = nullptr;

        if (!head) {
            head = tail = newLeg;
        }
        else {
            tail->next = newLeg;
            tail = newLeg;
        }
        legCount++;
    }

    void removeLeg(int portIdx) {
        if (!head) return;

        if (head->portIndex == portIdx) {
            JourneyLeg* temp = head;
            head = head->next;
            delete temp;
            if (!head) tail = nullptr;
            legCount--;
            return;
        }

        JourneyLeg* prev = nullptr;
        JourneyLeg* curr = head;

        while (curr != nullptr && curr->portIndex != portIdx) {
            prev = curr;
            curr = curr->next;
        }

        if (!curr) return;

        prev->next = curr->next;
        if (curr == tail) tail = prev;
        delete curr;
        legCount--;
    }

    void clear() {
        while (head) {
            JourneyLeg* temp = head;
            head = head->next;
            delete temp;
        }
        tail = nullptr;
        legCount = 0;
    }

    ~MultiLegJourney() { clear(); }
};
struct oceanRouteGraph {
    // TODO: duration is to be calculated for each route as well
    graphVerticePort** graphPorts;
    int currentSize;
    int capacity;
    SimpleMap<std::string, int> portIndexMap;
    // ====== GLOBAL STATE FLAGS FOR ACTIONS ======
    bool runDirectRoutes = false;
    bool runCheapest = false;
    bool runFastest = false;
    bool runCompanyFilter = false;
    bool runSubgraph = false;
    bool runSimulation = false;
    bool typingDate = false;
    bool typingCompany = false;
    bool useAStar = false;

    PathResult currentPath;
    //int* highlightedPath;
    //std::vector<int> highlightedPath;
    SimpleVector<int, 1024> highlightedPath;
    int highlightedPathSize;
    int selectedOrigin = -1;
    int selectedDestination = -1;
    string selectedCompany = "";
    // Mercator projection removed; only using PortCoords.txt
    bool showAlgorithmVisualization = false;
    SimpleVector<int, 512> vizOpenSet;
    SimpleVector<int, 512> vizClosedSet;

    // Add these in your class
    float whooshProgress = 0.0f;        // 0.0 to 1.0
    bool showWhoosh = false;
    sf::Clock whooshClock;


    /// QUEUEING LOGIC AND VARIABLES:
    long long globalSimulationTime = 0;  // minutes since start â€” increases every frame
    /*
    struct WaitingShipGraph {
        string company;
        long long arrivalTime;     // when ship arrived at port
        long long serviceStartTime = -1;  // when it started docking
        long long serviceEndTime = -1;    // when it finishes
        int dockSlot = -1;         // which dock it's using (-1 = waiting)
    };
    */

    // EACH PORT HAS ONE QUEUE PER COMPANY
    // We use SimpleVector<WaitingShip> for each company
    SimpleMap<string, SimpleVector<WaitingShip, 32>> companyQueues;  // [portIndex][company] = queue

    // Each port has limited docks
    SimpleVector<int, 64> availableDocks;  // availableDocks[portIndex] = number of free docks

    // GLOBAL CONSOLE LOG â€” WORKS EVERYWHERE
    SimpleVector<string, 50> consoleLines;
    bool consoleFirstTime = true; // static removed
    bool simulationMode = false;

    //bool isCalculatingPath = false;
    // int currentCalculatingNode = -1;
    // Subgraph filtering
    string activeCompanyFilter = "";           // e.g., "CMA CGM"
    bool showOnlyActiveRoutes = false;         // toggle
    SimpleVector<int, 512> visiblePorts;       // ports that pass filter
    SimpleVector<pair<int, int>, 1024> visibleEdges;  // edges that pass filter

    void addLog(const string& msg) {
        if (consoleFirstTime) {
            consoleLines.push_back("=== OCEAN ROUTE SYSTEM LOG ===");
            consoleLines.push_back("Ready.");
            consoleFirstTime = false;
        }
        consoleLines.push_back(msg);
        if (consoleLines.size() > 35) {
            for (int i = 0; i < consoleLines.size() - 35; i++) {
                consoleLines[i] = consoleLines[i + 1];
            }
            consoleLines.sz = 35;
        }
    }

    void generateSubgraph() {
        visiblePorts.clear();
        visibleEdges.clear();

        if (!showOnlyActiveRoutes) {
            // Show all
            for (int i = 0; i < currentSize; i++) {
                if (graphPorts[i]) visiblePorts.push_back(i);
            }
            // Add all edges later in drawing
            return;
        }

        // COMPANY FILTER
        for (int i = 0; i < currentSize; i++) {
            if (!graphPorts[i]) continue;

            // Check if port has at least one route with the company
            graphEdgeRoute* route = graphPorts[i]->routeHead;
            bool hasCompanyRoute = false;
            while (route) {
                if (route->shippingCompanyName == activeCompanyFilter) {
                    hasCompanyRoute = true;
                    break;
                }
                route = route->next;
            }

            if (hasCompanyRoute || activeCompanyFilter.empty()) {
                visiblePorts.push_back(i);
            }
        }

        // Build visible edges
        for (int i = 0; i < currentSize; i++) {
            if (!graphPorts[i]) continue;
            graphEdgeRoute* route = graphPorts[i]->routeHead;
            while (route) {
                int dest = getPortIndex(route->destinationName);
                if (dest != -1 &&
                    (route->shippingCompanyName == activeCompanyFilter || activeCompanyFilter.empty())) {
                    visibleEdges.push_back({ i, dest });
                }
                route = route->next;
            }
        }
    }


    oceanRouteGraph() {
        this->capacity = TOTALNUMBEROFPORTS;
        this->currentSize = 0;
        graphPorts = new graphVerticePort * [TOTALNUMBEROFPORTS];
        for (int i = 0; i < TOTALNUMBEROFPORTS; i++) {
            graphPorts[i] = nullptr;
        }
        // highlightedPath = nullptr;
        highlightedPathSize = 0;
        // by default, no coordinates loaded
        // Mercator projection removed

        availableDocks = SimpleVector<int, 64>();
        for (int i = 0; i < TOTALNUMBEROFPORTS; i++) {
            availableDocks.push_back(3);  // 3 docks per port
        }
    }

    ~oceanRouteGraph() {
        // TODO: will have to run functions to delete each node of each list -- then delete the head ptr -- ie make it null -- and then do nullptr and delte her
        for (int i = 0; i < currentSize; i++) {
            if (graphPorts[i] == nullptr) {
                continue;
            }

            graphEdgeRoute* currentNode = graphPorts[i]->routeHead;
            while (currentNode != nullptr) {
                graphEdgeRoute* tempNode = currentNode;
                currentNode = currentNode->next;
                tempNode->next = nullptr;
                delete tempNode;
            }

            delete graphPorts[i];
            graphPorts[i] = nullptr;
        }

        delete[] graphPorts;
        graphPorts = nullptr;

        /*
        if (highlightedPath != nullptr) {
            delete[] highlightedPath;
            highlightedPath = nullptr;
        }
        */
    }

        // â†â†â†â†â† YOUR MULTI-LEG CODE GOES HERE â€” 100% CORRECT â†’â†’â†’â†’â†’
        

        MultiLegJourney currentJourney;
        bool buildingMultiLeg = false;
        // â†â†â†â†â† END OF MULTI-LEG CODE â†’â†’â†’â†’â†’


    // parse the string from get line ourselver
    bool stringParsingForRoute(const string& lineFromFile, string& sourceName, string& destinationName, string& date, string& arrivalTime, string& departureTime, int& voyageCost, string& shippingCompanyName) {
        // we are extracting a total of 7 fields from the line 

        // as a result -- we will traverse the string character by character
        const char* currentCharPointer = lineFromFile.c_str();
        const char* currentStartingLetter = currentCharPointer;
        int currentField = 0;

        // TODO: might have to move the initialzation of variables here -- but i do not deem that necessary for now

        while (*currentCharPointer) {
            if (*currentCharPointer == ' ') { // simulating a delimiter here
                string currentString(currentStartingLetter, currentCharPointer - currentStartingLetter); // using simple pointer arithematic -- in the string constructer -- thing of producing indexes from 0 -- to whatever index that we have our word

                // currentString is our produced substring

                switch (currentField) {
                case 0:
                    sourceName = currentString;
                    break;
                case 1:
                    destinationName = currentString;
                    break;
                case 2:
                    date = currentString;
                    break;
                case 3:
                    arrivalTime = currentString;
                    break;
                case 4:
                    departureTime = currentString;
                    break;
                case 5:
                    voyageCost = stoi(currentString); // using a sting to int converter as the currentString is a string and the voyageCost is an int variable
                    break;
                case 6: // NOTE: this part doesn't actually work and isnt needed as the last currentString won't have a terminator -- meaning -- the string will reach its end without hitting a space so the loop will exit -- hence thelogic for this exists outside the loop
                    shippingCompanyName = currentString;
                    break;
                }

                // incrementing current field
                currentField++;
                currentStartingLetter = currentCharPointer + 1; // pointer aritematic -- in the next letter after the space now

            }

            currentCharPointer++; // out of if move to the next character in ALL cases
        }

        if (currentStartingLetter < currentCharPointer) { // the currentStarting letter remains at the starting letter of the shipping compnay -- won't update as no ship encountered
            string currentString(currentStartingLetter, currentCharPointer - currentStartingLetter);
            if (currentField == 6) { // currentField value is preserved as it was declared outside the loop
                shippingCompanyName = currentString;
            }
            else if (currentField == 5) { // TODO: prolly remove this as it wont be needed
                voyageCost = stoi(currentString);
            }

            currentField++;
        }

        return (currentField == 7);

    }

    bool stringStreamForRoute(const string& lineFromFile, string& sourceName, string& destinationName, string& date, string& arrivalTime, string& departureTime, int& voyageCost, string& shippingCompanyName) {
        stringstream ss(lineFromFile);
        string voyageCostString = "";

        if (!(ss >> sourceName >> destinationName >> date >> arrivalTime >> departureTime >> voyageCostString >> shippingCompanyName)) {
            return false;
        }

        voyageCost = stoi(voyageCostString);
        // TODO: learn logic behind string stream

        string leftOverString;
        if (ss >> leftOverString) {
            return false;
        }

        return !sourceName.empty() && !destinationName.empty() && !date.empty() && !arrivalTime.empty() && !departureTime.empty() && !voyageCostString.empty() && !shippingCompanyName.empty();
    }

    bool stringParsingForCharges(const string& lineFromFile, string& nameOfPort, int& dailyChargesOfPort) {
        const char* currentCharPointer = lineFromFile.c_str();
        const char* currentStartingLetter = currentCharPointer;
        int currentField = 0;

        while (*currentCharPointer) {
            if (*currentCharPointer == ' ') {
                string currentString(currentStartingLetter, currentCharPointer - currentStartingLetter);

                switch (currentField) {
                case 0:
                    nameOfPort = currentString;
                    break;
                case 1:
                    dailyChargesOfPort = stoi(currentString);
                    break;
                }

                currentField++;
                currentStartingLetter = currentCharPointer + 1;
            }
            currentCharPointer++;
        }

        if (currentStartingLetter < currentCharPointer) {
            string currentString(currentStartingLetter, currentCharPointer - currentStartingLetter);
            if (currentField == 1) {
                dailyChargesOfPort = stoi(currentString);
            }
            else if (currentField == 0) {
                nameOfPort = currentString;
            }

            currentField++;
        }

        return (currentField == 2);
    }

    bool stringStreamForCharges(const string& lineFromFile, string& nameOfPort, int& dailyChargesOfPort) {
        stringstream ss(lineFromFile);
        string dailyChargesString = "";

        if (!(ss >> nameOfPort >> dailyChargesString)) {
            return false;
        }

        dailyChargesOfPort = stoi(dailyChargesString);

        string leftOverString;
        if (ss >> leftOverString) {
            return false;
        }


        return !nameOfPort.empty() && !dailyChargesString.empty();
    }

    bool searchingPortExistence(string sourceName, int& indexOfExisitingPort) {
        string key = sourceName;
        // normalize
        auto normalize = [](const string& s) {
            string out;
            for (char c : s) {
                if (my_isalnum(c)) out.push_back(my_tolower(c));
            }
            return out;
            };
        string normalized = normalize(key);
        int idx = portIndexMap.find(normalized);
        if (idx != -1) {
            indexOfExisitingPort = portIndexMap[normalized];
            return true;
        }
        for (int i = 0; i < currentSize; i++) {
            if (graphPorts[i] != nullptr && graphPorts[i]->portName == sourceName) {
                indexOfExisitingPort = i;
                return true;
            }
        }

        indexOfExisitingPort = -1;
        return false;
    }

    void addAtEndOfLinkedList(graphEdgeRoute*& currentHead, const string& destinationName, const string& date, const string& arrivalTime, const string& departureTime, const int& voyageCost, const string& shippingCompanyName) {
        // creating a new node
        graphEdgeRoute* newNode = new graphEdgeRoute(destinationName, date, departureTime, arrivalTime, voyageCost, shippingCompanyName);
        if (newNode == nullptr) {
            cerr << "Overflow! No Memory Allocated!" << endl;
            return;
        }

        if (currentHead == nullptr) {
            currentHead = newNode;
        }
        else {
            // traverse to the last node first
            graphEdgeRoute* last = currentHead;
            while (last->next != nullptr) {
                last = last->next;
            }

            last->next = newNode;
        }
    }

    // now we work on creating the linked list from the file
    void createGraphFromFile(const string& fileNameForPorts, const string& fileNameForCharges) {
        // 1 ) open the file
        // 2) check for errors
        // 3) read the file 
        // 4) for each line there are two cases we can encounter:
        //      i) either it already exists so we add it to the end of the linked list -- pass its head to a helper function
        //      ii) either we have to add it to the end of the list -- current size needed for that -- will have to add a validation for this purpose
        // 5) then we run another helper function to parse the file of port charges -- to assign each port with its port charges
        ifstream portFile(fileNameForPorts); // step 1 completed
        if (!portFile.is_open()) {
            cerr << "ERROR! File Could Not Be Opened!" << endl;
            return; // step 2 done here
        }

        // for step 3 -- based on research i did -- either we could use sstream -- makes it easier or we could manually parse the string
        // ill put in both the logics in helper functions so that we can interchange with ease

        // TODO: these will go inside the loop so that they are cleared and reinitialize every single time the loop runs to prevent issues of unessary crap -- and out logic of adding to the list goes there as well
        string currentLine = "";
        while (getline(portFile, currentLine)) {
            // TODO: check if line is empty or not -- in the loop
            if (currentLine.empty()) {
                continue; // to the next iteration of this loop if not EOP
            }
            string sourceName = "";
            string destinationName = "";
            string date = "";
            string arrivalTime = "";
            string departureTime = "";
            int voyageCost = 0;
            string shippingCompanyName = "";

            bool parsingComplete = stringParsingForRoute(currentLine, sourceName, destinationName, date, arrivalTime, departureTime, voyageCost, shippingCompanyName);
            /*
            if (parsingComplete == true) {
                cerr << "Line Successfully Parsed!" << endl;
            } else {
                cerr << "Line Not Parsed Successfully!" << endl;
                return;
            }
            */
            // Step 3 completed
            // start step 4 -- creating the graph

            // we begin by searching out current array
            int indexOfExisitingPort = 0;
            bool portExistence = searchingPortExistence(sourceName, indexOfExisitingPort);
            // two cases -- either exists or it doesnt
            // i) Exists -- get head of the linked list -- add to the end of the linked list
            // ii) Doesn't Exist -- add the head at the current index --- then add the destination at the first node position -- use add at end logic

            if (portExistence == true) {
                // TODO: now that we got the head we run an add at end helper function for this
                // graphPorts[indexOfExisitingPort]->portName = sourceName;
                addAtEndOfLinkedList(graphPorts[indexOfExisitingPort]->routeHead, destinationName, date, arrivalTime, departureTime, voyageCost, shippingCompanyName);
            }
            else {
                // else we create a head -- at it at the currentSize index -- increment current size
                // then at current head -- create and edge node and add it -- same add at end function
                graphVerticePort* currentNode = new graphVerticePort(sourceName, 0); // adding 0 for rn
                // add the currentNode to the array at current size
                graphPorts[currentSize] = currentNode;
                graphPorts[currentSize]->portName = sourceName;
                // Add to lookup map (normalized key)
                auto normalize = [](const string& s) {
                    string out;
                    for (char c : s) if (my_isalnum(c)) out.push_back(my_tolower(c));
                    return out;
                    };
                portIndexMap[normalize(sourceName)] = currentSize;
                addAtEndOfLinkedList(graphPorts[currentSize]->routeHead, destinationName, date, arrivalTime, departureTime, voyageCost, shippingCompanyName);
                currentSize++;
            }
            // Ensure destination is also present as a vertex (even if it has no outgoing edges)
            int destIndex = -1;
            if (!searchingPortExistence(destinationName, destIndex)) {
                if (currentSize < capacity) {
                    graphVerticePort* destNode = new graphVerticePort(destinationName, 0);
                    graphPorts[currentSize] = destNode;
                    // normalized map key
                    auto normalize = [](const string& s) {
                        string out;
                        for (char c : s) if (my_isalnum(c)) out.push_back(my_tolower(c));
                        return out;
                        };
                    portIndexMap[normalize(destinationName)] = currentSize;
                    currentSize++;
                }
            }

            // Step 4 completed
        }


        portFile.close();
        // now do step 5
        // TODO:in the end parse the daily charge file to complete creation

        // TODO: PortCharges.txt: This represents the cost incurred per day when a ship remains docked for refueling, cargotransfer, maintenance, or extended layovers exceeding 12 hours.
        // TODO: create destructors
        // TODO: close file 

        ifstream chargesFile(fileNameForCharges);
        string chargesLine = "";
        while (getline(chargesFile, chargesLine)) {
            if (chargesLine.empty()) {
                continue;
            }
            string nameOfPort = "";
            int dailyChargesOfPort = 0;

            // TODO: continue from fxiing string stream
            bool outputDrawnFromFile = stringParsingForCharges(chargesLine, nameOfPort, dailyChargesOfPort);
            // we search in the array now and assign

            /*
            if (outputDrawnFromFile == true) {
                cerr << "Line Parsed Successfully!" << endl;
            } else {
                cerr << "Line Not Parsed Successfully!" << endl;
                return;
            }
            */
            for (int i = 0; i < currentSize; i++) { // had abug here -- was looping on TOTALNUMBEROFPORTS -- but only currentSize ports exist -- trying to access a nullptr vars == undefined behaviour
                if (graphPorts[i] == nullptr) {
                    continue;
                }

                if (graphPorts[i]->portName == nameOfPort) {
                    graphPorts[i]->dailyPortCharge = dailyChargesOfPort;
                    break;
                }
            }
        }

        chargesFile.close();
        //step 5 complete

        // Try to load coordinates file if present (optional)
        loadPortCoordinates("./PortCoords.txt");
    }


    void printGraphAfterCreation() {
        if (currentSize == 0) {
            cerr << "No Graph Nodes Exist to Print!" << endl;
            return;
        }

        for (int i = 0; i < currentSize; i++) {
            if (graphPorts[i] == nullptr) {
                continue;
            }

            cout << graphPorts[i]->portName << " (" << graphPorts[i]->dailyPortCharge << ")";
            if (graphPorts[i]->hasCoordinates) {
                cout << " [" << graphPorts[i]->latitude << "," << graphPorts[i]->longitude << "]";
            }
            cout << " -> ";
            if (graphPorts[i]->routeHead == nullptr) {
                cout << " (No Route)";
                continue;
            }

            graphEdgeRoute* currentNode = graphPorts[i]->routeHead;
            while (currentNode != nullptr) {
                cout << currentNode->voyageCost << " ";
                currentNode = currentNode->next;
            }
            cout << endl;
        }
    }

    int getPortIndex(const string& portName) { // helper function 
        // use normalized map lookup first
        auto normalize = [](const string& s) {
            string out;
            for (char c : s) {
                if (my_isalnum(c)) out.push_back(my_tolower(c));
            }
            return out;
            };
        string key = normalize(portName);
        int idx = portIndexMap.find(key);
        if (idx != -1) return portIndexMap[key];
        for (int i = 0; i < currentSize; i++) {
            if (graphPorts[i] != nullptr && graphPorts[i]->portName == portName) {
                return i;
            }
        }
        return -1;
    }

    int calculateHeuristic(int fromIndex, int toIndex) { //gets the euclidean distance i changed a bit from help but it should work
        if (fromIndex < 0 || fromIndex >= currentSize || toIndex < 0 || toIndex >= currentSize) {
            return 0;
        }
        if (graphPorts[fromIndex] == nullptr || graphPorts[toIndex] == nullptr) {
            return 0;
        }

        float dx = graphPorts[fromIndex]->position.x - graphPorts[toIndex]->position.x;
        float dy = graphPorts[fromIndex]->position.y - graphPorts[toIndex]->position.y;
        return static_cast<int>(sqrt(dx * dx + dy * dy) * 10);
    }

    void reconstructPath(PathResult& result, int* parent, int start, int end) { // builds path from parent array
        if (parent[end] == -1 && end != start) {
            result.path = nullptr;
            result.pathSize = 0;
            return;
        }

        int tempPath[TOTALNUMBEROFPORTS];
        int pathCount = 0;
        int current = end;

        while (current != -1 && pathCount < TOTALNUMBEROFPORTS) {
            tempPath[pathCount] = current;
            pathCount++;
            current = parent[current];
        }

        if (pathCount > 0) {
            result.pathSize = pathCount;
            result.path = new int[pathCount];

            for (int i = 0; i < pathCount; i++) {
                result.path[i] = tempPath[pathCount - 1 - i];
            }
        }
        else {
            result.path = nullptr;
            result.pathSize = 0;
        }
    }


    bool parseDateTime(const string& dateStr, const string& timeStr,
        int& day, int& month, int& year,
        int& hour, int& minute)
    {
        // Expected formats:
        // date  = "dd/mm/yyyy"
        // time  = "hh:mm"

        if (dateStr.size() < 10 || timeStr.size() < 4)
            return false;

        // Parse date
        day = stoi(dateStr.substr(0, 2));
        month = stoi(dateStr.substr(3, 2));
        year = stoi(dateStr.substr(6, 4));

        // Parse time
        hour = stoi(timeStr.substr(0, 2));
        minute = stoi(timeStr.substr(3, 2));

        return true;
    }


    long long parseDateTimeToMinutes(const string& dateStr, const string& timeStr) {
        int day, month, year, hour, minute;
        if (!parseDateTime(dateStr, timeStr, day, month, year, hour, minute))
            return 0;

        static const int daysInMonth[13] = { 0,31,28,31,30,31,30,31,31,30,31,30,31 };
        long long total = 0;

        // Years from 2024 to year-1
        for (int y = 2024; y < year; y++) {
            total += 365LL * 24 * 60;
            if (y % 4 == 0) total += 24 * 60; // leap year
        }

        // Months from Jan to month-1
        for (int m = 1; m < month; m++) {
            total += daysInMonth[m] * 24LL * 60;
            if (m == 2 && year % 4 == 0) total += 24 * 60;
        }

        total += (day - 1LL) * 24 * 60;
        total += hour * 60LL;
        total += minute;

        return total;
    }

    PathResult dijkstraShortestPath(
        int startIndex, int endIndex,
        const string& preferredCompany = "",
        const SimpleVector<string>& avoidPorts = {},
        int maxVoyageHours = -1
    ) {
        PathResult result;
        if (startIndex < 0 || endIndex < 0 || startIndex >= currentSize || endIndex >= currentSize) return result;

        int* dist = new int[currentSize];
        int* parent = new int[currentSize];
        bool* visited = new bool[currentSize];

        for (int i = 0; i < currentSize; i++) {
            dist[i] = MAX_COST;
            parent[i] = -1;
            visited[i] = false;
        }

        SimplePriorityQueue<PQNode, 512> pq;
        dist[startIndex] = 0;
        pq.push(PQNode(startIndex, 0));

        while (!pq.empty()) {
            PQNode curr = pq.top(); pq.pop();
            int u = curr.portIndex;
            if (visited[u]) continue;
            visited[u] = true;

            if (u == endIndex) {
                reconstructPath(result, parent, startIndex, endIndex);
                result.totalCost = dist[u];
                result.found = true;
                break;
            }

            graphEdgeRoute* route = graphPorts[u]->routeHead;
            bool usedPreferred = false;

            // PHASE 1: Try PREFERRED COMPANY first
            if (!preferredCompany.empty()) {
                route = graphPorts[u]->routeHead;
                while (route != nullptr) {
                    int v = getPortIndex(route->destinationName);
                    if (v == -1 || visited[v]) { route = route->next; continue; }
                    // â†â†â†â†â† ADD THIS BLOCK HERE â†’â†’â†’â†’â†’
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // â†â†â†â†â† END BLOCK â†’â†’â†’â†’â†’

                    // Avoid ports
                    bool skip = false;
                    for (int i = 0; i < avoidPorts.size(); i++) {
                        if (graphPorts[v]->portName == avoidPorts[i]) { skip = true; break; }
                    }
                    if (skip) { route = route->next; continue; }

                    
                    // Max voyage time
                    if (maxVoyageHours > 0) {
                        long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                        long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                        if (arr < dep) arr += 24 * 60;
                        if ((arr - dep) > maxVoyageHours * 60LL) { route = route->next; continue; }
                    }
                    
                    if (route->shippingCompanyName == preferredCompany) {
                        int edgeCost = route->voyageCost + (v != endIndex ? graphPorts[v]->dailyPortCharge : 0);
                        if (dist[u] + edgeCost < dist[v]) {
                            dist[v] = dist[u] + edgeCost;
                            parent[v] = u;
                            pq.push(PQNode(v, dist[v]));
                        }
                        usedPreferred = true;
                    }
                    route = route->next;
                }
            }

            // PHASE 2: If no preferred route found â†’ allow ANY company
            if (!usedPreferred || !preferredCompany.empty()) {
                route = graphPorts[u]->routeHead;
                while (route != nullptr) {
                    int v = getPortIndex(route->destinationName);
                    if (v == -1 || visited[v]) { route = route->next; continue; }

                    // â†â†â†â†â† GLOBAL COMPANY FILTER â€” MUST BE IN BOTH PHASES â†’â†’â†’â†’â†’
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // â†â†â†â†â† END FILTER â†’â†’â†’â†’â†’

                    bool skip = false;
                    for (int i = 0; i < avoidPorts.size(); i++) {
                        if (graphPorts[v]->portName == avoidPorts[i]) { skip = true; break; }
                    }
                    if (skip) { route = route->next; continue; }

                    
                    if (maxVoyageHours > 0) {
                        long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                        long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                        if (arr < dep) arr += 24 * 60;
                        if ((arr - dep) > maxVoyageHours * 60LL) { route = route->next; continue; }
                    }
    
                    int edgeCost = route->voyageCost + (v != endIndex ? graphPorts[v]->dailyPortCharge : 0);
                    if (dist[u] + edgeCost < dist[v]) {
                        dist[v] = dist[u] + edgeCost;
                        parent[v] = u;
                        pq.push(PQNode(v, dist[v]));
                    }
                    route = route->next;
                }
            }
        }

        delete[] dist; delete[] parent; delete[] visited;
        return result;
    }

    PathResult aStarCheapestWithPrefs(
        int startIndex, int endIndex,
        const string& preferredCompany = "",
        const SimpleVector<string>& avoidPorts = {},
        int maxVoyageHours = -1
    ) {
        PathResult result;
        if (startIndex < 0 || endIndex < 0) return result;

        int* gScore = new int[currentSize];
        int* fScore = new int[currentSize];
        int* parent = new int[currentSize];
        bool* visited = new bool[currentSize];

        for (int i = 0; i < currentSize; i++) {
            gScore[i] = MAX_COST;
            fScore[i] = MAX_COST;
            parent[i] = -1;
            visited[i] = false;
        }

        SimplePriorityQueue<PQNode, 512> pq;
        gScore[startIndex] = 0;
        fScore[startIndex] = calculateHeuristic(startIndex, endIndex);
        pq.push(PQNode(startIndex, gScore[startIndex], calculateHeuristic(startIndex, endIndex)));

        while (!pq.empty()) {
            PQNode curr = pq.top(); pq.pop();
            int u = curr.portIndex;
            if (visited[u]) continue;
            visited[u] = true;

            if (u == endIndex) {
                reconstructPath(result, parent, startIndex, endIndex);
                result.totalCost = gScore[u];
                result.found = true;
                break;
            }

            graphEdgeRoute* route = graphPorts[u]->routeHead;
            bool usedPreferred = false;

            if (!preferredCompany.empty()) {
                route = graphPorts[u]->routeHead;
                while (route != nullptr) {
                    int v = getPortIndex(route->destinationName);
                    if (v == -1 || visited[v]) { route = route->next; continue; }

                    cout << "From function: " <<  activeCompanyFilter << endl;
                    // â†â†â†â†â† ADD THIS BLOCK HERE â†’â†’â†’â†’â†’
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // â†â†â†â†â† END BLOCK â†’â†’â†’â†’â†’

                    bool skip = false;
                    for (int i = 0; i < avoidPorts.size(); i++) {
                        if (graphPorts[v]->portName == avoidPorts[i]) { skip = true; break; }
                    }
                    if (skip) { route = route->next; continue; }

                    // --- 1. Parse times once ---
                    long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                    long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);

                    // Fix next-day arrival
                    if (arr < dep) arr += 24LL * 60LL;

                    // --- 2. Adjust departure to next day if we arrive late ---
                    long long adjustedDep = dep;
                    if (adjustedDep < arr) {
                        adjustedDep += 24LL * 60LL;
                    }

                    // If we STILL miss the ship â†’ skip
                    if (arr > adjustedDep) {
                        route = route->next;
                        continue;
                    }

                    // Adjust arrival to match next-day departure if needed
                    long long adjustedArr = arr;
                    if (adjustedArr < adjustedDep)
                        adjustedArr += 24LL * 60LL;

                    // --- 3. Max voyage duration filter ---
                    if (maxVoyageHours > 0) {
                        if (adjustedArr - adjustedDep > maxVoyageHours * 60LL) {
                            route = route->next;
                            continue;
                        }
                    }

                    // --- 4. Cost of taking this route ---
                    int edgeCost = route->voyageCost +
                        (v != endIndex ? graphPorts[v]->dailyPortCharge : 0);

                    long long tentativeG = gScore[u] + edgeCost;

                    // --- 5. Bias preferred company ---
                    if (route->shippingCompanyName == preferredCompany) {
                        tentativeG -= 3;  // small bias
                        usedPreferred = true;
                    }

                    // --- 6. Relax the edge normally ---
                    if (tentativeG < gScore[v]) {
                        gScore[v] = tentativeG;
                        parent[v] = u;
                        pq.push(PQNode(v, tentativeG));
                    }

                    route = route->next;
                }
            }

            if (!usedPreferred || !preferredCompany.empty()) {
                route = graphPorts[u]->routeHead;
                while (route != nullptr) {
                    int v = getPortIndex(route->destinationName);
                    if (v == -1 || visited[v]) { route = route->next; continue; }

                    // â†â†â†â†â† GLOBAL COMPANY FILTER â€” MUST BE IN BOTH PHASES â†’â†’â†’â†’â†’
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // â†â†â†â†â† END FILTER â†’â†’â†’â†’â†’

                    bool skip = false;
                    for (int i = 0; i < avoidPorts.size(); i++) {
                        if (graphPorts[v]->portName == avoidPorts[i]) { skip = true; break; }
                    }
                    if (skip) { route = route->next; continue; }

                    if (maxVoyageHours > 0) {
                        long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                        long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                        if (arr < dep) arr += 24 * 60;
                        if ((arr - dep) > maxVoyageHours * 60LL) { route = route->next; continue; }
                    }

                    int edgeCost = route->voyageCost + (v != endIndex ? graphPorts[v]->dailyPortCharge : 0);
                    int tentativeG = gScore[u] + edgeCost;
                    if (tentativeG < gScore[v]) {
                        parent[v] = u;
                        gScore[v] = tentativeG;
                        fScore[v] = gScore[v] + calculateHeuristic(v, endIndex);
                        pq.push(PQNode(v, gScore[v], calculateHeuristic(v, endIndex)));
                    }
                    route = route->next;
                }
            }
        }

        delete[] gScore; delete[] fScore; delete[] parent; delete[] visited;
        return result;
    }

    PathResult dijkstraFastestWithPrefs(
        int startIndex, int endIndex,
        const string& preferredCompany = "",
        const SimpleVector<string>& avoidPorts = {},
        int maxVoyageHours = -1
    ) {
        PathResult result;
        if (startIndex < 0 || endIndex < 0) return result;

        long long* bestArrival = new long long[currentSize];
        int* parent = new int[currentSize];
        bool* visited = new bool[currentSize];

        for (int i = 0; i < currentSize; i++) {
            bestArrival[i] = LLONG_MAX;
            parent[i] = -1;
            visited[i] = false;
        }

        SimplePriorityQueue<pair<long long, int>, 512> pq;
        bestArrival[startIndex] = 0;
        pq.push({ 0, startIndex });

        while (!pq.empty()) {
            pair<long long, int> curr = pq.top(); pq.pop();
            long long time = curr.first;
            int u = curr.second;
            if (visited[u]) continue;
            visited[u] = true;

            if (u == endIndex) {
                reconstructPath(result, parent, startIndex, endIndex);
                result.totalTimeMinutes = bestArrival[u];
                cout << endl << "Total Time: " << bestArrival[u] << endl;
                result.found = true;
                break;
            }

            graphEdgeRoute* route = graphPorts[u]->routeHead;
            bool usedPreferred = false;

            if (!preferredCompany.empty()) {
                route = graphPorts[u]->routeHead;
                while (route != nullptr) {
                    int v = getPortIndex(route->destinationName);
                    if (v == -1 || visited[v]) { route = route->next; continue; }
                    // â†â†â†â†â† ADD THIS BLOCK HERE â†’â†’â†’â†’â†’
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // â†â†â†â†â† END BLOCK â†’â†’â†’â†’â†’

                    bool skip = false;
                    for (int i = 0; i < avoidPorts.size(); i++) {
                        if (graphPorts[v]->portName == avoidPorts[i]) { skip = true; break; }
                    }
                    if (skip) { route = route->next; continue; }

                    /*
                    if (maxVoyageHours > 0) {
                        long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                        long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                        if (arr < dep) arr += 24 * 60;
                        if ((arr - dep) > maxVoyageHours * 60LL) { route = route->next; continue; }
                    }

                    long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                    long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                    if (arr < dep) arr += 24 * 60;

                    if (time <= dep && route->shippingCompanyName == preferredCompany && arr < bestArrival[v]) {
                        bestArrival[v] = arr;
                        parent[v] = u;
                        pq.push({ arr, v });
                        usedPreferred = true;
                    }
                    route = route->next;
                    */

                    long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                    long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);

                    // If arrival is earlier than departure -> next day
                    if (arr < dep) arr += 24LL * 60LL;

                    // Adjust departure to be after current time (catching the ship)
                    long long adjustedDep = dep;
                    if (adjustedDep < time)
                        adjustedDep += 24LL * 60LL;

                    // If even after adjusting, you still cannot catch it â†’ skip
                    if (time > adjustedDep) {
                        route = route->next;
                        continue;
                    }

                    // Adjust arrival accordingly
                    long long adjustedArr = arr;
                    if (adjustedArr < adjustedDep)
                        adjustedArr += 24LL * 60LL;

                    // === ADD SHIP TO DOCKING QUEUE ===
                    WaitingShip newShip;
                    newShip.company = route->shippingCompanyName;
                    newShip.arrivalTime = adjustedArr;
                    newShip.serviceEndTime = adjustedArr + 8 * 60;  // 8 hours service

                    // Add to this port's queue
                    graphPorts[v]->dockingQueue.push_back(newShip);
                    // Optional: reset docked count if needed
                    graphPorts[v]->currentDocked = 0;
                    // === END ADD SHIP ==

                    // Optional: max voyage duration filter
                    if (maxVoyageHours > 0) {
                        if (adjustedArr - adjustedDep > maxVoyageHours * 60LL) {
                            route = route->next;
                            continue;
                        }
                    }

                    // Relax edge: update best arrival
                    if (adjustedArr < bestArrival[v]) {
                        bestArrival[v] = adjustedArr;
                        parent[v] = u;
                        pq.push({ adjustedArr, v });
                    }

                    route = route->next;
                }
            }

            if (!usedPreferred || !preferredCompany.empty()) {
                route = graphPorts[u]->routeHead;
                while (route != nullptr) {
                    int v = getPortIndex(route->destinationName);
                    if (v == -1 || visited[v]) { route = route->next; continue; }

                    // â†â†â†â†â† GLOBAL COMPANY FILTER â€” MUST BE IN BOTH PHASES â†’â†’â†’â†’â†’
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // â†â†â†â†â† END FILTER â†’â†’â†’â†’â†’

                    bool skip = false;
                    for (int i = 0; i < avoidPorts.size(); i++) {
                        if (graphPorts[v]->portName == avoidPorts[i]) { skip = true; break; }
                    }
                    if (skip) { route = route->next; continue; }

                    if (maxVoyageHours > 0) {
                        long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                        long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                        if (arr < dep) arr += 24 * 60;
                        if ((arr - dep) > maxVoyageHours * 60LL) { route = route->next; continue; }
                    }

                    long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                    long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                    if (arr < dep) arr += 24 * 60;

                    if (time <= dep && arr < bestArrival[v]) {
                        bestArrival[v] = arr;
                        parent[v] = u;
                        pq.push({ arr, v });
                    }
                    route = route->next;
                }
            }
        }

        delete[] bestArrival; delete[] parent; delete[] visited;
        return result;
    }

    PathResult aStarFastestWithPrefs(
        int startIndex, int endIndex,
        const string& preferredCompany = "",
        const SimpleVector<string>& avoidPorts = {},
        int maxVoyageHours = -1
    ) {
        PathResult result;
        if (startIndex < 0 || endIndex < 0) return result;

        long long* gScore = new long long[currentSize];
        int* parent = new int[currentSize];
        bool* visited = new bool[currentSize];

        for (int i = 0; i < currentSize; i++) {
            gScore[i] = LLONG_MAX;
            parent[i] = -1;
            visited[i] = false;
        }

        auto timeHeuristic = [&](int from, int to) -> long long {
            if (!graphPorts[from]->hasCoordinates || !graphPorts[to]->hasCoordinates) return 0;
            float dx = graphPorts[from]->position.x - graphPorts[to]->position.x;
            float dy = graphPorts[from]->position.y - graphPorts[to]->position.y;
            return (long long)(sqrt(dx * dx + dy * dy) * 0.1f);
            };

        SimplePriorityQueue<PQNode, 512> pq;
        gScore[startIndex] = 0;
        pq.push(PQNode(startIndex, 0, timeHeuristic(startIndex, endIndex)));

        while (!pq.empty()) {
            PQNode curr = pq.top(); pq.pop();
            int u = curr.portIndex;
            long long arrivalAtU = gScore[u];

            if (visited[u]) continue;
            visited[u] = true;

            if (u == endIndex) {
                reconstructPath(result, parent, startIndex, endIndex);
                result.totalTimeMinutes = gScore[u];
                cout << endl << "Total Time: " << gScore[u] << endl;
                result.found = true;
                break;
            }

            graphEdgeRoute* route = graphPorts[u]->routeHead;
            bool usedPreferred = false;

            if (!preferredCompany.empty()) {
                route = graphPorts[u]->routeHead;
                while (route != nullptr) {
                    int v = getPortIndex(route->destinationName);
                    if (v == -1 || visited[v]) { route = route->next; continue; }

                    // â†â†â†â†â† ADD THIS BLOCK HERE â†’â†’â†’â†’â†’
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // â†â†â†â†â† END BLOCK â†’â†’â†’â†’â†’

                    bool skip = false;
                    for (int i = 0; i < avoidPorts.size(); i++) {
                        if (graphPorts[v]->portName == avoidPorts[i]) { skip = true; break; }
                    }
                    if (skip) { route = route->next; continue; }

                    /*
                    if (maxVoyageHours > 0) {
                        long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                        long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                        if (arr < dep) arr += 24 * 60;
                        if ((arr - dep) > maxVoyageHours * 60LL) { route = route->next; continue; }
                    }

                    long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                    long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                    if (arr < dep) arr += 24 * 60;

                    if (arrivalAtU <= dep && route->shippingCompanyName == preferredCompany && arr < gScore[v]) {
                        gScore[v] = arr;
                        parent[v] = u;
                        pq.push(PQNode(v, arr, timeHeuristic(v, endIndex)));
                        usedPreferred = true;
                    }
                    route = route->next;
                    */

                    long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                    long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);

                    // Normalize arrival (next-day arrival case)
                    if (arr < dep) arr += 24LL * 60LL;

                    // --- ADJUST DEPARTURE BASED ON ARRIVAL TIME AT U ---
                    long long adjustedDep = dep;
                    if (adjustedDep < arrivalAtU) {
                        adjustedDep += 24LL * 60LL;   // catch tomorrow's ship
                    }

                    // If even after adjusting you still miss it â†’ skip
                    if (arrivalAtU > adjustedDep) {
                        route = route->next;
                        continue;
                    }

                    // Adjust arrival accordingly
                    long long adjustedArr = arr;
                    if (adjustedArr < adjustedDep)
                        adjustedArr += 24LL * 60LL;

                    // === ADD SHIP TO DOCKING QUEUE ===
                    WaitingShip newShip;
                    newShip.company = route->shippingCompanyName;
                    newShip.arrivalTime = adjustedArr;
                    newShip.serviceEndTime = adjustedArr + 8 * 60;  // 8 hours service

                    // Add to this port's queue
                    graphPorts[v]->dockingQueue.push_back(newShip);
                    // Optional: reset docked count if needed
                    graphPorts[v]->currentDocked = 0;
                    // === END ADD SHIP ===

                    // --- FILTER: Max voyage hours ---
                    if (maxVoyageHours > 0) {
                        if (adjustedArr - adjustedDep > maxVoyageHours * 60LL) {
                            route = route->next;
                            continue;
                        }
                    }

                    // --- RELAX EDGE: If preferred company â†’ add small bias ---
                    long long candidateCost = adjustedArr;
                    if (route->shippingCompanyName == preferredCompany) {
                        candidateCost -= 5; // small priority bias
                    }

                    if (candidateCost < gScore[v]) {
                        gScore[v] = candidateCost;
                        parent[v] = u;
                        pq.push(PQNode(v, candidateCost, timeHeuristic(v, endIndex)));
                    }

                    route = route->next;
                }
            }

            if (!usedPreferred || !preferredCompany.empty()) {
                route = graphPorts[u]->routeHead;
                while (route != nullptr) {
                    int v = getPortIndex(route->destinationName);
                    if (v == -1 || visited[v]) { route = route->next; continue; }
                    // â†â†â†â†â† GLOBAL COMPANY FILTER â€” MUST BE IN BOTH PHASES â†’â†’â†’â†’â†’
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // â†â†â†â†â† END FILTER â†’â†’â†’â†’â†’

                    bool skip = false;
                    for (int i = 0; i < avoidPorts.size(); i++) {
                        if (graphPorts[v]->portName == avoidPorts[i]) { skip = true; break; }
                    }
                    if (skip) { route = route->next; continue; }

                    if (maxVoyageHours > 0) {
                        long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                        long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                        if (arr < dep) arr += 24 * 60;
                        if ((arr - dep) > maxVoyageHours * 60LL) { route = route->next; continue; }
                    }

                    long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                    long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                    if (arr < dep) arr += 24 * 60;

                    if (arrivalAtU <= dep && arr < gScore[v]) {
                        gScore[v] = arr;
                        parent[v] = u;
                        pq.push(PQNode(v, arr, timeHeuristic(v, endIndex)));
                    }
                    route = route->next;
                }
            }
        }

        delete[] gScore; delete[] parent; delete[] visited;
        return result;
    }

    // FASTEST MULTI-LEG ROUTE â€” PURE LINKED LIST (NO VECTOR!)
    /*
    PathResult findFastestMultiLeg(const MultiLegJourney& journey, bool useAStar = true) {
        PathResult finalResult;
        finalResult.found = true;
        finalResult.totalTimeMinutes = 0;

        if (journey.legCount < 2) {
            finalResult.found = false;
            return finalResult;
        }

        JourneyLeg* fromLeg = journey.head;
        JourneyLeg* toLeg = fromLeg->next;

        while (toLeg != nullptr) {
            int from = fromLeg->portIndex;
            int to = toLeg->portIndex;

            PathResult segment;
            if (useAStar)
                segment = aStarFastestWithPrefs(from, to, "", {}, -1);
            else
                segment = dijkstraFastestWithPrefs(from, to, "", {}, -1);

            if (!segment.found) {
                finalResult.found = false;
                addLog("No route from " + graphPorts[from]->portName + " to " + graphPorts[to]->portName);
                return finalResult;
            }

            // Append segment path (skip first node â€” already in previous leg)
            for (int i = 1; i < segment.pathSize; i++) {
                finalResult.path[finalResult.pathSize++] = segment.path[i];
            }

            finalResult.totalTimeMinutes += segment.totalTimeMinutes;

            // Move to next pair
            fromLeg = toLeg;
            toLeg = toLeg->next;
        }

        return finalResult;
    }
    */
    /*
    PathResult findFastestMultiLeg(const MultiLegJourney& journey, bool useAStar = true) {
        PathResult finalResult;
        finalResult.found = true;
        finalResult.totalTimeMinutes = 0;

        if (journey.legCount < 2) {
            finalResult.found = false;
            return finalResult;
        }

        JourneyLeg* fromLeg = journey.head;
        JourneyLeg* toLeg = fromLeg->next;

        while (toLeg != nullptr) {
            int from = fromLeg->portIndex;
            int to = toLeg->portIndex;

            PathResult segment = useAStar ?
                aStarFastestWithPrefs(from, to, "", {}, -1) :
                dijkstraFastestWithPrefs(from, to, "", {}, -1);

            if (!segment.found || segment.pathSize == 0) {
                finalResult.found = false;
                return finalResult;
            }

            // â†â†â†â†â† SAFE COPY â†’â†’â†’â†’â†’
            for (int j = 1; j < segment.pathSize; j++) {
                if (finalResult.pathSize < 510) {  // leave room for null terminator if needed
                    //finalResult.
                }
            }

            finalResult.totalTimeMinutes += segment.totalTimeMinutes;

            fromLeg = toLeg;
            toLeg = toLeg->next;
        }

        return finalResult;
    }
    */

    /*
    PathResult findFastestMultiLeg(const MultiLegJourney& journey, bool useAStar = true) {
        PathResult result;
        result.path = nullptr;
        result.pathSize = 0;
        result.totalCost = 0;
        result.totalTimeMinutes = 0;
        result.found = true;

        if (journey.legCount < 2) {
            result.found = false;
            return result;
        }

        // Temporary buffer â€” safe zone
        int tempPath[1024];
        int tempSize = 0;

        JourneyLeg* fromLeg = journey.head;
        JourneyLeg* toLeg = fromLeg->next;

        while (toLeg != nullptr && result.found) {
            int from = fromLeg->portIndex;
            int to = toLeg->portIndex;

            PathResult segment = useAStar ?
                aStarFastestWithPrefs(from, to, "", {}, -1) :
                dijkstraFastestWithPrefs(from, to, "", {}, -1);

            if (!segment.found || segment.pathSize == 0) {
                result.found = false;
                addLog("No route from " + graphPorts[from]->portName + " to " + graphPorts[to]->portName);
                break;
            }

            // Append segment (skip first node â€” already added)
            for (int j = 1; j < segment.pathSize; j++) {
                if (tempSize >= 1020) {  // safety limit
                    addLog("Warning: Multi-leg path too long â€” truncated!");
                    result.found = true;
                    goto allocate_final;
                }
                tempPath[tempSize++] = segment.path[j];
            }

            result.totalTimeMinutes += segment.totalTimeMinutes;

            fromLeg = toLeg;
            toLeg = toLeg->next;
        }

    allocate_final:
        if (result.found && tempSize > 0) {
            result.path = new int[tempSize];
            for (int i = 0; i < tempSize; i++) {
                result.path[i] = tempPath[i];
            }
            result.pathSize = tempSize;
        }

        return result;
    }
    */
    void findCheapestRoute(int originIndex, int destinationIndex, const string& filterCompany = "")
    {
        if (originIndex < 0 || destinationIndex < 0) {
            cout << "Error: Please select both origin and destination ports." << endl;
            return;
        }

        if (useAStar)
            currentPath = aStarCheapestWithPrefs(originIndex, destinationIndex, filterCompany);
        else
            currentPath = dijkstraShortestPath(originIndex, destinationIndex, filterCompany);

        if (currentPath.found && currentPath.path != nullptr && currentPath.pathSize > 0) {
            cout << "\n=== CHEAPEST ROUTE FOUND ===" << endl;
            cout << "From: " << graphPorts[originIndex]->portName << endl;
            cout << "To: " << graphPorts[destinationIndex]->portName << endl;
            cout << "Total Cost: $" << currentPath.totalCost << endl;

            /*
            // â†â†â†â†â† ADD THE QUEUE CODE RIGHT HERE â€” AFTER THE COUT LINES â†’â†’â†’â†’â†’
            
            for (int i = 0; i < currentPath.pathSize - 1; i++) {
                int portIdx = currentPath.path[i];

                WaitingShip ship;
                ship.company = filterCompany.empty() ? "CargoLine" : filterCompany;
                ship.arrivalTime = 100000 + i * 3000;
                ship.serviceEndTime = ship.arrivalTime + 8 * 60;

                graphPorts[portIdx]->dockingQueue.push_back(ship);
            }
            // â†â†â†â†â† END OF QUEUE CODE â†’â†’â†’â†’â†’
            */
            // â†â†â†â†â† ADD SHIPS TO QUEUE â€” RIGHT HERE â†’â†’â†’â†’â†’
            for (int i = 0; i < currentPath.pathSize - 1; i++) {
                int portIdx = currentPath.path[i];

                WaitingShip ship;
                ship.company = selectedCompany.empty() ? "CargoLine" : selectedCompany;
                ship.arrivalTime = globalSimulationTime + i * 5000;
                ship.serviceEndTime = ship.arrivalTime + 8 * 60;

                graphPorts[portIdx]->dockingQueue.push_back(ship);
                cout << "ADDED SHIP to " << graphPorts[portIdx]->portName << endl;
            }
            // â†â†â†â†â† END â†’â†’â†’â†’â†’

            // AFTER your queue-adding loop
            cout << "=== DOCKING QUEUE DEBUG ===" << endl;
            for (int i = 0; i < currentPath.pathSize - 1; i++) {
                int portIdx = currentPath.path[i];
                cout << "Port " << graphPorts[portIdx]->portName
                    << " now has " << graphPorts[portIdx]->dockingQueue.size()
                    << " ships in queue" << endl;
            }
            cout << "==========================" << endl;

            addLog("=== CHEAPEST ROUTE FOUND ===");
            addLog("From: " + graphPorts[originIndex]->portName);

            addLog("=== DOCKING QUEUE DEBUG ===");
            for (int i = 0; i < currentPath.pathSize - 1; i++) {
                int portIdx = currentPath.path[i];
                addLog("Port " + graphPorts[portIdx]->portName +
                    " now has " + to_string(graphPorts[portIdx]->dockingQueue.size()) +
                    " ships in queue");
            }
            addLog("==========================");

            cout << "Path: ";
            for (int i = 0; i < currentPath.pathSize; i++) {
                cout << graphPorts[currentPath.path[i]]->portName;
                if (i < currentPath.pathSize - 1) cout << " -> ";
            }

            // Copy to highlightedPath
            highlightedPath.clear();
            for (int i = 0; i < currentPath.pathSize; i++) {
                highlightedPath.push_back(currentPath.path[i]);
            }
            cout << endl;
        }
        else {
            cout << "\nNo route found." << endl;
        }
    }

    void findFastestRoute(int originIndex, int destinationIndex, const string& filterCompany = "")
    {
        if (originIndex < 0 || destinationIndex < 0) {
            cout << "Error: Please select both origin and destination ports." << endl;
            return;
        }

        if (useAStar)
            currentPath = aStarFastestWithPrefs(originIndex, destinationIndex, filterCompany);
        else
            currentPath = dijkstraFastestWithPrefs(originIndex, destinationIndex, filterCompany);

        if (currentPath.found && currentPath.path != nullptr && currentPath.pathSize > 0) {
            cout << "\n=== FASTEST ROUTE FOUND ===" << endl;
            cout << "From: " << graphPorts[originIndex]->portName << endl;
            cout << "To: " << graphPorts[destinationIndex]->portName << endl;

            /*
            // â†â†â†â†â† ADD THE EXACT SAME QUEUE CODE HERE â†’â†’â†’â†’â†’
            for (int i = 0; i < currentPath.pathSize - 1; i++) {
                int portIdx = currentPath.path[i];

                WaitingShip ship;
                ship.company = filterCompany.empty() ? "ExpressLine" : filterCompany;
                ship.arrivalTime = 100000 + i * 3000;
                ship.serviceEndTime = ship.arrivalTime + 8 * 60;

                graphPorts[portIdx]->dockingQueue.push_back(ship);
            }
            // â†â†â†â†â† END â†’â†’â†’â†’â†’
            */
          
            // AFTER your queue-adding loop
            cout << "=== DOCKING QUEUE DEBUG ===" << endl;
            for (int i = 0; i < currentPath.pathSize - 1; i++) {
                int portIdx = currentPath.path[i];
                cout << "Port " << graphPorts[portIdx]->portName
                    << " now has " << graphPorts[portIdx]->dockingQueue.size()
                    << " ships in queue" << endl;
            }
            cout << "==========================" << endl;

            addLog("=== FASTEST ROUTE FOUND ===");
            addLog("From: " + graphPorts[originIndex]->portName);
            

            cout << "Path: ";
            for (int i = 0; i < currentPath.pathSize; i++) {
                cout << graphPorts[currentPath.path[i]]->portName;
                if (i < currentPath.pathSize - 1) cout << " -> ";
            }
            cout << endl;

            // Copy to highlightedPath
            highlightedPath.clear();
            for (int i = 0; i < currentPath.pathSize; i++) {
                highlightedPath.push_back(currentPath.path[i]);
            }
        }
        else {
            cout << "\nNo route found." << endl;
        }
    }


    sf::Color getCostColor(int cost) const {
        if (cost < 15000) return sf::Color(100, 255, 150, 200);      // Green - Low cost
        else if (cost < 25000) return sf::Color(255, 230, 100, 200); // Yellow - Medium cost
        else if (cost < 35000) return sf::Color(255, 180, 100, 200); // Orange - High cost
        else return sf::Color(255, 100, 100, 200);                   // Red - Very high cost
    }

    void assignPositions(float width, float height) {
        if (currentSize == 0) return;
        float mapWidth = width;
        float mapHeight = height;
        for (int i = 0; i < currentSize; i++) {
            if (graphPorts[i] == nullptr) continue;
            if (graphPorts[i]->hasCoordinates) {
                float lon = graphPorts[i]->longitude;
                float lat = graphPorts[i]->latitude;
                // Simple equirectangular projection (linear mapping)
                float x = (lon + 180.0f) / 360.0f * mapWidth;
                float y = (90.0f - lat) / 180.0f * mapHeight;
                graphPorts[i]->position = sf::Vector2f(x, y);
            }
            else {
                // If no coordinates, do not place the port
                graphPorts[i]->position = sf::Vector2f(-1000.f, -1000.f); // Off-screen
            }
        }
    }

    // Convert lat/lon (degrees) to screen coordinates
    static sf::Vector2f latLonToScreen(float lat, float lon, float width, float height) {
        float x = (lon + 180.0f) / 360.0f * width;
        float y = (90.0f - lat) / 180.0f * height;
        return sf::Vector2f(x, y);
    }

    bool loadPortCoordinates(const string& fileNameForCoords) {
        ifstream file(fileNameForCoords);
        if (!file.is_open()) {
            return false;
        }
        string line;
        while (getline(file, line)) {
            if (line.empty()) continue;
            string portName;
            double lat = 0.0, lon = 0.0;
            stringstream ss(line);
            if (!(ss >> portName >> lat >> lon)) {
                // Could be a multi-word port name, try to parse with last two tokens as lat lon
                vector<string> tokens;
                string token;
                ss.clear(); ss.str(line);
                while (ss >> token) tokens.push_back(token);
                if (tokens.size() < 3) continue;
                try {
                    lat = stod(tokens[tokens.size() - 2]);
                    lon = stod(tokens[tokens.size() - 1]);
                }
                catch (...) { continue; }
                portName = "";
                for (size_t i = 0; i < tokens.size() - 2; ++i) {
                    if (i > 0) portName += " ";
                    portName += tokens[i];
                }
            }
            // Assign coordinates to the port if found
            for (int i = 0; i < currentSize; i++) {
                if (graphPorts[i] && graphPorts[i]->portName == portName) {
                    graphPorts[i]->latitude = static_cast<float>(lat);
                    graphPorts[i]->longitude = static_cast<float>(lon);
                    graphPorts[i]->hasCoordinates = true;
                    break;
                }
            }
        }
        file.close();
        return true;
    }


    void handlePanelAction(const string& action, bool& showQueryPanel)
    {
        cout << "Action pressed: " << action << endl;
        
        if (action == "Find Cheapest Route") {
            if (selectedOrigin != -1 && selectedDestination != -1) {
                highlightedPath.clear();  // â† THIS FIXES THE HIGHLIGHT BUG
                string companyToUse = showOnlyActiveRoutes ? activeCompanyFilter : selectedCompany;
                // Always use COST-BASED algorithms for "Cheapest"
                if (useAStar)
                    currentPath = aStarCheapestWithPrefs(selectedOrigin, selectedDestination, companyToUse);
                else
                    currentPath = dijkstraShortestPath(selectedOrigin, selectedDestination, companyToUse);

                // Copy path to highlightedPath
                highlightedPath.clear();
                if (currentPath.found && currentPath.pathSize > 0) {
                    for (int i = 0; i < currentPath.pathSize; i++) {
                        highlightedPath.push_back(currentPath.path[i]);
                    }
                }
                showQueryPanel = false;

                cout << "\n=== CHEAPEST ROUTE FOUND (A* enabled: " << (useAStar ? "YES" : "NO") << ") ===" << endl;
                cout << "From: " << graphPorts[selectedOrigin]->portName << endl;
                cout << "To: " << graphPorts[selectedDestination]->portName << endl;
                cout << "Total Cost: $" << currentPath.totalCost << endl;
            }
            else {
                cout << "Error: Please select both origin and destination ports first." << endl;
            }
        }
        else if (action == "Find Fastest Route") {
            if (selectedOrigin != -1 && selectedDestination != -1) {
                highlightedPath.clear();  // â† THIS FIXES THE HIGHLIGHT BUG
                string companyToUse = showOnlyActiveRoutes ? activeCompanyFilter : selectedCompany;
                // Always use TIME-BASED algorithms for "Fastest"
                if (useAStar)
                    currentPath = aStarFastestWithPrefs(selectedOrigin, selectedDestination, companyToUse);
                else
                    currentPath = dijkstraFastestWithPrefs(selectedOrigin, selectedDestination, companyToUse);

                // Copy path to highlightedPath
                highlightedPath.clear();
                if (currentPath.found && currentPath.pathSize > 0) {
                    for (int i = 0; i < currentPath.pathSize; i++) {
                        highlightedPath.push_back(currentPath.path[i]);
                    }
                }

                cout << "\n=== FASTEST ROUTE FOUND (A* enabled: " << (useAStar ? "YES" : "NO") << ") ===" << endl;
                cout << "From: " << graphPorts[selectedOrigin]->portName << endl;
                cout << "To: " << graphPorts[selectedDestination]->portName << endl;
                cout << "Total Time: " << currentPath.totalTimeMinutes << " minutes" << endl;

                showQueryPanel = false;
            }
            else {
                cout << "Error: Please select both origin and destination ports first." << endl;
            }
        }
        else if (action == "ToggleAlgorithm")
        {
            useAStar = !useAStar;
            cout << "Algorithm switched to: "
                << (useAStar ? "A*" : "Dijkstra") << endl;
        }
        else if (action == "Build Multi-Leg Route") {
            buildingMultiLeg = !buildingMultiLeg;

            if (buildingMultiLeg) {
                cout << "Multi-leg mode ON â€” click ports in order" << endl;
                // DO NOT CLEAR â€” keep previous journey!
                currentJourney.clear(); // â† DELETE THIS LINE
                if (currentJourney.legCount == 0) {
                    addLog("Started new multi-leg journey");
                }
            }
            else {
                cout << "Multi-leg mode OFF" << endl;
                addLog("Multi-leg journey saved (" + to_string(currentJourney.legCount) + " legs)");
            }
        }
        else if (action == "Filter by Company") {
            if (selectedCompany.empty()) {
                // If no company typed â†’ turn OFF filter
                showOnlyActiveRoutes = false;
                activeCompanyFilter = "";
                addLog("Company filter OFF â€” showing all routes");
            }
            else {
                // Turn ON filter with selected company
                activeCompanyFilter = selectedCompany;
                showOnlyActiveRoutes = true;
                // â† THIS ACTIVATES THE FILTER
                addLog("Company filter ON: " + activeCompanyFilter);
            }

            generateSubgraph();  // â† REBUILD VISIBLE PORTS/EDGES
            highlightedPath.clear();  // â† clear old path
            showQueryPanel = false;
        }
        else if (action == "Generate Subgraph") {
            runSubgraph = true;
            // TODO: Implement subgraph generation
        }
        else if (action == "Simulate Journey") {
            runSimulation = true;
            // TODO: Implement journey simulation
        }
    }

    void visualizeExplorationStep(int currentNode, sf::RenderWindow& window, sf::Sprite* mapSprite, bool mapLoaded, int windowWidth, int windowHeight, sf::Font& font) {
        if (!showAlgorithmVisualization) return;

        // Background
        if (mapLoaded && mapSprite) {
            window.draw(*mapSprite);
        }
        else {
            window.clear(sf::Color(15, 20, 35));
        }

        // Grid
        for (int x = 0; x < windowWidth; x += 50) {
            sf::RectangleShape line(sf::Vector2f(1, (float)windowHeight));
            line.setPosition(sf::Vector2f((float)x, 0.0f));
            line.setFillColor(sf::Color(30, 35, 50, 30));
            window.draw(line);
        }
        for (int y = 0; y < windowHeight; y += 50) {
            sf::RectangleShape line(sf::Vector2f((float)windowWidth, 1));
            line.setPosition(sf::Vector2f(0.0f, (float)y));
            line.setFillColor(sf::Color(30, 35, 50, 30));
            window.draw(line);
        }

        // Dim edges
        for (int i = 0; i < currentSize; i++) {
            if (!graphPorts[i]) continue;
            graphEdgeRoute* r = graphPorts[i]->routeHead;
            while (r) {
                int v = getPortIndex(r->destinationName);
                if (v != -1 && graphPorts[v]) {
                    sf::Vertex line[] = {
                        { sf::Vector2f(graphPorts[i]->position), sf::Color(80,80,80,80) },
                        { sf::Vector2f(graphPorts[v]->position), sf::Color(80,80,80,80) }
                    };
                    window.draw(line, 2, sf::PrimitiveType::Lines);
                }
                r = r->next;
            }
        }

        // Nodes
        for (int i = 0; i < currentSize; i++) {
            if (!graphPorts[i]) continue;

            sf::Color color = sf::Color(60, 120, 200);

            // Closed = red
            for (int j = 0; j < vizClosedSet.size(); j++) {
                if (vizClosedSet[j] == i) color = sf::Color(255, 100, 100);
            }
            // Open = yellow
            for (int j = 0; j < vizOpenSet.size(); j++) {
                if (vizOpenSet[j] == i) color = sf::Color(255, 255, 100);
            }
            // Current = green
            if (i == currentNode) color = sf::Color(100, 255, 100);
            // Selected
            if (i == selectedOrigin) color = sf::Color(100, 255, 150);
            if (i == selectedDestination) color = sf::Color(255, 180, 100);

            sf::CircleShape node(16);
            node.setPosition(graphPorts[i]->position - sf::Vector2f(16, 16));
            node.setFillColor(color);
            node.setOutlineThickness(4);
            node.setOutlineColor(sf::Color::White);
            window.draw(node);
        }

        window.display();
        sf::sleep(sf::milliseconds(100));
    }
    

    void visualizeGraph(int windowWidth = 1800, int windowHeight = 900) {
        // Try to load optional coordinates and world map before assigning positions
        // If a PortCoords.txt file exists in the workspace, load it
        loadPortCoordinates("./PortCoords.txt");
        assignPositions(static_cast<float>(windowWidth), static_cast<float>(windowHeight));

        sf::RenderWindow window(sf::VideoMode({ static_cast<unsigned int>(windowWidth), static_cast<unsigned int>(windowHeight) }), "Ocean Route Network - Interactive Graph", sf::Style::Close);
        window.setFramerateLimit(160);

        sf::Texture mapTexture;
        sf::Sprite* mapSprite = nullptr;
        bool mapLoaded = false;
        if (mapTexture.loadFromFile("./world_map.png")) {
            mapSprite = new sf::Sprite(mapTexture);
            // Scale to window size
            sf::Vector2u texSize = mapTexture.getSize();
            float scaleX = static_cast<float>(windowWidth) / static_cast<float>(texSize.x);
            float scaleY = static_cast<float>(windowHeight) / static_cast<float>(texSize.y);
            mapSprite->setScale(sf::Vector2f(scaleX, scaleY));
            mapLoaded = true;
        }

        sf::Font font;
        if (!font.openFromFile("/System/Library/Fonts/Helvetica.ttc")) {
            if (!font.openFromFile("C:\\Windows\\Fonts\\Arial.ttf")) {
                if (!font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
                    cerr << "Could not load font!" << endl;
                    return;
                }
            }
        }


        // State variables for future features
        int hoveredPort = -1;
        int selectedPort = -1; // For route highlighting and pathfinding
        // Note: selectedOrigin, selectedDestination, and selectedCompany are now member variables
        string selectedDate = ""; // Selected date for filtering
        float animationTime = 0.0f; // For sequential animations
        sf::Clock animationClock; // For timing animations

        // UI Panel state
        bool showQueryPanel = false; // Toggle query panel visibility
        int hoveredButton = -1; // Which button is being hovered (-1 = none)
        bool selectingOrigin = true; // Toggle between selecting origin/destination

        // Algorithm toggle state
        bool useAStar = false; // false = Dijkstra, true = A*

        // Path highlighting
        //std::vector<int> highlightedPath;

        // Button positions for click detection (will be set during rendering)
        struct ButtonBounds {
            float x, y, width, height;
            string action;
        };
        SimpleVector<ButtonBounds, 16> buttonBounds;

        // Toggle button bounds
        ButtonBounds toggleButtonBounds;

        while (window.isOpen()) {
            while (std::optional<sf::Event> event = window.pollEvent()) {
                // Recalculate mouse position here
                sf::Vector2i mp = sf::Mouse::getPosition(window);
                sf::Vector2f clickPosF(static_cast<float>(mp.x), static_cast<float>(mp.y));

                if (event->is<sf::Event::Closed>()) {
                    window.close();
                }

                // Mouse click handling
                if (event->is<sf::Event::MouseButtonPressed>()) {
                    float mx = clickPosF.x;
                    float my = clickPosF.y;
                    // Toggle query panel button click
                    if (mx >= toggleButtonBounds.x && mx <= toggleButtonBounds.x + toggleButtonBounds.width &&
                        my >= toggleButtonBounds.y && my <= toggleButtonBounds.y + toggleButtonBounds.height)
                    {
                        showQueryPanel = !showQueryPanel;
                        continue;
                    }
                    // If panel is open: check panel action buttons
                    if (showQueryPanel)
                    {
                        for (auto& btn : buttonBounds)
                        {
                            if (mx >= btn.x && mx <= btn.x + btn.width &&
                                my >= btn.y && my <= btn.y + btn.height)
                            {

                                if (btn.action == "ToggleAlgorithm") {
                                    useAStar = !useAStar;
                                    cout << "Algorithm switched to: "
                                        << (useAStar ? "A*" : "Dijkstra") << endl;

                                    highlightedPath.clear();   // â­ NOW WE CAN CLEAR IT HERE
                                    continue;
                                } 

                                if (btn.action == "Build Multi-Leg Route") {
                                    
                                    buildingMultiLeg = !buildingMultiLeg;
                                    if (!buildingMultiLeg) {
                                        cout << "Multi-leg mode OFF" << endl;
                                    }
                                    else {
                                        cout << "Multi-leg mode ON â€” click ports in order" << endl;
                                    }
                                    currentJourney = MultiLegJourney();  // clear old journey
                                    highlightedPath.clear();             // optional: clear old path
                                    continue;
                                    
                                   
                                }

                                if (btn.action == "Simulate Journey") {
                                    
                                    simulationMode = true;
                                    if (currentPath.found && currentPath.pathSize > 0) {
                                        cout << "SIMULATING JOURNEY..." << endl;
                                        showQueryPanel = false;
                                        // ADD SHIPS TO EVERY PORT ON THE PATH
                                        for (int i = 0; i < currentPath.pathSize - 1; i++) {
                                            int portIdx = currentPath.path[i];

                                            WaitingShip ship;
                                            ship.company = selectedCompany.empty() ? "CargoLine" : selectedCompany;
                                            ship.arrivalTime = globalSimulationTime + i * 5000;
                                            ship.serviceEndTime = ship.arrivalTime + 8 * 60;

                                            graphPorts[portIdx]->dockingQueue.push_back(ship);
                                            cout << "Ship arrived at " << graphPorts[portIdx]->portName << endl;
                                        }

                                        globalSimulationTime += 100000;  // advance time

                                        // â†â†â†â† START WHOOSH EFFECT â†’â†’â†’â†’
                                        showWhoosh = true;
                                        whooshProgress = 0.0f;
                                        whooshClock.restart();
                                        // â†â†â†â† END â†’â†’â†’â†’
                                    }

                                    //simulationMode = false;
                                    

                                    /*
                                    simulationMode = true;
                                    if (currentJourney.legCount < 2) {
                                        addLog("Error: Build a multi-leg journey with at least 2 stops first!");
                                        return;
                                    }

                                    addLog("SIMULATING MULTI-LEG JOURNEY...");

                                    // â†â†â†â†USE MULTI-LEG FASTEST PATH (A* OR DIJKSTRA) â†’â†’â†’â†’â†’
                                    
                                    currentPath = findFastestMultiLeg(currentJourney, useAStar);

                                    if (!currentPath.found) {
                                        addLog("No complete route found for this journey!");
                                        return;
                                    }

                                    // â†â†â†â†â† COPY TO HIGHLIGHTED PATH FOR DRAWING â†’â†’â†’â†’â†’
                                    highlightedPath.clear();
                                    for (int i = 0; i < currentPath.pathSize; i++) {
                                        highlightedPath.push_back(currentPath.path[i]);
                                    }
                                    

                                    // â†â†â†â†â† ADD SHIPS TO EVERY PORT ON THE FINAL ROUTE â†’â†’â†’â†’â†’
                                    for (int i = 0; i < currentPath.pathSize - 1; i++) {
                                        int portIdx = currentPath.path[i];

                                        WaitingShip ship;
                                        ship.company = selectedCompany.empty() ? "CargoLine" : selectedCompany;
                                        ship.arrivalTime = globalSimulationTime + i * 8000;  // spaced out
                                        ship.serviceEndTime = ship.arrivalTime + 8 * 60;

                                        graphPorts[portIdx]->dockingQueue.push_back(ship);
                                        addLog("Ship arrived at " + graphPorts[portIdx]->portName);
                                    }

                                    // â†â†â†â†â† ADVANCE TIME + START WHOOSH â†’â†’â†’â†’â†’
                                    globalSimulationTime += 150000;
                                    showWhoosh = true;
                                    whooshProgress = 0.0f;
                                    whooshClock.restart();

                                    addLog("Journey simulation started â€” " + to_string(currentPath.totalTimeMinutes / 60) + " hours total");
                                    showQueryPanel = false;
                                    */
                                }

                                if (btn.action == "Filter by Company") {
                                    activeCompanyFilter = selectedCompany;
                                    showOnlyActiveRoutes = !showOnlyActiveRoutes;
                                    generateSubgraph();
                                }

                                
                                
                                handlePanelAction(btn.action, showQueryPanel);
                            }
                        }
                        // Clicking inside date box â†’ start typing date
                        float panelX = (windowWidth - 400.0f) / 2.0f;
                        float panelY = 100.0f;
                        float dateBoxX = panelX + 20.0f;
                        float dateBoxY = panelY + 60.0f + 75.0f + 75.0f;
                        // If clicked inside date text box
                        if (mx >= dateBoxX && mx <= dateBoxX + 360.0f &&
                            my >= dateBoxY + 25.0f && my <= dateBoxY + 60.0f)
                        {
                            typingDate = true;
                            typingCompany = false;
                            continue;
                        }
                        // If clicked inside company text box
                        float companyBoxY = dateBoxY + 75.0f;
                        if (mx >= dateBoxX && mx <= dateBoxX + 360.0f &&
                            my >= companyBoxY + 25.0f && my <= companyBoxY + 60.0f)
                        {
                            typingCompany = true;
                            typingDate = false;
                            continue;
                        }
                        continue;
                    }
                    // If panel closed: allow ports to be selected

                    if (!showQueryPanel) {
                        /*
                        if (hoveredPort != -1) {
                            if (buildingMultiLeg) {
                                // MULTI-LEG MODE: Add port to journey
                                currentJourney.addLeg(hoveredPort);
                                cout << "Added: " << graphPorts[hoveredPort]->portName << endl;
                            }
                            else {
                                // NORMAL MODE: Select origin/destination
                                if (selectedOrigin == -1) {
                                    selectedOrigin = hoveredPort;
                                    cout << "Origin: " << graphPorts[hoveredPort]->portName << endl;
                                }
                                else if (selectedDestination == -1) {
                                    selectedDestination = hoveredPort;
                                    cout << "Destination: " << graphPorts[hoveredPort]->portName << endl;
                                }
                                else {
                                    // Click again to change origin
                                    selectedOrigin = hoveredPort;
                                    selectedDestination = -1;
                                    cout << "Origin changed to: " << graphPorts[hoveredPort]->portName << endl;
                                }
                            }
                        }
                        */
                        if (buildingMultiLeg) {
                            // â†â†â†â†â† CLICK-TO-REMOVE + CLICK-TO-ADD â†’â†’â†’â†’â†’
                            bool removed = false;
                            JourneyLeg* leg = currentJourney.head;
                            while (leg) {
                                sf::Vector2f pos = graphPorts[leg->portIndex]->position;
                                float dx = clickPosF.x - pos.x;
                                float dy = clickPosF.y - pos.y;
                                if (dx * dx + dy * dy < 400) {
                                    int portToRemove = leg->portIndex;  // â† SAVE FIRST!
                                    string portName = graphPorts[portToRemove]->portName;

                                    currentJourney.removeLeg(portToRemove);  // now safe to delete

                                    addLog("Removed stop: " + portName);
                                    removed = true;
                                    break;
                                }
                                leg = leg->next;
                            }

                            if (!removed) {
                                currentJourney.addLeg(hoveredPort);
                                addLog("Added stop: " + graphPorts[hoveredPort]->portName);
                            }
                            // â†â†â†â†â† END â†’â†’â†’â†’â†’
                        }
                        else {
                            // NORMAL MODE: Select origin/destination
                            if (selectedOrigin == -1) {
                                selectedOrigin = hoveredPort;
                                cout << "Origin: " << graphPorts[hoveredPort]->portName << endl;
                            }
                            else if (selectedDestination == -1) {
                                selectedDestination = hoveredPort;
                                cout << "Destination: " << graphPorts[hoveredPort]->portName << endl;
                            }
                            else {
                                selectedOrigin = hoveredPort;
                                selectedDestination = -1;
                                cout << "Origin changed to: " << graphPorts[hoveredPort]->portName << endl;
                            }
                        }
                    }
                }
                // KEYBOARD INPUT HANDLING
                if (event->is<sf::Event::TextEntered>()) {
                    const auto* textEvent = event->getIf<sf::Event::TextEntered>();
                    if (textEvent) {
                        char c = static_cast<char>(textEvent->unicode);
                        // Ignore control characters except backspace
                        if (c == 8) { // Backspace
                            if (typingDate && !selectedDate.empty()) selectedDate.pop_back();
                            if (typingCompany && !selectedCompany.empty()) selectedCompany.pop_back();
                        }
                        else if (c >= 32 && c <= 126) { // Only printable characters
                            if (typingDate) {
                                if (selectedDate.size() < 10)   // dd/mm/yyyy
                                    selectedDate += c;
                                cout << selectedDate << endl;
                            }
                            if (typingCompany) {
                                if (selectedCompany.size() < 20)
                                    selectedCompany += c;
                                cout << selectedCompany << endl;
                                
                            }
                        }
                    }
                }
                



                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                sf::Vector2f mousePosF(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));

                hoveredPort = -1;
                for (int i = 0; i < currentSize; i++) {
                    if (graphPorts[i] == nullptr) continue;
                    float dx = mousePosF.x - graphPorts[i]->position.x;
                    float dy = mousePosF.y - graphPorts[i]->position.y;
                    float nodeRadius = 12.0f;
                    float hoverThresh = (nodeRadius + 10.0f) * (nodeRadius + 10.0f);
                    if (dx * dx + dy * dy < hoverThresh) {
                        hoveredPort = i;
                        break;
                    }
                }

                if (mapLoaded) {
                    window.clear(sf::Color::Black);
                    if (mapSprite) window.draw(*mapSprite);
                }
                else {
                    window.clear(sf::Color(15, 20, 35));
                }
                // Reset for this frame
                buttonBounds.clear();
                hoveredButton = -1;



                for (int x = 0; x < windowWidth; x += 50) {
                    sf::RectangleShape line(sf::Vector2f(1.0f, static_cast<float>(windowHeight)));
                    line.setPosition(sf::Vector2f(static_cast<float>(x), 0.0f));
                    line.setFillColor(sf::Color(30, 35, 50, 30));
                    window.draw(line);
                }
                for (int y = 0; y < windowHeight; y += 50) {
                    sf::RectangleShape line(sf::Vector2f(static_cast<float>(windowWidth), 1.0f));
                    line.setPosition(sf::Vector2f(0.0f, static_cast<float>(y)));
                    line.setFillColor(sf::Color(30, 35, 50, 30));
                    window.draw(line);
                }

                // Draw title with enhanced UI space for controls
                sf::Text title(font, "OCEAN SHIPPING ROUTE NETWORK", 26);
                title.setFillColor(sf::Color(120, 200, 255));
                title.setStyle(sf::Text::Bold);
                title.setPosition(sf::Vector2f(30.0f, 15.0f));
                window.draw(title);

                // Status indicators
                sf::Text legend(font, "Hover: View Details | Click: Select Port | Red Routes: Hover to Reveal Cost", 13);
                legend.setFillColor(sf::Color(180, 180, 180));
                legend.setPosition(sf::Vector2f(30.0f, 50.0f));
                window.draw(legend);

                // ================== TOGGLE QUERY PANEL BUTTON ==================
                sf::RectangleShape toggleButton(sf::Vector2f(180.f, 40.f));
                toggleButton.setPosition(sf::Vector2f(windowWidth - 220.f, 20.f));

                toggleButtonBounds = {
                    windowWidth - 220.f, 20.f,
                    180.f, 40.f,
                    "ToggleQueryPanel"
                };

                toggleButton.setFillColor(showQueryPanel ? sf::Color(200, 50, 50) : sf::Color(50, 150, 250));
                toggleButton.setOutlineThickness(2.f);
                toggleButton.setOutlineColor(sf::Color::White);
                window.draw(toggleButton);

                sf::Text toggleText(font, showQueryPanel ? "Close Panel" : "Open Panel", 16);
                toggleText.setFillColor(sf::Color::White);
                toggleText.setPosition(sf::Vector2f(windowWidth - 205.f, 28.f));
                window.draw(toggleText);


                // ========== UI QUERY PANEL (POPUP - Only show if toggled) ==========
                if (showQueryPanel) {
                    float panelX = (windowWidth - 400.0f) / 2.0f; // Center horizontally
                    float panelY = 100.0f;
                    float panelWidth = 400.0f;
                    float panelHeight = windowHeight - 200.0f;

                    // Semi-transparent overlay
                    sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(windowWidth), static_cast<float>(windowHeight)));
                    overlay.setPosition(sf::Vector2f(0.0f, 0.0f));
                    overlay.setFillColor(sf::Color(0, 0, 0, 150));
                    window.draw(overlay);

                    // Main panel background
                    sf::RectangleShape mainPanel(sf::Vector2f(panelWidth, panelHeight));
                    mainPanel.setPosition(sf::Vector2f(panelX, panelY));
                    mainPanel.setFillColor(sf::Color(20, 25, 40, 250));
                    mainPanel.setOutlineThickness(3.0f);
                    mainPanel.setOutlineColor(sf::Color(80, 150, 255));
                    window.draw(mainPanel);

                    // Panel title
                    sf::Text panelTitle(font, "ROUTE QUERY PANEL", 22);
                    panelTitle.setFillColor(sf::Color(120, 200, 255));
                    panelTitle.setStyle(sf::Text::Bold);
                    panelTitle.setPosition(sf::Vector2f(panelX + 20.0f, panelY + 15.0f));
                    window.draw(panelTitle);

                    float yOffset = panelY + 60.0f;

                    // --- ORIGIN PORT SELECTION ---
                    sf::Text originLabel(font, "Origin Port:", 16);
                    originLabel.setFillColor(sf::Color(200, 210, 230));
                    originLabel.setStyle(sf::Text::Bold);
                    originLabel.setPosition(sf::Vector2f(panelX + 20.0f, yOffset));
                    window.draw(originLabel);

                    sf::RectangleShape originBox(sf::Vector2f(360.0f, 35.0f));
                    originBox.setPosition(sf::Vector2f(panelX + 20.0f, yOffset + 25.0f));
                    originBox.setFillColor(sf::Color(35, 40, 55));
                    originBox.setOutlineThickness(1.5f);
                    originBox.setOutlineColor(sf::Color(100, 120, 160));
                    window.draw(originBox);

                    string originText = (selectedOrigin != -1) ? graphPorts[selectedOrigin]->portName : "Click port on map...";
                    sf::Text originDisplay(font, originText, 14);
                    originDisplay.setFillColor((selectedOrigin != -1) ? sf::Color(100, 255, 150) : sf::Color(150, 160, 180));
                    originDisplay.setPosition(sf::Vector2f(panelX + 30.0f, yOffset + 33.0f));
                    window.draw(originDisplay);

                    yOffset += 75.0f;

                    // --- DESTINATION PORT SELECTION ---
                    sf::Text destLabel(font, "Destination Port:", 16);
                    destLabel.setFillColor(sf::Color(200, 210, 230));
                    destLabel.setStyle(sf::Text::Bold);
                    destLabel.setPosition(sf::Vector2f(panelX + 20.0f, yOffset));
                    window.draw(destLabel);

                    sf::RectangleShape destBox(sf::Vector2f(360.0f, 35.0f));
                    destBox.setPosition(sf::Vector2f(panelX + 20.0f, yOffset + 25.0f));
                    destBox.setFillColor(sf::Color(35, 40, 55));
                    destBox.setOutlineThickness(1.5f);
                    destBox.setOutlineColor(sf::Color(100, 120, 160));
                    window.draw(destBox);

                    string destText = (selectedDestination != -1) ? graphPorts[selectedDestination]->portName : "Click port on map...";
                    sf::Text destDisplay(font, destText, 14);
                    destDisplay.setFillColor((selectedDestination != -1) ? sf::Color(100, 255, 150) : sf::Color(150, 160, 180));
                    destDisplay.setPosition(sf::Vector2f(panelX + 30.0f, yOffset + 33.0f));
                    window.draw(destDisplay);

                    yOffset += 75.0f;

                    // --- DATE SELECTION ---
                    sf::Text dateLabel(font, "Select Date:", 16);
                    dateLabel.setFillColor(sf::Color(200, 210, 230));
                    dateLabel.setStyle(sf::Text::Bold);
                    dateLabel.setPosition(sf::Vector2f(panelX + 20.0f, yOffset));
                    window.draw(dateLabel);

                    sf::RectangleShape dateBox(sf::Vector2f(360.0f, 35.0f));
                    dateBox.setPosition(sf::Vector2f(panelX + 20.0f, yOffset + 25.0f));
                    dateBox.setFillColor(sf::Color(35, 40, 55));
                    dateBox.setOutlineThickness(1.5f);
                    // dateBox.setOutlineColor(sf::Color(100, 120, 160));
                    // Highlight if typing date
                    if (typingDate) {
                        dateBox.setOutlineColor(sf::Color(100, 255, 150));  // green
                    }
                    else {
                        dateBox.setOutlineColor(sf::Color(100, 120, 160));  // normal
                    }

                    window.draw(dateBox);

                    string dateText = selectedDate.empty() ? "DD/MM/YYYY" : selectedDate;
                    sf::Text dateDisplay(font, dateText, 14);
                    dateDisplay.setFillColor(selectedDate.empty() ? sf::Color(150, 160, 180) : sf::Color(100, 255, 150));
                    dateDisplay.setPosition(sf::Vector2f(panelX + 30.0f, yOffset + 33.0f));
                    window.draw(dateDisplay);

                    yOffset += 75.0f;

                    // --- COMPANY FILTER ---
                    sf::Text companyLabel(font, "Filter by Company:", 16);
                    companyLabel.setFillColor(sf::Color(200, 210, 230));
                    companyLabel.setStyle(sf::Text::Bold);
                    companyLabel.setPosition(sf::Vector2f(panelX + 20.0f, yOffset));
                    window.draw(companyLabel);

                    sf::RectangleShape companyBox(sf::Vector2f(360.0f, 35.0f));
                    companyBox.setPosition(sf::Vector2f(panelX + 20.0f, yOffset + 25.0f));
                    companyBox.setFillColor(sf::Color(35, 40, 55));
                    companyBox.setOutlineThickness(1.5f);
                    // companyBox.setOutlineColor(sf::Color(100, 120, 160));
                    // Highlight if typing company
                    if (typingCompany)
                        companyBox.setOutlineColor(sf::Color(255, 200, 120));  // orange
                    else
                        companyBox.setOutlineColor(sf::Color(100, 120, 160));  // normal

                    window.draw(companyBox);

                    string companyText = selectedCompany.empty() ? "All Companies" : selectedCompany;
                    sf::Text companyDisplay(font, companyText, 14);
                    companyDisplay.setFillColor(selectedCompany.empty() ? sf::Color(150, 160, 180) : sf::Color(255, 200, 100));
                    companyDisplay.setPosition(sf::Vector2f(panelX + 30.0f, yOffset + 33.0f));
                    window.draw(companyDisplay);

                    yOffset += 85.0f;

                    // Divider line
                    sf::RectangleShape divider(sf::Vector2f(360.0f, 2.0f));
                    divider.setPosition(sf::Vector2f(panelX + 20.0f, yOffset));
                    divider.setFillColor(sf::Color(80, 150, 255, 100));
                    window.draw(divider);

                    yOffset += 20.0f;

                    // --- ACTION BUTTONS ---
                    struct Button {
                        string label;
                        sf::Color color;
                    };

                    Button buttons[] = {
                        //{"Hi", sf::Color(100,200,120)},
                        {"Build Multi-Leg Route", sf::Color(100, 200, 255)},
                        {"Find Cheapest Route", sf::Color(100, 200, 120)},
                        {"Find Fastest Route", sf::Color(255, 180, 80)},
                        {"Filter by Company", sf::Color(180, 100, 220)},
                        {"Generate Subgraph", sf::Color(220, 120, 180)},
                        {"Simulate Journey", sf::Color(255, 140, 140)}
                    };

                    // --- ALGORITHM TOGGLE BUTTON ---
                    Button algoToggleBtn = { useAStar ? "Algorithm: A* (Click to switch)" : "Algorithm: Dijkstra (Click to switch)", sf::Color(80, 120, 200) };
                    float algoBtnYOffset = yOffset;
                    ButtonBounds algoBtnBounds;
                    algoBtnBounds.x = panelX + 20.0f;
                    algoBtnBounds.y = yOffset;
                    algoBtnBounds.width = 360.0f;
                    algoBtnBounds.height = 45.0f;
                    algoBtnBounds.action = "ToggleAlgorithm";
                    buttonBounds.push_back(algoBtnBounds);

                    sf::RectangleShape algoBtnShape(sf::Vector2f(360.0f, 45.0f));
                    algoBtnShape.setPosition(sf::Vector2f(panelX + 20.0f, yOffset));
                    algoBtnShape.setFillColor(algoToggleBtn.color);
                    algoBtnShape.setOutlineThickness(2.0f);
                    algoBtnShape.setOutlineColor(sf::Color(255, 255, 255, 100));
                    window.draw(algoBtnShape);

                    sf::Text algoBtnText(font, algoToggleBtn.label, 15);
                    algoBtnText.setFillColor(sf::Color(255, 255, 255));
                    algoBtnText.setStyle(sf::Text::Bold);
                    sf::FloatRect algoTextBounds = algoBtnText.getLocalBounds();
                    algoBtnText.setPosition(sf::Vector2f(panelX + 200.0f - algoTextBounds.size.x / 2.0f, yOffset + 12.0f));
                    window.draw(algoBtnText);

                    yOffset += 55.0f;

                    for (int i = 0; i < 6; i++) {
                        // Store button bounds for click detection
                        ButtonBounds btnBounds;
                        btnBounds.x = panelX + 20.0f;
                        btnBounds.y = yOffset;
                        btnBounds.width = 360.0f;
                        btnBounds.height = 45.0f;
                        btnBounds.action = buttons[i].label;
                        buttonBounds.push_back(btnBounds);

                        // Hover detection
                        if (mousePosF.x >= btnBounds.x &&
                            mousePosF.x <= btnBounds.x + btnBounds.width &&
                            mousePosF.y >= btnBounds.y &&
                            mousePosF.y <= btnBounds.y + btnBounds.height)
                        {
                            hoveredButton = i;
                        }


                        sf::RectangleShape button(sf::Vector2f(360.0f, 45.0f));
                        button.setPosition(sf::Vector2f(panelX + 20.0f, yOffset));

                        // Hover effect
                        if (hoveredButton == i) {
                            button.setFillColor(sf::Color(
                                my_min(buttons[i].color.r + 40, 255),
                                my_min(buttons[i].color.g + 40, 255),
                                my_min(buttons[i].color.b + 40, 255)
                            ));
                            button.setOutlineThickness(3.0f);
                            button.setOutlineColor(sf::Color(255, 255, 255, 200));
                        }
                        else {
                            button.setFillColor(buttons[i].color);
                            button.setOutlineThickness(2.0f);
                            button.setOutlineColor(sf::Color(255, 255, 255, 100));
                        }
                        window.draw(button);

                        sf::Text buttonText(font, buttons[i].label, 15);
                        buttonText.setFillColor(sf::Color(255, 255, 255));
                        buttonText.setStyle(sf::Text::Bold);
                        sf::FloatRect textBounds = buttonText.getLocalBounds();
                        buttonText.setPosition(sf::Vector2f(panelX + 200.0f - textBounds.size.x / 2.0f, yOffset + 12.0f));
                        window.draw(buttonText);

                        yOffset += 55.0f;
                    }

                    yOffset += 10.0f;

                    // --- AUTO PATHFINDING & HIGHLIGHTING ---
                    if (selectedOrigin != -1 && selectedDestination != -1 && highlightedPath.empty()) {
                        if (useAStar) {
                            //highlightedPath.clear();
                            findFastestRoute(selectedOrigin, selectedDestination, selectedCompany);
                        }
                        else {
                            highlightedPath.clear();
                            findCheapestRoute(selectedOrigin, selectedDestination, selectedCompany);
                        }
                        if (currentPath.found && currentPath.path != nullptr && currentPath.pathSize > 0) {
                            for (int i = 0; i < currentPath.pathSize; ++i) {
                                highlightedPath.push_back(currentPath.path[i]);
                            }
                        }
                    }

                    // Status message area with dynamic messages
                    sf::RectangleShape statusBox(sf::Vector2f(360.0f, 70.0f));
                    statusBox.setPosition(sf::Vector2f(panelX + 20.0f, yOffset));
                    statusBox.setFillColor(sf::Color(35, 40, 55, 200));
                    statusBox.setOutlineThickness(1.5f);
                    statusBox.setOutlineColor(sf::Color(100, 120, 160));
                    window.draw(statusBox);

                    // Dynamic status based on selections
                    string statusTitle = "Status: ";
                    string statusMessage = "";
                    sf::Color statusColor = sf::Color(180, 200, 220);

                    if (selectedOrigin != -1 && selectedDestination != -1) {
                        statusTitle += "Ready to Search";
                        statusMessage = "Click an action button above\nto find routes.";
                        statusColor = sf::Color(100, 255, 150);
                    }
                    else if (selectedOrigin != -1) {
                        statusTitle += "Origin Selected";
                        statusMessage = "Click a port to select\ndestination.";
                        statusColor = sf::Color(255, 200, 100);
                    }
                    else {
                        statusTitle += "Awaiting Selection";
                        statusMessage = "Click a port to select\norigin first.";
                        statusColor = sf::Color(180, 200, 220);
                    }

                    sf::Text statusText(font, statusTitle, 13);
                    statusText.setFillColor(statusColor);
                    statusText.setStyle(sf::Text::Bold);
                    statusText.setPosition(sf::Vector2f(panelX + 30.0f, yOffset + 10.0f));
                    window.draw(statusText);

                    sf::Text statusMsg(font, statusMessage, 12);
                    statusMsg.setFillColor(sf::Color(150, 160, 180));
                    statusMsg.setPosition(sf::Vector2f(panelX + 30.0f, yOffset + 32.0f));
                    window.draw(statusMsg);
                } // End of if (showQueryPanel)

                if (!showQueryPanel) {
                    // Draw all edges (routes) as dark red lines with arrowheads
                    // Draw all edges (routes) with proper highlighting
                    for (int i = 0; i < currentSize; i++) {
                        if (graphPorts[i] == nullptr) continue;
                        graphEdgeRoute* route = graphPorts[i]->routeHead;
                        sf::Vector2f p1 = graphPorts[i]->position;
                        while (route != nullptr) {
                            int destIndex = getPortIndex(route->destinationName);
                            if (destIndex != -1 && graphPorts[destIndex] != nullptr) {
                                sf::Vector2f p2 = graphPorts[destIndex]->position;

                                if (p1.x > 0 && p1.y > 0 && p2.x > 0 && p2.y > 0) {
                                    sf::Vector2f delta = p2 - p1;
                                    float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                                    float angle = std::atan2(delta.y, delta.x) * 180.f / 3.14159265f;

                                    //sf::Color edgeColor(120, 30, 30, 180); // default dark red
                                    //sf::Color edgeColor(50, 240, 255, 255);
                                    //sf::Color edgeColor(160, 32, 240, 255);
                                    /*
                                    sf::Color edgeColor = showOnlyActiveRoutes ?
                                        sf::Color(100, 255, 150, 255) :  // bright green when filtered
                                        sf::Color(120, 30, 30, 180);     // dark red when not
                                    */

                                    sf::Color edgeColor = showOnlyActiveRoutes ?
                                        getCostColor(route->voyageCost) :           // â† COST COLOR IN SUBGRAPH
                                        sf::Color(120, 30, 30, 180);                // â† DARK RED WHEN NOT FILTERED

                                    // Highlight if hovered port is either endpoint
                                    /*
                                    if (hoveredPort == i) { // hoveredPort == destIndex is removed to prevent highlughting of every thing
                                        // edgeColor = sf::Color(160, 32, 240, 255); // bright purple
                                        edgeColor = getCostColor(route->voyageCost);
                                    }
                                    */
                                    if (hoveredPort == i) {
                                        edgeColor = getCostColor(route->voyageCost);
                                    }
                                    else if (showOnlyActiveRoutes && route->shippingCompanyName != activeCompanyFilter) {
                                        edgeColor = sf::Color(80, 80, 80, 50);  // very faded
                                    }

                                    // Highlight if part of the computed path
                                    for (size_t h = 0; h + 1 < highlightedPath.size(); h++) {
                                        if ((highlightedPath[h] == i && highlightedPath[h + 1] == destIndex) ||
                                            (highlightedPath[h] == destIndex && highlightedPath[h + 1] == i)) {
                                            edgeColor = sf::Color(100, 255, 150, 255); // bright green
                                        }
                                    }

                                    float thickness = .5f; // default thin for non-highlighted
                                    if (hoveredPort == i) thickness = 3.0f; // hovered edges thicker // || hoveredPort == destIndex removed
                                    for (size_t h = 0; h + 1 < highlightedPath.size(); h++) {
                                        if ((highlightedPath[h] == i && highlightedPath[h + 1] == destIndex) ||
                                            (highlightedPath[h] == destIndex && highlightedPath[h + 1] == i)) {
                                            thickness = 4.0f; // highlighted path edges thickest
                                        }
                                    }

                                    sf::RectangleShape line(sf::Vector2f(length, thickness));
                                    line.setPosition(p1);
                                    line.setRotation(sf::degrees(angle));
                                    line.setFillColor(edgeColor);
                                    window.draw(line);

                                    // Draw simple arrowhead
                                    sf::Vector2f dir = delta / length;
                                    sf::Vector2f normal(-dir.y, dir.x);
                                    sf::Vector2f arrowTip = p2;
                                    sf::Vector2f left = arrowTip - dir * 18.f + normal * 6.f;
                                    sf::Vector2f right = arrowTip - dir * 18.f - normal * 6.f;

                                    sf::VertexArray arrow(sf::PrimitiveType::Lines, 4);
                                    arrow[0].position = arrowTip; arrow[0].color = edgeColor;
                                    arrow[1].position = left; arrow[1].color = edgeColor;
                                    arrow[2].position = arrowTip; arrow[2].color = edgeColor;
                                    arrow[3].position = right; arrow[3].color = edgeColor;
                                    window.draw(arrow);
                                }
                            }

                            // â†â†â†â†â† ADD SUBGRAPH FILTER RIGHT HERE â†’â†’â†’â†’â†’
                            bool edgeVisible = true;
                            if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                                if (route->shippingCompanyName != activeCompanyFilter) {
                                    edgeVisible = false;
                                }
                            }
                            if (!edgeVisible) {
                                route = route->next;   // â† SKIP THIS EDGE
                                continue;              // â† GO TO NEXT ROUTE
                            }
                            // â†â†â†â†â† END FILTER â†’â†’â†’â†’â†’

                            sf::Vector2f p2 = graphPorts[destIndex]->position;
                            if (p1.x > 0 && p1.y > 0 && p2.x > 0 && p2.y > 0) {
                                // ... your entire drawing code stays 100% unchanged ...
                                sf::Color edgeColor(120, 30, 30, 180);
                                // ... hover, path highlight, thickness, line, arrow ...
                            }

                            route = route->next;

                          
                        }
                        
                        

                        // === DOCKING QUEUE SIMULATION ===
                        long long currentSimTime = 100000;  // Replace with real time later

                        // Finish docked ships
                        while (graphPorts[i]->currentDocked < graphPorts[i]->maxDocks &&
                            graphPorts[i]->dockingQueue.size() > 0 &&
                            currentSimTime >= graphPorts[i]->dockingQueue[0].serviceEndTime) {
                            // Remove first ship
                            for (int j = 0; j < graphPorts[i]->dockingQueue.size() - 1; j++) {
                                graphPorts[i]->dockingQueue[j] = graphPorts[i]->dockingQueue[j + 1];
                            }
                            graphPorts[i]->dockingQueue.sz--;
                            graphPorts[i]->currentDocked--;
                        }

                        // Dock new ships
                        while (graphPorts[i]->currentDocked < graphPorts[i]->maxDocks &&
                            graphPorts[i]->dockingQueue.size() > 0 &&
                            currentSimTime >= graphPorts[i]->dockingQueue[0].arrivalTime) {
                            graphPorts[i]->currentDocked++;
                            // First ship is now docked â€” just leave it in queue[0]
                        }


                        if (graphPorts[i]->dockingQueue.size() > 0) {
                            sf::Vector2f basePos = graphPorts[i]->position + sf::Vector2f(30, -50);

                            for (int q = 0; q < graphPorts[i]->dockingQueue.size(); q++) {
                                // Draw waiting ship
                                sf::CircleShape ship(7);
                                ship.setPosition(basePos + sf::Vector2f(q * 20, 0));
                                ship.setFillColor(sf::Color(255, 200, 100));
                                ship.setOutlineThickness(2);
                                ship.setOutlineColor(sf::Color::White);
                                window.draw(ship);

                                // Draw dashed line between ships
                                if (q > 0) {
                                    sf::Vertex dash[] = {
                                    { sf::Vector2f(basePos) + sf::Vector2f((q - 1) * 20.f + 7.f, 3.f), sf::Color::Yellow },
                                    { sf::Vector2f(basePos) + sf::Vector2f(q * 20.f + 7.f, 3.f), sf::Color::Yellow }
                                };
                                    window.draw(dash, 2, sf::PrimitiveType::Lines);
                                }
                            }

                            // Optional: Show queue size
                            sf::Text queueText(font, to_string(graphPorts[i]->dockingQueue.size()), 12);
                            queueText.setFillColor(sf::Color::Yellow);
                            queueText.setPosition(basePos + sf::Vector2f(-10, -20));
                            window.draw(queueText);
                        }
                        
                        


                        if (simulationMode) {
                            // === ANIMATED DOCKING QUEUE â€” REPLACE YOUR CURRENT ONE WITH THIS ===
                            if (graphPorts[i]->dockingQueue.size() > 0 || graphPorts[i]->currentDocked > 0) {
                                sf::Vector2f portPos = graphPorts[i]->position;
                                sf::Vector2f queueStart = portPos + sf::Vector2f(40, -60);
                                float spacing = 22.0f;

                                int waiting = graphPorts[i]->dockingQueue.size();
                                int docked = graphPorts[i]->currentDocked;
                                int totalInPort = waiting + docked;

                                // Draw docked ships (inside the port â€” green)
                                for (int d = 0; d < docked; d++) {
                                    sf::CircleShape dockedShip(10);
                                    dockedShip.setPosition(portPos + sf::Vector2f(-20 + d * 20, 20));
                                    dockedShip.setFillColor(sf::Color(100, 255, 100));
                                    dockedShip.setOutlineThickness(3);
                                    dockedShip.setOutlineColor(sf::Color::White);
                                    window.draw(dockedShip);
                                }


                                // Draw waiting ships (yellow, moving toward dock)
                                for (int q = 0; q < waiting; q++) {
                                    float progress = 1.0f - (float)q / (float)(totalInPort > 0 ? totalInPort : 1);
                                    float animX = queueStart.x + q * spacing - progress * 60;

                                    sf::CircleShape ship(8);
                                    ship.setPosition(sf::Vector2f(animX, queueStart.y));
                                    ship.setFillColor(sf::Color(255, 200, 100));
                                    ship.setOutlineThickness(2);
                                    ship.setOutlineColor(sf::Color::White);
                                    window.draw(ship);

                                    if (q > 0) {
                                        sf::Vertex dash[] = {
                                            { sf::Vector2f(animX + 8.f, queueStart.y + 4.f), sf::Color::Yellow },
                                            { sf::Vector2f(animX + spacing - 8.f, queueStart.y + 4.f), sf::Color::Yellow }
                                        };

                                        window.draw(dash, 2, sf::PrimitiveType::Lines);
                                    }

                                    // Arrow from last waiting ship to dock
                                    if (q == waiting - 1 && docked > 0) {
                                        sf::Vector2f from = sf::Vector2f(animX + 8, queueStart.y);
                                        sf::Vector2f to = portPos + sf::Vector2f(-20, 20);
                                        sf::Vertex arrow[] = {
                                            { sf::Vector2f(from), sf::Color::Yellow },
                                            { sf::Vector2f(to), sf::Color::Yellow }
                                        };

                                        window.draw(arrow, 2, sf::PrimitiveType::Lines);
                                    }
                                }


                                // Status text
                                sf::Text status(font, "Queue: " + to_string(waiting) + " | Docked: " + to_string(docked), 12);
                                status.setFillColor(sf::Color::Yellow);
                                status.setPosition(queueStart + sf::Vector2f(-20, -30));
                                window.draw(status);
                            }
                            // === END ANIMATED QUEUE ===

                        }

                        
                    }
                    

                    // ===================== INSERT INFOBOX HERE =====================

                    // Draw info panel when hovering over a port
                    if (hoveredPort != -1) {
                        sf::RectangleShape infoBox(sf::Vector2f(350.0f, 220.0f));
                        infoBox.setPosition(sf::Vector2f(30.0f, static_cast<float>(windowHeight - 250)));
                        infoBox.setFillColor(sf::Color(20, 25, 40, 240));
                        infoBox.setOutlineThickness(2.0f);
                        infoBox.setOutlineColor(sf::Color(80, 150, 255));
                        window.draw(infoBox);

                        sf::Text infoTitle(font, graphPorts[hoveredPort]->portName, 20);
                        infoTitle.setFillColor(sf::Color(120, 200, 255));
                        infoTitle.setStyle(sf::Text::Bold);
                        infoTitle.setPosition(sf::Vector2f(40.0f, static_cast<float>(windowHeight - 240)));
                        window.draw(infoTitle);

                        sf::Text chargeText(font, "Daily Charge: $" + to_string(graphPorts[hoveredPort]->dailyPortCharge), 14);
                        chargeText.setFillColor(sf::Color(200, 210, 230));
                        chargeText.setPosition(sf::Vector2f(40.0f, static_cast<float>(windowHeight - 210)));
                        window.draw(chargeText);

                        sf::Text routeHeader(font, "Outgoing Routes:", 16);
                        routeHeader.setFillColor(sf::Color(180, 200, 255));
                        routeHeader.setStyle(sf::Text::Bold);
                        routeHeader.setPosition(sf::Vector2f(40.0f, static_cast<float>(windowHeight - 180)));
                        window.draw(routeHeader);

                        int routeCount = 0;
                        graphEdgeRoute* route = graphPorts[hoveredPort]->routeHead;
                        while (route != nullptr && routeCount < 4) {
                            sf::Text routeText(font, "-> " + route->destinationName +
                                " ($" + to_string(route->voyageCost) + ")", 12);
                            routeText.setFillColor(sf::Color(180, 190, 210));
                            routeText.setPosition(sf::Vector2f(50.0f, static_cast<float>(windowHeight - 155 + routeCount * 22)));
                            window.draw(routeText);

                            routeCount++;
                            route = route->next;
                        }
                    }
                    // ===================== END INFOBOX INSERT =====================


                    // Draw all ports (nodes) with support for exploration states and selection highlighting
                    for (int i = 0; i < currentSize; i++) {
                        if (graphPorts[i] == nullptr) continue;

                        bool isOrigin = (selectedOrigin == i);
                        bool isDestination = (selectedDestination == i);
                        bool isHovered = (hoveredPort == i);
                        bool isHighlightedNode = false;
                        for (int idx : highlightedPath) {
                            if (idx == i) { isHighlightedNode = true; break; }
                        }

                        // Outer glow with enhanced brightness for special states
                        float glowRadius = 20.0f;
                        sf::Color glowColor(80, 150, 255, 40);
                        if (isOrigin) {
                            glowRadius = 50.0f;
                            glowColor = sf::Color(100, 255, 150, 120);
                        }
                        else if (isDestination) {
                            glowRadius = 50.0f;
                            glowColor = sf::Color(255, 180, 100, 120);
                        }
                        else if (isHovered) {
                            glowRadius = 35.0f;
                            glowColor = sf::Color(150, 100, 255, 100);
                        }
                        else if (isHighlightedNode) {
                            glowRadius = 30.0f;
                            glowColor = sf::Color(80, 255, 100, 80);
                        }
                        sf::CircleShape glow(glowRadius * 0.5f);
                        glow.setPosition(sf::Vector2f(graphPorts[i]->position.x - glowRadius * 0.5f, graphPorts[i]->position.y - glowRadius * 0.5f));
                        glow.setFillColor(glowColor);
                        window.draw(glow);

                        // Main node with state-based coloring (half size)
                        sf::CircleShape node(6.0f);
                        node.setPosition(sf::Vector2f(graphPorts[i]->position.x - 12.5f, graphPorts[i]->position.y - 12.5f));
                        if (isOrigin) {
                            node.setFillColor(sf::Color(100, 255, 150));
                            node.setOutlineThickness(3.0f);
                            node.setOutlineColor(sf::Color(150, 255, 180));
                        }
                        else if (isDestination) {
                            node.setFillColor(sf::Color(255, 180, 100));
                            node.setOutlineThickness(3.0f);
                            node.setOutlineColor(sf::Color(255, 220, 150));
                        }
                        else if (isHovered) {
                            node.setFillColor(sf::Color(150, 100, 255));
                            node.setOutlineThickness(3.0f);
                            node.setOutlineColor(sf::Color(200, 150, 255));
                        }
                        else if (isHighlightedNode) {
                            node.setFillColor(sf::Color(80, 255, 100));
                            node.setOutlineThickness(3.0f);
                            node.setOutlineColor(sf::Color(120, 255, 150));
                        }
                        else {
                            node.setFillColor(sf::Color(60, 120, 200));
                            node.setOutlineThickness(2.0f);
                            node.setOutlineColor(sf::Color(100, 160, 240));
                        }
                        window.draw(node);


                        // Port name label with indicator for selected ports
                        string portLabel = graphPorts[i]->portName;
                        if (isOrigin) portLabel += " [O]";
                        if (isDestination) portLabel += " [D]";

                        sf::Text portName(font, portLabel, 14);

                        // Set colors and outline
                        portName.setFillColor(sf::Color(220, 230, 255));
                        portName.setOutlineColor(sf::Color::Black);
                        portName.setOutlineThickness(2.f);
                        portName.setStyle(sf::Text::Bold);

                        // ABOVE ports
                        const char* specialPortsAbove[] = { "Osaka", "Montreal", "AbuDhabi", "Stockholm", "London" };

                        // LEFT ports
                        const char* specialPortsLeft[] = { "Oslo", "Doha", "Marseille", "Dublin" };

                        // RIGHT ports
                        const char* specialPortsRight[] = { "Copenhagen" };

                        bool drawAbove = false;
                        bool drawLeft = false;
                        bool drawRight = false;

                        // Check ABOVE list
                        for (int sp = 0; sp < 5; sp++) {
                            if (portLabel.find(specialPortsAbove[sp]) != std::string::npos) {
                                drawAbove = true;
                                break;
                            }
                        }

                        // Check LEFT list  (3 items now)
                        for (int sp = 0; sp < 4; sp++) {
                            if (portLabel.find(specialPortsLeft[sp]) != std::string::npos) {
                                drawLeft = true;
                                break;
                            }
                        }

                        // Check RIGHT list
                        for (int sp = 0; sp < 1; sp++) {
                            if (portLabel.find(specialPortsRight[sp]) != std::string::npos) {
                                drawRight = true;
                                break;
                            }
                        }

                        // Text size
                        sf::FloatRect textBounds = portName.getLocalBounds();
                        float nodeRadiusForLabel = 6.0f;

                        float textX, textY;

                        if (drawLeft) {
                            //---------------------------------------
                            //        LEFT OF NODE (same Y level)
                            //---------------------------------------
                            textX = graphPorts[i]->position.x - textBounds.size.x - 14.f;
                            textY = graphPorts[i]->position.y - (textBounds.size.y / 2.f) - 6.f;

                        }
                        else if (drawRight) {
                            //---------------------------------------
                            //        RIGHT OF NODE (same Y level)
                            //---------------------------------------
                            textX = graphPorts[i]->position.x + 14.f;   // mirror of left spacing
                            textY = graphPorts[i]->position.y - (textBounds.size.y / 2.f) - 6.f;

                        }
                        else if (drawAbove) {
                            //---------------------------------------
                            //               ABOVE NODE
                            //---------------------------------------
                            textX = graphPorts[i]->position.x - (textBounds.size.x / 2.f) - 4.f;
                            textY = graphPorts[i]->position.y
                                - nodeRadiusForLabel
                                - textBounds.size.y
                                - 14.f;  // moved further up

                        }
                        else {
                            //---------------------------------------
                            //               BELOW NODE
                            //---------------------------------------
                            textX = graphPorts[i]->position.x - (textBounds.size.x / 2.f) - 2.f;
                            textY = graphPorts[i]->position.y
                                + nodeRadiusForLabel
                                + 2.f;   // moved slightly upward
                        }

                        portName.setPosition(sf::Vector2f(textX, textY));

                        // Draw
                        window.draw(portName);

                    }

                    // Future: Right-side control panel for algorithm controls
                    // Future: Top-right panel for pathfinding options and filters
                    // Future: Animation progress bar at bottom
                    // Future: Multi-leg journey visualization panel

                    //window.display();
                }
                // === DRAW MULTI-LEG JOURNEY ===
                // Draw multi-leg journey arrows
                JourneyLeg* leg = currentJourney.head;
                JourneyLeg* prev = nullptr;
                while (leg) {
                    if (prev) {
                        sf::Vector2f p1 = graphPorts[prev->portIndex]->position;
                        sf::Vector2f p2 = graphPorts[leg->portIndex]->position;

                        // Main line
                        sf::Vertex line[] = {
                            { sf::Vector2f(p1), sf::Color::Cyan },
                            { sf::Vector2f(p2), sf::Color::Cyan }
                        };

                        window.draw(line, 2, sf::PrimitiveType::Lines);

                        // Arrowhead
                        sf::Vector2f dir = p2 - p1;
                        float len = sqrt(dir.x * dir.x + dir.y * dir.y);
                        if (len > 20) {
                            dir.x /= len;
                            dir.y /= len;
                            sf::Vector2f perp(-dir.y, dir.x);

                            sf::Vector2f tip1 = p2 - sf::Vector2f(dir.x * 25 + perp.x * 10, dir.y * 25 + perp.y * 10);
                            sf::Vector2f tip2 = p2 - sf::Vector2f(dir.x * 25 - perp.x * 10, dir.y * 25 - perp.y * 10);

                            sf::Vertex arrow[] = {
                                { sf::Vector2f(p2), sf::Color::Cyan },
                                { sf::Vector2f(tip1), sf::Color::Cyan },
                                { sf::Vector2f(p2), sf::Color::Cyan },
                                { sf::Vector2f(tip2), sf::Color::Cyan }
                            };
                            window.draw(arrow, 4, sf::PrimitiveType::Lines);
                        }
                    }
                    prev = leg;
                    leg = leg->next;
                }
                // === END MULTI-LEG DRAWING ===


                // === WHOOSH EFFECT â€” EPIC SWOOSH ACROSS PATH ===
                if (showWhoosh && currentPath.found && currentPath.pathSize > 0) {
                    float duration = 5.0f;  // 2 seconds
                    whooshProgress = whooshClock.getElapsedTime().asSeconds() / duration;

                    if (whooshProgress >= 1.0f) {
                        showWhoosh = false;
                    }
                    else {
                        // Calculate current position along path
                        float t = whooshProgress * (currentPath.pathSize - 1);
                        int segment = (int)t;
                        float frac = t - segment;

                        if (segment < currentPath.pathSize - 1) {
                            sf::Vector2f p1 = graphPorts[currentPath.path[segment]]->position;
                            sf::Vector2f p2 = graphPorts[currentPath.path[segment + 1]]->position;
                            sf::Vector2f pos = p1 + (p2 - p1) * frac;

                            // Draw glowing whoosh particle
                            sf::CircleShape whoosh(20);
                            whoosh.setPosition(pos - sf::Vector2f(20, 20));
                            whoosh.setFillColor(sf::Color(100, 255, 255, 180));
                            whoosh.setOutlineThickness(8);
                            whoosh.setOutlineColor(sf::Color(0, 255, 255, 255));
                            window.draw(whoosh);

                            // Trail effect
                            for (int trail = 1; trail <= 3; trail++) {
                                float trailT = whooshProgress - trail * 1.15f;
                                if (trailT < 0) break;

                                float trailPos = trailT * (currentPath.pathSize - 1);
                                int trailSeg = (int)trailPos;
                                float trailFrac = trailPos - trailSeg;

                                if (trailSeg < currentPath.pathSize - 1) {
                                    sf::Vector2f t1 = graphPorts[currentPath.path[trailSeg]]->position;
                                    sf::Vector2f t2 = graphPorts[currentPath.path[trailSeg + 1]]->position;
                                    sf::Vector2f trailPos = t1 + (t2 - t1) * trailFrac;

                                    sf::CircleShape trailDot(8 - trail * 0.5f);
                                    trailDot.setPosition(trailPos - sf::Vector2f(trailDot.getRadius(), trailDot.getRadius()));
                                    trailDot.setFillColor(sf::Color(0, 255, 255, (int)(255 * (1.0f - trail * 0.1f))));
                                    window.draw(trailDot);
                                }
                            }
                        }
                    }
                }
                // === END WHOOSH ===

                // === LEFT-SIDE CONSOLE PANEL ===
                //static SimpleVector<string, 50> consoleLines;
                static bool firstTime = true;

                if (firstTime) {
                    consoleLines.push_back("=== OCEAN ROUTE SYSTEM LOG ===");
                    consoleLines.push_back("Ready. Click ports to select route.");
                    firstTime = false;
                }

   
                float panelHeight = 400.0f;
                float panelY = (windowHeight - panelHeight) / 2.0f;

                sf::RectangleShape consolePanel(sf::Vector2f(400.0f, panelHeight));
                consolePanel.setPosition(sf::Vector2f(0.0f, panelY));
                consolePanel.setFillColor(sf::Color(0, 0, 0, 200));
                window.draw(consolePanel);

                // Title
                sf::Text titleText(font, "SYSTEM LOG", 20);
                titleText.setFillColor(sf::Color::Cyan);
                titleText.setStyle(sf::Text::Bold);
                titleText.setPosition(sf::Vector2f(0.0f, panelY));  // â† FIXED
                window.draw(titleText);

                // â†â†â†â†â† ADD AUTO-CLEARING RIGHT HERE â†’â†’â†’â†’â†’
                int maxLines = 16;  // how many lines fit in panel
                if (consoleLines.size() > maxLines) {
                    for (int i = 0; i < maxLines; i++) {
                        consoleLines[i] = consoleLines[consoleLines.size() - maxLines + i];
                    }
                    consoleLines.sz = maxLines;
                }
                // â†â†â†â†â† END AUTO-CLEARING â†’â†’â†’â†’â†’

                // Draw log lines
                for (int i = 0; i < consoleLines.size(); i++) {
                    sf::Text line(font, consoleLines[i], 14);
                    line.setFillColor(i == consoleLines.size() - 1 ? sf::Color::Yellow : sf::Color(200, 200, 200));
                    line.setPosition(sf::Vector2f(20.0f, panelY + 50.0f + i * 22.0f));
                    window.draw(line);
                }

                /*
                // === REAL-TIME PATHFINDING VISUALIZATION â€” WORKS 100% ===
                if (currentPath.found && highlightedPath.size() > 0) {
                    static int animIndex = 0;
                    static sf::Clock animClock;

                    if (animClock.getElapsedTime().asMilliseconds() > 80) {
                        animIndex++;
                        if (animIndex >= highlightedPath.size()) {
                            animIndex = 0;  // loop
                        }
                        animClock.restart();
                    }

                    int currentNode = highlightedPath[animIndex];

                    // Draw PULSING GREEN RING on current node
                    float pulseSize = 20 + 10 * sin(animClock.getElapsedTime().asSeconds() * 8);
                    sf::CircleShape pulse(pulseSize);
                    pulse.setPosition(graphPorts[currentNode]->position - sf::Vector2f(pulseSize, pulseSize));
                    pulse.setFillColor(sf::Color::Transparent);
                    pulse.setOutlineThickness(6);
                    pulse.setOutlineColor(sf::Color(100, 255, 100, 180));
                    window.draw(pulse);

                    // Draw bright core
                    sf::CircleShape core(12);
                    core.setPosition(graphPorts[currentNode]->position - sf::Vector2f(12, 12));
                    core.setFillColor(sf::Color(100, 255, 100));
                    window.draw(core);
                }
                */
                // === BOTTOM-RIGHT COST LEGEND (ONLY WHEN SUBGRAPH ACTIVE) ===
                if (showOnlyActiveRoutes) {
                    float legendX = windowWidth - 300;
                    float legendY = windowHeight - 200;

                    // Background
                    sf::RectangleShape legendBg(sf::Vector2f(280, 180));
                    legendBg.setPosition(sf::Vector2f(legendX, legendY));
                    legendBg.setFillColor(sf::Color(0, 0, 0, 220));
                    window.draw(legendBg);

                    // Title
                    sf::Text legendTitle(font, "ROUTE COST LEGEND", 18);
                    legendTitle.setFillColor(sf::Color::Cyan);
                    legendTitle.setStyle(sf::Text::Bold);
                    legendTitle.setPosition(sf::Vector2f(legendX + 10, legendY + 10));
                    window.draw(legendTitle);

                    // Legend items
                    struct { int cost; string label; } items[] = {
                        {10000, "< $15,000 - Low Cost"},
                        {20000, "$15K - $25K - Medium"},
                        {30000, "$25K - $35K - High"},
                        {40000, "> $35,000 - Very High"}
                    };

                    for (int i = 0; i < 4; i++) {
                        // Color box
                        sf::RectangleShape box(sf::Vector2f(20, 20));
                        box.setPosition(sf::Vector2f(legendX + 20, legendY + 50 + i * 30));
                        box.setFillColor(getCostColor(items[i].cost));
                        box.setOutlineThickness(2);
                        box.setOutlineColor(sf::Color::White);
                        window.draw(box);

                        // Text
                        sf::Text label(font, items[i].label, 14);
                        label.setFillColor(sf::Color::White);
                        label.setPosition(sf::Vector2f(legendX + 50, legendY + 50 + i * 30));
                        window.draw(label);
                    }

                    // Current filter
                    sf::Text filter(font, "FILTER: " + activeCompanyFilter, 14);
                    filter.setFillColor(sf::Color::Yellow);
                    filter.setPosition(sf::Vector2f(legendX + 10, legendY + 170));
                    window.draw(filter);
                }
                window.display();
            }
        }
    }
};

int main()
{
    oceanRouteGraph obj;
    obj.createGraphFromFile("./Routes.txt", "./PortCharges.txt");
    obj.printGraphAfterCreation();

    cout << "\nLaunching interactive graph visualization..." << endl;
    obj.visualizeGraph(1800, 980);

    return 0;
}
