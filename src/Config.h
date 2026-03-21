#pragma once
#include "Particles.h"
#include <string>

bool saveConfig(const std::string& path, const EmitterSettings& emitter, const GlobalParams& globals); // Saves the emitter and global settings to a text file

bool loadConfig(const std::string& path, EmitterSettings& emitter, GlobalParams& globals); // Loads the emitter and global settings from a text file
