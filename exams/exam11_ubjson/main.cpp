#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <algorithm>

#include "types.h"
#include "ppm.h"

template<typename T>
T raw_read(std::ifstream& input) {
	T buffer = 0;
	input.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));
	return buffer;
}

struct generic_value {
	uint64_t integer = 0;
	std::string string;
	double floating_point = 0;
	bool boolean = false;
	char character = 0;
	std::vector<std::pair<std::string, std::optional<generic_value>>> object;
	std::vector<std::optional<generic_value>> array;
};

static std::optional<generic_value> parse_object(std::ifstream& input);
static std::optional<generic_value> parse_array(std::ifstream& input);

static std::vector<uint64_t> read_integer_batched(std::ifstream& input, uint8_t integer_type, uint64_t count) {

	std::vector<uint64_t> values(count);

	if (integer_type == 'i') { // int 8

		std::vector<int8_t> temp(count);
		input.read(reinterpret_cast<char*>(temp.data()), count * sizeof(int8_t));
		std::copy(temp.begin(), temp.end(), values.begin());
	}
	else if (integer_type == 'U') { // uint 8

		std::vector<uint8_t> temp(count);
		input.read(reinterpret_cast<char*>(temp.data()), count * sizeof(uint8_t));
		std::copy(temp.begin(), temp.end(), values.begin());
	}
	else if (integer_type == 'I') { // int 16

		std::vector<int16_t> temp(count);
		input.read(reinterpret_cast<char*>(temp.data()), count * sizeof(int16_t));

		for (auto& value : values) {
			auto byte1 = ((value >> 0) & 0b11111111) << 8;
			auto byte2 = ((value >> 8) & 0b11111111) << 0;
			value = byte1 | byte2;
		}

		std::copy(temp.begin(), temp.end(), values.begin());
	}
	else if (integer_type == 'l') { // int 32

		std::vector<int32_t> temp(count);
		input.read(reinterpret_cast<char*>(temp.data()), count * sizeof(int32_t));

		for (auto& value : values) {
			auto byte1 = ((value >> 0) & 0b11111111) << 24;
			auto byte2 = ((value >> 8) & 0b11111111) << 16;
			auto byte3 = ((value >> 16) & 0b11111111) << 8;
			auto byte4 = ((value >> 24) & 0b11111111) << 0;
			value = byte1 | byte2 | byte3 | byte4;
		}

		std::copy(temp.begin(), temp.end(), values.begin());
	}
	else {
		std::cerr << "Failed to parse integer type: " << integer_type << std::endl;
	}

	return values;
}

static uint64_t read_integer(std::ifstream& input, uint8_t integer_type) {

	uint64_t value = 0;

	if (integer_type == 'i') { // int 8
		value = raw_read<int8_t>(input);
	}
	else if (integer_type == 'U') { // uint 8
		value = raw_read<uint8_t>(input);
	}
	else if (integer_type == 'I') { // int 16
		uint64_t temp = raw_read<int16_t>(input);

		auto byte1 = ((temp >> 0) & 0b11111111) << 8;
		auto byte2 = ((temp >> 8) & 0b11111111) << 0;

		value = byte1 | byte2;
	}
	else if (integer_type == 'l') { // int 32
		uint64_t temp = raw_read<int32_t>(input);

		auto byte1 = ((temp >> 0) & 0b11111111) << 24;
		auto byte2 = ((temp >> 8) & 0b11111111) << 16;
		auto byte3 = ((temp >> 16) & 0b11111111) << 8;
		auto byte4 = ((temp >> 24) & 0b11111111) << 0;

		value = byte1 | byte2 | byte3 | byte4;
	}
	else {
		std::cerr << "Failed to parse integer type: " << integer_type << std::endl;
	}

	return value;
}

static std::string read_string(std::ifstream& input, uint8_t size_type) {
	uint64_t key_size = read_integer(input, size_type);
	std::string key(key_size, 0);
	input.read(reinterpret_cast<char*>(key.data()), key_size);
	return key;
}

