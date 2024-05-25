#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <sstream>

#include "mat.h"
#include "types.h"

struct header_data {
	int height = 0;
	int width = 0;
	std::string chroma_subsampling;
};

static header_data read_header(std::istream& input) {
	header_data header;

	std::string stream_header;
	std::getline(input, stream_header);
	std::istringstream stream_header_stream(stream_header);

	std::string magic_string;
	stream_header_stream >> magic_string;

	while (!stream_header_stream.eof()) {
		std::string tagged_field;
		stream_header_stream >> tagged_field;
		char identifier = tagged_field[0];
		std::string value = tagged_field.substr(1);

		switch (identifier)
		{
		case 'W':
			header.width = std::stoi(value);
			break;
		case 'H':
			header.height = std::stoi(value);
			break;
		case 'C':
			header.chroma_subsampling = value;
			break;
		default:
			break;
		}
	}

	if (header.chroma_subsampling.empty()) {
		header.chroma_subsampling = "420jpeg";
	}

	return header;
}

static std::pair<mat<vec3b>, bool> read_frame(const header_data& header, std::istream& input) {

	mat<vec3b> frame;
	frame.resize(header.height, header.width);

	std::string frame_header;
	std::getline(input, frame_header);

	if (input.eof()) {
		return std::pair<mat<vec3b>, bool>(frame, false);
	}

	std::istringstream frame_header_stream(frame_header);

	std::string frame_magic_string;
	frame_header_stream >> frame_magic_string;

	while (!frame_header_stream.eof()) {
		std::string tagged_field;
		frame_header_stream >> tagged_field;
	}

	//Y
	for (int row = 0; row < header.height; ++row) {
		for (int col = 0; col < header.width; ++col) {
			uint8_t pixel = 0;
			input.read(reinterpret_cast<char*>(&pixel), sizeof(pixel));

			if (input.eof()) {			
				return std::pair<mat<vec3b>, bool>(frame, false);
			}

			frame(row, col)[0] = pixel;
		}
	}

	//Cb
	for (int row = 0; row < header.height / 2; ++row) {
		for (int col = 0; col < header.width / 2; ++col) {
			uint8_t pixel = 0;
			input.read(reinterpret_cast<char*>(&pixel), sizeof(pixel));

			if (input.eof()) {
				return std::pair<mat<vec3b>, bool>(frame, false);
			}

			auto double_row = row * 2;
			auto double_col = col * 2;
			frame(double_row, double_col)[1] = pixel;
			frame(double_row, double_col + 1)[1] = pixel;
			frame(double_row + 1, double_col)[1] = pixel;
			frame(double_row + 1, double_col + 1)[1] = pixel;
		}
	}

	//Cr
	for (int row = 0; row < header.height / 2; ++row) {
		for (int col = 0; col < header.width / 2; ++col) {
			uint8_t pixel = 0;
			input.read(reinterpret_cast<char*>(&pixel), sizeof(pixel));

			if (input.eof()) {
				return std::pair<mat<vec3b>, bool>(frame, false);
			}

			auto double_row = row * 2;
			auto double_col = col * 2;
			frame(double_row, double_col)[2] = pixel;
			frame(double_row, double_col + 1)[2] = pixel;
			frame(double_row + 1, double_col)[2] = pixel;
			frame(double_row + 1, double_col + 1)[2] = pixel;
		}
	}

	return std::pair<mat<vec3b>, bool>(frame, true);
}

template<typename T>
static T clamp(T value, T min, T max) {
	if (value < min) {
		return min;
	}
	else if (value > max) {
		return max;
	}
	else {
		return value;
	}
}

bool y4m_extract_color(const std::string& filename, std::vector<mat<vec3b>>& frames) {

	std::ifstream input(filename, std::ios::binary);

	if (!input) {
		return false;
	}

	header_data header = read_header(input);

	if (header.chroma_subsampling != "420jpeg") {
		return false;
	}

	while (true) {
		auto frame = read_frame(header, input);

		if (!frame.second) {
			break;
		}

		mat<vec3b> converted_frame(frame.first.rows(), frame.first.cols());

		for (int row = 0; row < frame.first.rows(); ++row) {
			for (int col = 0; col < frame.first.cols(); ++col) {

				vec3b original = frame.first(row, col);

				original[0] = clamp<uint8_t>(original[0], 16, 235);
				original[1] = clamp<uint8_t>(original[1], 16, 240);
				original[2] = clamp<uint8_t>(original[2], 16, 240);

				int32_t r = static_cast<int32_t>(1.164 * (original[0] - 16) + 1.596 * (original[2] - 128));
				int32_t g = static_cast<int32_t>(1.164 * (original[0] - 16) - 0.392 * (original[1] - 128) - 0.813 * (original[2] - 128));
				int32_t b = static_cast<int32_t>(1.164 * (original[0] - 16) + 2.017 * (original[1] - 128));

				vec3b converted{};
				converted[0] = static_cast<uint8_t>(clamp<int32_t>(r, 0, 255));
				converted[1] = static_cast<uint8_t>(clamp<int32_t>(g, 0, 255));
				converted[2] = static_cast<uint8_t>(clamp<int32_t>(b, 0, 255));

				converted_frame(row, col) = converted;
			}
		}

		frames.push_back(converted_frame);
	}

	return true;
}