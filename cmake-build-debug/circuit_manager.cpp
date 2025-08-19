//
// Created by Haghjoo on 8/18/2025.
//
#include "circuit_manager.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <regex>
#include "command_process.h"
#include "circuit_component.h"
void CircuitManager::toLower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
}

void CircuitManager::mapComponentToNodes(const std::string& compName, const std::string& n1, const std::string& n2) {
    nodeToComponents[n1].push_back(compName);
    nodeToComponents[n2].push_back(compName);
}

void CircuitManager::unmapComponentFromNodes(const std::string& compName, const std::string& n1, const std::string& n2) {
    auto removeFromNode = [&](const std::string& node) {
        auto& vec = nodeToComponents[node];
        vec.erase(std::remove(vec.begin(), vec.end(), compName), vec.end());
        if (vec.empty() && grounds.find(node) == grounds.end()) {
            nodeToComponents.erase(node);
        }
    };
    removeFromNode(n1);
    removeFromNode(n2);
}

bool CircuitManager::componentExists(const std::string& name) const {
    return componentMap.find(name) != componentMap.end();
}

bool CircuitManager::hasComponent(const std::string& name) const {
    return componentMap.find(name) != componentMap.end();
}

bool CircuitManager::hasNode(const std::string& nodeName) const {
    return nodeToComponents.find(nodeName) != nodeToComponents.end();
}

void CircuitManager::addComponent(std::unique_ptr<CircuitComponent> comp) {
    const std::string& name = comp->name;
    const std::string& n1 = comp->node1;
    const std::string& n2 = comp->node2;
    ComponentType type = comp->type;
    double val = comp->value;

    if (componentExists(name)) {
        std::string typeStr;
        switch (type) {
            case ComponentType::RESISTOR: typeStr = "Resistor"; break;
            case ComponentType::DIODE: typeStr = "Diode"; break;
            case ComponentType::CAPACITOR: typeStr = "Capacitor"; break;
            case ComponentType::INDUCTOR: typeStr = "Inductor"; break;
            case ComponentType::VOLTAGE_SOURCE: typeStr = "Voltage Source"; break;
            case ComponentType::CURRENT_SOURCE: typeStr = "Current Source"; break;
            case ComponentType::SINUSOIDAL_SOURCE: typeStr = "Sinusoidal Source"; break;
            case ComponentType::PULSE_VOLTAGE_SOURCE: typeStr = "Pulse Voltage Source"; break;
            case ComponentType::VCVS: typeStr = "VCVS"; break;
            case ComponentType::VCCS: typeStr = "VCCS"; break;
            case ComponentType::CCVS: typeStr = "CCVS"; break;
            case ComponentType::CCCS: typeStr = "CCCS"; break;
        }
        throw std::runtime_error("Error: " + typeStr + " " + name + " already exists in the circuit");
    }

    if (!isValidNodeName(n1) || !isValidNodeName(n2)) {
        throw std::runtime_error("Error: Invalid node name");
    }

    if ((type == ComponentType::RESISTOR ||
         type == ComponentType::CAPACITOR ||
         type == ComponentType::INDUCTOR ||
         type == ComponentType::VOLTAGE_SOURCE ||
         type == ComponentType::CURRENT_SOURCE) && val <= 0) {
        throw std::invalid_argument("Error: Value cannot be zero or negative");
    }

    CircuitComponent* ptr = comp.get();
    components.push_back(std::move(comp));
    componentMap[name] = ptr;

    mapComponentToNodes(name, n1, n2);

    std::cout << "Added: ";
    ptr->printInfo();
}

void CircuitManager::addGround(const std::string& nodeName) {
    if (!isValidNodeName(nodeName)) {
        throw std::runtime_error("Error: Invalid node name");
    }
    if (grounds.find(nodeName) != grounds.end()) {
        throw std::runtime_error("Error: Ground node " + nodeName + " already exists");
    }
    grounds.insert(nodeName);
    if (nodeToComponents.find(nodeName) == nodeToComponents.end()) {
        nodeToComponents[nodeName] = {};
    }
    std::cout << "Added Ground: " << nodeName << std::endl;
}

