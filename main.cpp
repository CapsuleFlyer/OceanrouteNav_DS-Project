#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>
#include <vector>
#include <queue>
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

    graphVerticePort() {
        this->portName = "";
        this->dailyPortCharge = 0;
        this->routeHead = nullptr;
    }

    graphVerticePort(string portName, int dailyPortCharge) {
        this->portName = portName;
        this->dailyPortCharge = dailyPortCharge;
        this->routeHead = nullptr;
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
    
    PQNode(int idx, int c, int h = 0) : portIndex(idx), cost(c), heuristic(h) {}
    
    bool operator>(const PQNode& other) const {
        return (cost + heuristic) > (other.cost + other.heuristic);
    }
};

struct oceanRouteGraph {
    // TODO: duration is to be calculated for each route as well
    graphVerticePort** graphPorts;
    int currentSize;
    int capacity;
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


    oceanRouteGraph() {
        this->capacity = TOTALNUMBEROFPORTS;
        this->currentSize = 0;
        graphPorts = new graphVerticePort * [TOTALNUMBEROFPORTS];
        for (int i = 0; i < TOTALNUMBEROFPORTS; i++) {
            graphPorts[i] = nullptr;
        }
        highlightedPath = nullptr;
        highlightedPathSize = 0;
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
                currentStartingLetter = currentCharPointer + 1; // pointer aritematic -- at the next letter after the space now

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
        for (int i = 0; i < currentSize; i++) {
            if (graphPorts[i]->portName == sourceName) {
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
                addAtEndOfLinkedList(graphPorts[currentSize]->routeHead, destinationName, date, arrivalTime, departureTime, voyageCost, shippingCompanyName);
                currentSize++;
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

            cout << graphPorts[i]->portName << " (" << graphPorts[i]->dailyPortCharge << ") -> ";
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
        
        priority_queue<PQNode, vector<PQNode>, greater<PQNode>> pq;
        
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
        
        priority_queue<PQNode, vector<PQNode>, greater<PQNode>> pq;
        
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

        float centerX = width / 2.0f;
        float centerY = height / 2.0f;
        float radiusX = width * 0.40f;  // Changed from 0.35f
        float radiusY = height * 0.45f;  // Changed from 0.35f

        for (int i = 0; i < currentSize; i++) {
            if (graphPorts[i] == nullptr) continue;

            float angle = (2.0f * 3.14159f * static_cast<float>(i)) / static_cast<float>(currentSize);
            graphPorts[i]->position.x = centerX + radiusX * std::cos(angle);
            graphPorts[i]->position.y = centerY + radiusY * std::sin(angle);
        }
    }


    sf::Color getCostColor(int cost) const {
         if (cost < 15000) return sf::Color(100, 255, 150, 200);      // Green - Low cost
         else if (cost < 25000) return sf::Color(255, 230, 100, 200); // Yellow - Medium cost
         else if (cost < 35000) return sf::Color(255, 180, 100, 200); // Orange - High cost
         else return sf::Color(255, 100, 100, 200);                   // Red - Very high cost
    }

    void drawCurvedArrow(sf::RenderWindow& window, sf::Vector2f start, sf::Vector2f end, sf::Color color, int cost) {
        sf::Vector2f mid = (start + end) / 2.0f;
        sf::Vector2f perpendicular(-(end.y - start.y), end.x - start.x);
        float length = std::sqrt(perpendicular.x * perpendicular.x + perpendicular.y * perpendicular.y);
        if (length > 0) {
            perpendicular /= length;
        }
        sf::Vector2f control = mid + perpendicular * 50.0f;

        int segments = 30;
        sf::VertexArray curve(sf::PrimitiveType::LineStrip, segments + 1);

        float thickness = 2.0f + (static_cast<float>(cost) / 500.0f);
        thickness = std::min(thickness, 6.0f);

        for (int i = 0; i <= segments; i++) {
            float t = static_cast<float>(i) / static_cast<float>(segments);
            float t2 = t * t;
            float mt = 1.0f - t;
            float mt2 = mt * mt;

            sf::Vector2f point = mt2 * start + 2.0f * mt * t * control + t2 * end;
            curve[i].position = point;
            curve[i].color = color;
        }
        window.draw(curve);

        sf::Vector2f secondLast = curve[segments - 5].position;
        sf::Vector2f last = curve[segments].position;
        sf::Vector2f dir = last - secondLast;
        float dirLength = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (dirLength > 0) {
            dir /= dirLength;
        }

        sf::Vector2f perpDir(-dir.y, dir.x);
        sf::ConvexShape arrow(3);
        arrow.setPoint(0, last);
        arrow.setPoint(1, last - dir * 15.0f + perpDir * 8.0f);
        arrow.setPoint(2, last - dir * 15.0f - perpDir * 8.0f);
        arrow.setFillColor(color);
        window.draw(arrow);
    }

    void handlePanelAction(const string& action)
    {
        cout << "Action pressed: " << action << endl;

        if (action == "Find Direct Routes") {
            runDirectRoutes = true;
            // TODO: Implement direct routes finding
        }
        else if (action == "Find Cheapest Route") {
            runCheapest = true;
            if (selectedOrigin != -1 && selectedDestination != -1) {
                findCheapestRoute(selectedOrigin, selectedDestination, selectedCompany);
            } else {
                cout << "Error: Please select both origin and destination ports first." << endl;
            }
        }
        else if (action == "Find Fastest Route") {
            runFastest = true;
            if (selectedOrigin != -1 && selectedDestination != -1) {
                findFastestRoute(selectedOrigin, selectedDestination, selectedCompany);
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
        assignPositions(static_cast<float>(windowWidth), static_cast<float>(windowHeight));

        sf::RenderWindow window(sf::VideoMode({ static_cast<unsigned int>(windowWidth), static_cast<unsigned int>(windowHeight) }), "Ocean Route Network - Interactive Graph", sf::Style::Close);
        window.setFramerateLimit(100);

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
        //vector<int> highlightedPath; // For optimal path visualization
        //vector<int> exploredPorts; // For algorithm exploration visualization
        //vector<pair<int, int>> highlightedRoutes; // For filtered route display
        float animationTime = 0.0f; // For sequential animations
        sf::Clock animationClock; // For timing animations

        // UI Panel state
        bool showQueryPanel = false; // Toggle query panel visibility
        int hoveredButton = -1; // Which button is being hovered (-1 = none)
        bool selectingOrigin = true; // Toggle between selecting origin/destination

        // Button positions for click detection (will be set during rendering)
        struct ButtonBounds {
            float x, y, width, height;
            string action;
            
        };
        vector<ButtonBounds> buttonBounds;

        // Toggle button bounds
        ButtonBounds toggleButtonBounds;

        while (window.isOpen()) {

            while (std::optional<sf::Event> event = window.pollEvent()) {

                // --- FIX: Recalculate mouse position here ---
                sf::Vector2i mp = sf::Mouse::getPosition(window);
                sf::Vector2f clickPosF(static_cast<float>(mp.x), static_cast<float>(mp.y));

                if (event->is<sf::Event::Closed>()) {
                    window.close();
                }

                // Mouse click handling
                if (event->is<sf::Event::MouseButtonPressed>()) {

                    float mx = clickPosF.x;
                    float my = clickPosF.y;

                    // --- Toggle query panel button click ---
                    if (mx >= toggleButtonBounds.x && mx <= toggleButtonBounds.x + toggleButtonBounds.width &&
                        my >= toggleButtonBounds.y && my <= toggleButtonBounds.y + toggleButtonBounds.height)
                    {
                        showQueryPanel = !showQueryPanel;
                        continue;
                    }

                    // --- If panel is open: check panel action buttons ---
                    if (showQueryPanel)
                    {
                        for (auto& btn : buttonBounds)
                        {
                            if (mx >= btn.x && mx <= btn.x + btn.width &&
                                my >= btn.y && my <= btn.y + btn.height)
                            {
                                handlePanelAction(btn.action);
                            }
                        }

                        // Clicking inside date box â†’ start typing date
                        
                            float panelX = (windowWidth - 400.0f) / 2.0f;
                            float panelY = 100.0f;

                            float dateBoxX = panelX + 20.0f;
                            float dateBoxY = panelY + 60.0f + 75.0f + 75.0f; // based on your offsets

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

                    // --- If panel closed: allow ports to be selected ---
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


                // ================= KEYBOARD INPUT HANDLING =================
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
                    if (dx * dx + dy * dy < 900.0f) {
                        hoveredPort = i;
                        break;
                    }
                }

                window.clear(sf::Color(15, 20, 35));
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
                        {"Find Direct Routes", sf::Color(60, 150, 220)},
                        {"Find Cheapest Route", sf::Color(100, 200, 120)},
                        {"Find Fastest Route", sf::Color(255, 180, 80)},
                        {"Filter by Company", sf::Color(180, 100, 220)},
                        {"Generate Subgraph", sf::Color(220, 120, 180)},
                        {"Simulate Journey", sf::Color(255, 140, 140)}
                    };

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
                                std::min(buttons[i].color.r + 40, 255),
                                std::min(buttons[i].color.g + 40, 255),
                                std::min(buttons[i].color.b + 40, 255)
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
                    // Draw all edges (routes) with support for highlighting
                    for (int i = 0; i < currentSize; i++) {
                        if (graphPorts[i] == nullptr) continue;

                        graphEdgeRoute* route = graphPorts[i]->routeHead;
                        while (route != nullptr) {
                            int destIndex = -1;
                            for (int j = 0; j < currentSize; j++) {
                                if (graphPorts[j] != nullptr && graphPorts[j]->portName == route->destinationName) {
                                    destIndex = j;
                                    break;
                                }
                            }


                            

                            if (destIndex != -1) {
                                sf::Color edgeColor = sf::Color(255, 100, 100, 150);

                                // add graphics for dijikstra/a* //

                                if (hoveredPort == i) {
                                    edgeColor = getCostColor(route->voyageCost);
                                    edgeColor.a = 255;
                                }

                                drawCurvedArrow(window, graphPorts[i]->position, graphPorts[destIndex]->position, edgeColor, route->voyageCost);
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
                        bool isExplored = false;
                        
                        // add graphics for dijikstra/a* //

                        // Outer glow with enhanced brightness for special states
                        float glowRadius = 35.0f;
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
                            glowRadius = 40.0f;
                            glowColor = sf::Color(80, 150, 255, 80);
                        }
                        // Future: Add glow effects for isExplored state

                        sf::CircleShape glow(glowRadius);
                        glow.setPosition(sf::Vector2f(graphPorts[i]->position.x - glowRadius, graphPorts[i]->position.y - glowRadius));
                        glow.setFillColor(glowColor);
                        window.draw(glow);

                        // Main node with state-based coloring
                        sf::CircleShape node(25.0f);
                        node.setPosition(sf::Vector2f(graphPorts[i]->position.x - 25.0f, graphPorts[i]->position.y - 25.0f));

                        if (isOrigin) {
                            node.setFillColor(sf::Color(100, 255, 150));
                            node.setOutlineThickness(4.0f);
                            node.setOutlineColor(sf::Color(150, 255, 180));
                        }
                        else if (isDestination) {
                            node.setFillColor(sf::Color(255, 180, 100));
                            node.setOutlineThickness(4.0f);
                            node.setOutlineColor(sf::Color(255, 220, 150));
                        }
                        else if (isHovered) {
                            node.setFillColor(sf::Color(150, 100, 255));
                            node.setOutlineThickness(3.0f);
                            node.setOutlineColor(sf::Color(200, 150, 255));
                        }
                        else {
                            node.setFillColor(sf::Color(60, 120, 200));
                            node.setOutlineThickness(2.0f);
                            node.setOutlineColor(sf::Color(100, 160, 240));
                        }
                        // Future: Add colors for isExplored (yellow)

                        window.draw(node);

                        // Port name label with indicator for selected ports
                        string portLabel = graphPorts[i]->portName;
                        if (isOrigin) portLabel += " [O]";
                        if (isDestination) portLabel += " [D]";

                        sf::Text portName(font, portLabel, 14);
                        portName.setFillColor(sf::Color(220, 230, 255));
                        portName.setStyle(sf::Text::Bold);
                        sf::FloatRect textBounds = portName.getLocalBounds();
                        portName.setPosition(sf::Vector2f(graphPorts[i]->position.x - textBounds.size.x / 2.0f, graphPorts[i]->position.y + 30.0f));
                        window.draw(portName);

                        // Future: Draw dock queue indicators (dashed lines) when ships are waiting
                        // Future: Draw special icons for preferred ports
                    }

                    // Draw info panel when hovering over a port (left side for future control space)
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
                            sf::Text routeText(font, "-> " + route->destinationName + " ($" + to_string(route->voyageCost) + ")", 12);
                            routeText.setFillColor(sf::Color(180, 190, 210));
                            routeText.setPosition(sf::Vector2f(50.0f, static_cast<float>(windowHeight - 155 + routeCount * 22)));
                            window.draw(routeText);
                            routeCount++;
                            route = route->next;
                        }
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

int main() {
    oceanRouteGraph obj;
    obj.createGraphFromFile("./Routes.txt", "./PortCharges.txt");
    obj.printGraphAfterCreation();

    cout << "\nLaunching interactive graph visualization..." << endl;
    obj.visualizeGraph(1800, 980);

    return 0;
}
