#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>

// ==== BEGIN: Custom STL Replacements ====
template<typename T, int N = 256> struct SimpleVector {
    T data[N];
    int sz;
    SimpleVector() : sz(0) {}
    void push_back(const T& v) { if (sz < N) data[sz++] = v; }
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
    void push(const T& v) { if ((back+1)%N != front) { data[back] = v; back = (back+1)%N; } }
    void pop() { if (!empty()) front = (front+1)%N; }
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
        while (i > 0 && data[(i-1)/2] > data[i]) {
            T tmp = data[i]; data[i] = data[(i-1)/2]; data[(i-1)/2] = tmp;
            i = (i-1)/2;
        }
    }
    void pop() {
        if (sz == 0) return;
        data[0] = data[--sz];
        int i = 0;
        while (2*i+1 < sz) {
            int j = 2*i+1;
            if (j+1 < sz && data[j+1] < data[j]) j++;
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
        std::pair<K,V> operator*() const { return {*k, *v}; }
    };
    iterator begin() { return iterator(keys, values); }
    iterator end() { return iterator(keys+sz, values+sz); }
    int size() const { return sz; }
    bool contains(const K& k) const { for (int i = 0; i < sz; ++i) if (keys[i] == k) return true; return false; }
    int find(const K& k) const { for (int i = 0; i < sz; ++i) if (keys[i] == k) return i; return -1; }
};

inline int my_min(int a, int b) { return a < b ? a : b; }
inline int my_max(int a, int b) { return a > b ? a : b; }
inline float my_min(float a, float b) { return a < b ? a : b; }
inline float my_max(float a, float b) { return a > b ? a : b; }
inline char my_tolower(char c) { return (c >= 'A' && c <= 'Z') ? (c + 32) : c; }
inline bool my_isalnum(char c) { return (c >= 'A' && c <= 'Z')||(c >= 'a' && c <= 'z')||(c >= '0' && c <= '9'); }
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