void CircuitManager::deleteComponent(const std::string& name) {
    auto it = componentMap.find(name);
    if (it == componentMap.end()) {
        std::string typeStr;
        if (!name.empty()) {
            switch (toupper(name[0])) {
                case 'R': typeStr = "resistor"; break;
                case 'D': typeStr = "diode"; break;
                case 'C': typeStr = "capacitor"; break;
                case 'L': typeStr = "inductor"; break;
                case 'V': typeStr = "voltage source"; break;
                case 'I': typeStr = "current source"; break;
                case 'E': typeStr = "VCVS"; break;
                case 'G': typeStr = "VCCS"; break;
                case 'H': typeStr = "CCVS"; break;
                case 'F': typeStr = "CCCS"; break;
            }
        }
        throw std::runtime_error("Error: Cannot delete " + typeStr + "; component " + name + " not found");
    }

    CircuitComponent* compPtr = it->second;
    std::string n1 = compPtr->node1, n2 = compPtr->node2;

    componentMap.erase(it);

    components.erase(
        std::remove_if(components.begin(), components.end(),
                      [&](const std::unique_ptr<CircuitComponent>& c) {
                          return c->name == name;
                      }),
        components.end()
    );

    unmapComponentFromNodes(name, n1, n2);

    std::cout << "Deleted: " << name << std::endl;
}

void CircuitManager::deleteGround(const std::string& nodeName) {
    if (grounds.find(nodeName) == grounds.end()) {
        throw std::runtime_error("Error: Ground node " + nodeName + " not found");
    }
    grounds.erase(nodeName);

    if (nodeToComponents[nodeName].empty()) {
        nodeToComponents.erase(nodeName);
    }
    std::cout << "Deleted Ground: " << nodeName << std::endl;
}

bool CircuitManager::renameNode(const std::string& oldName, const std::string& newName) {
    if (nodeToComponents.find(oldName) == nodeToComponents.end()) {
        std::cout << "ERROR: Node " << oldName << " does not exist in the circuit" << std::endl;
        return false;
    }
    if (nodeToComponents.find(newName) != nodeToComponents.end()) {
        std::cout << "ERROR: Node name " << newName << " already exists" << std::endl;
        return false;
    }
    if (newName.empty() || !std::all_of(newName.begin(), newName.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    })) {
        std::cout << "ERROR: Invalid node name format" << std::endl;
        return false;
    }

    for (auto& compPtr : components) {
        if (compPtr->node1 == oldName) compPtr->node1 = newName;
        if (compPtr->node2 == oldName) compPtr->node2 = newName;

        if (compPtr->type == ComponentType::VCVS) {
            VCVS* vcvs = static_cast<VCVS*>(compPtr.get());
            if (vcvs->ctrlNode1 == oldName) vcvs->ctrlNode1 = newName;
            if (vcvs->ctrlNode2 == oldName) vcvs->ctrlNode2 = newName;
        }
        else if (compPtr->type == ComponentType::VCCS) {
            VCCS* vccs = static_cast<VCCS*>(compPtr.get());
            if (vccs->ctrlNode1 == oldName) vccs->ctrlNode1 = newName;
            if (vccs->ctrlNode2 == oldName) vccs->ctrlNode2 = newName;
        }
    }

    nodeToComponents[newName] = std::move(nodeToComponents[oldName]);
    nodeToComponents.erase(oldName);

    if (grounds.find(oldName) != grounds.end()) {
        grounds.erase(oldName);
        grounds.insert(newName);
    }

    std::cout << "SUCCESS: Node renamed from " << oldName << " to " << newName << std::endl;
    return true;
}

void CircuitManager::listNodes() const {
    if (nodeToComponents.empty()) {
        std::cout << "No nodes in the circuit." << std::endl;
        return;
    }
    std::cout << "Available nodes:" << std::endl;
    bool first = true;
    for (const auto& pair : nodeToComponents) {
        if (!first) std::cout << ", ";
        std::cout << pair.first;
        first = false;
    }
    std::cout << std::endl;
}

