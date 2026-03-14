#include "Config.h"
#include <fstream>

static void writeKV(std::ofstream& f, const char* k, float v) { f << k << "=" << v << "\n"; }
static void writeKV3(std::ofstream& f, const char* k, const glm::vec3& v) { f << k << "=" << v.x << "," << v.y << "," << v.z << "\n"; }
static bool readKV3(const std::string& line, const char* k, glm::vec3& out) {
    if (line.rfind(k, 0) != 0) return false;
    auto eq = line.find('=');
    auto c1 = line.find(',', eq + 1);
    auto c2 = line.find(',', c1 + 1);
    if (eq == std::string::npos || c1 == std::string::npos || c2 == std::string::npos) return false;
    out.x = std::stof(line.substr(eq + 1, c1 - eq - 1));
    out.y = std::stof(line.substr(c1 + 1, c2 - c1 - 1));
    out.z = std::stof(line.substr(c2 + 1));
    return true;
}
static bool readKV(const std::string& line, const char* k, float& out) {
    if (line.rfind(k, 0) != 0) return false;
    auto eq = line.find('=');
    if (eq == std::string::npos) return false;
    out = std::stof(line.substr(eq + 1));
    return true;
}

bool saveConfig(const std::string& path, const EmitterSettings& emitter, const GlobalParams& globals) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    writeKV3(f, "emitter.origin", emitter.origin);
    writeKV(f, "emitter.radius", emitter.radius);
    writeKV(f, "emitter.baseSize", emitter.baseSize);
    writeKV(f, "emitter.initialSpeedMin", emitter.initialSpeedMin);
    writeKV(f, "emitter.initialSpeedMax", emitter.initialSpeedMax);
    writeKV3(f, "globals.wind", globals.wind);
    writeKV(f, "globals.buoyancy", globals.buoyancy);
    writeKV(f, "globals.cooling", globals.cooling);
    writeKV(f, "globals.humidity", globals.humidity);
    writeKV(f, "globals.turbAmp", globals.turbAmp);
    writeKV(f, "globals.turbFreq", globals.turbFreq);
    return true;
}

bool loadConfig(const std::string& path, EmitterSettings& emitter, GlobalParams& globals) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        readKV3(line, "emitter.origin", emitter.origin) ||
        readKV(line, "emitter.radius", emitter.radius) ||
        readKV(line, "emitter.baseSize", emitter.baseSize) ||
        readKV(line, "emitter.initialSpeedMin", emitter.initialSpeedMin) ||
        readKV(line, "emitter.initialSpeedMax", emitter.initialSpeedMax) ||
        readKV3(line, "globals.wind", globals.wind) ||
        readKV(line, "globals.buoyancy", globals.buoyancy) ||
        readKV(line, "globals.cooling", globals.cooling) ||
        readKV(line, "globals.humidity", globals.humidity) ||
        readKV(line, "globals.turbAmp", globals.turbAmp) ||
        readKV(line, "globals.turbFreq", globals.turbFreq);
    }
    return true;
}
