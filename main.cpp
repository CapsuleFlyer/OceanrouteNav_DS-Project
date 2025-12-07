#include "graph.hpp"
#include <iostream>

using namespace std;

int main()
{
    oceanRouteGraph graphObj;
    graphObj.createGraphFromFile("./Routes.txt", "./PortCharges.txt");
    graphObj.printGraphAfterCreation();

    cout << "\nLaunching interactive graph visualization..." << endl;

    try {
        graphObj.visualizeGraph(1800, 980);
    }
    catch (const exception& e) {
        cerr << "Program crashed: " << e.what() << endl;
        system("pause");  // Windows – keeps console open
        return 1;
    }
    catch (...) {
        cerr << "Unknown error – program terminated safely." << endl;
        system("pause");
        return 1;
    }

    return 0;
}