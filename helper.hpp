#pragma once
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>
#include <stdexcept>

#define TOTALNUMBEROFPORTS 40
#define MAX_COST 999999999

using namespace std;

// ===========================
// CUSTOM STL CONTAINERS
// ===========================

template<typename T, int N = 256>
struct SimpleVector {
    T data[N];
    int sz;

    SimpleVector() : sz(0) {}

    void push_back(const T& v) {
        if (sz >= N) throw std::overflow_error("SimpleVector overflow");
        data[sz++] = v;
    }

    bool empty() const { return sz == 0; }
    void clear() { sz = 0; }
    int size() const { return sz; }

    T& operator[](int i) { 
        if (i < 0 || i >= sz) throw std::out_of_range("SimpleVector index out of bounds"); 
        return data[i]; 
    }

    const T& operator[](int i) const { 
        if (i < 0 || i >= sz) throw std::out_of_range("SimpleVector index out of bounds"); 
        return data[i]; 
    }

    T* begin() { return data; }
    T* end() { return data + sz; }
};

template<typename T, int N = 256>
struct SimpleQueue {
    T data[N];
    int front, back;

    SimpleQueue() : front(0), back(0) {}

    void push(const T& v) {
        int nextBack = (back + 1) % N;
        if (nextBack == front) throw std::overflow_error("SimpleQueue overflow");
        data[back] = v;
        back = nextBack;
    }

    void pop() {
        if (empty()) throw std::underflow_error("SimpleQueue underflow");
        front = (front + 1) % N;
    }

    T& top() {
        if (empty()) throw std::underflow_error("SimpleQueue empty");
        return data[front];
    }

    bool empty() const { return front == back; }
};

template<typename T, int N = 256>
struct SimplePriorityQueue {
    T data[N];
    int sz;

    SimplePriorityQueue() : sz(0) {}

    void push(const T& v) {
        if (sz >= N) throw std::overflow_error("SimplePriorityQueue overflow");
        int i = sz++;
        data[i] = v;
        while (i > 0 && data[(i - 1) / 2] > data[i]) {
            std::swap(data[i], data[(i - 1) / 2]);
            i = (i - 1) / 2;
        }
    }

    void pop() {
        if (sz == 0) throw std::underflow_error("SimplePriorityQueue empty");
        data[0] = data[--sz];
        int i = 0;
        while (2 * i + 1 < sz) {
            int j = 2 * i + 1;
            if (j + 1 < sz && data[j + 1] < data[j]) j++;
            if (!(data[j] < data[i])) break;
            std::swap(data[i], data[j]);
            i = j;
        }
    }

    T& top() {
        if (sz == 0) throw std::underflow_error("SimplePriorityQueue empty");
        return data[0];
    }

    bool empty() const { return sz == 0; }
};

template<typename K, typename V, int N = 256>
struct SimpleMap {
    K keys[N];
    V values[N];
    int sz;

    SimpleMap() : sz(0) {}

    V& operator[](const K& k) {
        for (int i = 0; i < sz; ++i) if (keys[i] == k) return values[i];
        if (sz >= N) throw std::overflow_error("SimpleMap overflow");
        keys[sz] = k;
        values[sz] = V();
        return values[sz++];
    }

    struct iterator {
        K* k; V* v;
        iterator(K* kk, V* vv) : k(kk), v(vv) {}
        iterator& operator++() { ++k; ++v; return *this; }
        bool operator!=(const iterator& o) const { return k != o.k; }
        std::pair<K, V> operator*() const { return {*k, *v}; }
    };

    iterator begin() { return iterator(keys, values); }
    iterator end() { return iterator(keys + sz, values + sz); }

    int size() const { return sz; }
    bool contains(const K& k) const { for (int i = 0; i < sz; i++) if (keys[i] == k) return true; return false; }
    int find(const K& k) const { for (int i = 0; i < sz; i++) if (keys[i] == k) return i; return -1; }
};

// ===========================
// UTILITY FUNCTIONS
// ===========================
inline int my_min(int a, int b) { return a < b ? a : b; }
inline int my_max(int a, int b) { return a > b ? a : b; }
inline float my_min(float a, float b) { return a < b ? a : b; }
inline float my_max(float a, float b) { return a > b ? a : b; }
inline char my_tolower(char c) { return (c >= 'A' && c <= 'Z') ? (c + 32) : c; }
inline bool my_isalnum(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }

// ===========================
// GRAPH-RELATED DATA STRUCTURES
// ===========================

struct graphEdgeRoute {
    string destinationName, date, departureTime, arrivalTime, shippingCompanyName;
    int voyageCost;
    graphEdgeRoute* next = nullptr;

    graphEdgeRoute() : voyageCost(0) {}
    graphEdgeRoute(string dest, string d, string dep, string arr, int cost, string comp)
        : destinationName(dest), date(d), departureTime(dep), arrivalTime(arr), voyageCost(cost), shippingCompanyName(comp), next(nullptr) {}
};

struct WaitingShip {
    string company;
    long long arrivalTime = 0, serviceStartTime = -1, serviceEndTime = -1;
    int dockSlot = -1;

    WaitingShip() = default;
    WaitingShip(string c, long long at) : company(c), arrivalTime(at) {}
};

struct graphVerticePort {
    string portName;
    int dailyPortCharge;
    graphEdgeRoute* routeHead = nullptr;
    sf::Vector2f position;
    float latitude = 0.0f, longitude = 0.0f;
    bool hasCoordinates = false;
    SimpleVector<WaitingShip, 32> dockingQueue;
    int maxDocks = 3, currentDocked = 0;

    graphVerticePort() : portName(""), dailyPortCharge(0), routeHead(nullptr) {}
    graphVerticePort(string name, int charge) : portName(name), dailyPortCharge(charge), routeHead(nullptr) {}
};

struct PathResult {
    int* path = nullptr;
    int pathSize = 0;
    int totalCost = MAX_COST;
    long long totalTimeMinutes = 0;
    bool found = false;

    PathResult() = default;
    ~PathResult() { delete[] path; }

    PathResult(const PathResult& other)
        : pathSize(other.pathSize), totalCost(other.totalCost), totalTimeMinutes(other.totalTimeMinutes), found(other.found)
    {
        if (other.path && pathSize > 0) { path = new int[pathSize]; for (int i = 0; i < pathSize; i++) path[i] = other.path[i]; }
    }

    PathResult& operator=(const PathResult& other) {
        if (this != &other) {
            delete[] path;
            pathSize = other.pathSize;
            totalCost = other.totalCost;
            totalTimeMinutes = other.totalTimeMinutes;
            found = other.found;
            if (other.path && pathSize > 0) { path = new int[pathSize]; for (int i = 0; i < pathSize; i++) path[i] = other.path[i]; }
            else path = nullptr;
        }
        return *this;
    }
};

struct PQNode {
    int portIndex = 0, cost = 0, heuristic = 0;
    PQNode() = default;
    PQNode(int idx, int c, int h = 0) : portIndex(idx), cost(c), heuristic(h) {}
    bool operator>(const PQNode& other) const { return (cost + heuristic) > (other.cost + other.heuristic); }
    bool operator<(const PQNode& other) const { return (cost + heuristic) < (other.cost + other.heuristic); }
};

struct JourneyLeg {
    int portIndex = -1;
    string company;
    long long departureTime = 0, arrivalTime = 0;
    JourneyLeg* next = nullptr;
};

class MultiLegJourney {
public:
    JourneyLeg* head = nullptr;
    JourneyLeg* tail = nullptr;
    int legCount = 0;

    void addLeg(int portIdx, const string& comp = "") {
        JourneyLeg* newLeg = new JourneyLeg();
        newLeg->portIndex = portIdx;
        newLeg->company = comp;
        if (!head) head = tail = newLeg;
        else { tail->next = newLeg; tail = newLeg; }
        legCount++;
    }

    void removeLeg(int portIdx) {
        JourneyLeg* prev = nullptr;
        JourneyLeg* curr = head;
        while (curr && curr->portIndex != portIdx) { prev = curr; curr = curr->next; }
        if (!curr) return;
        if (!prev) head = curr->next;
        else prev->next = curr->next;
        if (curr == tail) tail = prev;
        delete curr;
        legCount--;
    }

    void clear() { while (head) { JourneyLeg* tmp = head; head = head->next; delete tmp; } tail = nullptr; legCount = 0; }
    ~MultiLegJourney() { clear(); }
};
