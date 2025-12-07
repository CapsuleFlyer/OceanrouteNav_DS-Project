#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>
#include "helper.hpp"
#include "graph.hpp"
#define TOTALNUMBEROFPORTS 40
#define MAX_COST 999999999
using namespace std;

int main()
{
    oceanRouteGraph obj;
    obj.createGraphFromFile("./Routes.txt", "./PortCharges.txt");
    obj.printGraphAfterCreation();

    cout << "\nLaunching interactive graph visualization..." << endl;
    obj.visualizeGraph(1800, 980);

    return 0;
}