#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct BinaryImage {
    int W = 0;
    int H = 0;
    std::vector<uint8_t> ImageData;

    bool ReadFromPBM(const std::string& filename);
};

