
//
//
// Created by Ava-PC on 8/14/2025.
//
#include "circuit_component.h"
#include <iostream>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <vector>
#include <iomanip>
#include <fstream>
using namespace std;

CircuitComponent::CircuitComponent(ComponentType t, const string& n, const string& n1, const string& n2, double v)
    : type(t), name(n), node1(n1), node2(n2), value(v), offset(0), amplitude(0), frequency(0) {}

void CircuitComponent::printInfo() const {
    cout << name << "  " << node1 << "  " << node2 << "  ";
    switch (type) {
        case ComponentType::RESISTOR:
            cout << scientific << setprecision(2) << value << " ohm";
            break;
        case ComponentType::CAPACITOR:
            cout << scientific << setprecision(2) << value << " F";
            break;
        case ComponentType::INDUCTOR:
            cout << scientific << setprecision(2) << value << " H";
            break;
        case ComponentType::DIODE:
            cout << "DIODE(Vf=" << value << "V)";
            break;
        case ComponentType::VOLTAGE_SOURCE:
            cout << fixed << setprecision(2) << value << " V";
            break;
        case ComponentType::CURRENT_SOURCE:
            cout << fixed << setprecision(2) << value << " A";
            break;
        case ComponentType::SINUSOIDAL_SOURCE:
            cout << "SIN(" << offset << " " << amplitude << " " << frequency << ")";
            break;
        case ComponentType::PULSE_VOLTAGE_SOURCE:
            cout << "PULSE(" << value << " " << offset << " " << amplitude << " " << frequency << ")";
            break;
        case ComponentType::VCVS:
            cout << "VCVS(gain=" << value << ")";
            break;
        case ComponentType::VCCS:
            cout << "VCCS(transconductance=" << value << " S)";
            break;
        case ComponentType::CCVS:
            cout << "CCVS(gain=" << value << " Ohm)";
            break;
        case ComponentType::CCCS:
            cout << "CCCS(gain=" << value << ")";
            break;
    }
    cout << defaultfloat << endl;
}

// ================= پیاده‌سازی کلاس Resistor =================
Resistor::Resistor(const string& n, const string& n1, const string& n2, double r)
    : CircuitComponent(ComponentType::RESISTOR, n, n1, n2, r) {}

// ================= پیاده‌سازی کلاس IdealDiode =================
IdealDiode::IdealDiode(const string& n, const string& n1, const string& n2, double vf)
    : CircuitComponent(ComponentType::DIODE, n, n1, n2, vf) {}

// ================= پیاده‌سازی کلاس Capacitor =================
Capacitor::Capacitor(const string& n, const string& n1, const string& n2, double c)
    : CircuitComponent(ComponentType::CAPACITOR, n, n1, n2, c) {}

// ================= پیاده‌سازی کلاس Inductor =================
Inductor::Inductor(const string& n, const string& n1, const string& n2, double l)
    : CircuitComponent(ComponentType::INDUCTOR, n, n1, n2, l) {}

// ================= پیاده‌سازی کلاس VoltageSource =================
VoltageSource::VoltageSource(const string& n, const string& n1, const string& n2, double v)
    : CircuitComponent(ComponentType::VOLTAGE_SOURCE, n, n1, n2, v) {}

// ================= پیاده‌سازی کلاس CurrentSource =================
CurrentSource::CurrentSource(const string& n, const string& n1, const string& n2, double i)
    : CircuitComponent(ComponentType::CURRENT_SOURCE, n, n1, n2, i) {}

void CurrentSource::printInfo() const {
    cout << name << "  " << node1 << "  " << node2 << "  "
         << fixed << setprecision(2) << value << " A" << defaultfloat << endl;
}

// ================= پیاده‌سازی کلاس SinusoidalSource =================
SinusoidalSource::SinusoidalSource(const string& n, const string& n1, const string& n2,
                 double off, double amp, double freq)
    : CircuitComponent(ComponentType::SINUSOIDAL_SOURCE, n, n1, n2, 0) {
    offset = off;
    amplitude = amp;
    frequency = freq;
}

// ================= پیاده‌سازی کلاس PulseVoltageSource =================
PulseVoltageSource::PulseVoltageSource(const string& n, const string& n1, const string& n2,
                     double v1, double v2, double td, double tr, double tf, double pw, double per)
    : CircuitComponent(ComponentType::PULSE_VOLTAGE_SOURCE, n, n1, n2, 0),
      v1(v1), v2(v2), td(td), tr(tr), tf(tf), pw(pw), per(per) {}