struct graphVerticePort {
    string portName; // can be both source and destination
    int dailyPortCharge;
    // Queue shipManagement; // TODO> add this as a queue for ship management by making your own priority queue(heap)
    graphEdgeRoute* routeHead;
    sf::Vector2f position; // For SFML rendering
    float latitude; // optional
    float longitude; // optional
    bool hasCoordinates;

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

struct PathResult { // just stores the pathing, need to test out different kinks for now
    int* path;
    int pathSize;
    int totalCost;
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
        } else {
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
            } else {
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
    
    PathResult currentPath;
    int* highlightedPath;
    int highlightedPathSize;
    int selectedOrigin = -1;
    int selectedDestination = -1;
    string selectedCompany = "";
    // Mercator projection removed; only using PortCoords.txt


    oceanRouteGraph() {
        this->capacity = TOTALNUMBEROFPORTS;
        this->currentSize = 0;
        graphPorts = new graphVerticePort * [TOTALNUMBEROFPORTS];
        for (int i = 0; i < TOTALNUMBEROFPORTS; i++) {
            graphPorts[i] = nullptr;
        }
        highlightedPath = nullptr;
        highlightedPathSize = 0;
        // by default, no coordinates loaded
        // Mercator projection removed
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
        
        if (highlightedPath != nullptr) {
            delete[] highlightedPath;
            highlightedPath = nullptr;
        }
    }

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
        } else {
            result.path = nullptr;
            result.pathSize = 0;
        }
    }
    
    PathResult dijkstraShortestPath(int startIndex, int endIndex, const string& filterCompany = "") { // cheapest route so less efficient but checks everything
        PathResult result;
        
        if (startIndex < 0 || startIndex >= currentSize || endIndex < 0 || endIndex >= currentSize) {
            return result;
        }
        if (graphPorts[startIndex] == nullptr || graphPorts[endIndex] == nullptr) {
            return result;
        }
        
        int* dist = new int[currentSize];
        int* parent = new int[currentSize];
        bool* visited = new bool[currentSize];
        
        for (int i = 0; i < currentSize; i++) {
            dist[i] = MAX_COST;
            parent[i] = -1;
            visited[i] = false;
        }
        
        SimplePriorityQueue<PQNode, 256> pq;
        
        dist[startIndex] = 0;
        pq.push(PQNode(startIndex, 0));
        
        while (!pq.empty()) {
            PQNode current = pq.top();
            pq.pop();
            
            int u = current.portIndex;
            
            if (visited[u]) continue;
            visited[u] = true;
            
            if (u == endIndex) {
                reconstructPath(result, parent, startIndex, endIndex);
                result.totalCost = dist[u];
                result.found = (result.path != nullptr && result.pathSize > 0);
                delete[] dist;
                delete[] parent;
                delete[] visited;
                return result;
            }
            
            graphEdgeRoute* route = graphPorts[u]->routeHead;
            while (route != nullptr) {
                int v = getPortIndex(route->destinationName);
                
                if (v != -1 && !visited[v]) {
                    if (!filterCompany.empty() && route->shippingCompanyName != filterCompany) {
                        route = route->next;
                        continue;
                    }
                    
                    int edgeCost = route->voyageCost;
                    if (v != endIndex) {
                        edgeCost += graphPorts[v]->dailyPortCharge;
                    }
                    
                    if (dist[u] != MAX_COST && dist[u] + edgeCost < dist[v]) {
                        dist[v] = dist[u] + edgeCost;
                        parent[v] = u;
                        pq.push(PQNode(v, dist[v]));
                    }
                }
                route = route->next;
            }
        }
        
        if (dist[endIndex] != MAX_COST) {
            reconstructPath(result, parent, startIndex, endIndex);
            result.totalCost = dist[endIndex];
            result.found = (result.path != nullptr && result.pathSize > 0);
        }
        
        delete[] dist;
        delete[] parent;
        delete[] visited;
        
        return result;
    }
    
    PathResult aStarShortestPath(int startIndex, int endIndex, const string& filterCompany = "") { // optimal path that doesnt go through every node
        PathResult result;
        
        if (startIndex < 0 || startIndex >= currentSize || endIndex < 0 || endIndex >= currentSize) {
            return result;
        }
        if (graphPorts[startIndex] == nullptr || graphPorts[endIndex] == nullptr) {
            return result;
        }
        
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
        
        SimplePriorityQueue<PQNode, 256> pq;
        
        gScore[startIndex] = 0;
        fScore[startIndex] = calculateHeuristic(startIndex, endIndex);
        pq.push(PQNode(startIndex, gScore[startIndex], calculateHeuristic(startIndex, endIndex)));
        
        while (!pq.empty()) {
            PQNode current = pq.top();
            pq.pop();
            
            int u = current.portIndex;
            
            if (visited[u]) continue;
            visited[u] = true;
            
            if (u == endIndex) {
                reconstructPath(result, parent, startIndex, endIndex);
                result.totalCost = gScore[u];
                result.found = (result.path != nullptr && result.pathSize > 0);
                delete[] gScore;
                delete[] fScore;
                delete[] parent;
                delete[] visited;
                return result;
            }
            
            graphEdgeRoute* route = graphPorts[u]->routeHead;
            while (route != nullptr) {
                int v = getPortIndex(route->destinationName);
                
                if (v != -1 && !visited[v]) {
                    if (!filterCompany.empty() && route->shippingCompanyName != filterCompany) {
                        route = route->next;
                        continue;
                    }
                    
                    int edgeCost = route->voyageCost;
                    if (v != endIndex) {
                        edgeCost += graphPorts[v]->dailyPortCharge;
                    }
                    
                    int tentativeGScore = (gScore[u] == MAX_COST) ? MAX_COST : gScore[u] + edgeCost;
                    
                    if (tentativeGScore < gScore[v]) {
                        parent[v] = u;
                        gScore[v] = tentativeGScore;
                        fScore[v] = gScore[v] + calculateHeuristic(v, endIndex);
                        pq.push(PQNode(v, gScore[v], calculateHeuristic(v, endIndex)));
                    }
                }
                route = route->next;
            }
        }
        
        if (gScore[endIndex] != MAX_COST) {
            reconstructPath(result, parent, startIndex, endIndex);
            result.totalCost = gScore[endIndex];
            result.found = (result.path != nullptr && result.pathSize > 0);
        }
        
        delete[] gScore;
        delete[] fScore;
        delete[] parent;
        delete[] visited;
        
        return result;
    }
    
    void findCheapestRoute(int originIndex, int destinationIndex, const string& filterCompany = "") { // these will be called to calculate the paths
        if (originIndex < 0 || destinationIndex < 0) {
            cout << "Error: Please select both origin and destination ports." << endl;
            return;
        }
        
        currentPath = dijkstraShortestPath(originIndex, destinationIndex, filterCompany);
        
        if (currentPath.found && currentPath.path != nullptr && currentPath.pathSize > 0) {
            // can add graphics // i did console output for now idk how it would work
            
            cout << "\n=== CHEAPEST ROUTE FOUND ===" << endl;
            cout << "From: " << graphPorts[originIndex]->portName << endl;
            cout << "To: " << graphPorts[destinationIndex]->portName << endl;
            cout << "Total Cost: $" << currentPath.totalCost << endl;
            cout << "Path: "; // for now it should be fine
            for (int i = 0; i < currentPath.pathSize; i++) {
                cout << graphPorts[currentPath.path[i]]->portName;
                if (i < currentPath.pathSize - 1)
                    cout << " -> ";
            }
            cout << endl;
            cout << "===========================" << endl;
        } else {
            cout << "\nNo route found from " << graphPorts[originIndex]->portName 
                 << " to " << graphPorts[destinationIndex]->portName << endl;
        }
    }
    
    void findFastestRoute(int originIndex, int destinationIndex, const string& filterCompany = "") {
        if (originIndex < 0 || destinationIndex < 0) {
            cout << "Error: Please select both origin and destination ports." << endl;
            return;
        }
        
        currentPath = aStarShortestPath(originIndex, destinationIndex, filterCompany);
        
        if (currentPath.found && currentPath.path != nullptr && currentPath.pathSize > 0) {
            // can add graphics // i did console output for now idk how it would work
            
            cout << "\n=== FASTEST/OPTIMAL ROUTE FOUND (A*) ===" << endl;
            cout << "From: " << graphPorts[originIndex]->portName << endl;
            cout << "To: " << graphPorts[destinationIndex]->portName << endl;
            cout << "Total Cost: $" << currentPath.totalCost << endl;
            cout << "Path: ";
            for (int i = 0; i < currentPath.pathSize; i++) {
                cout << graphPorts[currentPath.path[i]]->portName;
                if (i < currentPath.pathSize - 1)
                    cout << " -> ";
                }
            cout << endl;
            cout << "========================================" << endl;
        } else {
            cout << "\nNo route found from " << graphPorts[originIndex]->portName 
                 << " to " << graphPorts[destinationIndex]->portName << endl;
        }
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
            } else {
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
                } catch (...) { continue; }
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
            runCheapest = true;
            if (selectedOrigin != -1 && selectedDestination != -1) {
                findCheapestRoute(selectedOrigin, selectedDestination, selectedCompany);
                showQueryPanel = false;
            } else {
                cout << "Error: Please select both origin and destination ports first." << endl;
            }
        }
        else if (action == "Find Fastest Route") {
            runFastest = true;
            if (selectedOrigin != -1 && selectedDestination != -1) {
                findFastestRoute(selectedOrigin, selectedDestination, selectedCompany);
                showQueryPanel = false;
            } else {
                cout << "Error: Please select both origin and destination ports first." << endl;
            }
        }
        else if (action == "Filter by Company") {
            runCompanyFilter = true;
            // TODO: Implement company filtering
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


    void visualizeGraph(int windowWidth = 1400, int windowHeight = 900) {
        // Try to load optional coordinates and world map before assigning positions
        // If a PortCoords.txt file exists in the workspace, load it
        loadPortCoordinates("./PortCoords.txt");
        assignPositions(static_cast<float>(windowWidth), static_cast<float>(windowHeight));

        sf::RenderWindow window(sf::VideoMode({ static_cast<unsigned int>(windowWidth), static_cast<unsigned int>(windowHeight) }), "Ocean Route Network - Interactive Graph", sf::Style::Close);
        window.setFramerateLimit(100);

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
        std::vector<int> highlightedPath;

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
                                handlePanelAction(btn.action, showQueryPanel);
                            }
                        }
                        // Clicking inside date box → start typing date
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
                    if (!showQueryPanel)
                    {
                        if (hoveredPort != -1)
                        {
                            if (selectedOrigin == -1)
                                selectedOrigin = hoveredPort;
                            else if (selectedDestination == -1)
                                selectedDestination = hoveredPort;
                            else
                            {
                                selectedOrigin = hoveredPort;
                                selectedDestination = -1;
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
                } else {
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
                            findFastestRoute(selectedOrigin, selectedDestination, selectedCompany);
                        } else {
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
                    for (int i = 0; i < currentSize; i++) {
                        if (graphPorts[i] == nullptr) continue;
                        graphEdgeRoute* route = graphPorts[i]->routeHead;
                        sf::Vector2f p1 = graphPorts[i]->position;
                        while (route != nullptr) {
                            int destIndex = getPortIndex(route->destinationName);
                            if (destIndex != -1 && graphPorts[destIndex] != nullptr) {
                                sf::Vector2f p2 = graphPorts[destIndex]->position;
                                // Only draw if both ports have valid positions
                                if (p1.x > 0 && p1.y > 0 && p2.x > 0 && p2.y > 0) {
                                    // Draw main line
                                    sf::Color edgeColor = sf::Color(120, 30, 30, 180);

                                    // Highlight if hovered port is either endpoint
                                    if (hoveredPort == i || hoveredPort == destIndex) {
                                        edgeColor = sf::Color(160, 32, 240, 255); // bright red highlight
                                    }

                                    sf::VertexArray lineArray(sf::Lines, 2);
                                    lineArray[0].position = p1;
                                    lineArray[0].color = edgeColor;
                                    lineArray[1].position = p2;
                                    lineArray[1].color = edgeColor; 

                                    // Draw arrowhead for direction
                                    sf::Vector2f dir = p2 - p1;
                                    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                                    if (len > 0) dir /= len;
                                    sf::Vector2f unit = dir * 18.f; // arrow size
                                    sf::Vector2f normal(-dir.y, dir.x);
                                    sf::Vector2f arrowTip = p2;
                                    sf::Vector2f left = arrowTip - unit + normal * 6.0f;
                                    sf::Vector2f right = arrowTip - unit - normal * 6.0f;
                                    sf::VertexArray arrowArray(sf::Lines, 4);
                                    arrowArray[0].position = arrowTip;
                                    arrowArray[0].color = edgeColor;
                                    arrowArray[1].position = left;
                                    arrowArray[1].color = edgeColor;
                                    arrowArray[2].position = arrowTip;
                                    arrowArray[2].color = edgeColor;
                                    arrowArray[3].position = right;
                                    arrowArray[3].color = edgeColor;
                                    window.draw(arrowArray);

                                    sf::Vector2f p1 = graphPorts[i]->position;
                                    sf::Vector2f p2 = graphPorts[destIndex]->position;
                                    sf::Vector2f mouseToP1 = mousePosF - p1;
                                    sf::Vector2f edgeDir = p2 - p1;
                                    float edgeLen2 = edgeDir.x*edgeDir.x + edgeDir.y*edgeDir.y;

                                    if (edgeLen2 > 0) {
                                        float t = (mouseToP1.x*edgeDir.x + mouseToP1.y*edgeDir.y) / edgeLen2;
                                        t = std::clamp(t, 0.0f, 1.0f);  // projection along segment
                                        sf::Vector2f closest = p1 + edgeDir * t;
                                        float dist2 = (mousePosF - closest).x*(mousePosF - closest).x + (mousePosF - closest).y*(mousePosF - closest).y;
                                        if (dist2 < 100.0f) { // threshold = 10 pixels
                                            edgeColor = sf::Color(255, 80, 80, 255);
                                        }
                                    }

                                }
                            }
                            route = route->next;
                        }
                    }
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
                        } else if (isDestination) {
                            glowRadius = 50.0f;
                            glowColor = sf::Color(255, 180, 100, 120);
                        } else if (isHovered) {
                            glowRadius = 35.0f;
                            glowColor = sf::Color(150, 100, 255, 100);
                        } else if (isHighlightedNode) {
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
                        } else if (isDestination) {
                            node.setFillColor(sf::Color(255, 180, 100));
                            node.setOutlineThickness(3.0f);
                            node.setOutlineColor(sf::Color(255, 220, 150));
                        } else if (isHovered) {
                            node.setFillColor(sf::Color(150, 100, 255));
                            node.setOutlineThickness(3.0f);
                            node.setOutlineColor(sf::Color(200, 150, 255));
                        } else if (isHighlightedNode) {
                            node.setFillColor(sf::Color(80, 255, 100));
                            node.setOutlineThickness(3.0f);
                            node.setOutlineColor(sf::Color(120, 255, 150));
                        } else {
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
                       
                        portName.setFillColor(sf::Color(220, 230, 255));
                        portName.setStyle(sf::Text::Bold);

                        sf::FloatRect textBounds = portName.getLocalBounds();
                        float nodeRadiusForLabel = 6.0f; // keep consistent with node radius
                        portName.setPosition(sf::Vector2f(graphPorts[i]->position.x - textBounds.size.x / 2.0f, graphPorts[i]->position.y + nodeRadiusForLabel + 6.0f));
                        window.draw(portName);
                    }
                }

                // Future: Right-side control panel for algorithm controls
                // Future: Top-right panel for pathfinding options and filters
                // Future: Animation progress bar at bottom
                // Future: Multi-leg journey visualization panel

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
