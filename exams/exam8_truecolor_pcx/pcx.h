#pragma once

#include <string>
#include <cstdint>
#include "mat.h"
#include "types.h"

bool load_pcx(const std::string& filename, mat<vec3b>& img);
