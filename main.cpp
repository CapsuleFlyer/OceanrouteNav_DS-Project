#include "graph.hpp"
#include <iostream>

using namespace std;

int main()
{
    oceanRouteGraph graphObj;
    graphObj.createGraphFromFile("./Routes.txt", "./PortCharges.txt");
    graphObj.printGraphAfterCreation();

    cout << "\nLaunching interactive graph visualization..." << endl;
    graphObj.visualizeGraph(1800, 980);

    return 0;
}
