#pragma once
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
    SimpleVector<WaitingShip, 32> dockingQueue;   // ← QUEUE OF WAITING SHIPS
    int maxDocks = 3;                             // ← MAX DOCKING SLOTS
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

struct PathResult { // just stores the pathing
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
