//
// Created by Ava-PC on 8/14/2025.
//

#ifndef CIRCUIT_COMPONENT_H
#define CIRCUIT_COMPONENT_H

#include <iostream>
#include <string>
#include <iomanip>
#include <vector>
#include "command_process.h"

using namespace std;

// انواع المان‌های مدار
enum class ComponentType {
    RESISTOR,
    DIODE,
    CAPACITOR,
    INDUCTOR,
    VOLTAGE_SOURCE,
    CURRENT_SOURCE,
    SINUSOIDAL_SOURCE,
    PULSE_VOLTAGE_SOURCE,
    VCVS,
    VCCS,
    CCVS,
    CCCS,
    SUBCIRCUIT

};

// کلاس پایه برای تمام المان‌های مدار
class CircuitComponent {
public:
    ComponentType type;
    string name;
    string node1, node2;
    double value;
    double offset;
    double amplitude;
    double frequency;

    CircuitComponent(ComponentType t, const string& n, const string& n1, const string& n2, double v);
    virtual void printInfo() const;
    virtual ~CircuitComponent() = default;
};

// کلاس مقاومت
class Resistor : public CircuitComponent {
public:
    Resistor(const string& n, const string& n1, const string& n2, double r);
};

// کلاس دیود
class IdealDiode : public CircuitComponent {
public:
    IdealDiode(const string& n, const string& n1, const string& n2, double vf = 0.7);
};

// کلاس خازن
class Capacitor : public CircuitComponent {
public:
    Capacitor(const string& n, const string& n1, const string& n2, double c);
};

// کلاس سلف
class Inductor : public CircuitComponent {
public:
    Inductor(const string& n, const string& n1, const string& n2, double l);
};

// کلاس منبع ولتاژ
class VoltageSource : public CircuitComponent {
public:
    VoltageSource(const string& n, const string& n1, const string& n2, double v);
};

// کلاس منبع جریان
class CurrentSource : public CircuitComponent {
public:
    CurrentSource(const string& n, const string& n1, const string& n2, double i);
    void printInfo() const override;
};

// کلاس منبع سینوسی
class SinusoidalSource : public CircuitComponent {
public:
    SinusoidalSource(const string& n, const string& n1, const string& n2,
                     double off, double amp, double freq);
};

// کلاس منبع پالسی
class PulseVoltageSource : public CircuitComponent {
public:
    double v1, v2, td, tr, tf, pw, per;
    PulseVoltageSource(const string& n, const string& n1, const string& n2,
                     double v1, double v2, double td, double tr, double tf, double pw, double per);
    void printInfo() const override;
};

// کلاس منبع ولتاژ کنترل‌شده با ولتاژ (VCVS)
class VCVS : public CircuitComponent {
public:
    string ctrlNode1, ctrlNode2;
    VCVS(const string& n, const string& n1, const string& n2,
         const string& ctrl1, const string& ctrl2, double gain);
    void printInfo() const override;
};

// کلاس منبع جریان کنترل‌شده با ولتاژ (VCCS)
class VCCS : public CircuitComponent {
public:
    string ctrlNode1, ctrlNode2;
    VCCS(const string& n, const string& n1, const string& n2,
         const string& ctrl1, const string& ctrl2, double transconductance);
    void printInfo() const override;
};

// کلاس منبع ولتاژ کنترل‌شده با جریان (CCVS)
class CCVS : public CircuitComponent {
public:
    string controlSourceName;
    CCVS(const string& n, const string& n1, const string& n2,
         const string& vname, double gain);
    void printInfo() const override;
};

// کلاس منبع جریان کنترل‌شده با جریان (CCCS)
class CCCS : public CircuitComponent {
public:
    string controlSourceName;
    CCCS(const string& n, const string& n1, const string& n2,
         const string& vname, double gain);
    void printInfo() const override;
};

class SubcircuitHC : public CircuitComponent {
public:
    std::string filepath;
    std::vector<std::unique_ptr<CircuitComponent>> internalComponents;
    SubcircuitHC(const std::string& name,
                 const std::string& node1,
                 const std::string& node2,
                 const std::string& path);

    void loadInternal();
    void printInfo() const override;
};



#endif