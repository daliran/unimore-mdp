#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <sstream>
#include "mat.h"

struct header_data {
	uint64_t height = 0;
	uint64_t width = 0;
	std::string chroma_subsampling;
};

struct frame_data {
	mat<uint8_t> Y;
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

static std::pair<frame_data, bool> read_frame(const header_data& header, std::istream& input) {

	frame_data frame;
	frame.Y.resize(header.height, header.width);

	std::string frame_header;
	std::getline(input, frame_header);

	if (input.eof()) {
		return std::pair<frame_data, bool>(frame, false);
	}

	std::istringstream frame_header_stream(frame_header);

	std::string frame_magic_string;
	frame_header_stream >> frame_magic_string;

	while (!frame_header_stream.eof()) {
		std::string tagged_field;
		frame_header_stream >> tagged_field;
		/*char identifier = tagged_field[0];
		std::string value = tagged_field.substr(1);*/
	}

	//Y
	for (uint64_t row = 0; row < header.height; ++row) {
		for (uint64_t col = 0; col < header.width; ++col) {

			uint8_t pixel = 0;
			input.read(reinterpret_cast<char*>(&pixel), sizeof(pixel));

			if (input.eof()) {
				return std::pair<frame_data, bool>(frame, false);
			}

			frame.Y(row, col) = pixel;
		}
	}

	//Cb
	for (uint64_t row = 0; row < header.height/4; ++row) {
		for (uint64_t col = 0; col < header.width/4; ++col) {

			uint8_t pixel = 0;
			input.read(reinterpret_cast<char*>(&pixel), sizeof(pixel));

			if (input.eof()) {
				return std::pair<frame_data, bool>(frame, false);
			}
		}
	}

	//Cr
	for (uint64_t row = 0; row < header.height / 4; ++row) {
		for (uint64_t col = 0; col < header.width / 4; ++col) {

			uint8_t pixel = 0;
			input.read(reinterpret_cast<char*>(&pixel), sizeof(pixel));

			if (input.eof()) {
				return std::pair<frame_data, bool>(frame, false);
			}
		}
	}

	return std::pair<frame_data, bool>(frame, true);
}

bool y4m_extract_gray(const std::string& filename, std::vector<mat<uint8_t>>& frames) {

	std::ifstream input(filename);

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

		frames.push_back(frame.first.Y);
	}
	
	return true;
}
