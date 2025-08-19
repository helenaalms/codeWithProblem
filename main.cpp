
#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <algorithm>
#include <regex>
#include <memory>
#include <cctype>
#include <fstream>
#include <dirent.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL_ttf.h>
#include "circuit_component.h"
#include "circuit_manager.h"

using namespace std;

unordered_map<string, int> elementCounters = {
    {"RESISTOR", 0},
    {"DIODE", 0},
    {"CAPACITOR", 0},
    {"INDUCTOR", 0},
    {"VOLTAGE_SOURCE", 0},
    {"CURRENT_SOURCE", 0},
    {"SINUSOIDAL_SOURCE", 0},
    {"PULSE_VOLTAGE_SOURCE", 0},
    {"VCVS", 0},
    {"VCCS", 0},
    {"CCVS", 0},
    {"CCCS", 0}
};
const int GRID_SIZE = 20; // اندازه هر خانه گرید
SDL_Color GRID_LINE_COLOR = {220, 220, 220, 255}; // رنگ خطوط گرید (کمرنگ)
SDL_Color GRID_POINT_COLOR = {180, 180, 180, 255}; // رنگ نقاط تقاطع (تیره‌تر)
struct Wire {
    pair<int, int> start;
    pair<int, int> end;
    Wire(std::pair<int, int> s, std::pair<int, int> e)
        : start(s), end(e) {}
};
vector<Wire> wires;
// ساختار اصلاح شده CircuitElement
// ساختار اصلاح شده CircuitElement با اضافه شدن rotation
struct CircuitElement {
    string type;
    SDL_Rect rect;
    string label;
    int id;
    string node1;
    string node2;
    double value;
    bool isSelected;
    bool isDragging;
    int rotation;
    // برای منابع سینوسی
    double offset = 0;
    double amplitude = 0;
    double frequency = 0;

    // برای منابع پالسی
    double v1 = 0;
    double v2 = 0;
    double td = 0;
    double tr = 0;
    double tf = 0;
    double pw = 0;
    double per = 0;

    // برای VCVS و VCCS
    string ctrlNode1;
    string ctrlNode2;

    // برای CCVS و CCCS
    string controlSourceName;

    SDL_Color color;
    // Constructor برای مقداردهی اولیه
    CircuitElement() : isSelected(false), isDragging(false), rotation(0) {}
};
bool showPopup = false;
vector<string> popupFields;
vector<string> popupInputs;
int popupCurrentField = 0;
CircuitElement popupElement;
bool showSavePopup = false;
string savePopupInput = "";
bool savePopupConfirmed = false;
bool showNewProjectConfirm = false; // برای تأییدیه ایجاد پروژه جدید

// Helper function to parse value strings with units
double parseValue(const string& valueStr) {
    if (valueStr.empty()) {
        throw invalid_argument("Empty value string");
    }

    double factor = 1.0;
    string s = valueStr;

    // [1] حذف کاراکترهای غیرمجاز
    s.erase(remove_if(s.begin(), s.end(), [](char c) {
        return !(isdigit(c) || c == '.' || c == '-' || c == '+' || tolower(c) == 'e');
    }), s.end());

    // [2] بررسی واحدها
    if (!s.empty() && isalpha(static_cast<unsigned char>(s.back()))) {
        char unit = tolower(s.back());
        s.pop_back();

        switch (unit) {
            case 'm': factor = 1e-3; break;
            case 'u': factor = 1e-6; break;
            case 'n': factor = 1e-9; break;
            case 'k': factor = 1e3; break;
            case 'M': factor = 1e6; break;
            default: throw invalid_argument("Invalid unit");
        }
    }

    try {
        return stod(s) * factor;
    }
    catch (...) {
        throw invalid_argument("Invalid number format");
    }
}
unordered_map<string, vector<string>> elementInputFields = {
    {"RESISTOR", {"Resistance"}},
    {"CAPACITOR", {"Capacitance"}},
    {"INDUCTOR", {"Inductance"}},
    {"DIODE", {}},
    {"VOLTAGE_SOURCE", {"Voltage"}},
    {"CURRENT_SOURCE", {"Current"}},
    {"SINUSOIDAL_SOURCE", {"Offset", "Amplitude", "Frequency"}},
    {"PULSE_VOLTAGE_SOURCE", {"V1", "V2", "TD", "TR", "TF", "PW", "PER"}},
    {"VCVS", {"CtrlNode1", "CtrlNode2", "Gain"}},
    {"VCCS", {"CtrlNode1", "CtrlNode2", "Transconductance"}},
    {"CCVS", {"ControlSource", "Gain"}},
    {"CCCS", {"ControlSource", "Gain"}}
};
void addElementByShortcut(SDL_Keycode key, int mouseX, int mouseY,
                         vector<CircuitElement>& circuitElements,
                         unordered_map<string, int>& elementCounters,
                         int& currentElementId) {
    // Snap به گرید
    int snappedX = ((mouseX + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;
    int snappedY = ((mouseY + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;

    CircuitElement newElement;
    newElement.rect = {
        snappedX - (2 * GRID_SIZE),
        snappedY - (1 * GRID_SIZE),
        4 * GRID_SIZE,
        2 * GRID_SIZE
    };
    newElement.id = ++currentElementId;
    newElement.node1 = "n" + to_string(currentElementId*2);
    newElement.node2 = "n" + to_string(currentElementId*2+1);
    newElement.isSelected = false;
    newElement.isDragging = false;
    newElement.rotation = 0;

    switch(key) {
        case SDLK_r: // مقاومت
            elementCounters["RESISTOR"]++;
            newElement.type = "RESISTOR";
            newElement.label = "R" + to_string(elementCounters["RESISTOR"]);
            newElement.value = 1000; // مقدار پیش‌فرض
            break;

        case SDLK_c: // خازن
            elementCounters["CAPACITOR"]++;
            newElement.type = "CAPACITOR";
            newElement.label = "C" + to_string(elementCounters["CAPACITOR"]);
            newElement.value = 1e-6; // 1μF
            break;

        case SDLK_l: // سلف
            elementCounters["INDUCTOR"]++;
            newElement.type = "INDUCTOR";
            newElement.label = "L" + to_string(elementCounters["INDUCTOR"]);
            newElement.value = 1e-3; // 1mH
            break;

        case SDLK_d: // دیود
            elementCounters["DIODE"]++;
            newElement.type = "DIODE";
            newElement.label = "D" + to_string(elementCounters["DIODE"]);
            newElement.value = 0.7; // ولتاژ آستانه
            break;

        case SDLK_v: // منبع ولتاژ
            elementCounters["VOLTAGE_SOURCE"]++;
            newElement.type = "VOLTAGE_SOURCE";
            newElement.label = "V" + to_string(elementCounters["VOLTAGE_SOURCE"]);
            newElement.value = 5.0; // 5V
            break;

        case SDLK_i: // منبع جریان
            elementCounters["CURRENT_SOURCE"]++;
            newElement.type = "CURRENT_SOURCE";
            newElement.label = "I" + to_string(elementCounters["CURRENT_SOURCE"]);
            newElement.value = 0.1; // 100mA
            break;

        default:
            return;
    }

    // تنظیم popup برای دریافت مقادیر از کاربر
    popupElement = newElement;
    popupFields = elementInputFields[newElement.type];
    popupInputs = vector<string>(popupFields.size(), "");
    popupCurrentField = 0;
    showPopup = true;

    // فعال کردن ورود متن
    if (!SDL_IsTextInputActive()) {
        SDL_StartTextInput();
    }
}
vector<string> getSavedNetlistFiles(const string& folderPath) {
    vector<string> filenames;
    DIR* dir = opendir(folderPath.c_str());
    if (!dir) {
        cerr << "Error: Cannot open directory " << folderPath << endl;
        return filenames;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string name = entry->d_name;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".txt") {
            filenames.push_back(name.substr(0, name.size() - 4)); // حذف پسوند
        }
    }

    closedir(dir);
    return filenames;
}


void drawGrid(SDL_Renderer* renderer) {
    // رسم خطوط افقی و عمودی کمرنگ
    SDL_SetRenderDrawColor(renderer, GRID_LINE_COLOR.r, GRID_LINE_COLOR.g, GRID_LINE_COLOR.b, GRID_LINE_COLOR.a);
    for (int x = 0; x < 1400; x += GRID_SIZE) {
        SDL_RenderDrawLine(renderer, x, 0, x, 700);
    }
    for (int y = 0; y < 700; y += GRID_SIZE) {
        SDL_RenderDrawLine(renderer, 0, y, 1400, y);
    }

    // رسم نقاط تقاطع با رنگ تیره‌تر
    SDL_SetRenderDrawColor(renderer, GRID_POINT_COLOR.r, GRID_POINT_COLOR.g, GRID_POINT_COLOR.b, GRID_POINT_COLOR.a);
    for (int x = 0; x < 1400; x += GRID_SIZE) {
        for (int y = 0; y < 700; y += GRID_SIZE) {
            SDL_RenderDrawPoint(renderer, x, y);
            // برای نقطه بزرگتر می‌توانید از چند پیکسل استفاده کنید
            SDL_RenderDrawPoint(renderer, x+1, y);
            SDL_RenderDrawPoint(renderer, x-1, y);
            SDL_RenderDrawPoint(renderer, x, y+1);
            SDL_RenderDrawPoint(renderer, x, y-1);
        }
    }
}
// تابع برای رسم مستطیل با در نظر گرفتن چرخش
void drawRotatedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int rotation, SDL_Color color) {
    if (rotation == 0) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &rect);
        return;
    }

    // For rotated elements, we'll draw lines manually
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    if (rotation == 1) { // 90 degrees
        for (int y = rect.y; y < rect.y + rect.h; y++) {
            SDL_RenderDrawLine(renderer,
                rect.x + rect.w - (y - rect.y) * rect.w / rect.h,
                y,
                rect.x + (y - rect.y) * rect.w / rect.h,
                y);
        }
    } else if (rotation == 2) { // 180 degrees
        for (int y = rect.y; y < rect.y + rect.h; y++) {
            SDL_RenderDrawLine(renderer,
                rect.x,
                y,
                rect.x + rect.w,
                y);
        }
    } else if (rotation == 3) { // 270 degrees
        for (int y = rect.y; y < rect.y + rect.h; y++) {
            SDL_RenderDrawLine(renderer,
                rect.x + (y - rect.y) * rect.w / rect.h,
                y,
                rect.x + rect.w - (y - rect.y) * rect.w / rect.h,
                y);
        }
    }
}
void drawWireLShape(SDL_Renderer* renderer, pair<int, int> start, pair<int, int> end, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, start.first, start.second, end.first, start.second);
    SDL_RenderDrawLine(renderer, end.first, start.second, end.first, end.second);
}
void drawConnectionPoint(SDL_Renderer* renderer, int x, int y, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dx = -2; dx <= 2; dx++) {
        for (int dy = -2; dy <= 2; dy++) {
            if (dx*dx + dy*dy <= 4) {
                SDL_RenderDrawPoint(renderer, x + dx, y + dy);
            }
        }
    }
}

// Validate node names
bool isValidNodeName(const string& node) {
    regex pattern(R"(^[A-Za-z0-9_]+$)");
    return regex_match(node, pattern);
}
void applyPopupValues(CircuitElement& elem, const vector<string>& inputs) {
    // بررسی اولیه ورودی‌ها
    if (elem.type.empty()) {
        cerr << "Error: Element type is empty!" << endl;
        return;
    }

    if (inputs.empty()) {
        cerr << "Error: No input values provided for " << elem.type << endl;
        return;
    }

    try {
        // پردازش بر اساس نوع المان
        if (elem.type == "RESISTOR" || elem.type == "CAPACITOR" ||
            elem.type == "INDUCTOR" || elem.type == "VOLTAGE_SOURCE" ||
            elem.type == "CURRENT_SOURCE") {

            if (inputs.size() < 1) {
                throw invalid_argument("Insufficient input values");
            }

            double val = parseValue(inputs[0]);
            if (val <= 0) {
                throw invalid_argument("Value must be positive");
            }
            elem.value = val;
        }
        else if (elem.type == "SINUSOIDAL_SOURCE") {
            if (inputs.size() < 3) {
                throw invalid_argument("Need 3 values: offset, amplitude, frequency");
            }

            elem.offset = parseValue(inputs[0]);
            elem.amplitude = parseValue(inputs[1]);
            elem.frequency = parseValue(inputs[2]);

            if (elem.frequency <= 0) {
                throw invalid_argument("Frequency must be positive");
            }
        }
        else if (elem.type == "PULSE_VOLTAGE_SOURCE") {
            if (inputs.size() < 7) {
                throw invalid_argument("Need 7 values: V1, V2, TD, TR, TF, PW, PER");
            }

            elem.v1 = parseValue(inputs[0]);
            elem.v2 = parseValue(inputs[1]);
            elem.td = parseValue(inputs[2]);
            elem.tr = parseValue(inputs[3]);
            elem.tf = parseValue(inputs[4]);
            elem.pw = parseValue(inputs[5]);
            elem.per = parseValue(inputs[6]);

            if (elem.tr <= 0 || elem.tf <= 0 || elem.pw <= 0 || elem.per <= 0) {
                throw invalid_argument("Time parameters must be positive");
            }
        }
        else if (elem.type == "VCVS" || elem.type == "VCCS") {
            if (inputs.size() < 3) {
                throw invalid_argument("Need 3 values: CtrlNode1, CtrlNode2, Gain");
            }

            // کنترل‌نودها به صورت رشته هستند و نیاز به تبدیل ندارند
            elem.ctrlNode1 = inputs[0];
            elem.ctrlNode2 = inputs[1];

            // اعتبارسنجی نام نودها
            if (!isValidNodeName(elem.ctrlNode1) || !isValidNodeName(elem.ctrlNode2)) {
                throw invalid_argument("Invalid node name format");
            }

            elem.value = parseValue(inputs[2]);
        }
        else if (elem.type == "CCVS" || elem.type == "CCCS") {
            if (inputs.size() < 2) {
                throw invalid_argument("Need 2 values: ControlSource, Gain");
            }

            elem.controlSourceName = inputs[0];
            elem.value = parseValue(inputs[1]);

            // بررسی وجود منبع کنترل
            if (elementCounters.find(elem.controlSourceName) == elementCounters.end()) {
                throw invalid_argument("Control source not found: " + elem.controlSourceName);
            }
        }
        else {
            throw invalid_argument("Unknown element type: " + elem.type);
        }
    }
    catch (const exception& e) {
        cerr << "Error applying values to " << elem.type << " (" << elem.label << "): "
             << e.what() << endl;
        throw; // خطا را به سطح بالا منتقل می‌کند
    }
}

