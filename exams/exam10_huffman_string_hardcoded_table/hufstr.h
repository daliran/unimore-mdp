#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

class hufstr {
private:
    struct symbol_data {
        uint8_t sym = 0;
        uint8_t len = 0;
        uint32_t code = 0;
    };

    std::unordered_map<uint8_t, symbol_data> symbols_data_;
    std::vector<std::reference_wrapper<symbol_data>> sorted_symbol_data_;

public:
    hufstr();
    std::vector<uint8_t> compress(const std::string& s) const;
    std::string decompress(const std::vector<uint8_t>& v) const;
};