void CircuitManager::listComponents(const std::string& typeFilter) const {
    std::string filter = typeFilter;
    toLower(filter);

    std::unordered_map<std::string, ComponentType> filterMap = {
        {"r", ComponentType::RESISTOR},
        {"d", ComponentType::DIODE},
        {"c", ComponentType::CAPACITOR},
        {"l", ComponentType::INDUCTOR},
        {"v", ComponentType::VOLTAGE_SOURCE},
        {"i", ComponentType::CURRENT_SOURCE},
        {"s", ComponentType::SINUSOIDAL_SOURCE},
        {"p", ComponentType::PULSE_VOLTAGE_SOURCE},
        {"e", ComponentType::VCVS},
        {"g", ComponentType::VCCS},
        {"h", ComponentType::CCVS},
        {"f", ComponentType::CCCS}
    };

    ComponentType targetType;
    bool hasFilter = false;

    if (!filter.empty()) {
        auto it = filterMap.find(filter);
        if (it != filterMap.end()) {
            targetType = it->second;
            hasFilter = true;
        } else {
            std::cout << "Error: Invalid component type filter. Valid types: r, d, c, l, v, i, s, p, e, g, h, f" << std::endl;
            return;
        }
    }

    bool anyPrinted = false;
    for (const auto& compPtr : components) {
        std::string typeStr;

        switch (compPtr->type) {
            case ComponentType::RESISTOR:         typeStr = "resistor"; break;
            case ComponentType::DIODE:            typeStr = "diode"; break;
            case ComponentType::CAPACITOR:        typeStr = "capacitor"; break;
            case ComponentType::INDUCTOR:         typeStr = "inductor"; break;
            case ComponentType::VOLTAGE_SOURCE:   typeStr = "voltagesource"; break;
            case ComponentType::CURRENT_SOURCE:   typeStr = "currentsource"; break;
            case ComponentType::SINUSOIDAL_SOURCE:typeStr = "sinusoidalsource"; break;
        }

        toLower(typeStr);

        if (filter.empty() || filter == typeStr || !hasFilter || compPtr->type == targetType) {
            compPtr->printInfo();
            anyPrinted = true;
        }
    }

    if (!anyPrinted) {
        if (hasFilter) {
            std::cout << "No components of type '" << typeFilter << "' found." << std::endl;
        } else {
            std::cout << "No components in the circuit." << std::endl;
        }
    }
}

void CircuitManager::printAll() const {
    bool hasAnything = false;
    if (!components.empty()) {
        std::cout << "Circuit components:" << std::endl;
        for (const auto& compPtr : components) {
            compPtr->printInfo();
        }
        hasAnything = true;
    }
    if (!grounds.empty()) {
        std::cout << "Ground nodes:" << std::endl;
        for (const auto& g : grounds) {
            std::cout << "GND " << g << std::endl;
        }
        hasAnything = true;
    }
    if (!hasAnything) {
        std::cout << "No components or ground nodes in the circuit." << std::endl;
    }
}

void CircuitManager::clearAll() {
    components.clear();
    componentMap.clear();
    nodeToComponents.clear();
    grounds.clear();
    std::cout << "All components and ground nodes cleared." << std::endl;
}