static std::optional<generic_value> read_value(std::ifstream& input, uint8_t value_type) {
	generic_value value;

	if (value_type == 'S') {
		uint8_t size_type = raw_read<uint8_t>(input);
		value.string = read_string(input, size_type);
	}
	else if (value_type == 'Z') {
		return std::nullopt;
	}
	else if (value_type == 'N') {
		std::cerr << "Not supported" << std::endl;
		return std::nullopt;
	}
	else if (value_type == 'C') {
		std::cerr << "Not supported" << std::endl;
		return std::nullopt;
	}
	else if (value_type == 'H') {
		std::cerr << "Not supported" << std::endl;
		return std::nullopt;
	}
	else if (value_type == 'd' || value_type == 'D') {
		std::cerr << "Not supported" << std::endl;
		return std::nullopt;
	}
	else if (value_type == 'T' || value_type == 'F') {
		std::cerr << "Not supported" << std::endl;
		return std::nullopt;
	}
	else {
		value.integer = read_integer(input, value_type);
	}

	return value;
}

static std::optional<generic_value> parse_object(std::ifstream& input) {

	generic_value current_value;

	while (true) {

		if (input.peek() == '}') {
			input.ignore(1);
			break;
		}

		uint8_t size_type = raw_read<uint8_t>(input);

		if (input.eof()) {
			break;
		}

		std::string property_key = read_string(input, size_type);

		uint8_t value_type = raw_read<uint8_t>(input);

		if (value_type == '{') {
			std::pair<std::string, std::optional<generic_value>> pair(property_key, parse_object(input));
			current_value.object.push_back(pair);
		}
		else if (value_type == '[') {
			std::pair<std::string, std::optional<generic_value>> pair(property_key, parse_array(input));
			current_value.object.push_back(pair);
		}
		else {
			std::pair<std::string, std::optional<generic_value>> pair(property_key, read_value(input, value_type));
			current_value.object.push_back(pair);
		}
	}

	return current_value;
}

static std::optional<generic_value> parse_array(std::ifstream& input) {

	generic_value current_value;

	std::optional<uint8_t> value_type = std::nullopt;
	std::optional<uint64_t> array_size = std::nullopt;

	if (input.peek() == '$') {
		input.ignore(1);
		value_type = raw_read<uint8_t>(input);
	}

	if (input.peek() == '#') {
		input.ignore(1);
		uint8_t integer_type = raw_read<uint8_t>(input);
		array_size = read_integer(input, integer_type);
	}

	while (!array_size.has_value() || (array_size.has_value() && array_size.value() > 0)) {

		if (input.eof()) {
			break;
		}

		if (!array_size.has_value()) {
			if (input.peek() == ']') {
				input.ignore(1);
				break;
			}
		}

		uint8_t local_value_type;

		if (!value_type.has_value()) {
			local_value_type = raw_read<uint8_t>(input);
		}
		else {
			local_value_type = value_type.value();
		}

		if (local_value_type == '{') {
			current_value.array.push_back(parse_object(input));
		}
		else if (local_value_type == '[') {
			current_value.array.push_back(parse_array(input));
		}
		else {
			if (value_type.has_value() && array_size.has_value() &&
				(value_type.value() == 'i' || value_type.value() == 'U'
					|| value_type.value() == 'I' || value_type.value() == 'l')) {

				// optimized read with integers
				auto integers = read_integer_batched(input, local_value_type, array_size.value());

				for (auto& integer : integers) {
					generic_value value;
					value.integer = integer;
					current_value.array.push_back(value);
				}

				array_size.value() = 0;
			}
			else {
				current_value.array.push_back(read_value(input, local_value_type));

				if (array_size.has_value()) {
					array_size.value()--;
				}
			}
		}	
	}

	return current_value;
}

struct ubjson_canvas {
	uint64_t width = 0;
	uint64_t height = 0;
	vec3b background;
};

struct ubjson_image {
	uint64_t x = 0;
	uint64_t y = 0;
	uint64_t width = 0;
	uint64_t height = 0;
	std::vector<vec3b> data;
};

struct ubjson_data {
	ubjson_canvas canvas = {};
	std::vector<ubjson_image> images;
};

static std::optional<generic_value> parse_ubjson(std::ifstream& input) {

	uint8_t value_type = raw_read<uint8_t>(input);

	std::optional<generic_value> value;

	if (value_type == '{') {
		value = parse_object(input);
	}
	else if (value_type == '[') {
		value = parse_array(input);
	}

	return value;
}