// User schematics and name lists
unordered_map<string, string> userSchematics;
vector<string> userSchematicNames;
vector<string> allSchematicNames;

// Predefined schematics
const unordered_map<string, string> predefinedSchematics = {
    {"draft1", "R1 n1 n2 100\nV1 n2 0 5\n"},
    {"draft2", "V1 in 0 DC 0 AC 1 SIN(0 1 1k)\nR1 in out 1k\nC1 out 0 1uF\n"},
    {"draft3", "V1 n1 0 12\nR1 n1 n2 1k\nR2 n2 0 2k\n"},
    {"elecphase1", "V1 input 0 5\nR1 input output 10k\nR2 output 0 20k\n"}
};

string generateNetlist(const std::vector<CircuitElement>& elements,
                          const std::vector<Wire>& wires) {
    std::stringstream netlist;

    for (const auto& elem : elements) {
        netlist << elem.label << " " << elem.node1 << " " << elem.node2 << " ";

        if (elem.type == "RESISTOR") {
            // تبدیل به واحدهای استاندارد (kΩ, MΩ etc.)
            if (elem.value >= 1e6) {
                netlist << (elem.value/1e6) << "MEG";
            } else if (elem.value >= 1e3) {
                netlist << (elem.value/1e3) << "K";
            } else {
                netlist << elem.value;
            }
            netlist << "\n";
        }
        else if (elem.type == "CAPACITOR") {
            // تبدیل به واحدهای استاندارد (pF, nF, µF)
            if (elem.value >= 1e-6) {
                netlist << (elem.value/1e-6) << "U";
            } else if (elem.value >= 1e-9) {
                netlist << (elem.value/1e-9) << "N";
            } else if (elem.value >= 1e-12) {
                netlist << (elem.value/1e-12) << "P";
            } else {
                netlist << elem.value;
            }
            netlist << "\n";
        }
        else if (elem.type == "INDUCTOR") {
            netlist << elem.value << "\n";
        }
        else if (elem.type == "VOLTAGE_SOURCE") {
            netlist << "DC " << elem.value << "\n";
        }
        else if (elem.type == "CURRENT_SOURCE") {
            netlist << "DC " << elem.value << "\n";
        }
        else if (elem.type == "SINUSOIDAL_SOURCE") {
            netlist << "SIN(" << elem.offset << " " << elem.amplitude
                   << " " << elem.frequency << ")\n";
        }
        else if (elem.type == "PULSE_VOLTAGE_SOURCE") {
            netlist << "PULSE(" << elem.v1 << " " << elem.v2 << " " << elem.td
                   << " " << elem.tr << " " << elem.tf << " " << elem.pw
                   << " " << elem.per << ")\n";
        }
        else if (elem.type == "DIODE") {
            netlist << "D1N4148\n";  // مدل دیود پیش‌فرض
        }
        // منابع وابسته
        else if (elem.type == "VCVS") {
            netlist << elem.ctrlNode1 << " " << elem.ctrlNode2 << " "
                   << elem.value << "\n";  // مقدار gain
        }
        else if (elem.type == "VCCS") {
            netlist << elem.ctrlNode1 << " " << elem.ctrlNode2 << " "
                   << elem.value << "\n";  // مقدار transconductance
        }
        else if (elem.type == "CCVS") {
            netlist << elem.controlSourceName << " " << elem.value << "\n";
        }
        else if (elem.type == "CCCS") {
            netlist << elem.controlSourceName << " " << elem.value << "\n";
        }
    }

    if (!wires.empty()) {
        netlist << "\n* Connections:\n";
        for (const auto& wire : wires) {
            netlist << "* W " << wire.start.first << "_" << wire.start.second
                   << " -> " << wire.end.first << "_" << wire.end.second << "\n";
        }
    }

    bool hasDiode = std::any_of(elements.begin(), elements.end(),
        [](const CircuitElement& e) { return e.type == "DIODE"; });

    if (hasDiode) {
        netlist << "\n.model D1N4148 D(IS=2.52n RS=0.568 N=1.752 CJO=4p M=0.4 TT=20n)\n";
    }

    return netlist.str();
}

