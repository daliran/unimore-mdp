#pragma once

#include <string>
#include <cstdint>
#include "mat.h"

bool load_pcx(const std::string& filename, mat<uint8_t>& img);