static ubjson_data parse_data(std::optional<generic_value> data) {

	ubjson_data parsed_data;

	for (const auto& data_elements : data.value().object) {

		if (data_elements.first == "canvas") {
			const auto& canvas_obj = data_elements.second.value().object;

			for (const auto& canvas_element : canvas_obj) {

				if (canvas_element.first == "width") {
					parsed_data.canvas.width = canvas_element.second.value().integer;
				}
				else if (canvas_element.first == "height") {
					parsed_data.canvas.height = canvas_element.second.value().integer;
				}
				else if (canvas_element.first == "background") {

					const auto& canvas_background = canvas_element.second.value().array;

					const auto& canvas_r = canvas_background[0].value().integer;
					const auto& canvas_g = canvas_background[1].value().integer;
					const auto& canvas_b = canvas_background[2].value().integer;
					parsed_data.canvas.background = vec3b(static_cast<uint8_t>(canvas_r), static_cast<uint8_t>(canvas_g), static_cast<uint8_t>(canvas_b));
				}
			}
		}
		else if (data_elements.first == "elements") {

			const auto& elements_obj = data_elements.second.value().object;

			for (const auto& elements_element : elements_obj) {

				if (elements_element.first == "image") {

					ubjson_image image;

					const auto& image_obj = elements_element.second.value().object;

					for (const auto& image_element : image_obj) {

						if (image_element.first == "x") {
							image.x = image_element.second.value().integer;
						}
						else if (image_element.first == "y") {
							image.y = image_element.second.value().integer;
						}
						else if (image_element.first == "width") {
							image.width = image_element.second.value().integer;
						}
						else if (image_element.first == "height") {
							image.height = image_element.second.value().integer;
						}
						else if (image_element.first == "data") {

							const auto& image_data = image_element.second.value().array;

							for (uint64_t i = 0; i < image_data.size(); i += 3) {
								const auto& r = image_data[i].value().integer;
								const auto& g = image_data[i + 1].value().integer;
								const auto& b = image_data[i + 2].value().integer;
								image.data.push_back(vec3b(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)));
							}
						}
					}

					parsed_data.images.push_back(image);

				}
			}
		}
	}

	return parsed_data;
}

int main(int argc, char* argv[]) {

	if (argc != 3) {
		return EXIT_FAILURE;
	}

	std::ifstream input(argv[1], std::ios::binary);

	if (!input) {
		return EXIT_FAILURE;
	}

	std::optional<generic_value> data = parse_ubjson(input);
	ubjson_data parsed_data = parse_data(data);

	image<vec3b> canvas(parsed_data.canvas.width, parsed_data.canvas.height);

	for (auto& pixel : canvas) {
		pixel = parsed_data.canvas.background;
	}

	writeP6("canvas.ppm", canvas);

	uint32_t image_index = 1;

	for (auto& current_image : parsed_data.images) {
		image<vec3b> converted_image(current_image.width, current_image.height);
		std::copy(current_image.data.begin(), current_image.data.end(), converted_image.begin());
		writeP6("image" + std::to_string(image_index) + ".ppm", converted_image);
		image_index++;

		for (uint64_t col = 0; col < canvas.width(); ++col) {
			for (uint64_t row = 0; row < canvas.height(); ++row) {
				if (col >= current_image.x && col < current_image.width + current_image.x &&
					row >= current_image.y && row < current_image.height + current_image.y) {
					canvas(col, row) = converted_image(col - current_image.x, row - current_image.y);
				}
			}
		}
	}

	std::string file_output = argv[2];
	writeP6(file_output, canvas);

	for (const auto& data_elements : data.value().object) {

		if (data_elements.first == "elements") {
			const auto& elements_obj = data_elements.second.value().object;

			for (const auto& elements_element : elements_obj) {

				std::cout << elements_element.first << " : ";

				for (const auto& element_property_item : elements_element.second.value().object) {
					std::cout << element_property_item.first << ",";
				}

				std::cout << std::endl;

			}
		}
	}

	return EXIT_SUCCESS;
}