std::string CircuitManager::getNetlist() const {
    std::ostringstream oss;
    for (const auto& gnd : grounds) {
        oss << "GND GND_" << gnd << " " << gnd << std::endl;
    }
    for (const auto& compPtr : components) {
        switch (compPtr->type) {
            case ComponentType::RESISTOR:
                oss << "R " << compPtr->name << " " << compPtr->node1 << " " << compPtr->node2 << " " << compPtr->value << std::endl;
                break;
            case ComponentType::CAPACITOR:
                oss << "C " << compPtr->name << " " << compPtr->node1 << " " << compPtr->node2 << " " << compPtr->value << std::endl;
                break;
            case ComponentType::INDUCTOR:
                oss << "L " << compPtr->name << " " << compPtr->node1 << " " << compPtr->node2 << " " << compPtr->value << std::endl;
                break;
            case ComponentType::DIODE:
                oss << "D " << compPtr->name << " " << compPtr->node1 << " " << compPtr->node2 << " " << compPtr->value << std::endl;
                break;
            case ComponentType::VOLTAGE_SOURCE:
                oss << "V " << compPtr->name << " " << compPtr->node1 << " " << compPtr->node2 << " " << compPtr->value << std::endl;
                break;
            case ComponentType::CURRENT_SOURCE:
                oss << "I " << compPtr->name << " " << compPtr->node1 << " " << compPtr->node2 << " " << compPtr->value << std::endl;
                break;
            case ComponentType::SINUSOIDAL_SOURCE:
                oss << "V " << compPtr->name << " " << compPtr->node1 << " " << compPtr->node2 << " SIN("
                    << compPtr->offset << " " << compPtr->amplitude << " " << compPtr->frequency << ")" << std::endl;
                break;
            case ComponentType::PULSE_VOLTAGE_SOURCE: {
                PulseVoltageSource* pulse = static_cast<PulseVoltageSource*>(compPtr.get());
                oss << "V " << pulse->name << " " << pulse->node1 << " " << pulse->node2 << " PULSE("
                    << pulse->v1 << " " << pulse->v2 << " " << pulse->td << " " << pulse->tr << " "
                    << pulse->tf << " " << pulse->pw << " " << pulse->per << ")" << std::endl;
                break;
            }
            case ComponentType::VCVS: {
                VCVS* vcvs = static_cast<VCVS*>(compPtr.get());
                oss << "E " << vcvs->name << " " << vcvs->node1 << " " << vcvs->node2 << " "
                    << vcvs->ctrlNode1 << " " << vcvs->ctrlNode2 << " " << vcvs->value << std::endl;
                break;
            }
            case ComponentType::VCCS: {
                VCCS* vccs = static_cast<VCCS*>(compPtr.get());
                oss << "G " << vccs->name << " " << vccs->node1 << " " << vccs->node2 << " "
                    << vccs->ctrlNode1 << " " << vccs->ctrlNode2 << " " << vccs->value << std::endl;
                break;
            }
            case ComponentType::CCVS: {
                CCVS* ccvs = static_cast<CCVS*>(compPtr.get());
                oss << "H " << ccvs->name << " " << ccvs->node1 << " " << ccvs->node2 << " "
                    << ccvs->controlSourceName << " " << ccvs->value << std::endl;
                break;
            }
            case ComponentType::CCCS: {
                CCCS* cccs = static_cast<CCCS*>(compPtr.get());
                oss << "F " << cccs->name << " " << cccs->node1 << " " << cccs->node2 << " "
                    << cccs->controlSourceName << " " << cccs->value << std::endl;
                break;
            }
            case ComponentType::SUBCIRCUIT: {
                auto* sub = dynamic_cast<SubcircuitHC*>(compPtr.get());
                if (sub) {
                    oss << "X " << sub->name << " " << sub->node1 << " " << sub->node2
                        << " " << sub->filepath << std::endl;
                }
                break;
            }



        }
    }
    return oss.str();
}

bool CircuitManager::isValidNodeName(const std::string& node) {
    static const std::regex pattern(R"(^[A-Za-z0-9_]+$)");
    return std::regex_match(node, pattern);
}

bool CircuitManager::saveToFile(const string& filename) {
    ofstream outFile(filename);

    if (!outFile.is_open()) {
        cerr << "Error: Could not open file for writing: " << filename << endl;
        return false;
    }

    string netlist = getNetlist(); // باید مدار را به فرمت Netlist تبدیل کند
    if (netlist.empty()) {
        cerr << "Error: Netlist is empty!" << endl;
        return false;
    }

    outFile << netlist;
    outFile.close();
    return true;
}

