#include <fstream>
#include <vector>
#include <unordered_map>
#include "pcx.h"
#include "utils.h"

struct pcx_header {
	uint8_t manufacturer; // always 0x0A
	uint8_t version; // 5 for the exam
	uint8_t encoding; // 1 = PCX run length encoding
	uint8_t bits_per_plane; // bits per pixel in each color plane (1, 2, 4, 8)
	uint16_t window_x_min; // little endian
	uint16_t window_y_min; // little endian
	uint16_t window_x_max; // little endian
	uint16_t window_y_max; // little endian
	uint16_t vertical_dpi; // little endian, ignore
	uint16_t horizontal_dpi; // little endian, ignore
	uint8_t palette[48]; // ignore
	uint8_t reserved;  // ignore
	uint8_t color_planes; // number of color planes
	uint16_t bytes_per_plane_line; // little endian
	uint16_t palette_info; // little endian, ignore
	uint16_t horizontal_screen_size; // little endian, ignore
	uint16_t vertical_screen_size; // little endian, ignore
	uint8_t padding[54]; // ignore

	uint32_t width() const {
		return window_x_max - window_x_min + 1;
	}

	uint32_t height() const {
		return window_y_max - window_y_min + 1;
	}

	uint16_t color_depth() const {
		return color_planes * bits_per_plane;
	}

	// bytes required for complete uncompressed scan line
	uint32_t total_bytes_per_line() const {
		return color_planes * bytes_per_plane_line;
	}
};

static pcx_header read_header(std::ifstream& input) {
	pcx_header header{};
	input.read(reinterpret_cast<char*>(&header), sizeof(header));
	return header;
}

static std::pair<std::vector<uint8_t>, bool> read_scan_line(const pcx_header& header, std::ifstream& input) {

	std::vector<uint8_t> decompressed_scan_line;

	while (decompressed_scan_line.size() < header.total_bytes_per_line()) {

		uint8_t command;
		raw_read(input, command);

		if (input.eof()) {
			return std::pair<std::vector<uint8_t>, bool>(decompressed_scan_line, false);
		}

		uint8_t repetitions = 1;
		bool is_run = (command & 0b11000000) == 0b11000000;

		if (is_run) {
			repetitions = command & 0b00111111;
			uint8_t value;
			raw_read(input, value);

			for (int i = 0; i < repetitions; ++i) {
				decompressed_scan_line.push_back(value);
			}
		}
		else {
			decompressed_scan_line.push_back(command);
		}
	}

	return std::pair<std::vector<uint8_t>, bool>(decompressed_scan_line, true);
}

bool load_pcx(const std::string& filename, mat<vec3b>& img) {

	std::ifstream input(filename, std::ios::binary);

	if (!input) {
		return false;
	}

	pcx_header header = read_header(input);

	img.resize(header.height(), header.width());

	mat<uint8_t> image_with_palette_index(header.height(), header.width());

	for (uint32_t row = 0; row < header.height(); ++row) {

		auto line = read_scan_line(header, input);

		if (!line.second) {
			break;
		}

		for (uint32_t col = 0; col < header.width(); ++col) {
			image_with_palette_index(row, col) = line.first[col];
		}
	}

	input.seekg(-769, std::ios::end);

	int peek = input.peek();

	std::unordered_map<uint8_t, vec3b> palette;

	if (peek == 12) { 	// There is a palette
		input.ignore(1);

		for (uint32_t color_index = 0; color_index < 256; ++color_index) {
			vec3b color{};
			raw_read(input, color[0]);
			raw_read(input, color[1]);
			raw_read(input, color[2]);
			palette[color_index] = color;
		}

		for (uint32_t row = 0; row < header.height(); ++row) {
			for (uint32_t col = 0; col < header.width(); ++col) {
				const auto& index = image_with_palette_index(row, col);
				img(row, col) = palette[index];
			}
		}
	}

	return true;
}