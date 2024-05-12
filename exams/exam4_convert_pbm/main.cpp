#include "pbm.h"
#include <string>
#include <fstream>

static std::pair<BinaryImage, bool> ReadFromPBM(const std::string& filename) {

	BinaryImage image;

	std::ifstream input(filename, std::ios::binary);

	if (!input) {
		return std::pair<BinaryImage, bool>(image, false);
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
	image.W = std::stoi(raw_width);

	std::string raw_heigth;
	input >> raw_heigth;
	image.H = std::stoi(raw_heigth);

	input.ignore(1);

	int div = image.W / 8;
	int mod = image.W % 8;
	int bytes_per_row = mod != 0 ? div + 1 : div;

	uint8_t buffer = 0;
	uint8_t bits_in_buffer = 0;

	for (int row = 0; row < image.H; ++row) {
		for (int col = 0; col < bytes_per_row; ++col) {

			uint8_t value = 0;
			input.read(reinterpret_cast<char*>(&value), sizeof(value));

			if (input.eof()) {
				return std::pair<BinaryImage, bool>(image, false);
			}

			image.ImageData.push_back(value);
		}
	}

	return std::pair<BinaryImage, bool>(image, true);
}

int main(int argc, char* argv[]) {

	auto image = ReadFromPBM(argv[1]);

	if (!image.second) {
		return EXIT_FAILURE;
	}

	auto converted_image =  BinaryImageToImage(image.first);

	return EXIT_SUCCESS;
}