void CircuitManager::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Error: Could not open file " + filename);
    }

    clearAll();

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '*') {
            continue;
        }

        std::istringstream iss(line);
        std::string type, name, node1, node2, valueStr;
        iss >> type >> name >> node1 >> node2;

        if (type == "V") {
            iss >> valueStr;
            double value = parseValue(valueStr);
            addComponent(std::make_unique<VoltageSource>(name, node1, node2, value));
        }
        else if (type == "R") {
            iss >> valueStr;
            double value = parseValue(valueStr);
            addComponent(std::make_unique<Resistor>(name, node1, node2, value));
        }
        else if (type == "C") {
            iss >> valueStr;
            double value = parseValue(valueStr);
            addComponent(std::make_unique<Capacitor>(name, node1, node2, value));
        }
        else if (type == "L") {
            iss >> valueStr;
            double value = parseValue(valueStr);
            addComponent(std::make_unique<Inductor>(name, node1, node2, value));
        }
        else if (type == "D") {
            double vf = 0.7;
            if (iss >> valueStr) vf = parseValue(valueStr);
            addComponent(std::make_unique<IdealDiode>(name, node1, node2, vf));
        }
        else if (type == "GND") {
            addGround(node1);
        }
    }

    std::cout << "Circuit loaded from " << filename << std::endl;
}
void CircuitManager::loadFromNetlist(const string& netlist) {
    istringstream iss(netlist);
    string line;
    while (getline(iss, line)) {
        if (!line.empty()) {
            processCommand(*this, line);
        }
    }
}
// در circuit_manager.cpp (قبل از آخرین } )

vector<Component> CircuitManager::getComponents() const {
    vector<Component> compList;

    for (const auto& compPtr : components) {
        Component c;
        c.type = [&]() {
            switch(compPtr->type) {
                case ComponentType::RESISTOR: return "RESISTOR";
                case ComponentType::CAPACITOR: return "CAPACITOR";
                case ComponentType::INDUCTOR: return "INDUCTOR";
                case ComponentType::DIODE: return "DIODE";
                case ComponentType::VOLTAGE_SOURCE: return "VOLTAGE_SOURCE";
                case ComponentType::CURRENT_SOURCE: return "CURRENT_SOURCE";
                case ComponentType::SINUSOIDAL_SOURCE: return "SINUSOIDAL_SOURCE";
                case ComponentType::PULSE_VOLTAGE_SOURCE: return "PULSE_VOLTAGE_SOURCE";
                case ComponentType::VCVS: return "VCVS";
                case ComponentType::VCCS: return "VCCS";
                case ComponentType::CCVS: return "CCVS";
                case ComponentType::CCCS: return "CCCS";
                case ComponentType::SUBCIRCUIT: return "SUBCIRCUIT";
                default: return "UNKNOWN";
            }
        }();

        c.name = compPtr->name;
        c.value = compPtr->value;
        c.node1 = compPtr->node1;
        c.node2 = compPtr->node2;

        compList.push_back(c);
    }

    return compList;
}

vector<Connection> CircuitManager::getConnections() const {
    vector<Connection> connList;

    // اتصالات از طریق نودهای مشترک
    for (const auto& nodePair : nodeToComponents) {
        const string& node = nodePair.first;
        const vector<string>& comps = nodePair.second;

        if (comps.size() >= 2) {
            // ایجاد اتصالات بین تمام ترکیب‌های ممکن
            for (size_t i = 0; i < comps.size(); i++) {
                for (size_t j = i+1; j < comps.size(); j++) {
                    Connection conn;
                    conn.from = comps[i];
                    conn.to = comps[j];
                    connList.push_back(conn);
                }
            }
        }
    }

    return connList;
}

double CircuitManager::parseValue(const std::string& valueStr) {
    try {
        return std::stod(valueStr);
    } catch (const std::exception& e) {
        throw std::runtime_error("Error: Invalid value format - " + valueStr);
    }
}