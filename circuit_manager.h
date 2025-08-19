#ifndef CIRCUIT_MANAGER_H
#define CIRCUIT_MANAGER_H

#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include "circuit_component.h"

// در circuit_manager.h (قبل از تعریف کلاس)

struct Component {
    std::string type;
    std::string name;
    double value;
    std::string node1;
    std::string node2;
};

struct Connection {
    std::string from;
    std::string to;
};

class CircuitManager {
public:
    friend bool solveDC(CircuitManager&, std::unordered_map<std::string,int>&, std::vector<double>&, std::unordered_map<std::string,double>&);
    friend bool solveTransient(CircuitManager&, double, int, const std::vector<std::pair<char,std::string>>&);
    friend void processAnalysisCommand(CircuitManager&, const std::string&);

    std::vector<std::unique_ptr<CircuitComponent>> components;
    std::unordered_map<std::string, CircuitComponent*> componentMap;
    std::unordered_map<std::string, std::vector<std::string>> nodeToComponents;
    std::unordered_set<std::string> grounds;

    static void toLower(std::string& str);
    void mapComponentToNodes(const std::string& compName, const std::string& n1, const std::string& n2);
    void unmapComponentFromNodes(const std::string& compName, const std::string& n1, const std::string& n2);

public:
    bool componentExists(const std::string& name) const;
    bool hasComponent(const std::string& name) const;
    bool hasNode(const std::string& nodeName) const;
    void addComponent(std::unique_ptr<CircuitComponent> comp);
    void addGround(const std::string& nodeName);
    void deleteComponent(const std::string& name);
    void deleteGround(const std::string& nodeName);
    bool renameNode(const std::string& oldName, const std::string& newName);
    void listNodes() const;
    void listComponents(const std::string& typeFilter = "") const;
    void printAll() const;
    void clearAll();
    std::string getNetlist() const;
    static bool isValidNodeName(const std::string& node);
    bool saveToFile(const std::string& filename);
    void loadFromFile(const std::string& filename);
    void loadFromNetlist(const string& netlist);
    vector<Component> getComponents() const;
    vector<Connection> getConnections() const;
private:
    static double parseValue(const std::string& valueStr);
};

#endif // CIRCUIT_MANAGER_H