#pragma once
#include "Particles.h"
#include <string>

bool saveConfig(const std::string& path, const EmitterSettings& emitter, const GlobalParams& globals);
bool loadConfig(const std::string& path, EmitterSettings& emitter, GlobalParams& globals);
