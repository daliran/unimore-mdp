#include <string>
#include <fstream>
#include <cstdint>
#include "ppm.h"
#include "math.h"

bool LoadPPM(const std::string& filename, mat<vec3b>& img) {

	std::ifstream input(filename, std::ios::binary);

	if (!input) {
		return false;
	}

	std::string magic_number;
	input >> magic_number >> std::ws;

	char comment_peek_char = input.peek();

	if (comment_peek_char == '#') {
		input.ignore(1);
		std::string comment;
		std::getline(input, comment);
	}

	std::string width;
	input >> width >> std::ws;
	int width_value = std::stoi(width);

	std::string height;
	input >> height >> std::ws;
	int height_value = std::stoi(height);

	std::string max_value;
	input >> max_value >> std::ws;

	img.resize(height_value, width_value);

	for (int row = 0; row < height_value; ++row) {
		for (int col = 0; col < width_value; ++col) {
			auto& vec = img(row, col);
			input.read(reinterpret_cast<char*>(&vec[0]), sizeof(vec[0]));
			input.read(reinterpret_cast<char*>(&vec[1]), sizeof(vec[1]));
			input.read(reinterpret_cast<char*>(&vec[2]), sizeof(vec[2]));
		}
	}

	return true;
}

void SplitRGB(const mat<vec3b>& img, mat<uint8_t>& img_r, mat<uint8_t>& img_g, mat<uint8_t>& img_b) {

	img_r.resize(img.rows(), img.cols());
	img_g.resize(img.rows(), img.cols());
	img_b.resize(img.rows(), img.cols());

	for (int row = 0; row < img.rows(); ++row) {
		for (int col = 0; col < img.cols(); ++col) {

			const auto& pixel = img(row, col);
			img_r(row, col) = pixel[0];
			img_g(row, col) = pixel[1];
			img_b(row, col) = pixel[2];
		}
	}
}