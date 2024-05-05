#include "pbm.h"
#include <fstream>

bool BinaryImage::ReadFromPBM(const std::string& filename) {

	std::ifstream input(filename, std::ios::binary);

	if (!input) {
		return false;
	}

	std::string magic_number;
	std::getline(input, magic_number);

	char next_char = input.peek();

	if (next_char == '#') {
		std::string comment;
		std::getline(input, comment);
	}

	std::string raw_width;
	input >> raw_width;
	this->W = std::stoi(raw_width);

	std::string raw_heigth;
	input >> raw_heigth;
	this->H = std::stoi(raw_heigth);

	input.ignore(1);

	int div = this->W / 8;
	int mod = this->W % 8;
	int bytes_per_row = mod != 0 ? div + 1 : div;

	for (int row = 0; row < this->H; ++row) {
		for (int col = 0; col < bytes_per_row; ++col) {

			uint8_t value = 0;
			input.read(reinterpret_cast<char*>(&value), sizeof(value));

			if (input.eof()) {
				return false;
			}

			this->ImageData.push_back(value);
		}
	}

	return true;
}