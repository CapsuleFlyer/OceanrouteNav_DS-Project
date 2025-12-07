#pragma once
#include "helper.hpp"
using namespace std;

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
    SimpleVector<int, 1024> highlightedPath;
    int highlightedPathSize;
    int selectedOrigin = -1;
    int selectedDestination = -1;
    string selectedCompany = "";
    bool showAlgorithmVisualization = false;
    SimpleVector<int, 512> vizOpenSet;
    SimpleVector<int, 512> vizClosedSet;

    float whooshProgress = 0.0f;        // 0.0 to 1.0
    bool showWhoosh = false;
    sf::Clock whooshClock;


    /// QUEUEING LOGIC AND VARIABLES
    long long globalSimulationTime = 0;  // minutes since start — increases every frame
   
    // We use SimpleVector<WaitingShip> for each company
    SimpleMap<string, SimpleVector<WaitingShip, 32>> companyQueues;  // [portIndex][company] = queue

    // Each port has limited docks
    SimpleVector<int, 64> availableDocks;  // availableDocks[portIndex] = number of free docks

    // GLOBAL CONSOLE LOG — WORKS EVERYWHERE
    SimpleVector<string, 50> consoleLines;
    bool consoleFirstTime = true; // static removed
    bool simulationMode = false;

    // Subgraph filtering
    string activeCompanyFilter = "";           // e.g., "CMA CGM"
    bool showOnlyActiveRoutes = false;         // toggle
    SimpleVector<int, 512> visiblePorts;       // ports that pass filter
    SimpleVector<pair<int, int>, 1024> visibleEdges;  // edges that pass filter

    // Booking system
    int bookingOrigin = -1;
    int bookingDestination = -1;
    string bookingDate = "";
    bool showBookingResults = false;
    SimpleVector<PathResult, 8> bookingOptions;  // all possible routes (direct + connecting)

    string selectedDate = ""; // Selected date for filtering

    bool animateBookingRoute = false;
    int currentBookingRouteIndex = 0;
    float bookingAnimProgress = 0.0f;
    sf::Clock bookingAnimClock;
    int currentSegmentIndex = 0;

    bool showLayoverInfo = false;
    int layoverPortIndex = -1;
    long long layoverArrivalTime = 0;
    long long layoverNextDeparture = 0;

    MultiLegJourney currentJourney;
    bool buildingMultiLeg = false;

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

    void findAllFeasibleRoutes(int origin, int destination, const string& date = "") {
        bookingOptions.clear();
        showBookingResults = true;

        // === 1. DIRECT ROUTES — EXACT DATE STRING MATCH ===
        graphEdgeRoute* route = graphPorts[origin]->routeHead;
        while (route != nullptr) {
            int destIdx = getPortIndex(route->destinationName);
            if (destIdx == destination) {
                bool dateOk = date.empty() || (route->date == date);

                if (dateOk) {
                    PathResult direct;
                    direct.found = true;
                    direct.totalCost = route->voyageCost + graphPorts[destination]->dailyPortCharge;
                    direct.pathSize = 0;
                    direct.path = new int[512];
                    direct.path[direct.pathSize++] = origin;
                    direct.path[direct.pathSize++] = destination;

                    bookingOptions.push_back(direct);
                    addLog("DIRECT: " + route->date + " -> $" + to_string(direct.totalCost));
                }
            }
            route = route->next;
        }

        // === 2. 1-STOP — ONLY REAL SAILINGS + EXACT DATE MATCH ===
        for (int mid = 0; mid < currentSize; mid++) {
            if (!graphPorts[mid] || mid == origin || mid == destination) continue;

            graphEdgeRoute* leg1 = nullptr;
            graphEdgeRoute* leg2 = nullptr;
            long long arrivalAtMid = 0;

            // Find leg1: origin → mid on exact date (or any if no date)
            route = graphPorts[origin]->routeHead;
            while (route && !leg1) {
                if (getPortIndex(route->destinationName) == mid) {
                    if (date.empty() || route->date == date) {
                        leg1 = route;
                        long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);
                        long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                        if (arr < dep) arr += 24 * 60;
                        arrivalAtMid = arr;
                    }
                }
                route = route->next;
            }
            if (!leg1) continue;

            // Find leg2: mid → destination with transfer time
            route = graphPorts[mid]->routeHead;
            while (route && !leg2) {
                if (getPortIndex(route->destinationName) == destination) {
                    long long depFromMid = parseDateTimeToMinutes(route->date, route->departureTime);
                    if (depFromMid >= arrivalAtMid + 120) {  // 2+ hour transfer
                        leg2 = route;
                    }
                }
                route = route->next;
            }
            if (!leg2) continue;

            showLayoverInfo = true;
            layoverPortIndex = mid;
            layoverArrivalTime = arrivalAtMid;
            layoverNextDeparture = parseDateTimeToMinutes(leg2->date, leg2->departureTime);

            // Build real 1-stop route
            PathResult connected;
            connected.found = true;
            connected.totalCost = leg1->voyageCost + leg2->voyageCost + graphPorts[destination]->dailyPortCharge;
            connected.pathSize = 0;
            connected.path = new int[512];
            connected.path[connected.pathSize++] = origin;
            connected.path[connected.pathSize++] = mid;
            connected.path[connected.pathSize++] = destination;

            bookingOptions.push_back(connected);
            addLog("1-STOP via " + graphPorts[mid]->portName + " (" + leg1->date + " -> " + leg2->date + ")");
        }

        addLog("Found " + to_string(bookingOptions.size()) + " real routes");
    }

    string minutesToDateTime(long long minutes) {
        int totalDays = minutes / (24 * 60);
        int hours = (minutes % (24 * 60)) / 60;
        int mins = minutes % 60;

        // Simple — just show time and day offset
        string sign = totalDays >= 0 ? "+" : "";
        if (totalDays == 0) return to_string(hours) + ":" + (mins < 10 ? "0" : "") + to_string(mins);
        return to_string(totalDays) + "d " + to_string(hours) + ":" + (mins < 10 ? "0" : "") + to_string(mins);
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

        
    }

    // parse the string from get line ourselves
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
                    try { voyageCost = stoi(currentString); }
                    catch (...) { voyageCost = 0; return false; } // using a sting to int converter as the currentString is a string and the voyageCost is an int variable
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
                try { voyageCost = stoi(currentString); }
                catch (...) { voyageCost = 0; return false; }
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

        try { voyageCost = stoi(voyageCostString); }
        catch (...) { voyageCost = 0; return false; }
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
                    try { dailyChargesOfPort = stoi(currentString); }
                    catch (...) { dailyChargesOfPort = 0; return false; }       
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
                try { dailyChargesOfPort = stoi(currentString); }
                catch (...) { dailyChargesOfPort = 0; return false; }
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

        try { dailyChargesOfPort = stoi(dailyChargesString); }
        catch (...) { dailyChargesOfPort = 0; return false; }

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

    int calculateHeuristic(int fromIndex, int toIndex) { //gets the euclidean distance
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

    bool parseDateTime(const string& dateStr, const string& timeStr, int& day, int& month, int& year, int& hour, int& minute){
    if (dateStr.size() < 10 || timeStr.size() < 5) return false;

    try {
        day    = stoi(dateStr.substr(0,2));
        month  = stoi(dateStr.substr(3,2));
        year   = stoi(dateStr.substr(6,4));
        hour   = stoi(timeStr.substr(0,2));
        minute = stoi(timeStr.substr(3,2));

        // Basic sanity checks
        if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1000 || hour > 23 || minute > 59)
            return false;

        return true;
    }
    catch (...) {
        return false;   // any stoi failure → treat as invalid
    }
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

    PathResult dijkstraShortestPath(int startIndex, int endIndex, const string& preferredCompany = "", const SimpleVector<string>& avoidPorts = {}, int maxVoyageHours = -1) {
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
                    // ←←←←← ADD THIS BLOCK HERE →→→→→
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // ←←←←← END BLOCK →→→→→

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

            // PHASE 2: If no preferred route found → allow ANY company
            if (!usedPreferred || !preferredCompany.empty()) {
                route = graphPorts[u]->routeHead;
                while (route != nullptr) {
                    int v = getPortIndex(route->destinationName);
                    if (v == -1 || visited[v]) { route = route->next; continue; }

                    // ←←←←← GLOBAL COMPANY FILTER — MUST BE IN BOTH PHASES →→→→→
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // ←←←←← END FILTER →→→→→

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

    PathResult aStarCheapestWithPrefs(int startIndex, int endIndex, const string& preferredCompany = "", const SimpleVector<string>& avoidPorts = {}, int maxVoyageHours = -1) {
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
                    // ←←←←← ADD THIS BLOCK HERE →→→→→
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // ←←←←← END BLOCK →→→→→

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

                    // If we STILL miss the ship → skip
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

                    // ←←←←← GLOBAL COMPANY FILTER — MUST BE IN BOTH PHASES →→→→→
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // ←←←←← END FILTER →→→→→

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

    PathResult dijkstraFastestWithPrefs(int startIndex, int endIndex, const string& preferredCompany = "", const SimpleVector<string>& avoidPorts = {}, int maxVoyageHours = -1) {
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
                    // ←←←←← ADD THIS BLOCK HERE →→→→→
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // ←←←←← END BLOCK →→→→→

                    bool skip = false;
                    for (int i = 0; i < avoidPorts.size(); i++) {
                        if (graphPorts[v]->portName == avoidPorts[i]) { skip = true; break; }
                    }
                    if (skip) { route = route->next; continue; }

                    long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                    long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);

                    // If arrival is earlier than departure -> next day
                    if (arr < dep) arr += 24LL * 60LL;

                    // Adjust departure to be after current time (catching the ship)
                    long long adjustedDep = dep;
                    if (adjustedDep < time)
                        adjustedDep += 24LL * 60LL;

                    // If even after adjusting, you still cannot catch it → skip
                    if (time > adjustedDep) {
                        route = route->next;
                        continue;
                    }

                    // Adjust arrival accordingly
                    long long adjustedArr = arr;
                    if (adjustedArr < adjustedDep)
                        adjustedArr += 24LL * 60LL;

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

                    // ←←←←← GLOBAL COMPANY FILTER — MUST BE IN BOTH PHASES →→→→→
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // ←←←←← END FILTER →→→→→

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

    PathResult aStarFastestWithPrefs(int startIndex, int endIndex, const string& preferredCompany = "", const SimpleVector<string>& avoidPorts = {}, int maxVoyageHours = -1) {
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

                    // ←←←←← ADD THIS BLOCK HERE →→→→→
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // ←←←←← END BLOCK →→→→→

                    bool skip = false;
                    for (int i = 0; i < avoidPorts.size(); i++) {
                        if (graphPorts[v]->portName == avoidPorts[i]) { skip = true; break; }
                    }
                    if (skip) { route = route->next; continue; }


                    long long dep = parseDateTimeToMinutes(route->date, route->departureTime);
                    long long arr = parseDateTimeToMinutes(route->date, route->arrivalTime);

                    // Normalize arrival (next-day arrival case)
                    if (arr < dep) arr += 24LL * 60LL;

                    // --- ADJUST DEPARTURE BASED ON ARRIVAL TIME AT U ---
                    long long adjustedDep = dep;
                    if (adjustedDep < arrivalAtU) {
                        adjustedDep += 24LL * 60LL;   // catch tomorrow's ship
                    }

                    // If even after adjusting you still miss it → skip
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

                    // --- RELAX EDGE: If preferred company → add small bias ---
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
                    // ←←←←← GLOBAL COMPANY FILTER — MUST BE IN BOTH PHASES →→→→→
                    if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                        if (route->shippingCompanyName != activeCompanyFilter) {
                            route = route->next;
                            continue;
                        }
                    }
                    // ←←←←← END FILTER →→→→→

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

    SimpleVector<int, 512> findFastestMultiLegPath(const MultiLegJourney& journey, bool useAStar = true) {
        SimpleVector<int, 512> finalPath;

        if (journey.legCount < 2) {
            addLog("Error: Need at least 2 stops");
            return finalPath; // empty
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
                addLog("No route from " + graphPorts[from]->portName + " to " + graphPorts[to]->portName);
                finalPath.clear();
                return finalPath;
            }

            // SAFELY APPEND (skip first node)
            for (int j = 1; j < segment.pathSize; j++) {
                finalPath.push_back(segment.path[j]);
            }

            fromLeg = toLeg;
            toLeg = toLeg->next;
        }

        return finalPath; // returns valid path or empty
    }

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

            // ←←←←← ADD SHIPS TO QUEUE — RIGHT HERE →→→→→
            for (int i = 0; i < currentPath.pathSize - 1; i++) {
                int portIdx = currentPath.path[i];

                WaitingShip ship;
                ship.company = selectedCompany.empty() ? "CargoLine" : selectedCompany;
                ship.arrivalTime = globalSimulationTime + i * 5000;
                ship.serviceEndTime = ship.arrivalTime + 8 * 60;

                graphPorts[portIdx]->dockingQueue.push_back(ship);
                cout << "ADDED SHIP to " << graphPorts[portIdx]->portName << endl;
            }
            // ←←←←← END →→→→→

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
                highlightedPath.clear();  // ← THIS FIXES THE HIGHLIGHT BUG
                string companyToUse = showOnlyActiveRoutes ? activeCompanyFilter : "";
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
                highlightedPath.clear();  // ← THIS FIXES THE HIGHLIGHT BUG
                string companyToUse = showOnlyActiveRoutes ? activeCompanyFilter : "";
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
                cout << "Multi-leg mode ON — click ports in order" << endl;
                // DO NOT CLEAR — keep previous journey!
                currentJourney.clear(); // ← DELETE THIS LINE
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
                // If no company typed → turn OFF filter
                showOnlyActiveRoutes = false;
                activeCompanyFilter = "";
                addLog("Company filter OFF — showing all routes");
            }
            else {
                // Turn ON filter with selected company
                activeCompanyFilter = selectedCompany;
                showOnlyActiveRoutes = true;
                // ← THIS ACTIVATES THE FILTER
                addLog("Company filter ON: " + activeCompanyFilter);
            }

            generateSubgraph();  // ← REBUILD VISIBLE PORTS/EDGES
            highlightedPath.clear();  // ← clear old path
            showQueryPanel = false;
        }
        else if (action == "Book Route") {
            runSubgraph = true;
            if (selectedOrigin == -1 || selectedDestination == -1) {
                addLog("Error: Select origin and destination first");
                return;
            }

            bookingOrigin = selectedOrigin;
            bookingDestination = selectedDestination;
            bookingDate = selectedDate;

            addLog("Searching all routes from " + graphPorts[bookingOrigin]->portName +
                " to " + graphPorts[bookingDestination]->portName);
            highlightedPath.clear();  // ← CLEAR OLD GREEN PATH
            //currentPath.clear();      // ← CLEAR OLD currentPath

            findAllFeasibleRoutes(bookingOrigin, bookingDestination, bookingDate);
            showQueryPanel = false;
            animateBookingRoute = true;
            currentBookingRouteIndex = 0;
            currentSegmentIndex = 0;
            bookingAnimProgress = 0.0f;
            bookingAnimClock.restart();
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
                        { sf::Vector2f(graphPorts[i]->position), sf::Color(80,80,80,80)},
                        { sf::Vector2f(graphPorts[v]->position), sf::Color(80,80,80,80)}
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
        //string selectedDate = ""; // Selected date for filtering
        float animationTime = 0.0f; // For sequential animations
        sf::Clock animationClock; // For timing animations

        // UI Panel state
        bool showQueryPanel = false; // Toggle query panel visibility
        int hoveredButton = -1; // Which button is being hovered (-1 = none)
        bool selectingOrigin = true; // Toggle between selecting origin/destination

        // Algorithm toggle state
        bool useAStar = false; // false = Dijkstra, true = A*


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

                                    highlightedPath.clear();   // ⭐ NOW WE CAN CLEAR IT HERE
                                    continue;
                                } 

                                if (btn.action == "Build Multi-Leg Route") {
                                    
                                    buildingMultiLeg = !buildingMultiLeg;
                                    if (!buildingMultiLeg) {
                                        cout << "Multi-leg mode OFF" << endl;
                                    }
                                    else {
                                        cout << "Multi-leg mode ON — click ports in order" << endl;
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

                                        // ←←←← START WHOOSH EFFECT →→→→
                                        showWhoosh = true;
                                        whooshProgress = 0.0f;
                                        whooshClock.restart();
                                        // ←←←← END →→→→
                                    }

                                    showQueryPanel = false;
                                }

                                if (btn.action == "Filter by Company") {
                                    activeCompanyFilter = selectedCompany;
                                    showOnlyActiveRoutes = !showOnlyActiveRoutes;
                                    generateSubgraph();
                                }

                                
                                
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

                    if (!showQueryPanel) {
                        if (buildingMultiLeg) {
                            // ←←←←← CLICK-TO-REMOVE + CLICK-TO-ADD →→→→→
                            bool removed = false;
                            JourneyLeg* leg = currentJourney.head;
                            while (leg) {
                                sf::Vector2f pos = graphPorts[leg->portIndex]->position;
                                float dx = clickPosF.x - pos.x;
                                float dy = clickPosF.y - pos.y;
                                if (dx * dx + dy * dy < 400) {
                                    int portToRemove = leg->portIndex;  // ← SAVE FIRST!
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
                            // ←←←←← END →→→→→
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
                        {"Book Route", sf::Color(220, 120, 180)},
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

                                    sf::Color edgeColor = showOnlyActiveRoutes ?
                                        getCostColor(route->voyageCost) :           // ← COST COLOR IN SUBGRAPH
                                        sf::Color(120, 30, 30, 180);                // ← DARK RED WHEN NOT FILTERED

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

                            // ←←←←← ADD SUBGRAPH FILTER RIGHT HERE →→→→→
                            bool edgeVisible = true;
                            if (showOnlyActiveRoutes && !activeCompanyFilter.empty()) {
                                if (route->shippingCompanyName != activeCompanyFilter) {
                                    edgeVisible = false;
                                }
                            }
                            if (!edgeVisible) {
                                route = route->next;   // ← SKIP THIS EDGE
                                continue;              // ← GO TO NEXT ROUTE
                            }
                            // ←←←←← END FILTER →→→→→

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
                            // First ship is now docked — just leave it in queue[0]
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
                                        { sf::Vector2f(basePos + sf::Vector2f((q - 1) * 20 + 7, 3)), sf::Color::Yellow},
                                        { sf::Vector2f(basePos + sf::Vector2f(q * 20 + 7, 3)), sf::Color::Yellow}
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
                            // === ANIMATED DOCKING QUEUE — REPLACE YOUR CURRENT ONE WITH THIS ===
                            if (graphPorts[i]->dockingQueue.size() > 0 || graphPorts[i]->currentDocked > 0) {
                                sf::Vector2f portPos = graphPorts[i]->position;
                                sf::Vector2f queueStart = portPos + sf::Vector2f(40, -60);
                                float spacing = 22.0f;

                                int waiting = graphPorts[i]->dockingQueue.size();
                                int docked = graphPorts[i]->currentDocked;
                                int totalInPort = waiting + docked;

                                // Draw docked ships (inside the port — green)
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
                                            { sf::Vector2f(sf::Vector2f(animX + 8, queueStart.y + 4)), sf::Color::Yellow },
                                            { sf::Vector2f(sf::Vector2f(animX + spacing - 8, queueStart.y + 4)), sf::Color::Yellow }
                                        };
                                        window.draw(dash, 2, sf::PrimitiveType::Lines);
                                    }

                                    // Arrow from last waiting ship to dock
                                    if (q == waiting - 1 && docked > 0) {
                                        sf::Vector2f from = sf::Vector2f(animX + 8, queueStart.y);
                                        sf::Vector2f to = portPos + sf::Vector2f(-20, 20);
                                        sf::Vertex arrow[] = {
                                            { sf::Vector2f(from), sf::Color::Yellow},
                                            { sf::Vector2f(to), sf::Color::Yellow}
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
                            { sf::Vector2f(p1), sf::Color::Cyan},
                            { sf::Vector2f(p2), sf::Color::Cyan}
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
                                { sf::Vector2f(p2), sf::Color::Cyan},
                                { sf::Vector2f(tip1), sf::Color::Cyan},
                                { sf::Vector2f(p2), sf::Color::Cyan},
                                { sf::Vector2f(tip2), sf::Color::Cyan}
                            };
                            window.draw(arrow, 4, sf::PrimitiveType::Lines);
                        }
                    }
                    prev = leg;
                    leg = leg->next;
                }
                // === END MULTI-LEG DRAWING ===


                // === WHOOSH EFFECT — EPIC SWOOSH ACROSS PATH ===
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
                titleText.setPosition(sf::Vector2f(0.0f, panelY));  // ← FIXED
                window.draw(titleText);

                // ←←←←← ADD AUTO-CLEARING RIGHT HERE →→→→→
                int maxLines = 16;  // how many lines fit in panel
                if (consoleLines.size() > maxLines) {
                    for (int i = 0; i < maxLines; i++) {
                        consoleLines[i] = consoleLines[consoleLines.size() - maxLines + i];
                    }
                    consoleLines.sz = maxLines;
                }
                // ←←←←← END AUTO-CLEARING →→→→→

                // Draw log lines
                for (int i = 0; i < consoleLines.size(); i++) {
                    sf::Text line(font, consoleLines[i], 14);
                    line.setFillColor(i == consoleLines.size() - 1 ? sf::Color::Yellow : sf::Color(200, 200, 200));
                    line.setPosition(sf::Vector2f(20.0f, panelY + 50.0f + i * 22.0f));
                    window.draw(line);
                }

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

                

                // === BOOKING RESULTS PANEL — WITH COLOR LEGEND ===
                if (showBookingResults && bookingOptions.size() > 0) {
                    float panelX = windowWidth - 420;
                    float panelY = 100;

                    sf::RectangleShape panel(sf::Vector2f(400, 380));
                    panel.setPosition(sf::Vector2f(panelX, panelY));
                    panel.setFillColor(sf::Color(0, 0, 0, 240));
                    panel.setOutlineThickness(3);
                    panel.setOutlineColor(sf::Color(100, 200, 255));
                    window.draw(panel);

                    sf::Text title(font, "AVAILABLE ROUTES", 22);
                    title.setFillColor(sf::Color::Cyan);
                    title.setStyle(sf::Text::Bold);
                    title.setPosition(sf::Vector2f(panelX + 20, panelY + 15));
                    window.draw(title);

                    // LEGEND
                    sf::Text legend(font, "DIRECT ROUTE", 16);
                    legend.setFillColor(sf::Color(255, 80, 80));
                    legend.setPosition(sf::Vector2f(panelX + 20, panelY + 60));
                    window.draw(legend);

                    sf::Text legend2(font, "1-STOP CONNECTION", 16);
                    legend2.setFillColor(sf::Color(255, 220, 100));
                    legend2.setPosition(sf::Vector2f(panelX + 20, panelY + 90));
                    window.draw(legend2);

                    // === CLOSE BUTTON ===
                    sf::RectangleShape closeBtn(sf::Vector2f(30, 30));
                    closeBtn.setPosition(sf::Vector2f(panelX + 350, panelY + 10));
                    closeBtn.setFillColor(sf::Color(200, 50, 50));
                    window.draw(closeBtn);

                    sf::Text closeX(font, "X", 20);
                    closeX.setFillColor(sf::Color::White);
                    closeX.setStyle(sf::Text::Bold);
                    closeX.setPosition(sf::Vector2f(panelX + 358, panelY + 8));
                    window.draw(closeX);

                    // Draw each booking option
                    for (int i = 0; i < bookingOptions.size(); i++) {
                        const PathResult& opt = bookingOptions[i];
                        bool isDirect = (opt.pathSize == 2);

                        sf::Color textColor = isDirect ? sf::Color(255, 100, 100) : sf::Color(255, 230, 100);
                        sf::Color routeColor = isDirect ? sf::Color(255, 50, 50) : sf::Color(255, 200, 50);

                        string type = isDirect ? "DIRECT" : "1-STOP";
                        sf::Text routeText(font, type + " -> $" + to_string(opt.totalCost), 18);
                        routeText.setFillColor(textColor);
                        routeText.setStyle(sf::Text::Bold);
                        routeText.setPosition(sf::Vector2f(panelX + 20, panelY + 130 + i * 60));
                        window.draw(routeText);

                        // HIGHLIGHT ROUTE ON MAP — THICK + GLOW
                        for (int j = 0; j + 1 < opt.pathSize; j++) {
                            int a = opt.path[j];
                            int b = opt.path[j + 1];
                            sf::Vector2f p1 = graphPorts[a]->position;
                            sf::Vector2f p2 = graphPorts[b]->position;

                            // GLOW (faint outer line)
                            sf::Vertex glow[] = {
                                { sf::Vector2f(p1), sf::Color(routeColor.r, routeColor.g, routeColor.b, 80)},
                                { sf::Vector2f(p2), sf::Color(routeColor.r, routeColor.g, routeColor.b, 80)}
                            };
                            sf::VertexArray glowLines(sf::PrimitiveType::Lines, 2);
                            glowLines[0].position = p1; glowLines[0].color = sf::Color(routeColor.r, routeColor.g, routeColor.b, 80);
                            glowLines[1].position = p2; glowLines[1].color = sf::Color(routeColor.r, routeColor.g, routeColor.b, 80);
                            window.draw(glowLines);

                            // THICK MAIN LINE
                            sf::Vertex main[] = {
                                { sf::Vector2f(p1), routeColor},
                                { sf::Vector2f(p2), routeColor}
                            };
                            sf::VertexArray mainLines(sf::PrimitiveType::Lines, 2);
                            mainLines[0].position = p1; mainLines[0].color = routeColor;
                            mainLines[1].position = p2; mainLines[1].color = routeColor;
                            window.draw(mainLines);

                            // ARROWHEAD
                            sf::Vector2f dir = p2 - p1;
                            float len = sqrt(dir.x * dir.x + dir.y * dir.y);
                            if (len > 20) {
                                dir /= len;
                                sf::Vector2f perp(-dir.y, dir.x);
                                sf::Vector2f tip1 = p2 - (dir * 30.0f) + (perp * 15.0f);
                                sf::Vector2f tip2 = p2 - (dir * 30.0f) - (perp * 15.0f);

                                sf::Vertex arrow[] = {
                                    { sf::Vector2f(p2), routeColor},
                                    { sf::Vector2f(tip1), routeColor},
                                    { sf::Vector2f(p2), routeColor},
                                    { sf::Vector2f(tip2), routeColor}
                                };
                                window.draw(arrow, 4, sf::PrimitiveType::Lines);
                            }
                        }
                    }
                }


                // === SEQUENTIAL PATH REVEAL ANIMATION — CINEMATIC ===
                if (showBookingResults && bookingOptions.size() > 0) {
                    const PathResult& route = bookingOptions[currentBookingRouteIndex];

                    // Animation control
                    static float revealProgress = 0.0f;
                    static sf::Clock revealClock;
                    static int currentRevealRoute = 0;

                    if (revealClock.getElapsedTime().asSeconds() > 0.15f) {  // 0.5 sec per segment
                        revealProgress += 0.12f;
                        if (revealProgress >= 1.0f) {
                            revealProgress = 0.0f;
                            currentSegmentIndex++;

                            if (currentSegmentIndex + 1 >= route.pathSize) {
                                // Finished this route → next one
                                currentBookingRouteIndex++;
                                currentSegmentIndex = 0;
                                if (currentBookingRouteIndex >= bookingOptions.size()) {
                                    currentBookingRouteIndex = 0;  // loop or stop
                                }
                            }
                        }
                        revealClock.restart();
                    }

                    // Draw revealed portion of current route
                    bool isDirect = (route.pathSize == 2);
                    sf::Color routeColor = isDirect ? sf::Color(255, 80, 80) : sf::Color(255, 220, 80);

                    for (int j = 0; j < currentSegmentIndex; j++) {
                        if (j + 1 >= route.pathSize) break;
                        int a = route.path[j];
                        int b = route.path[j + 1];
                        sf::Vector2f p1 = graphPorts[a]->position;
                        sf::Vector2f p2 = graphPorts[b]->position;

                        // Full segment (already revealed)
                        sf::Vertex line[] = {
                            { sf::Vector2f(p1), routeColor},
                            { sf::Vector2f(p2), routeColor}
                        };
                        window.draw(line, 2, sf::PrimitiveType::Lines);
                    }

                    // Draw current animating segment
                    if (currentSegmentIndex + 1 < route.pathSize) {
                        int a = route.path[currentSegmentIndex];
                        int b = route.path[currentSegmentIndex + 1];
                        sf::Vector2f p1 = graphPorts[a]->position;
                        sf::Vector2f p2 = graphPorts[b]->position;
                        sf::Vector2f currentPos = p1 + (p2 - p1) * revealProgress;

                        // Glowing segment
                        sf::Vertex animLine[] = {
                            { sf::Vector2f(p1), routeColor},
                            { sf::Vector2f(currentPos), routeColor}
                        };
                        window.draw(animLine, 2, sf::PrimitiveType::Lines);

                        // Ship at the front
                        sf::CircleShape ship(14);
                        ship.setFillColor(sf::Color::White);
                        ship.setOutlineThickness(4);
                        ship.setOutlineColor(routeColor);
                        ship.setPosition(currentPos - sf::Vector2f(14, 14));
                        window.draw(ship);

                        // Glow pulse
                        float pulse = 20 + 10 * sin(revealClock.getElapsedTime().asSeconds() * 10);
                        sf::CircleShape glow(pulse);
                        glow.setFillColor(sf::Color(routeColor.r, routeColor.g, routeColor.b, 80));
                        glow.setPosition(currentPos - sf::Vector2f(pulse, pulse));
                        window.draw(glow);
                    }
                }
                

                // === SMALL HOVER LAYOVER INFO BOX — ELEGANT & PROFESSIONAL ===
                if (showLayoverInfo && layoverPortIndex != -1) {
                    sf::Vector2f portPos = graphPorts[layoverPortIndex]->position;

                    // Check if mouse is near the layover port
                    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                    sf::Vector2f mousePosF(mousePos.x, mousePos.y);
                    float dx = mousePosF.x - portPos.x;
                    float dy = mousePosF.y - portPos.y;
                    bool isHovered = (dx * dx + dy * dy < 40 * 40);  // 40px radius

                    if (isHovered) {
                        // Background box — small and clean
                        sf::RectangleShape box(sf::Vector2f(220, 160));
                        box.setFillColor(sf::Color(0, 0, 0, 240));
                        box.setOutlineThickness(3);
                        box.setOutlineColor(sf::Color(255, 215, 0));  // gold border
                        box.setPosition(sf::Vector2f(portPos.x - 110.0f, portPos.y - 180.0f));
                        window.draw(box);

                        // Helper to center text
                        auto centerText = [&](sf::Text& text, float y) {
                            // Get bounds — MUST be after setting text content and font!
                            // Get bounds — MUST be after setting text content and font!
                            sf::FloatRect rect = text.getLocalBounds();

                            // Set origin to center
                            text.setOrigin(rect.size / 2.0f);
                            text.setPosition(sf::Vector2f(portPos.x, y));
                            };

                        // Port name
                        sf::Text portName(font, graphPorts[layoverPortIndex]->portName, 20);
                        portName.setFillColor(sf::Color::White);
                        portName.setStyle(sf::Text::Bold);
                        centerText(portName, portPos.y - 150);
                        window.draw(portName);

                        // Arrival
                        sf::Text arr(font, "ARR: " + graphPorts[layoverPortIndex]->routeHead->arrivalTime, 16);
                        arr.setFillColor(sf::Color(100, 255, 100));
                        centerText(arr, portPos.y - 120);
                        window.draw(arr);

                        // Departure
                        sf::Text dep(font, "DEP:  " + graphPorts[layoverPortIndex]->routeHead->departureTime, 16);
                        dep.setFillColor(sf::Color(255, 150, 150));
                        centerText(dep, portPos.y - 95);
                        window.draw(dep);

                        // Layover time
                        long long layoverHours = (parseDateTimeToMinutes(graphPorts[layoverPortIndex]->routeHead->date, graphPorts[layoverPortIndex]->routeHead->departureTime) - parseDateTimeToMinutes(graphPorts[layoverPortIndex]->routeHead->date, graphPorts[layoverPortIndex]->routeHead->arrivalTime)) / 60;
                        sf::Text layover(font, "Layover: " + to_string(layoverHours) + "h", 18);
                        layover.setFillColor(layoverHours >= 2 ? sf::Color(100, 255, 100) : sf::Color(255, 100, 100));
                        layover.setStyle(sf::Text::Bold);
                        centerText(layover, portPos.y - 65);
                        window.draw(layover);

                        // Verdict
                        sf::Text verdict(font, layoverHours >= 2 ? "FEASIBLE" : "TOO TIGHT!", 22);
                        verdict.setFillColor(layoverHours >= 2 ? sf::Color(100, 255, 100) : sf::Color(255, 80, 80));
                        verdict.setStyle(sf::Text::Bold);
                        centerText(verdict, portPos.y - 35);
                        window.draw(verdict);
                    }
                }
                window.display();
            }
        }
    }
};
