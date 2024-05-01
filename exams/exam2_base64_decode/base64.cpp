#include "base64.h"
#include <unordered_map>
#include <cstdint>
#include <cmath>

static std::unordered_map<char, uint8_t> create_decoding_table() {

	std::unordered_map<char, uint8_t> table;

	for (char c = 'A'; c <= 'Z'; c++) {
		table[c] = c - 'A';
	}

	for (char c = 'a'; c <= 'z'; c++) {
		table[c] = c - 'a' + 26;
	}

	for (char c = '0'; c <= '9'; c++) {
		table[c] = c - '0' + 52;
	}

	table['+'] = 62;
	table['/'] = 63;

	return table;
}

static std::string decode_group(const std::string& group, const std::unordered_map<char, uint8_t>& table) {
	
	uint32_t group_bits = 0;

	int number_of_paddings = 0;

	for (char c : group) {

		if (c == '=') {
			group_bits <<= 6;
			group_bits |= 0;
			number_of_paddings++;
			continue;
		}

		auto it = table.find(c);

		if (it == table.end()) {
			continue;
		}

		group_bits <<= 6;
		group_bits |= it->second;
	}

	std::string decoded_group;

	for (int i = 0; i < 3 - number_of_paddings; ++i) {
		uint8_t value = (group_bits >> ((3 - i - 1) * 8)) & 0b11111111;
		decoded_group.push_back(static_cast<char>(value));
	}

	return decoded_group;
}

std::string base64_decode(const std::string& input) {

	if (input.empty()) {
		return std::string();
	}

	auto table = create_decoding_table();

	std::string decoded_string;

	std::string input_without_new_line = input;

	input_without_new_line.erase(std::find(input_without_new_line.begin(), input_without_new_line.end(), '\n'));

	for (size_t j = 0; j < input_without_new_line.length() / 4; ++j) {
		std::string group = input_without_new_line.substr(j * 4, 4);
		auto decoded_group = decode_group(group, table);
		decoded_string.append(decoded_group);
	}

	return decoded_string;
}