void PulseVoltageSource::printInfo() const {
    cout << name << "  " << node1 << "  " << node2 << "  PULSE("
         << v1 << " " << v2 << " " << td << " " << tr << " " << tf << " " << pw << " " << per << ")" << endl;
}

// ================= پیاده‌سازی کلاس VCVS =================
VCVS::VCVS(const string& n, const string& n1, const string& n2,
         const string& ctrl1, const string& ctrl2, double gain)
    : CircuitComponent(ComponentType::VCVS, n, n1, n2, gain),
      ctrlNode1(ctrl1), ctrlNode2(ctrl2) {}

void VCVS::printInfo() const {
    cout << name << "  " << node1 << "  " << node2 << "  "
         << ctrlNode1 << " " << ctrlNode2 << " " << value << " V"<< endl;
}

// ================= پیاده‌سازی کلاس VCCS =================
VCCS::VCCS(const string& n, const string& n1, const string& n2,
         const string& ctrl1, const string& ctrl2, double transconductance)
    : CircuitComponent(ComponentType::VCCS, n, n1, n2, transconductance),
      ctrlNode1(ctrl1), ctrlNode2(ctrl2) {}

void VCCS::printInfo() const {
    cout << name << "  " << node1 << "  " << node2 << "  "
         << ctrlNode1 << " " << ctrlNode2 << " " << value << " V" << endl;
}

// ================= پیاده‌سازی کلاس CCVS =================
CCVS::CCVS(const string& n, const string& n1, const string& n2,
         const string& vname, double gain)
    : CircuitComponent(ComponentType::CCVS, n, n1, n2, gain),
      controlSourceName(vname) {}

void CCVS::printInfo() const {
    cout << name << "  " << node1 << "  " << node2 << "  "
         << controlSourceName << " " << value << " V" << endl;
}

// ================= پیاده‌سازی کلاس CCCS =================
CCCS::CCCS(const string& n, const string& n1, const string& n2,
         const string& vname, double gain)
    : CircuitComponent(ComponentType::CCCS, n, n1, n2, gain),
      controlSourceName(vname) {}

void CCCS::printInfo() const {
    cout << name << "  " << node1 << "  " << node2 << "  "
         << controlSourceName << " " << value << " V" << endl;
}


#include <sstream>
#include <regex>

SubcircuitHC::SubcircuitHC(const std::string& name,
                           const std::string& node1,
                           const std::string& node2,
                           const std::string& path)
    : CircuitComponent(ComponentType::SUBCIRCUIT, name, node1, node2, 0.0),
      filepath(path) {
    loadInternal();
}

void SubcircuitHC::loadInternal() {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open subcircuit file " << filepath << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '*') continue;
        std::istringstream iss(line);
        std::string type, name, n1, n2, valStr;
        iss >> type >> name >> n1 >> n2;

        double val = 0.0;
        if (!(iss >> valStr)) {
            val = 0.0;
        } else {
            try {
                val = std::stod(valStr);
            } catch (...) {
                val = 0.0;
            }
        }


        if (type == "R") internalComponents.push_back(std::make_unique<Resistor>(name, n1, n2, val));
        else if (type == "C") internalComponents.push_back(std::make_unique<Capacitor>(name, n1, n2, val));
        else if (type == "L") internalComponents.push_back(std::make_unique<Inductor>(name, n1, n2, val));
        else if (type == "V") internalComponents.push_back(std::make_unique<VoltageSource>(name, n1, n2, val));
        else if (type == "I") internalComponents.push_back(std::make_unique<CurrentSource>(name, n1, n2, val));
        else if (type == "D") internalComponents.push_back(std::make_unique<IdealDiode>(name, n1, n2, val));
        // می‌تونی انواع دیگر رو هم اضافه کنی
    }

    std::cout << "Loaded subcircuit from " << filepath << " with "
              << internalComponents.size() << " components." << std::endl;
}

void SubcircuitHC::printInfo() const {
    std::cout << "Subcircuit: " << name << " (" << filepath << ") with "
              << internalComponents.size() << " internal components." << std::endl;
}