void loadCircuitElements(CircuitManager& circuit,
                        vector<CircuitElement>& elements,
                        int& currentElementId) {

    // پاکسازی المان‌های قبلی
    elements.clear();
    currentElementId = 0;

    // تنظیمات موقعیت‌دهی
    int startX = 400;
    int startY = 200;
    int x = startX;
    int y = startY;
    int spacing = 100;

    // لیست المان‌های پشتیبانی شده
    const vector<string> supportedElements = {
        "RESISTOR", "CAPACITOR", "INDUCTOR",
        "VOLTAGE_SOURCE", "CURRENT_SOURCE",
        "DIODE", "VCVS", "VCCS", "CCVS", "CCCS"
    };

    // بارگذاری هر المان
    for (const auto& comp : circuit.getComponents()) {
        // بررسی پشتیبانی از نوع المان
        if (find(supportedElements.begin(), supportedElements.end(), comp.type) == supportedElements.end()) {
            continue; // اگر المان پشتیبانی نمی‌شود، رد می‌شویم
        }

        CircuitElement element;
        element.id = ++currentElementId;
        element.type = comp.type;
        element.label = comp.name;
        element.value = comp.value;
        element.node1 = comp.node1;
        element.node2 = comp.node2;
        element.rotation = 0;
        element.isSelected = false;
        element.isDragging = false;

        // تنظیم موقعیت و اندازه
        element.rect = {
            x,
            y,
            4 * GRID_SIZE,
            2 * GRID_SIZE
        };

        // تنظیم رنگ بر اساس نوع المان
        if (element.type == "RESISTOR") {
            element.color = {0, 0, 255, 255}; // آبی
        }
        else if (element.type == "CAPACITOR") {
            element.color = {0, 128, 0, 255}; // سبز
        }
        else if (element.type == "INDUCTOR") {
            element.color = {128, 0, 128, 255}; // بنفش
        }
        else if (element.type == "VOLTAGE_SOURCE" || element.type == "CURRENT_SOURCE") {
            element.color = {255, 0, 0, 255}; // قرمز
        }
        else if (element.type == "DIODE") {
            element.color = {255, 165, 0, 255}; // نارنجی
        }
        else { // برای المان‌های کنترل شده
            element.color = {139, 69, 19, 255}; // قهوه‌ای
        }

        elements.push_back(element);

        // به‌روزرسانی موقعیت برای المان بعدی
        x += spacing;
        if (x > 1000) { // اگر به انتهای خط رسیدیم
            x = startX;
            y += spacing;
        }
    }

    // بارگذاری اتصالات (سیم‌ها)
    for (const auto& conn : circuit.getConnections()) {
        // یافتن المان مبدأ
        auto fromIt = find_if(elements.begin(), elements.end(),
            [&](const CircuitElement& e) { return e.label == conn.from; });

        // یافتن المان مقصد
        auto toIt = find_if(elements.begin(), elements.end(),
            [&](const CircuitElement& e) { return e.label == conn.to; });

        if (fromIt != elements.end() && toIt != elements.end()) {
            // محاسبه نقاط اتصال (مرکز المان‌ها)
            pair<int, int> start = {
                fromIt->rect.x + fromIt->rect.w/2,
                fromIt->rect.y + fromIt->rect.h/2
            };
            pair<int, int> end = {
                toIt->rect.x + toIt->rect.w/2,
                toIt->rect.y + toIt->rect.h/2
            };

            // اضافه کردن سیم
            wires.push_back({start, end});
        }
    }
}
vector<SDL_Rect> nodeSubButtons;
// تابع نمایش GUI با تغییرات لازم
void showAnalysisSelectionGUI(vector<string>& allSchematicNames, CircuitManager& circuit) {
    vector<Wire> wires;
    bool wiringMode = false;
    bool isDrawingWire = false;
    pair<int, int> wireStartPoint = {-1, -1};
    pair<int, int> wireCurrentPoint = {-1, -1};
    vector<string> subcircuitNames;
    string subFolder = "c:/Users/Ava-PC/ClionProjects/phase2/phase2_oop/subcircuits/";


    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << endl;
        return;
    }

    if (TTF_Init() == -1) {
        cerr << "TTF could not initialize! TTF_Error: " << TTF_GetError() << endl;
        SDL_Quit();
        return;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow("VoltCraft",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        1400, 700,
                                        SDL_WINDOW_SHOWN);

    if (!window) {
        cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << endl;
        TTF_Quit();
        SDL_Quit();
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return;
    }

    // Load font
    const char* fontPaths[] = {
        "arial.ttf",
        "fonts/arial.ttf",
        "assets/fonts/arial.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "FreeSans.ttf",
        nullptr
    };
    TTF_Font* font = nullptr;
    for (int i = 0; fontPaths[i] != nullptr; i++) {
        font = TTF_OpenFont(fontPaths[i], 24);
        if (font) break;
    }

    if (!font) {
        cerr << "Failed to load any font! TTF_Error: " << TTF_GetError() << endl;
    }
//ایکون
    SDL_Surface* iconSurface = IMG_Load("c:/Users/Ava-PC/ClionProjects/phase2/phase2_oop/voltcraft.png");
    if (iconSurface) {
        SDL_SetWindowIcon(window, iconSurface);
        SDL_FreeSurface(iconSurface);
    }
    //لوگو
    SDL_Surface* logoSurface = IMG_Load("c:/Users/Ava-PC/ClionProjects/phase2/phase2_oop/voltcraft.png");
    SDL_Texture* logoTexture = SDL_CreateTextureFromSurface(renderer, logoSurface);
    SDL_FreeSurface(logoSurface);
    // تنظیمات لوگو
    Uint32 logoStartTime = SDL_GetTicks(); // زمان شروع
    const Uint32 logoDisplayTime = 3000;   // مدت نمایش لوگو (3 ثانیه)
    const Uint32 fadeDuration = 500;       // مدت fade (0.5 ثانیه)
    SDL_Rect logoRect = {
        450,  // مرکز افقی (اگر پنجره 1400px عرض دارد)
        225,   // مرکز عمودی (اگر پنجره 700px ارتفاع دارد)
        500,               // عرض لوگو
        250                // ارتفاع لوگو
    };


    // Button rectangles
    SDL_Rect transientButton = {10, 20, 160, 40};
    SDL_Rect acSweepButton = {10, 70, 160, 40};
    SDL_Rect nodeLibraryButton = {10, 120, 160, 40};
    SDL_Rect fileButton = {10, 170, 160, 40};
    SDL_Rect libraryButton = {10, 220, 160, 40};
    SDL_Rect scopeButton = {10, 270, 160, 40};

    // State variables
    bool showTransientSubButtons = false;
    bool showACSweepSubButtons = false;
    bool showNodeLibrarySubButtons = false;
    bool showSweepTypeSubButtons = false;
    bool showFileSubButtons = false;
    bool showSchematicButtons = false;


    // Sub-buttons
    SDL_Rect subButtons[3];
  //  SDL_Rect nodeSubButtons[14];
    SDL_Rect sweepTypeSubButtons[3];
    vector<SDL_Rect> fileActionButtons;   // دکمه‌های ثابت مثل Save, Load
    vector<SDL_Rect> schematicButtons;    // لیست فایل‌های ذخیره‌شده


    // Colors
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color black = {0, 0, 0, 255};
    SDL_Color gray = {200, 200, 200, 255};
    SDL_Color darkerGray = {120, 120, 120, 255};
    SDL_Color blue = {0, 0, 255, 255};
    SDL_Color red = {255, 0, 0, 255};
    SDL_Color green = {0, 255, 0, 255};

    // Circuit elements
    vector<CircuitElement> circuitElements;
    int currentElementId = 0;
    CircuitElement* selectedElement = nullptr;
    int dragOffsetX = 0, dragOffsetY = 0;

    SDL_StartTextInput();

    // Main loop
    bool quit = false;
    while (!quit) {
        SDL_Event e;
        if (showPopup && !SDL_IsTextInputActive()) {
            SDL_StartTextInput();
        }

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x, y;
                SDL_GetMouseState(&x, &y);
                if (wiringMode) {

                    int snappedX = ((x + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;
                    int snappedY = ((y + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;
                    pair<int, int> clickedPoint = {snappedX, snappedY};
                    for (const auto& element : circuitElements) {
                        int cx = element.rect.x + element.rect.w / 2;
                        int cy = element.rect.y + element.rect.h / 2;
                        SDL_Rect pointRect = {cx - 5, cy - 5, 10, 10};
                        if (x >= pointRect.x && x <= pointRect.x + pointRect.w &&
                            y >= pointRect.y && y <= pointRect.y + pointRect.h) {
                            clickedPoint = {cx, cy};
                            break;
                            }
                    }


                    if (!isDrawingWire) {
                        wireStartPoint = clickedPoint;
                        wireCurrentPoint = clickedPoint;
                        isDrawingWire = true;
                        cout << "Wire start point selected." << endl;
                    } else {
                        wires.push_back({wireStartPoint, clickedPoint});
                        isDrawingWire = false;
                        wiringMode = false;
                        wireStartPoint = {-1, -1};
                        wireCurrentPoint = {-1, -1};
                        cout << "Wire created between two points." << endl;
                    }
                }


                // Check if clicking on a circuit element
                bool clickedOnElement = false;
                for (auto& element : circuitElements) {
                    if (x >= element.rect.x && x <= element.rect.x + element.rect.w &&
                        y >= element.rect.y && y <= element.rect.y + element.rect.h) {

                        selectedElement = &element;
                        element.isSelected = true;
                        element.isDragging = true;
                        dragOffsetX = x - element.rect.x;
                        dragOffsetY = y - element.rect.y;
                        clickedOnElement = true;

                        // Double click to rotate
                        if (e.button.clicks == 2) {
                            cout << "Rotating " << element.label << endl;
                            // Rotate the element 90 degrees
                            element.rotation = (element.rotation + 1) % 4;

                            int centerX = element.rect.x + element.rect.w/2;
                            int centerY = element.rect.y + element.rect.h/2;

                            centerX = ((centerX + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;
                            centerY = ((centerY + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;

                            // تنظیم مجدد موقعیت با حفظ مرکز
                            element.rect.x = centerX - element.rect.w/2;
                            element.rect.y = centerY - element.rect.h/2;
                        }
                    } else {
                        element.isSelected = false;
                    }
                }

                if (clickedOnElement) continue;

                // Check main buttons
                if (x >= transientButton.x && x <= transientButton.x + transientButton.w &&
                    y >= transientButton.y && y <= transientButton.y + transientButton.h) {
                    showTransientSubButtons = !showTransientSubButtons;
                    showACSweepSubButtons = false;
                    showNodeLibrarySubButtons = false;
                    showSweepTypeSubButtons = false;
                    showFileSubButtons = false;
                }
                else if (x >= acSweepButton.x && x <= acSweepButton.x + acSweepButton.w &&
                         y >= acSweepButton.y && y <= acSweepButton.y + acSweepButton.h) {
                    showACSweepSubButtons = !showACSweepSubButtons;
                    showTransientSubButtons = false;
                    showNodeLibrarySubButtons = false;
                    showSweepTypeSubButtons = false;
                    showFileSubButtons = false;
                }
                else if (x >= nodeLibraryButton.x && x <= nodeLibraryButton.x + nodeLibraryButton.w &&
                         y >= nodeLibraryButton.y && y <= nodeLibraryButton.y + nodeLibraryButton.h) {
                    showNodeLibrarySubButtons = !showNodeLibrarySubButtons;
                    showTransientSubButtons = false;
                    showACSweepSubButtons = false;
                    showSweepTypeSubButtons = false;
                    showFileSubButtons = false;
                }
                else if (x >= fileButton.x && x <= fileButton.x + fileButton.w &&
         y >= fileButton.y && y <= fileButton.y + fileButton.h) {
                    showFileSubButtons = !showFileSubButtons;
                    // سایر زیردکمه‌ها را مخفی کنید
                    showTransientSubButtons = false;
                    showACSweepSubButtons = false;
                    showNodeLibrarySubButtons = false;
                    showSweepTypeSubButtons = false;
                    schematicButtons.clear();
                    fileActionButtons.clear();
                    showSchematicButtons = false;

         }
                else if (showNewProjectConfirm) {
                    SDL_Rect yesButton = {500 + 50, 300 + 120, 100, 40};
                    SDL_Rect noButton = {500 + 250, 300 + 120, 100, 40};

                    if (x >= yesButton.x && x <= yesButton.x + yesButton.w &&
                        y >= yesButton.y && y <= yesButton.y + yesButton.h) {
                        // ایجاد پروژه جدید
                        circuit.clearAll();
                        circuitElements.clear();
                        wires.clear();
                        showNewProjectConfirm = false;
                        cout << "New project created" << endl; // برای دیباگ
                        }
                    else if (x >= noButton.x && x <= noButton.x + noButton.w &&
                             y >= noButton.y && y <= noButton.y + noButton.h) {
                        showNewProjectConfirm = false;
                        cout << "New project canceled" << endl; // برای دیباگ
                             }
                }
                else if (showFileSubButtons) {
    // کلیک روی دکمه‌های ثابت File
    for (size_t i = 0; i < fileActionButtons.size(); i++) {
        if (x >= fileActionButtons[i].x && x <= fileActionButtons[i].x + fileActionButtons[i].w &&
            y >= fileActionButtons[i].y && y <= fileActionButtons[i].y + fileActionButtons[i].h) {

            if (i == 0) { // Existing Schematics
                showSchematicButtons = true;
                // بارگذاری لیست مدارهای ذخیره‌شده
                schematicButtons.clear();
                int startY = fileButton.y + 4 * 50;
                int startX = fileButton.x + fileButton.w + 20;
                string folder = "c:/Users/Ava-PC/ClionProjects/phase2/phase2_oop/";
                vector<string> foundFiles = getSavedNetlistFiles(folder);

                for (const string& name : foundFiles) {
                    if (find(allSchematicNames.begin(), allSchematicNames.end(), name) == allSchematicNames.end()) {
                        allSchematicNames.push_back(name);
                    }
                }

                for (size_t j = 0; j < allSchematicNames.size(); j++) {
                    SDL_Surface* textSurface = TTF_RenderText_Solid(font, allSchematicNames[j].c_str(), blue);
                    SDL_Rect fileRect = {
                        startX,
                        startY + static_cast<int>(j) * (textSurface->h + 10),
                        textSurface->w + 20,
                        textSurface->h + 10
                    };
                    schematicButtons.push_back(fileRect);
                }
            }
            else if (i == 1) { // New Project
                showNewProjectConfirm = true;
            }
            else if (i == 2) { // Save Current Project
                showSavePopup = true;
                savePopupInput = "";
                savePopupConfirmed = false;
                SDL_StartTextInput();
            }
            else if (i == 3) { // Save As Subcircuit
                string folder = "c:/Users/Ava-PC/ClionProjects/phase2/phase2_oop/subcircuits/";
                string filename = savePopupInput + ".subckt";
                string fullPath = folder + filename;

                ofstream file(fullPath);
                if (!file.is_open()) {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Cannot save subcircuit", window);
                    return;
                }

                string netlist = circuit.getNetlist();
                file << netlist;
                file.close();

                // اضافه به لیست‌ها
                allSchematicNames.push_back(savePopupInput);
                userSchematics[savePopupInput] = netlist;

                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                                         "Saved",
                                         ("Subcircuit saved as " + filename).c_str(),
                                         window);
            }
        }
    }

    // پردازش کلیک روی مدارهای موجود
    if (showSchematicButtons) {
        for (size_t i = 0; i < schematicButtons.size(); i++) {
            if (x >= schematicButtons[i].x && x <= schematicButtons[i].x + schematicButtons[i].w &&
                y >= schematicButtons[i].y && y <= schematicButtons[i].y + schematicButtons[i].h) {

                string name = allSchematicNames[i];
                string netlist;

                // جستجو در مدارهای از پیش تعریف‌شده
                auto it_pre = predefinedSchematics.find(name);
                if (it_pre != predefinedSchematics.end()) {
                    netlist = it_pre->second;
                }
                // جستجو در مدارهای کاربر
                else {
                    auto it_user = userSchematics.find(name);
                    if (it_user != userSchematics.end()) {
                        netlist = it_user->second;
                    }
                }

                string folder = "c:/Users/Ava-PC/ClionProjects/phase2/phase2_oop/";
                string fullPath = folder + name + ".txt";

                try {
                    circuit.clearAll();
                    circuit.loadFromFile(fullPath);
                    circuitElements.clear();
                    wires.clear();
                    loadCircuitElements(circuit, circuitElements, currentElementId);
                    showSchematicButtons = false;
                    showFileSubButtons = false;
                    cout << "Loaded schematic from file: " << fullPath << endl;
                } catch (const exception& e) {
                    cerr << "Error loading schematic: " << e.what() << endl;
                }
            }
        }
    }
}
                else if (x >= libraryButton.x && x <= libraryButton.x + libraryButton.w &&
                         y >= libraryButton.y && y <= libraryButton.y + libraryButton.h) {
                    cout << "Library button clicked" << endl;
                }
                else if (x >= scopeButton.x && x <= scopeButton.x + scopeButton.w &&
                         y >= scopeButton.y && y <= scopeButton.y + scopeButton.h) {
                    cout << "Scope button clicked" << endl;
                }
                // Check sub-buttons if visible
                else if (showTransientSubButtons) {
                    for (int i = 0; i < 3; i++) {
                        if (x >= subButtons[i].x && x <= subButtons[i].x + subButtons[i].w &&
                            y >= subButtons[i].y && y <= subButtons[i].y + subButtons[i].h) {
                            cout << "Transient sub-button " << i << " clicked" << endl;
                        }
                    }
                }
                else if (showACSweepSubButtons) {
                    for (int i = 0; i < 3; i++) {
                        if (x >= subButtons[i].x && x <= subButtons[i].x + subButtons[i].w &&
                            y >= subButtons[i].y && y <= subButtons[i].y + subButtons[i].h) {
                            if (i == 0) { // Type of sweep
                                showSweepTypeSubButtons = !showSweepTypeSubButtons;
                            } else {
                                cout << "AC Sweep sub-button " << i << " clicked" << endl;
                            }
                        }
                    }
                }
                else if (showNodeLibrarySubButtons) {
                    for (int i = 0; i < 14 && i < nodeSubButtons.size(); i++) {
                        if (x >= nodeSubButtons[i].x && x <= nodeSubButtons[i].x + nodeSubButtons[i].w &&
                            y >= nodeSubButtons[i].y && y <= nodeSubButtons[i].y + nodeSubButtons[i].h) {

                            CircuitElement newElement;
                            int snappedX = ((x + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;
                            int snappedY = ((y + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;

                            // تنظیمات مشترک برای همه المان‌ها
                            newElement.rect = {
                                snappedX - (2 * GRID_SIZE),  // عرض 4 واحد گرید
                                snappedY - (1 * GRID_SIZE),   // ارتفاع 2 واحد گرید
                                4 * GRID_SIZE,
                                2 * GRID_SIZE
                            };
                            newElement.id = ++currentElementId;
                            newElement.node1 = "n" + to_string(currentElementId*2);
                            newElement.node2 = "n" + to_string(currentElementId*2+1);
                            newElement.isSelected = false;
                            newElement.isDragging = false;
                            newElement.rotation = 0;

                            if (!font) {
                                cerr << "Error: Font not loaded!" << endl;
                                continue;
                            }

                            if (i == 0) { // مقاومت
                                elementCounters["RESISTOR"]++;
                                newElement.type = "RESISTOR";
                                newElement.label = "R" + to_string(elementCounters["RESISTOR"]);
                            }
                            else if (i == 1) { // دیود
                                elementCounters["DIODE"]++;
                                newElement.type = "DIODE";
                                newElement.label = "D" + to_string(elementCounters["DIODE"]);
                                newElement.value = 0.7;
                                circuitElements.push_back(newElement);
                                circuit.addComponent(make_unique<IdealDiode>(newElement.label, newElement.node1, newElement.node2, newElement.value));

                                continue;
                            }
                            else if (i == 2) { // Capacitor
                                elementCounters["CAPACITOR"]++;
                                newElement.type = "CAPACITOR";
                                newElement.label = "C" + to_string(elementCounters["CAPACITOR"]);
                            }
                            else if (i == 3) { // Inductor
                                elementCounters["INDUCTOR"]++;
                                newElement.type = "INDUCTOR";
                                newElement.label = "L" + to_string(elementCounters["INDUCTOR"]);
                            }
                            else if (i == 4) { // Voltage Source
                                elementCounters["VOLTAGE_SOURCE"]++;
                                newElement.type = "VOLTAGE_SOURCE";
                                newElement.label = "V" + to_string(elementCounters["VOLTAGE_SOURCE"]);
                            }
                            else if (i == 5) { // Current Source
                                elementCounters["CURRENT_SOURCE"]++;
                                newElement.type = "CURRENT_SOURCE";
                                newElement.label = "I" + to_string(elementCounters["CURRENT_SOURCE"]);
                            }
                            else if (i == 6) { // Sinusoidal Source
                                elementCounters["SINUSOIDAL_SOURCE"]++;
                                newElement.type = "SINUSOIDAL_SOURCE";
                                newElement.label = "VS" + to_string(elementCounters["SINUSOIDAL_SOURCE"]);
                            }
                            else if (i == 7) { // Pulse Voltage Source
                                elementCounters["PULSE_VOLTAGE_SOURCE"]++;
                                newElement.type = "PULSE_VOLTAGE_SOURCE";
                                newElement.label = "VP" + to_string(elementCounters["PULSE_VOLTAGE_SOURCE"]);
                            }
                            else if (i == 8) { // VCVS
                                elementCounters["VCVS"]++;
                                newElement.type = "VCVS";
                                newElement.label = "E" + to_string(elementCounters["VCVS"]);
                            }
                            else if (i == 9) { // VCCS
                                elementCounters["VCCS"]++;
                                newElement.type = "VCCS";
                                newElement.label = "G" + to_string(elementCounters["VCCS"]);
                            }
                            else if (i == 10) { // CCVS
                                elementCounters["CCVS"]++;
                                newElement.type = "CCVS";
                                newElement.label = "H" + to_string(elementCounters["CCVS"]);
                            }
                            else if (i == 11) { // CCCS
                                elementCounters["CCCS"]++;
                                newElement.type = "CCCS";
                                newElement.label = "F" + to_string(elementCounters["CCCS"]);
                            }
                            else if (i == 12) { // Subcircuits
                                // Handle subcircuits separately

                            }
                            else if (i == 13) { // فرض بر اینه که دکمه Wire شماره 13 هست
                                if (isDrawingWire) {
                                    isDrawingWire = false;
                                    wireStartPoint = {-1, -1};
                                    wireCurrentPoint = {-1, -1};
                                    cout << "Previous wire drawing cancelled." << endl;
                                    continue;
                                }
                                wiringMode = true;
                                cout << "Wiring mode activated. Click on a connection point to start." << endl;
                                continue;
                            }
                            // [3] بررسی وجود فیلدهای ورودی برای المان
                            if (elementInputFields.find(newElement.type) == elementInputFields.end()) {
                                cerr << "Error: No input fields defined for " << newElement.type << endl;
                                continue;
                            }

                            // [4] تنظیم popup
                            popupElement = newElement;
                            popupFields = elementInputFields[newElement.type];
                            popupInputs = vector<string>(popupFields.size(), "");
                            popupCurrentField = 0;
                            showPopup = true;

                            // [5] مدیریت صحیح ورود متن
                            if (!SDL_IsTextInputActive()) {
                                SDL_StartTextInput();
                            }
                        }
                    }
                    int baseIndex = 14;
                    for (size_t i = 0; i < subcircuitNames.size(); i++) {
                        int idx = baseIndex + static_cast<int>(i);
                        if (idx < nodeSubButtons.size() &
                            x >= nodeSubButtons[idx].x && x <= nodeSubButtons[idx].x + nodeSubButtons[idx].w &&
                            y >= nodeSubButtons[idx].y && y <= nodeSubButtons[idx].y + nodeSubButtons[idx].h)
                        {

                            string subName = subcircuitNames[i];
                            string subPath = subFolder + subName + ".subckt";

                            // ساخت المان Subcircuit و اضافه به مدار
                            CircuitElement newElement;
                            newElement.type = "SUBCIRCUIT";
                            newElement.label = "X_" + subName;
                            newElement.node1 = "n" + to_string(currentElementId * 2);
                            newElement.node2 = "n" + to_string(currentElementId * 2 + 1);
                            newElement.id = ++currentElementId;
                            newElement.rect = {
                                600, 300, 4 * GRID_SIZE, 2 * GRID_SIZE
                            };
                            newElement.color = {0, 255, 255, 255}; // فیروزه‌ای
                            circuitElements.push_back(newElement);

                            circuit.addComponent(make_unique<SubcircuitHC>(newElement.label, newElement.node1, newElement.node2, subPath));
                            cout << "Subcircuit added: " << subName << endl;
                            }
                    }

                }
                else if (showSweepTypeSubButtons) {
                    for (int i = 0; i < 3; i++) {
                        if (x >= sweepTypeSubButtons[i].x && x <= sweepTypeSubButtons[i].x + sweepTypeSubButtons[i].w &&
                            y >= sweepTypeSubButtons[i].y && y <= sweepTypeSubButtons[i].y + sweepTypeSubButtons[i].h) {
                            cout << "Sweep type sub-button " << i << " clicked" << endl;
                        }
                    }
                }
                }
            else if (e.type == SDL_MOUSEBUTTONUP) {
                if (selectedElement) {
                    selectedElement->isDragging = false;
                    selectedElement = nullptr;
                }
            }
            else if (showPopup && e.type == SDL_TEXTINPUT) {
                if (popupCurrentField >= 0 && popupCurrentField < popupInputs.size()) {
                    popupInputs[popupCurrentField] += e.text.text;
                }
            }else if (showSavePopup && e.type == SDL_TEXTINPUT) {
                savePopupInput += e.text.text;
            }

            else if (e.type == SDL_KEYDOWN) {
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    if (showPopup) {
        // پردازش رویدادهای پاپ‌آپ المان‌ها
        if (popupCurrentField < 0 || popupCurrentField >= popupInputs.size()) {
            cerr << "Invalid field index: " << popupCurrentField << endl;
            continue;
        }

        if (e.key.keysym.sym == SDLK_BACKSPACE) {
            if (!popupInputs[popupCurrentField].empty()) {
                popupInputs[popupCurrentField].pop_back();
            }
        }
        else if (e.key.keysym.sym == SDLK_RETURN) {
            if (popupCurrentField == popupFields.size() - 1) {
                try {
                    applyPopupValues(popupElement, popupInputs);
                    if (popupElement.type == "RESISTOR") {
    circuit.addComponent(make_unique<Resistor>(popupElement.label, popupElement.node1, popupElement.node2, popupElement.value));
}
else if (popupElement.type == "CAPACITOR") {
    circuit.addComponent(make_unique<Capacitor>(popupElement.label, popupElement.node1, popupElement.node2, popupElement.value));
}
else if (popupElement.type == "INDUCTOR") {
    circuit.addComponent(make_unique<Inductor>(popupElement.label, popupElement.node1, popupElement.node2, popupElement.value));
}
else if (popupElement.type == "VOLTAGE_SOURCE") {
    circuit.addComponent(make_unique<VoltageSource>(popupElement.label, popupElement.node1, popupElement.node2, popupElement.value));
}
else if (popupElement.type == "CURRENT_SOURCE") {
    circuit.addComponent(make_unique<CurrentSource>(popupElement.label, popupElement.node1, popupElement.node2, popupElement.value));
}
else if (popupElement.type == "DIODE") {
    circuit.addComponent(make_unique<IdealDiode>(popupElement.label, popupElement.node1, popupElement.node2, popupElement.value));
}
else if (popupElement.type == "SINUSOIDAL_SOURCE") {
    circuit.addComponent(make_unique<SinusoidalSource>(popupElement.label, popupElement.node1, popupElement.node2,
        popupElement.offset, popupElement.amplitude, popupElement.frequency));
}
else if (popupElement.type == "PULSE_VOLTAGE_SOURCE") {
    circuit.addComponent(make_unique<PulseVoltageSource>(popupElement.label, popupElement.node1, popupElement.node2,
        popupElement.v1, popupElement.v2, popupElement.td, popupElement.tr,
        popupElement.tf, popupElement.pw, popupElement.per));
}
else if (popupElement.type == "VCVS") {
    circuit.addComponent(make_unique<VCVS>(popupElement.label, popupElement.node1, popupElement.node2,
        popupElement.ctrlNode1, popupElement.ctrlNode2, popupElement.value));
}
else if (popupElement.type == "VCCS") {
    circuit.addComponent(make_unique<VCCS>(popupElement.label, popupElement.node1, popupElement.node2,
        popupElement.ctrlNode1, popupElement.ctrlNode2, popupElement.value));
}
else if (popupElement.type == "CCVS") {
    circuit.addComponent(make_unique<CCVS>(popupElement.label, popupElement.node1, popupElement.node2,
        popupElement.controlSourceName, popupElement.value));
}
else if (popupElement.type == "CCCS") {
    circuit.addComponent(make_unique<CCCS>(popupElement.label, popupElement.node1, popupElement.node2,
        popupElement.controlSourceName, popupElement.value));
}

                    circuitElements.push_back(popupElement);
                }
                catch (const exception& e) {
                    cerr << "Failed to add element: " << e.what() << endl;
                }

                showPopup = false;
                popupFields.clear();
                popupInputs.clear();
                popupCurrentField = 0;
                SDL_StopTextInput();
            }
            else {
                popupCurrentField++;
            }
        }
    }else if (showSavePopup) {
        if (e.key.keysym.sym == SDLK_RETURN) {
            savePopupConfirmed = true;
            showSavePopup = false;
            SDL_StopTextInput();
        }
        else if (e.key.keysym.sym == SDLK_ESCAPE) {
            showSavePopup = false;
            SDL_StopTextInput();
        }
        else if (e.key.keysym.sym == SDLK_BACKSPACE) {
            if (!savePopupInput.empty()) {
                savePopupInput.pop_back();
            }
        }
    }
    else if (!isDrawingWire) {
        // پردازش کلیدهای میانبر برای ایجاد المان‌ها
        switch(e.key.keysym.sym) {
            case SDLK_r: // مقاومت (R)
            case SDLK_c: // خازن (C)
            case SDLK_l: // سلف (L)
            case SDLK_d: // دیود (D)
            case SDLK_v: // منبع ولتاژ (V)
            case SDLK_i: // منبع جریان (I)
                addElementByShortcut(e.key.keysym.sym, mouseX, mouseY,
                                    circuitElements, elementCounters, currentElementId);
            break;
        }
    }
}
            else if (e.type == SDL_MOUSEMOTION) {
                if (selectedElement && selectedElement->isDragging) {
                    int x, y;
                    SDL_GetMouseState(&x, &y);
                    // Snap به نزدیکترین نقطه تقاطع گرید
                    x = ((x + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;
                    y = ((y + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;
                    selectedElement->rect.x = x - (selectedElement->rect.w / 2); // وسط المان روی نقطه گرید قرار گیرد
                    selectedElement->rect.y = y - (selectedElement->rect.h / 2);
                }
            }
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        // Draw grid
        drawGrid(renderer);
        // رندر لوگو (جایگزین کد قبلی)
        Uint32 currentTime = SDL_GetTicks() - logoStartTime;
        if (currentTime < logoDisplayTime) {
            // گرادیانت پس‌زمینه
            for (int y = 0; y < 700; y++) {
                Uint8 r = 50 + y * 150 / 700;
                Uint8 g = 100 + y * 100 / 700;
                Uint8 b = 200 + y * 50 / 700;
                SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                SDL_RenderDrawLine(renderer, 0, y, 1400, y);
            }

            // محاسبه alpha برای fade
            float alpha = 1.0f;
            if (currentTime < fadeDuration) {
                alpha = (float)currentTime / fadeDuration; // Fade-in
            }
            else if (currentTime > logoDisplayTime - fadeDuration) {
                alpha = 1.0f - (float)(currentTime - (logoDisplayTime - fadeDuration)) / fadeDuration; // Fade-out
            }

            // محاسبه scale برای انیمیشن
            float scale = 0.9f + 0.1f * sin(currentTime * 0.005f);

            // محاسبه موقعیت و اندازه نهایی
            SDL_Rect finalRect = {
                logoRect.x - (int)((logoRect.w * scale - logoRect.w) / 2),
                logoRect.y - (int)((logoRect.h * scale - logoRect.h) / 2),
                (int)(logoRect.w * scale),
                (int)(logoRect.h * scale)
            };

            // تنظیم alpha و رندر
            SDL_SetTextureAlphaMod(logoTexture, (Uint8)(alpha * 255));
            SDL_RenderCopy(renderer, logoTexture, NULL, &finalRect);
        }

        // Draw all main buttons
        SDL_SetRenderDrawColor(renderer, gray.r, gray.g, gray.b, gray.a);
        SDL_RenderFillRect(renderer, &transientButton);
        SDL_RenderFillRect(renderer, &acSweepButton);
        SDL_RenderFillRect(renderer, &nodeLibraryButton);
        SDL_RenderFillRect(renderer, &fileButton);
        SDL_RenderFillRect(renderer, &libraryButton);
        SDL_RenderFillRect(renderer, &scopeButton);

        // Draw borders
        SDL_SetRenderDrawColor(renderer, black.r, black.g, black.b, black.a);
        SDL_RenderDrawRect(renderer, &transientButton);
        SDL_RenderDrawRect(renderer, &acSweepButton);
        SDL_RenderDrawRect(renderer, &nodeLibraryButton);
        SDL_RenderDrawRect(renderer, &fileButton);
        SDL_RenderDrawRect(renderer, &libraryButton);
        SDL_RenderDrawRect(renderer, &scopeButton);

        // Render button texts
        const char* buttonTexts[] = {
            "Transient", "AC Sweep", "Node Library",
            "File", "Library", "Scope"
        };
        SDL_Rect buttons[] = {transientButton, acSweepButton, nodeLibraryButton,
                             fileButton, libraryButton, scopeButton};

        for (int i = 0; i < 6; i++) {
            if (!font) continue;

            SDL_Surface* surface = TTF_RenderText_Solid(font, buttonTexts[i], black);
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect textRect = {
                    buttons[i].x + 10,
                    buttons[i].y + (buttons[i].h - surface->h)/2,
                    surface->w,
                    surface->h
                };
                SDL_RenderCopy(renderer, texture, NULL, &textRect);
                SDL_DestroyTexture(texture);
                SDL_FreeSurface(surface);
            }
        }

        // Draw circuit elements
        for (const auto& element : circuitElements) {
            // Draw connections based on rotation
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            if (element.rotation == 0 || element.rotation == 2) {
                // Horizontal orientation
                SDL_RenderDrawLine(renderer,
                    element.rect.x, element.rect.y + element.rect.h/2,
                    element.rect.x - 20, element.rect.y + element.rect.h/2);
                SDL_RenderDrawLine(renderer,
                    element.rect.x + element.rect.w, element.rect.y + element.rect.h/2,
                    element.rect.x + element.rect.w + 20, element.rect.y + element.rect.h/2);
            } else {
                // Vertical orientation
                SDL_RenderDrawLine(renderer,
                    element.rect.x + element.rect.w/2, element.rect.y,
                    element.rect.x + element.rect.w/2, element.rect.y - 20);
                SDL_RenderDrawLine(renderer,
                    element.rect.x + element.rect.w/2, element.rect.y + element.rect.h,
                    element.rect.x + element.rect.w/2, element.rect.y + element.rect.h + 20);
            }
            // Draw element with rotation
            SDL_Color elementColor = {0, 0, 255, 255}; // Default blue color
            if (element.type == "RESISTOR") {
                elementColor = {0, 0, 255, 255}; // Blue
            } else if (element.type == "DIODE") {
                elementColor = {255, 165, 0, 255}; // Orange
            } else if (element.type == "CAPACITOR" || element.type == "INDUCTOR") {
                elementColor = {0, 128, 0, 255}; // Green
            } else if (element.type == "VOLTAGE_SOURCE" || element.type == "CURRENT_SOURCE" ||
                       element.type == "SINUSOIDAL_SOURCE" || element.type == "PULSE_VOLTAGE_SOURCE") {
                elementColor = {128, 0, 128, 255}; // Purple
            } else if (element.type == "VCVS" || element.type == "VCCS" ||
                       element.type == "CCVS" || element.type == "CCCS") {
                elementColor = {139, 69, 19, 255}; // Brown
            }

            drawRotatedRect(renderer, element.rect, element.rotation, elementColor);

            // Draw border (different color if selected)
            if (element.isSelected) {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            }
            SDL_RenderDrawRect(renderer, &element.rect);

            // Draw label
            if (!font) continue;

            SDL_Surface* textSurface = TTF_RenderText_Solid(font, element.label.c_str(), black);
            if (textSurface) {
                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                SDL_Rect textRect = {
                    element.rect.x + (element.rect.w - textSurface->w)/2,
                    element.rect.y + (element.rect.h - textSurface->h)/2,
                    textSurface->w,
                    textSurface->h
                };
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                SDL_DestroyTexture(textTexture);
                SDL_FreeSurface(textSurface);
            }

            // Draw value
            string valueText;
            if (element.type == "RESISTOR") {
                valueText = to_string((int)element.value) + "Ω";
            } else if (element.type == "CAPACITOR") {
                valueText = to_string(element.value) + "F";
            } else if (element.type == "INDUCTOR") {
                valueText = to_string(element.value) + "H";
            } else if (element.type == "DIODE") {
                valueText = "Vf=" + to_string(element.value) + "V";
            } else if (element.type == "VOLTAGE_SOURCE"  ||
                       element.type == "SINUSOIDAL_SOURCE" || element.type == "PULSE_VOLTAGE_SOURCE") {
                valueText = to_string(element.value) + "V";
            } else if (element.type == "CURRENT_SOURCE") {
                valueText = "I=" + to_string(element.value) + "A";
            }else if (element.type == "DIODE") {
                valueText = "Vf=" + to_string(element.value) + "V";
            } else if (element.type == "VCVS" || element.type == "VCCS" ||
                       element.type == "CCVS" || element.type == "CCCS") {
                valueText = "G=" + to_string(element.value);
            }
            if (!font) continue;

            textSurface = TTF_RenderText_Solid(font, valueText.c_str(), black);
            if (textSurface) {
                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                SDL_Rect textRect = {
                    element.rect.x + (element.rect.w - textSurface->w)/2,
                    element.rect.y + element.rect.h + 2,
                    textSurface->w,
                    textSurface->h
                };
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                SDL_DestroyTexture(textTexture);
                SDL_FreeSurface(textSurface);
            }
            if (wiringMode) {
                SDL_Color pointColor = {255, 0, 0, 255}; // قرمز
                int cx = element.rect.x + element.rect.w / 2;
                int cy = element.rect.y + element.rect.h / 2;
                drawConnectionPoint(renderer, cx, cy, pointColor);
            }

        }

                // Draw transient sub-buttons if active
        if (showTransientSubButtons) {
            const char* transientTexts[] = {"Stop Time", "Time to start saving data", "Maximum Timestep"};

            for (int i = 0; i < 3; i++) {
                if (!font) continue;

                SDL_Surface* textSurface = TTF_RenderText_Solid(font, transientTexts[i], black);
                subButtons[i] = {
                    transientButton.x + transientButton.w + 20,
                    transientButton.y + i * (textSurface->h + 10),
                    textSurface->w + 20,
                    textSurface->h + 10
                };

                SDL_SetRenderDrawColor(renderer, darkerGray.r, darkerGray.g, darkerGray.b, darkerGray.a);
                SDL_RenderFillRect(renderer, &subButtons[i]);
                SDL_SetRenderDrawColor(renderer, black.r, black.g, black.b, black.a);
                SDL_RenderDrawRect(renderer, &subButtons[i]);

                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                SDL_Rect textRect = {
                    subButtons[i].x + 10,
                    subButtons[i].y + 5,
                    textSurface->w,
                    textSurface->h
                };
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                SDL_DestroyTexture(textTexture);
                SDL_FreeSurface(textSurface);
            }
        }

        // Draw AC Sweep sub-buttons if active
        if (showACSweepSubButtons) {
            const char* acSweepTexts[] = {"Type of sweep", "Start Frequency", "Stop Frequency"};

            for (int i = 0; i < 3; i++) {
                if (!font) continue;

                SDL_Surface* textSurface = TTF_RenderText_Solid(font, acSweepTexts[i], black);
                subButtons[i] = {
                    acSweepButton.x + acSweepButton.w + 20,
                    acSweepButton.y + i * (textSurface->h + 10),
                    textSurface->w + 20,
                    textSurface->h + 10
                };

                SDL_SetRenderDrawColor(renderer, darkerGray.r, darkerGray.g, darkerGray.b, darkerGray.a);
                SDL_RenderFillRect(renderer, &subButtons[i]);
                SDL_SetRenderDrawColor(renderer, black.r, black.g, black.b, black.a);
                SDL_RenderDrawRect(renderer, &subButtons[i]);

                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                SDL_Rect textRect = {
                    subButtons[i].x + 10,
                    subButtons[i].y + 5,
                    textSurface->w,
                    textSurface->h
                };
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                SDL_DestroyTexture(textTexture);
                SDL_FreeSurface(textSurface);
            }
        }

        // Draw sweep type sub-buttons if active
        if (showSweepTypeSubButtons) {
            const char* sweepTypeTexts[] = {"Octave", "Decade", "Linear"};

            for (int i = 0; i < 3; i++) {
                if (!font) continue;

                SDL_Surface* textSurface = TTF_RenderText_Solid(font, sweepTypeTexts[i], black);
                sweepTypeSubButtons[i] = {
                    subButtons[0].x + subButtons[0].w + 10,
                    subButtons[0].y + i * (textSurface->h + 10),
                    textSurface->w + 20,
                    textSurface->h + 10
                };

                SDL_SetRenderDrawColor(renderer, black.r, black.g, black.b, black.a);
                SDL_RenderFillRect(renderer, &sweepTypeSubButtons[i]);
                SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, white.a);
                SDL_RenderDrawRect(renderer, &sweepTypeSubButtons[i]);

                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                SDL_Rect textRect = {
                    sweepTypeSubButtons[i].x + 10,
                    sweepTypeSubButtons[i].y + 5,
                    textSurface->w,
                    textSurface->h
                };
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                SDL_DestroyTexture(textTexture);
                SDL_FreeSurface(textSurface);
            }
        }

        // Draw Node Library sub-buttons if active
        if (showNodeLibrarySubButtons) {
            const char* nodeLibraryTexts[] = {
                "Resistor", "Diode", "Capacitor", "Inductor",
                "Voltage Source", "Current Source", "Sinusoidal Source",
                "Pulse Voltage Source", "VCVS", "VCCS", "CCVS", "CCCS",
                "Subcircuits", "Wire"
            };
            vector<string> subFiles = getSavedNetlistFiles(subFolder);

            for (const string& subName : subFiles) {
                subcircuitNames.push_back(subName);
            }

            nodeSubButtons.clear(); // پاکسازی قبلی
            for (int i = 0; i < 14; i++) {
                if (!font) continue;

                SDL_Surface* textSurface = TTF_RenderText_Solid(font, nodeLibraryTexts[i], black);
                SDL_Rect rect = {
                    nodeLibraryButton.x + nodeLibraryButton.w + 20,
                    nodeLibraryButton.y + i * (textSurface->h + 10),
                    textSurface->w + 20,
                    textSurface->h + 10
                };
                nodeSubButtons.push_back(rect);

                SDL_SetRenderDrawColor(renderer, darkerGray.r, darkerGray.g, darkerGray.b, darkerGray.a);
                SDL_RenderFillRect(renderer, &nodeSubButtons[i]);
                SDL_SetRenderDrawColor(renderer, black.r, black.g, black.b, black.a);
                SDL_RenderDrawRect(renderer, &nodeSubButtons[i]);

                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                SDL_Rect textRect = {
                    nodeSubButtons[i].x + 10,
                    nodeSubButtons[i].y + 5,
                    textSurface->w,
                    textSurface->h
                };
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                SDL_DestroyTexture(textTexture);
                SDL_FreeSurface(textSurface);
            }
            int baseY = nodeLibraryButton.y + 14 * 50; // بعد از دکمه‌های اصلی
            for (size_t i = 0; i < subcircuitNames.size(); i++) {
                string label = "Sub: " + subcircuitNames[i];
                SDL_Surface* textSurface = TTF_RenderText_Solid(font, label.c_str(), black);
                SDL_Rect subRect = {
                    nodeLibraryButton.x + nodeLibraryButton.w + 20,
                    baseY + static_cast<int>(i) * (textSurface->h + 10),
                    textSurface->w + 20,
                    textSurface->h + 10
                };
                nodeSubButtons.push_back(subRect);

                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(renderer, &subRect);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderDrawRect(renderer, &subRect);

                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                SDL_Rect textRect = {
                    subRect.x + 10,
                    subRect.y + 5,
                    textSurface->w,
                    textSurface->h
                };
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                SDL_DestroyTexture(textTexture);
                SDL_FreeSurface(textSurface);
            }

        }

        // Draw file sub-buttons (saved files list) if active
        if (showFileSubButtons) {
            fileActionButtons.clear();
// دکمه‌های ثابت
            const char* fileOptions[] = {
                "Existing Schematics",
                "New Project",  // اضافه شده
                "Save Current Project",
                "Save As Subcircuit"
            };


for (int i = 0; i < 4; i++) {
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, fileOptions[i], black);
    SDL_Rect optionRect = {
        fileButton.x + fileButton.w + 20,
        fileButton.y + i * (textSurface->h + 10),
        textSurface->w + 20,
        textSurface->h + 10
    };
    fileActionButtons.push_back(optionRect);

    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, white.a);
    SDL_RenderFillRect(renderer, &optionRect);
    SDL_SetRenderDrawColor(renderer, black.r, black.g, black.b, black.a);
    SDL_RenderDrawRect(renderer, &optionRect);

    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_Rect textRect = {
        optionRect.x + 10,
        optionRect.y + 5,
        textSurface->w,
        textSurface->h
    };
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);
}
            if (showSchematicButtons) {
                schematicButtons.clear();
                int startY = fileButton.y + 4 * 50;
                int startX = fileButton.x + fileButton.w + 20;

                for (size_t i = 0; i < allSchematicNames.size(); i++) {
                    SDL_Surface* textSurface = TTF_RenderText_Solid(font, allSchematicNames[i].c_str(), blue);
                    SDL_Rect fileRect = {
                        startX,
                        startY + static_cast<int>(i) * (textSurface->h + 10),
                        textSurface->w + 20,
                        textSurface->h + 10
                    };
                    schematicButtons.push_back(fileRect);

                    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, white.a);
                    SDL_RenderFillRect(renderer, &fileRect);
                    SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, blue.a);
                    SDL_RenderDrawRect(renderer, &fileRect);

                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                    SDL_Rect textRect = {
                        fileRect.x + 10,
                        fileRect.y + 5,
                        textSurface->w,
                        textSurface->h
                    };
                    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                    SDL_DestroyTexture(textTexture);
                    SDL_FreeSurface(textSurface);
                }
            }

        }
        SDL_Color wireColor = {0, 0, 0, 255}; // مشکی
        for (const auto& wire : wires) {
            drawWireLShape(renderer, wire.start, wire.end, wireColor);
        }
        if (isDrawingWire) {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            int snappedX = ((mx + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;
            int snappedY = ((my + GRID_SIZE/2) / GRID_SIZE) * GRID_SIZE;
            drawWireLShape(renderer, wireStartPoint, {snappedX, snappedY}, wireColor);
        }
        if (showPopup) {
    // ابعاد و موقعیت جدید پاپ‌آپ
    SDL_Rect popupBox = {
        1400/2 - 200,  // مرکز افقی
        700/2 - 75,  // مرکز عمودی
        400,                  // عرض
        150                   // ارتفاع
    };

    // رسم حاشیه مشکی
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &popupBox);

    // رسم پس‌زمینه سفید داخلی
    SDL_Rect innerBox = {
        popupBox.x + 2,
        popupBox.y + 2,
        popupBox.w - 4,
        popupBox.h - 4
    };
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &innerBox);

    // نمایش متن prompt
    if (font) {
        string prompt = "Enter " + popupFields[popupCurrentField] + ":";
        SDL_Surface* promptSurface = TTF_RenderText_Solid(font, prompt.c_str(), black);
        if (promptSurface) {
            SDL_Texture* promptTexture = SDL_CreateTextureFromSurface(renderer, promptSurface);
            SDL_Rect promptRect = {innerBox.x + 20, innerBox.y + 20, promptSurface->w, promptSurface->h};
            SDL_RenderCopy(renderer, promptTexture, NULL, &promptRect);
            SDL_DestroyTexture(promptTexture);
            SDL_FreeSurface(promptSurface);
        }

        // کادر ورودی متن
        SDL_Rect inputRect = {innerBox.x + 20, innerBox.y + 60, innerBox.w - 40, 30};
        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_RenderFillRect(renderer, &inputRect);
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        SDL_RenderDrawRect(renderer, &inputRect);

        // متن ورودی
        string inputText = popupInputs[popupCurrentField];
        SDL_Surface* inputSurface = TTF_RenderText_Solid(font, inputText.c_str(), black);
        if (inputSurface) {
            SDL_Texture* inputTexture = SDL_CreateTextureFromSurface(renderer, inputSurface);
            SDL_Rect textRect = {
                inputRect.x + 5,
                inputRect.y + (inputRect.h - inputSurface->h)/2,
                inputSurface->w,
                inputSurface->h
            };
            SDL_RenderCopy(renderer, inputTexture, NULL, &textRect);
            SDL_DestroyTexture(inputTexture);
            SDL_FreeSurface(inputSurface);
        }
    }
}
        if (savePopupConfirmed) {
            savePopupConfirmed = false;
            if (!savePopupInput.empty()) {
                string filename = savePopupInput + ".txt";
                try {
                    string folder = "c:/Users/Ava-PC/ClionProjects/phase2/phase2_oop/";
                    filename = folder + filename;
                    cout << "Components before saving: " << circuit.getComponents().size() << endl;
                    string netlist = circuit.getNetlist();
                    cout << "Netlist:\n" << netlist << endl;

                    circuit.saveToFile(filename);
                    userSchematics[savePopupInput] = circuit.getNetlist();
                    userSchematicNames.push_back(savePopupInput);
                    allSchematicNames.push_back(savePopupInput);
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                                           "Saved",
                                           ("Saved as " + filename).c_str(),
                                           window);
                } catch (const exception& e) {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                           "Error",
                                           e.what(),
                                           window);
                }
            } else {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Error",
                                       "Filename cannot be empty",
                                       window);
            }
        }if (showSavePopup) {
    // پس‌زمینه محو
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
    SDL_Rect overlay = {0, 0, 1400, 700};
    SDL_RenderFillRect(renderer, &overlay);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // کادر پاپ‌آپ
    SDL_Rect popupRect = {500, 300, 400, 200};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &popupRect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &popupRect);

    // متن عنوان
    if (font) {
        SDL_Surface* titleSurface = TTF_RenderText_Solid(font, "Save Project", black);
        if (titleSurface) {
            SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
            SDL_Rect titleRect = {popupRect.x + 20, popupRect.y + 20, titleSurface->w, titleSurface->h};
            SDL_RenderCopy(renderer, titleTexture, NULL, &titleRect);
            SDL_FreeSurface(titleSurface);
            SDL_DestroyTexture(titleTexture);
        }

        // متن راهنما
        SDL_Surface* promptSurface = TTF_RenderText_Solid(font, "Enter filename:", black);
        if (promptSurface) {
            SDL_Texture* promptTexture = SDL_CreateTextureFromSurface(renderer, promptSurface);
            SDL_Rect promptRect = {popupRect.x + 20, popupRect.y + 60, promptSurface->w, promptSurface->h};
            SDL_RenderCopy(renderer, promptTexture, NULL, &promptRect);
            SDL_FreeSurface(promptSurface);
            SDL_DestroyTexture(promptTexture);
        }

        // کادر ورودی متن
        SDL_Rect inputRect = {popupRect.x + 20, popupRect.y + 100, 360, 40};
        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_RenderFillRect(renderer, &inputRect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &inputRect);

        // متن وارد شده
        if (!savePopupInput.empty()) {
            SDL_Surface* inputSurface = TTF_RenderText_Solid(font, savePopupInput.c_str(), black);
            if (inputSurface) {
                SDL_Texture* inputTexture = SDL_CreateTextureFromSurface(renderer, inputSurface);
                SDL_Rect textRect = {
                    inputRect.x + 5,
                    inputRect.y + (inputRect.h - inputSurface->h)/2,
                    min(inputSurface->w, inputRect.w - 10),
                    inputSurface->h
                };
                SDL_RenderCopy(renderer, inputTexture, NULL, &textRect);
                SDL_FreeSurface(inputSurface);
                SDL_DestroyTexture(inputTexture);
            }
        }
    }
}
// رندر پاپ‌آپ تأیید پروژه جدید
if (showNewProjectConfirm) {
    // پس‌زمینه محو
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
    SDL_Rect overlay = {0, 0, 1400, 700};
    SDL_RenderFillRect(renderer, &overlay);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // کادر پاپ‌آپ
    SDL_Rect popupRect = {500, 300, 400, 200};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &popupRect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderDrawRect(renderer, &popupRect);

    if (font) {
        // متن پیام
        SDL_Surface* msgSurface = TTF_RenderText_Solid(font, "Create new project?", {0, 0, 0, 255});
        SDL_Texture* msgTexture = SDL_CreateTextureFromSurface(renderer, msgSurface);
        SDL_Rect msgRect = {popupRect.x + 20, popupRect.y + 30, msgSurface->w, msgSurface->h};
        SDL_RenderCopy(renderer, msgTexture, NULL, &msgRect);
        SDL_FreeSurface(msgSurface);
        SDL_DestroyTexture(msgTexture);

        // دکمه تأیید
        SDL_Rect yesButton = {popupRect.x + 50, popupRect.y + 120, 100, 40};
        SDL_SetRenderDrawColor(renderer, 200, 255, 200, 255);
        SDL_RenderFillRect(renderer, &yesButton);
        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255);
        SDL_RenderDrawRect(renderer, &yesButton);

        SDL_Surface* yesSurface = TTF_RenderText_Solid(font, "Yes", {0, 0, 0, 255});
        SDL_Texture* yesTexture = SDL_CreateTextureFromSurface(renderer, yesSurface);
        SDL_Rect yesTextRect = {yesButton.x + 30, yesButton.y + 10, yesSurface->w, yesSurface->h};
        SDL_RenderCopy(renderer, yesTexture, NULL, &yesTextRect);
        SDL_FreeSurface(yesSurface);
        SDL_DestroyTexture(yesTexture);

        // دکمه انصراف
        SDL_Rect noButton = {popupRect.x + 250, popupRect.y + 120, 100, 40};
        SDL_SetRenderDrawColor(renderer, 255, 200, 200, 255);
        SDL_RenderFillRect(renderer, &noButton);
        SDL_SetRenderDrawColor(renderer, 100, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &noButton);

        SDL_Surface* noSurface = TTF_RenderText_Solid(font, "No", {0, 0, 0, 255});
        SDL_Texture* noTexture = SDL_CreateTextureFromSurface(renderer, noSurface);
        SDL_Rect noTextRect = {noButton.x + 35, noButton.y + 10, noSurface->w, noSurface->h};
        SDL_RenderCopy(renderer, noTexture, NULL, &noTextRect);
        SDL_FreeSurface(noSurface);
        SDL_DestroyTexture(noTexture);
    }
}





        // Update screen
        SDL_RenderPresent(renderer);
    }
    SDL_StopTextInput();

    // Cleanup
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    SDL_DestroyTexture(logoTexture);
    IMG_Quit();

}

// Forward declaration for analysis command processing
void processAnalysisCommand(CircuitManager& circuit, const string& command);

bool processCommand(CircuitManager& circuit, const string& command) {
    istringstream iss(command);
    string cmd;
    iss >> cmd;

    try {
        if (cmd == "add") {
            string compToken;
            iss >> compToken;
            if (compToken.empty()) {
                cout << "Error: Syntax error" << endl;
                return false;
            }

            if (compToken == "GND") {
                string node;
                if (!(iss >> node)) {
                    cout << "Error: Syntax error. Usage: add GND <node>" << endl;
                    return false;
                }
                circuit.addGround(node);
                return false;
            }

            if (!compToken.empty() && (compToken[0] == 'I' || compToken[0] == 'i')) {
                string node1, node2, valueToken;
                if (!(iss >> node1 >> node2 >> valueToken)) {
                    cout << "Error: Syntax error" << endl;
                    return false;
                }
                double value = parseValue(valueToken);
                if (value <= 0) {
                    cout << "Error: Current cannot be zero or negative" << endl;
                    return false;
                }
                circuit.addComponent(make_unique<CurrentSource>(compToken, node1, node2, value));
                return false;
            }

            if (compToken.rfind("CurrentSource", 0) == 0) {
                string node1, node2, valueToken;
                if (!(iss >> node1 >> node2 >> valueToken)) {
                    cout << "Error: Syntax error" << endl;
                    return false;
                }
                double value = parseValue(valueToken);
                if (value <= 0) {
                    cout << "Error: Current cannot be zero or negative" << endl;
                    return false;
                }
                circuit.addComponent(make_unique<CurrentSource>(compToken, node1, node2, value));
                return false;
            }

            char typeChar = compToken[0];
            string name = compToken;
            string node1, node2, valueToken;

            if (!(iss >> node1 >> node2)) {
                cout << "Error: Syntax error" << endl;
                return false;
            }

            if (typeChar == 'R') {
                if (!(iss >> valueToken)) {
                    cout << "Error: Syntax error" << endl;
                    return false;
                }
                double r = parseValue(valueToken);
                if (r <= 0) {
                    cout << "Error: Resistance cannot be zero or negative" << endl;
                    return false;
                }
                circuit.addComponent(make_unique<Resistor>(name, node1, node2, r));
            }
            else if (typeChar == 'C') {
                if (!(iss >> valueToken)) {
                    cout << "Error: Syntax error" << endl;
                    return false;
                }
                double c = parseValue(valueToken);
                if (c <= 0) {
                    cout << "Error: Capacitance cannot be zero or negative" << endl;
                    return false;
                }
                circuit.addComponent(make_unique<Capacitor>(name, node1, node2, c));
            }
            else if (typeChar == 'L') {
                if (!(iss >> valueToken)) {
                    cout << "Error: Syntax error" << endl;
                    return false;
                }
                double l = parseValue(valueToken);
                if (l <= 0) {
                    cout << "Error: Inductance cannot be zero or negative" << endl;
                    return false;
                }
                circuit.addComponent(make_unique<Inductor>(name, node1, node2, l));
            }
            else if (typeChar == 'D') {
                double vf = 0.7;
                if (!(iss >> valueToken)) {
                    // Default value
                } else if (valueToken != "D") {
                    try {
                        vf = stod(valueToken);
                        if (vf <= 0) throw invalid_argument("invalid");
                    } catch (...) {
                        cout << "Error: Model " << valueToken << " not found in library" << endl;
                        return false;
                    }
                }
                circuit.addComponent(make_unique<IdealDiode>(name, node1, node2, vf));
            }
            else if (typeChar == 'V') {
                string typeStr;
                getline(iss, typeStr);
                typeStr.erase(0, typeStr.find_first_not_of(" \t"));
                typeStr.erase(typeStr.find_last_not_of(" \t") + 1);

                if (typeStr.rfind("SIN", 0) == 0) {
                    regex sin_pattern(R"(SIN\(\s*([\w.]+)\s+([\w.]+)\s+([\w.]+)\s*\))");
                    smatch matches;
                    if (regex_search(typeStr, matches, sin_pattern)) {
                        double offset = parseValue(matches[1].str());
                        double amplitude = parseValue(matches[2].str());
                        double frequency = parseValue(matches[3].str());
                        circuit.addComponent(make_unique<SinusoidalSource>(name, node1, node2, offset, amplitude, frequency));
                    } else {
                        cout << "Error: Invalid SIN parameters format" << endl;
                        return false;
                    }
                }
                else if (typeStr.rfind("PULSE", 0) == 0) {
                    regex pulse_pattern(R"(PULSE\(\s*([\w.]+)\s+([\w.]+)\s+([\w.]+)\s+([\w.]+)\s+([\w.]+)\s+([\w.]+)\s+([\w.]+)\s*\))");
                    smatch matches;
                    if (regex_search(typeStr, matches, pulse_pattern)) {
                        double v1 = parseValue(matches[1].str());
                        double v2 = parseValue(matches[2].str());
                        double td = parseValue(matches[3].str());
                        double tr = parseValue(matches[4].str());
                        double tf = parseValue(matches[5].str());
                        double pw = parseValue(matches[6].str());
                        double per = parseValue(matches[7].str());
                        circuit.addComponent(make_unique<PulseVoltageSource>(name, node1, node2, v1, v2, td, tr, tf, pw, per));
                    } else {
                        cout << "Error: Invalid PULSE parameters format" << endl;
                        return false;
                    }
                }
                else {
                    double value = parseValue(typeStr);
                    if (value <= 0) {
                        cout << "Error: Voltage cannot be zero or negative" << endl;
                        return false;
                    }
                    circuit.addComponent(make_unique<VoltageSource>(name, node1, node2, value));
                }
            }
            else if (typeChar == 'E') {
                string ctrlNode1, ctrlNode2, gainToken;
                if (!(iss >> ctrlNode1 >> ctrlNode2 >> gainToken)) {
                    cout << "Error: Syntax error for VCVS" << endl;
                    return false;
                }
                double gain = parseValue(gainToken);
                circuit.addComponent(make_unique<VCVS>(name, node1, node2, ctrlNode1, ctrlNode2, gain));
            }
            else if (typeChar == 'G') {
                string ctrlNode1, ctrlNode2, transToken;
                if (!(iss >> ctrlNode1 >> ctrlNode2 >> transToken)) {
                    cout << "Error: Syntax error for VCCS" << endl;
                    return false;
                }
                double trans = parseValue(transToken);
                circuit.addComponent(make_unique<VCCS>(name, node1, node2, ctrlNode1, ctrlNode2, trans));
            }
            else if (typeChar == 'H') {
                string vname, gainToken;
                if (!(iss >> vname >> gainToken)) {
                    cout << "Error: Syntax error for CCVS" << endl;
                    return false;
                }
                double gain = parseValue(gainToken);
                circuit.addComponent(make_unique<CCVS>(name, node1, node2, vname, gain));
            }
            else if (typeChar == 'F') {
                string vname, gainToken;
                if (!(iss >> vname >> gainToken)) {
                    cout << "Error: Syntax error for CCCS" << endl;
                    return false;
                }
                double gain = parseValue(gainToken);
                circuit.addComponent(make_unique<CCCS>(name, node1, node2, vname, gain));
            }
            else {
                cout << "Error: Element " << name << " not found in library" << endl;
                return false;
            }
        }
        else if (cmd == "load") {
            string filename;
            if (!(iss >> filename)) {
                cout << "Error: Syntax error. Usage: load <filename>" << endl;
                return false;
            }
            circuit.loadFromFile(filename);
        }
        else if (cmd == "save") {
            string filename;
            if (!(iss >> filename)) {
                cout << "Error: Syntax error. Usage: save <filename>" << endl;
                return false;
            }
            circuit.saveToFile(filename);

            // Extract simple name from path
            string simpleName = filename;
            size_t lastSlash = simpleName.find_last_of("/\\");
            if (lastSlash != string::npos) {
                simpleName = simpleName.substr(lastSlash + 1);
            }
            size_t dotPos = simpleName.find_last_of('.');
            if (dotPos != string::npos) {
                simpleName = simpleName.substr(0, dotPos);
            }

            // Save schematic to user list
            userSchematics[simpleName] = circuit.getNetlist();

            // Add to name lists if not exists
            if (find(allSchematicNames.begin(), allSchematicNames.end(), simpleName) == allSchematicNames.end()) {
                allSchematicNames.push_back(simpleName);
                userSchematicNames.push_back(simpleName);
            }

            cout << "Circuit saved to " << filename
                 << " and added to schematics as '" << simpleName << "'" << endl;
        }
        else if (cmd == "delete") {
            string token;
            iss >> token;
            if (token.empty()) {
                cout << "Error: Syntax error" << endl;
                return false;
            }
            if (token == "GND") {
                string node;
                if (!(iss >> node)) {
                    cout << "Error: Syntax error. Usage: delete GND <node>" << endl;
                    return false;
                }
                circuit.deleteGround(node);
            } else {
                circuit.deleteComponent(token);
            }
        }
        else if (cmd == "rename") {
            string subcmd, oldName, newName;
            iss >> subcmd >> oldName >> newName;
            if (subcmd != "node" || oldName.empty() || newName.empty()) {
                cout << "ERROR: Invalid syntax - correct format:" << endl;
                cout << "rename node <old_name> <new_name>" << endl;
                return false;
            }
            circuit.renameNode(oldName, newName);
        }
        else if (cmd == "nodes") {
            circuit.listNodes();
        }
        else if (cmd == "list") {
            string typeFilter;
            iss >> typeFilter;

            if (!typeFilter.empty()) {
                if (typeFilter.front() == '[') {
                    typeFilter = typeFilter.substr(1);
                }
                if (typeFilter.back() == ']') {
                    typeFilter.pop_back();
                }
            }

            circuit.listComponents(typeFilter);
        }
        else if (cmd == "print") {
            circuit.printAll();
        }
        else if (cmd == "clear") {
            circuit.clearAll();
        }
        else if (cmd == "show") {
            string subcmd;
            iss >> subcmd;
            if (subcmd == "existing" || subcmd == "schematics") {
                cout << "Existing schematics:" << endl;
                cout << "====================" << endl;

                // Predefined schematics
                cout << "Predefined Schematics:" << endl;
                int idx = 1;
                for (const auto& kv : predefinedSchematics) {
                    cout << idx++ << "-" << kv.first << endl;
                }

                // User schematics
                cout << "\nUser Schematics:" << endl;
                for (const auto& name : userSchematicNames) {
                    cout << idx++ << "-" << name << endl;
                }


                // Schematic selection loop
                string innerCmd;
                while (true) {
                    cout << "\n(schematics) Enter schematic number to view, 'return' to go back, or 'exit': ";
                    getline(cin, innerCmd);

                    if (innerCmd == "return") {
                        cout << "Returning to main menu..." << endl;
                        break;
                    }
                    else if (innerCmd == "exit") {
                        return true; // Exit program
                    }
                    else if (!innerCmd.empty()) {
                        // Check if input is numeric
                        bool isNumber = true;
                        for (char c : innerCmd) {
                            if (!isdigit(c)) {
                                isNumber = false;
                                break;
                            }
                        }

                        if (isNumber) {
                            int index = stoi(innerCmd);
                            if (index >= 1 && index <= allSchematicNames.size()) {
                                string name = allSchematicNames[index-1];
                                // Display schematic netlist
                                auto it_pre = predefinedSchematics.find(name);
                                auto it_user = userSchematics.find(name);
                                if (it_pre != predefinedSchematics.end()) {
                                    cout << "\nSchematic: " << name << "\n" << it_pre->second << endl;
                                } else if (it_user != userSchematics.end()) {
                                    cout << "\nSchematic: " << name << "\n" << it_user->second << endl;
                                } else {
                                    cout << "Error: Schematic not found." << endl;
                                }
                            } else {
                                cout << "Error: Inappropriate input. Index out of range." << endl;
                            }
                        } else {
                            // Search by name
                            if (find(allSchematicNames.begin(), allSchematicNames.end(), innerCmd) != allSchematicNames.end()) {
                                auto it_pre = predefinedSchematics.find(innerCmd);
                                auto it_user = userSchematics.find(innerCmd);
                                if (it_pre != predefinedSchematics.end()) {
                                    cout << "\nSchematic: " << innerCmd << "\n" << it_pre->second << endl;
                                } else if (it_user != userSchematics.end()) {
                                    cout << "\nSchematic: " << innerCmd << "\n" << it_user->second << endl;
                                } else {
                                    cout << "Error: Schematic not found." << endl;
                                }
                            } else {
                                cout << "Error: Inappropriate input. Unknown schematic name." << endl;
                            }
                        }
                    }
                    else {
                        cout << "Error: Inappropriate input. Please enter a valid command." << endl;
                    }
                }
            } else {
                cout << "Invalid command. Use 'show schematics' to list existing schematics." << endl;
            }
        }
        else if (cmd == "choose") {
            string arg;
            if (iss >> arg) {
                if (isdigit(arg[0])) {
                    int index = stoi(arg);
                    if (index >= 1 && index <= allSchematicNames.size()) {
                        string name = allSchematicNames[index-1];

                        // Search predefined schematics
                        auto it_pre = predefinedSchematics.find(name);
                        if (it_pre != predefinedSchematics.end()) {
                            cout << "Schematic chosen: " << name << endl;
                            cout << it_pre->second;
                        }
                        // Search user schematics
                        else {
                            auto it_user = userSchematics.find(name);
                            if (it_user != userSchematics.end()) {
                                cout << "Schematic chosen: " << name << endl;
                                cout << it_user->second;
                            } else {
                                cout << "Error: Schematic not found." << endl;
                            }
                        }
                    } else {
                        cout << "Error: Invalid schematic index." << endl;
                    }
                } else {
                    // Search by name
                    auto it_pre = predefinedSchematics.find(arg);
                    if (it_pre != predefinedSchematics.end()) {
                        cout << "Schematic chosen: " << arg << endl;
                        cout << it_pre->second;
                    } else {
                        auto it_user = userSchematics.find(arg);
                        if (it_user != userSchematics.end()) {
                            cout << "Schematic chosen: " << arg << endl;
                            cout << it_user->second;
                        } else {
                            cout << "Error: Schematic '" << arg << "' not found." << endl;
                        }
                    }
                }
            } else {
                cout << "Error: 'choose' requires an argument." << endl;
            }
        }
        else if (cmd == "NewFile") {
            string file_path;
            if (iss >> file_path) {
                circuit.saveToFile(file_path);

                // Extract simple name from path
                string simpleName = file_path;
                size_t lastSlash = simpleName.find_last_of("/\\");
                if (lastSlash != string::npos) {
                    simpleName = simpleName.substr(lastSlash + 1);
                }
                size_t dotPos = simpleName.find_last_of('.');
                if (dotPos != string::npos) {
                    simpleName = simpleName.substr(0, dotPos);
                }

                // Save schematic to user list
                userSchematics[simpleName] = circuit.getNetlist();

                // Add to name lists if not exists
                if (find(allSchematicNames.begin(), allSchematicNames.end(), simpleName) == allSchematicNames.end()) {
                    allSchematicNames.push_back(simpleName);
                    userSchematicNames.push_back(simpleName);
                }

                cout << "Circuit saved to " << file_path
                     << " and added to schematics as '" << simpleName << "'" << endl;
            } else {
                cout << "Error: 'NewFile' requires a file path." << endl;
            }
        }
        else if (cmd == "delete_schematic") {
            string name;
            if (iss >> name) {
                // Delete from user schematics
                if (userSchematics.find(name) != userSchematics.end()) {
                    userSchematics.erase(name);

                    // Remove from name lists
                    auto it = find(userSchematicNames.begin(), userSchematicNames.end(), name);
                    if (it != userSchematicNames.end()) {
                        userSchematicNames.erase(it);
                    }

                    auto it2 = find(allSchematicNames.begin(), allSchematicNames.end(), name);
                    if (it2 != allSchematicNames.end()) {
                        allSchematicNames.erase(it2);
                    }

                    cout << "Schematic '" << name << "' deleted successfully." << endl;
                } else {
                    cout << "Error: User schematic '" << name << "' not found." << endl;
                }
            } else {
                cout << "Error: Please specify schematic name." << endl;
            }
        }
        else if (cmd == "exit") {
            return true;
        }
        else {
            cout << "Error: Syntax error" << endl;
        }
    }
    catch (const exception& e) {
        cout << e.what() << endl;
    }
    return false;
}

// Gaussian elimination solver
bool gaussianElimination(vector<vector<double>>& A, vector<double>& b, vector<double>& x) {
    int n = A.size();
    if ((int)b.size() != n) return false;

    for (int i = 0; i < n; i++) {
        double maxVal = fabs(A[i][i]);
        int pivotRow = i;
        for (int k = i + 1; k < n; k++) {
            if (fabs(A[k][i]) > maxVal) {
                maxVal = fabs(A[k][i]);
                pivotRow = k;
            }
        }
        if (fabs(maxVal) < 1e-14) {
            return false;
        }
        if (pivotRow != i) {
            swap(A[i], A[pivotRow]);
            swap(b[i], b[pivotRow]);
        }
        for (int k = i + 1; k < n; k++) {
            double factor = A[k][i] / A[i][i];
            for (int j = i; j < n; j++) {
                A[k][j] -= factor * A[i][j];
            }
            b[k] -= factor * b[i];
        }
    }

    x.assign(n, 0.0);
    for (int i = n - 1; i >= 0; i--) {
        double sum = b[i];
        for (int j = i + 1; j < n; j++) {
            sum -= A[i][j] * x[j];
        }
        x[i] = sum / A[i][i];
    }
    return true;
}

// DC solver for resistive circuits
bool solveDC(CircuitManager& circuit,
             unordered_map<string,int>& nodeIndex,
             vector<double>& nodeVoltages,
             unordered_map<string,double>& sourceCurrents)
{
    int idx = 0;
    for (auto& kv : circuit.nodeToComponents) {
        const string& node = kv.first;
        if (node == "GND" || circuit.grounds.count(node)) continue;
        nodeIndex[node] = idx++;
    }
    int N = idx;
    int countVsrc = 0;
    for (auto& kv : circuit.componentMap) {
        if (kv.second->type == ComponentType::VOLTAGE_SOURCE) countVsrc++;
    }
    int M = countVsrc;
    int dim = N + M;

    vector<vector<double>> A(dim, vector<double>(dim, 0.0));
    vector<double> b(dim, 0.0);

    int vsrcIdx = 0;
    for (auto& kv : circuit.componentMap) {
        CircuitComponent* c = kv.second;
        const string& name = c->name;
        const string& n1 = c->node1;
        const string& n2 = c->node2;
        double val = c->value;

        auto getNode = [&](const string& nd)->int {
            if (nd == "GND" || circuit.grounds.count(nd)) return -1;
            return nodeIndex[nd];
        };
        int i = getNode(n1);
        int j = getNode(n2);

        switch (c->type) {
            case ComponentType::RESISTOR: {
                double G = 1.0 / val;
                if (i >= 0) A[i][i] += G;
                if (j >= 0) A[j][j] += G;
                if (i >= 0 && j >= 0) {
                    A[i][j] -= G;
                    A[j][i] -= G;
                }
                break;
            }
            case ComponentType::CURRENT_SOURCE: {
                if (i >= 0) b[i] -= val;
                if (j >= 0) b[j] += val;
                break;
            }
            case ComponentType::VOLTAGE_SOURCE: {
                int p = N + (vsrcIdx++);
                if (i >= 0) {
                    A[i][p] = 1.0;
                    A[p][i] = 1.0;
                }
                if (j >= 0) {
                    A[j][p] = -1.0;
                    A[p][j] = -1.0;
                }
                b[p] = val;
                break;
            }
            default:
                break;
        }
    }

    vector<double> x(dim, 0.0);
    bool ok = gaussianElimination(A, b, x);
    if (!ok) return false;

    nodeVoltages.assign(N, 0.0);
    vsrcIdx = 0;
    for (auto& kv : nodeIndex) {
        nodeVoltages[kv.second] = x[kv.second];
    }
    for (auto& kv : circuit.componentMap) {
        if (kv.second->type == ComponentType::VOLTAGE_SOURCE) {
            sourceCurrents[kv.first] = x[N + (vsrcIdx++)];
        }
    }
    return true;
}
double getPulseValue(double t, double v1, double v2, double td, double tr, double tf, double pw, double per) {
    if (t < td) return v1;
    double tmod = fmod(t - td, per);
    if (tmod < tr)
        return v1 + (v2 - v1) * (tmod / tr);  // rise
    else if (tmod < tr + pw)
        return v2;                           // high
    else if (tmod < tr + pw + tf)
        return v2 - (v2 - v1) * ((tmod - tr - pw) / tf);  // fall
    else
        return v1;                           // low
}

// Transient solver with Backward Euler
bool solveTransient(CircuitManager& circuit,
                    double h,
                    int steps,
                    const vector<pair<char,string>>& requestVars)
{
    unordered_map<string,int> nodeIndex;
    int idx = 0;
    for (auto& kv : circuit.nodeToComponents) {
        const string& node = kv.first;
        if (node == "GND" || circuit.grounds.count(node)) continue;
        nodeIndex[node] = idx++;
    }
    int N = idx;

    int countVsrc = 0, countL = 0;
    for (auto& kv : circuit.componentMap) {
        ComponentType t = kv.second->type;
        if (t == ComponentType::VOLTAGE_SOURCE || t == ComponentType::PULSE_VOLTAGE_SOURCE)
            countVsrc++;
        if (t == ComponentType::INDUCTOR)
            countL++;
    }
    int M = countVsrc + countL;
    int dim = N + M;

    vector<double> Vprev(N, 0.0);
    unordered_map<string,double> ILprev, I_Vsrc_prev;
    for (auto& kv : circuit.componentMap) {
        if (kv.second->type == ComponentType::INDUCTOR) ILprev[kv.first] = 0.0;
        if (kv.second->type == ComponentType::VOLTAGE_SOURCE || kv.second->type == ComponentType::PULSE_VOLTAGE_SOURCE)
            I_Vsrc_prev[kv.first] = 0.0;
    }

    for (int step = 0; step < steps; step++) {
        double t = (step + 1) * h;

        vector<vector<double>> A_mat(dim, vector<double>(dim, 0.0));
        vector<double> b_vec(dim, 0.0);
        vector<double> x(dim, 0.0);
        vector<double> Vlast = Vprev; // ← کلید اصلی برای جریان خازن

        unordered_map<string,int> vsrcIndex, LIndex;
        int base = N;
        for (auto& kv : circuit.componentMap) {
            if (kv.second->type == ComponentType::VOLTAGE_SOURCE || kv.second->type == ComponentType::PULSE_VOLTAGE_SOURCE)
                vsrcIndex[kv.first] = base++;
        }
        for (auto& kv : circuit.componentMap) {
            if (kv.second->type == ComponentType::INDUCTOR)
                LIndex[kv.first] = base++;
        }

        auto getNode = [&](const string& nd)->int {
            if (nd == "GND" || circuit.grounds.count(nd)) return -1;
            return nodeIndex[nd];
        };

        for (auto& kv : circuit.componentMap) {
            CircuitComponent* c = kv.second;
            const string& name = c->name;
            int i = getNode(c->node1);
            int j = getNode(c->node2);

            switch (c->type) {
                case ComponentType::RESISTOR: {
                    double G = 1.0 / c->value;
                    if (i >= 0) A_mat[i][i] += G;
                    if (j >= 0) A_mat[j][j] += G;
                    if (i >= 0 && j >= 0) {
                        A_mat[i][j] -= G;
                        A_mat[j][i] -= G;
                    }
                    break;
                }
                case ComponentType::CURRENT_SOURCE: {
                    if (i >= 0) b_vec[i] -= c->value;
                    if (j >= 0) b_vec[j] += c->value;
                    break;
                }
                case ComponentType::CAPACITOR: {
                    double G_C = c->value / h;
                    if (i >= 0) A_mat[i][i] += G_C;
                    if (j >= 0) A_mat[j][j] += G_C;
                    if (i >= 0 && j >= 0) {
                        A_mat[i][j] -= G_C;
                        A_mat[j][i] -= G_C;
                    }
                    if (i >= 0) b_vec[i] += G_C * Vprev[i];
                    if (j >= 0) b_vec[j] -= G_C * Vprev[j];
                    break;
                }
                case ComponentType::INDUCTOR: {
                    int p = LIndex[name];
                    double coeff = -(c->value / h);
                    if (i >= 0) {
                        A_mat[i][p] = 1.0;
                        A_mat[p][i] = 1.0;
                    }
                    if (j >= 0) {
                        A_mat[j][p] = -1.0;
                        A_mat[p][j] = -1.0;
                    }
                    A_mat[p][p] = coeff;
                    b_vec[p] = -coeff * ILprev[name];
                    break;
                }
                case ComponentType::VOLTAGE_SOURCE: {
                    int p = vsrcIndex[name];
                    if (i >= 0) {
                        A_mat[i][p] = 1.0;
                        A_mat[p][i] = 1.0;
                    }
                    if (j >= 0) {
                        A_mat[j][p] = -1.0;
                        A_mat[p][j] = -1.0;
                    }
                    b_vec[p] = c->value;
                    break;
                }
                case ComponentType::PULSE_VOLTAGE_SOURCE: {
                    int p = vsrcIndex[name];
                    PulseVoltageSource* pulse = static_cast<PulseVoltageSource*>(c);
                    double Vpulse = getPulseValue(t, pulse->v1, pulse->v2, pulse->td, pulse->tr, pulse->tf, pulse->pw, pulse->per);
                    if (i >= 0) {
                        A_mat[i][p] = 1.0;
                        A_mat[p][i] = 1.0;
                    }
                    if (j >= 0) {
                        A_mat[j][p] = -1.0;
                        A_mat[p][j] = -1.0;
                    }
                    b_vec[p] = Vpulse;
                    break;
                }
                default:
                    break;
            }
        }

        if (!gaussianElimination(A_mat, b_vec, x)) {
            cout << "Error: Matrix singular at time " << t << "\n";
            return false;
        }

        for (auto& kv : nodeIndex)
            Vprev[kv.second] = x[kv.second];

        int vsCnt = 0;
        for (auto& kv : circuit.componentMap) {
            if (kv.second->type == ComponentType::VOLTAGE_SOURCE || kv.second->type == ComponentType::PULSE_VOLTAGE_SOURCE)
                I_Vsrc_prev[kv.first] = x[N + (vsCnt++)];
        }

        int lCnt = 0;
        for (auto& kv : circuit.componentMap) {
            if (kv.second->type == ComponentType::INDUCTOR)
                ILprev[kv.first] = x[N + countVsrc + (lCnt++)];
        }

        cout << "t = " << setw(10) << t << " s:\n";
        for (auto& vr : requestVars) {
            char kind = vr.first;
            const string& nm = vr.second;
            if (kind == 'V') {
                int id = nodeIndex[nm];
                cout << "  V(" << nm << ") = " << setw(10) << Vprev[id] << " V\n";
            } else {
                CircuitComponent* c = circuit.componentMap[nm];
                int i = (c->node1 == "GND" ? -1 : nodeIndex[c->node1]);
                int j = (c->node2 == "GND" ? -1 : nodeIndex[c->node2]);
                double V1 = (i < 0 ? 0.0 : Vprev[i]);
                double V2 = (j < 0 ? 0.0 : Vprev[j]);
                double I = 0.0;

                switch (c->type) {
                    case ComponentType::RESISTOR:
                        I = (V1 - V2) / c->value;
                        break;
                    case ComponentType::CAPACITOR: {
                        double Vp_i = (i < 0 ? 0.0 : Vlast[i]);
                        double Vp_j = (j < 0 ? 0.0 : Vlast[j]);
                        I = c->value * ((V1 - V2) - (Vp_i - Vp_j)) / h;
                        break;
                    }
                    case ComponentType::INDUCTOR:
                        I = ILprev[nm];
                        break;
                    case ComponentType::CURRENT_SOURCE:
                        I = c->value;
                        break;
                    case ComponentType::VOLTAGE_SOURCE:
                    case ComponentType::PULSE_VOLTAGE_SOURCE:
                        I = I_Vsrc_prev[nm];
                        break;
                    default:
                        cout << "  I(" << nm << ") not supported\n";
                        continue;
                }
                cout << "  I(" << nm << ") = " << setw(10) << I << " A\n";
            }
        }
        cout << endl;
    }

    return true;
}

// Parse variable tokens for analysis commands
bool parseVariableToken(const string& str, char &varType, string& varName) {
    static const regex pattern(R"(^([VI])\(\s*([A-Za-z0-9_]+)\s*\)$)");
    smatch m;
    if (regex_match(str, m, pattern)) {
        varType = m[1].str()[0];
        varName = m[2].str();
        return true;
    }
    return false;
}

// Process analysis commands (TRAN, DC)
void processAnalysisCommand(CircuitManager& circuit, const string& command) {
    istringstream iss(command);
    string firstToken;
    iss >> firstToken;
    string firstTokenLower = firstToken;
    transform(firstTokenLower.begin(), firstTokenLower.end(), firstTokenLower.begin(), ::tolower);

    if (firstTokenLower == "print" || firstTokenLower == ".print") {
        string secondToken;
        if (!(iss >> secondToken)) {
            cout << "Syntax error in command\n";
            return;
        }
        string secondTokenLower = secondToken;
        transform(secondTokenLower.begin(), secondTokenLower.end(), secondTokenLower.begin(), ::tolower);

        if (secondTokenLower == "tran") {
            double Tstep, Tstop, Tstart = 0.0, Tmaxstep = 0.0;
            if (!(iss >> Tstep >> Tstop)) {
                cout << "Syntax error in command\n";
                return;
            }
            vector<double> optionalNums;
            string maybe;
            while (iss.peek() != EOF) {
                if (iss >> maybe) {
                    try {
                        double val = stod(maybe);
                        optionalNums.push_back(val);
                        continue;
                    } catch (...) {
                        iss.clear();
                        iss.seekg(-static_cast<int>(maybe.size()), ios_base::cur);
                        break;
                    }
                }
            }
            if (optionalNums.size() > 2) {
                cout << "Syntax error in command\n";
                return;
            }
            if (optionalNums.size() >= 1) Tstart = optionalNums[0];
            if (optionalNums.size() == 2) Tmaxstep = optionalNums[1];

            vector<pair<char,string>> requestVars;
            string varToken;
            while (iss >> varToken) {
                char varType;
                string varName;
                if (!parseVariableToken(varToken, varType, varName)) {
                    cout << "Syntax error in command\n";
                    return;
                }
                if (varType == 'V') {
                    if (!circuit.hasNode(varName)) {
                        cout << "Node " << varName << " not found in circuit\n";
                        return;
                    }
                } else {
                    if (!circuit.hasComponent(varName)) {
                        cout << "Component " << varName << " not found in circuit\n";
                        return;
                    }
                }
                requestVars.emplace_back(varType, varName);
            }

            int steps = static_cast<int>(Tstop / Tstep);
            cout << "Starting transient analysis (TRAN):\n";
            if (!solveTransient(circuit, Tstep, steps, requestVars)) {
                cout << "Error: Transient solver failed\n";
            }
        }
        else if (secondTokenLower == "dc") {
            string sourceName;
            double startVal, endVal, incVal;
            if (!(iss >> sourceName >> startVal >> endVal >> incVal)) {
                cout << "Syntax error in command\n";
                return;
            }
            vector<pair<char,string>> requestVars;
            string varToken;
            while (iss >> varToken) {
                char varType;
                string varName;
                if (!parseVariableToken(varToken, varType, varName)) {
                    cout << "Syntax error in command\n";
                    return;
                }
                if (varType == 'V') {
                    if (!circuit.hasNode(varName)) {
                        cout << "Node " << varName << " not found in circuit\n";
                        return;
                    }
                } else {
                    if (!circuit.hasComponent(varName)) {
                        cout << "Component " << varName << " not found in circuit\n";
                        return;
                    }
                }
                requestVars.emplace_back(varType, varName);
            }

            cout << "Starting DC sweep (DC) on source " << sourceName
                 << " from " << startVal << " to " << endVal << " step " << incVal << "\n";

            unordered_map<string,int> nodeIndex;
            int idx = 0;
            for (auto& kv : circuit.nodeToComponents) {
                const string& node = kv.first;
                if (node == "GND" || circuit.grounds.count(node)) continue;
                nodeIndex[node] = idx++;
            }
            int N = idx;
            int countVsrc = 0;
            for (auto& kv : circuit.componentMap) {
                if (kv.second->type == ComponentType::VOLTAGE_SOURCE) countVsrc++;
            }
            int M = countVsrc;
            int dim = N + M;

            for (double vs = startVal; vs <= endVal + 1e-12; vs += incVal) {
                vector<vector<double>> A(dim, vector<double>(dim, 0.0));
                vector<double> b(dim, 0.0);

                unordered_map<string,int> vsrcIndex;
                int base = N;
                for (auto& kv : circuit.componentMap) {
                    if (kv.second->type == ComponentType::VOLTAGE_SOURCE) {
                        vsrcIndex[kv.first] = base++;
                    }
                }

                auto getNode = [&](const string& nd)->int {
                    if (nd == "GND" || circuit.grounds.count(nd)) return -1;
                    return nodeIndex[nd];
                };

                for (auto& kv : circuit.componentMap) {
                    CircuitComponent* c = kv.second;
                    const string& name = c->name;
                    const string& n1 = c->node1;
                    const string& n2 = c->node2;
                    int i = getNode(n1);
                    int j = getNode(n2);

                    switch (c->type) {
                        case ComponentType::RESISTOR: {
                            double G = 1.0 / c->value;
                            if (i >= 0) A[i][i] += G;
                            if (j >= 0) A[j][j] += G;
                            if (i >= 0 && j >= 0) {
                                A[i][j] -= G;
                                A[j][i] -= G;
                            }
                            break;
                        }
                        case ComponentType::CURRENT_SOURCE: {
                            if (i >= 0) b[i] -= c->value;
                            if (j >= 0) b[j] += c->value;
                            break;
                        }
                        case ComponentType::VOLTAGE_SOURCE: {
                            int p = vsrcIndex[name];
                            if (i >= 0) {
                                A[i][p] = 1.0;
                                A[p][i] = 1.0;
                            }
                            if (j >= 0) {
                                A[j][p] = -1.0;
                                A[p][j] = -1.0;
                            }
                            b[p] = vs;
                            break;
                        }
                        default:
                            break;
                    }
                }

                vector<double> x(dim, 0.0);
                bool ok = gaussianElimination(A, b, x);
                if (!ok) {
                    cout << "Error: Singular matrix at DC=" << vs << "\n";
                    continue;
                }

                vector<double> Vvec(N);
                for (auto& kv : nodeIndex) {
                    Vvec[kv.second] = x[kv.second];
                }
                unordered_map<string,double> I_Vsrc;
                int vsCnt = 0;
                for (auto& kv : circuit.componentMap) {
                    if (kv.second->type == ComponentType::VOLTAGE_SOURCE) {
                        I_Vsrc[kv.first] = x[N + (vsCnt++)];
                    }
                }

                cout << "DC = " << vs << " V:\n";
                for (auto& vr : requestVars) {
                    char kind = vr.first;
                    const string& nm = vr.second;
                    if (kind == 'V') {
                        if (!circuit.hasNode(nm)) {
                            cout << "  Node " << nm << " not found\n";
                        } else {
                            int id = nodeIndex[nm];
                            cout << "  V(" << nm << ") = " << setw(10) << Vvec[id] << " V\n";
                        }
                    } else {
                        if (!circuit.hasComponent(nm)) {
                            cout << "  Component " << nm << " not found\n";
                        } else {
                            CircuitComponent* c = circuit.componentMap[nm];
                            if (c->type == ComponentType::RESISTOR) {
                                int i = (c->node1 == "GND" ? -1 : nodeIndex[c->node1]);
                                int j = (c->node2 == "GND" ? -1 : nodeIndex[c->node2]);
                                double V1 = (i < 0 ? 0.0 : Vvec[i]);
                                double V2 = (j < 0 ? 0.0 : Vvec[j]);
                                double I = (V1 - V2)/c->value;
                                cout << "  I(" << nm << ") = " << setw(10) << I << " A\n";
                            }
                            else if (c->type == ComponentType::CURRENT_SOURCE) {
                                cout << "  I(" << nm << ") = " << setw(10) << c->value << " A\n";
                            }
                            else if (c->type == ComponentType::VOLTAGE_SOURCE) {
                                cout << "  I(" << nm << ") = " << setw(10) << I_Vsrc[nm] << " A\n";
                            }
                            else {
                                cout << "  I(" << nm << ") not supported\n";
                            }
                        }
                    }
                }
                cout << endl;
            }
        }
        else {
            cout << "Syntax error in command\n";
        }
    }
    else {
        cout << "Syntax error in command\n";
    }
}

int main(int argc, char* argv[]) {
    IMG_Init(IMG_INIT_PNG);
    CircuitManager circuit;
    string command;
    bool shouldExit = false;
    showAnalysisSelectionGUI(allSchematicNames, circuit);
    while (!shouldExit) {
        cout << "> ";
        getline(cin, command);
        if (command == "exit") {
            break;
        }

        // Convert to lowercase for command routing
        string commandLower = command;
        transform(commandLower.begin(), commandLower.end(), commandLower.begin(),
                 [](unsigned char c) { return tolower(c); });

        // Route analysis commands to specialized handler
        if (commandLower.rfind("print tran", 0) == 0 ||
            commandLower.rfind("print dc", 0) == 0 ||
            commandLower.rfind(".print tran", 0) == 0 ||
            commandLower.rfind(".print dc", 0) == 0) {
            processAnalysisCommand(circuit, command);
        }
        // Route all other commands to general handler
        else {
            shouldExit = processCommand(circuit, command);
        }
    }

    return 0;
}