#include "pgm16.h"
#include <fstream>

template<typename T>
std::pair<T, bool> raw_read(std::istream& input) {

	T buffer = 0;

	input.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));

	if (input.eof()) {
		return std::pair<T, bool>(buffer, false);
	}

	return std::pair<T, bool>(buffer, true);
}

static uint16_t tranform_to_big_endian(uint16_t number) {

	uint16_t buffer = 0;

	for (uint16_t i = 0; i < 8; ++i) {
		uint8_t shifts = 8 - 1 - i;
		bool bit = (number >> shifts) & 1;
		buffer |= (bit << (shifts + 8));
	}

	for (uint16_t i = 0; i < 8; ++i) {
		uint8_t shifts = 16 - 1 - i;
		bool bit = (number >> shifts) & 1;
		buffer |= (bit << (shifts - 8));
	}

	return buffer;
}

bool load(const std::string& filename, mat<uint16_t>& img, uint16_t& maxvalue) {

	std::ifstream input(filename, std::ios::binary);

	if (!input) {
		return false;
	}

	std::string magic_number;
	std::getline(input, magic_number);

	char is_comment = input.peek() == '#';

	if (is_comment) {
		std::string comment;
		std::getline(input, comment);
	}

	std::string raw_width;
	input >> raw_width;

	uint16_t width = std::stoi(raw_width);

	std::string raw_height;
	input >> raw_height;

	uint16_t height = std::stoi(raw_height);

	// Distard the new line
	input.ignore(1);

	std::string raw_max_value;
	std::getline(input, raw_max_value);

	maxvalue = std::stoi(raw_max_value);

	img.resize(height, width);

	// 0 = black, max_value = white
	for (uint16_t h = 0; h < height; ++h) {

		for (uint16_t w = 0; w < width; ++w) {

			if (maxvalue < 256) {
				// 1 byte
				auto result = raw_read<uint8_t>(input);

				if (!result.second) {
					return false;
				}

				img(h, w) = result.first;
			}
			else {

				// 2 bytes big endian
				auto result = raw_read<uint16_t>(input);

				if (!result.second) {
					return false;
				}

				img(h, w) = tranform_to_big_endian(result.first);
			}
		}
	}

	return true;
}