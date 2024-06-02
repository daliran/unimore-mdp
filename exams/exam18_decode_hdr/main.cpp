#include <fstream>
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <unordered_map>
#include <algorithm>
#include <limits>

template<typename T>
T raw_read(std::istream& input, size_t size = sizeof(T)) {
	T buffer = 0;
	input.read(reinterpret_cast<char*>(&buffer), size);
	return buffer;
}

template<typename T>
void raw_write(std::ostream& output, T value) {
	output.write(reinterpret_cast<char*>(&value), sizeof(value));
}

using vec3b = std::array<uint8_t, 3>;
using vec3f = std::array<float, 3>;

template<typename T>
class matrix {
private:
	uint64_t rows_ = 0;
	uint64_t cols_ = 0;
	std::vector<T> data_;

public:
	matrix() { }

	matrix(uint64_t rows, uint64_t cols) {
		resize(rows, cols);
	}

	void resize(uint64_t rows, uint64_t cols) {
		rows_ = rows;
		cols_ = cols;
		data_.resize(rows_ * cols_);
	}

	T& operator()(uint64_t row, uint64_t col) {
		return data_[row * cols_ + col];
	}

	const T& operator()(uint64_t row, uint64_t col) const {
		return data_[row * cols_ + col];
	}

	uint64_t rows() const {
		return rows_;
	}

	uint64_t cols() const {
		return cols_;
	}

	const auto begin() const {
		return data_.begin();
	}

	const auto end() const {
		return data_.end();
	}
};

class hdr {
private:
	matrix<vec3f> raster_;

	float reconstruct_value(uint8_t color, uint8_t e) {
		float multiplier = std::pow(2, e - 128);
		float result = (color + 0.5f) / 256 * multiplier;
		return static_cast<float>(result);
	}

	uint8_t map_tone(float value, float min, float delta) {
		float converted_value = (value - min) / delta;
		float gamma_corrected = std::pow(converted_value, 0.45f);
		uint8_t result = static_cast<uint8_t>(gamma_corrected * 255);
		return result;
	}

public:
	hdr(std::ifstream& input) {

		std::string magic_number;
		std::getline(input, magic_number);

		char peek_char = input.peek();

		std::unordered_map<std::string, std::string> variables;

		while (true) {
			std::string line;
			std::getline(input, line);

			if (line[0] == '#') {
				continue;
			}

			if (line.empty()) {
				break;
			}

			size_t index = line.find('=');
			std::string name = line.substr(0, index);
			std::string value = line.substr(index + 1);
			variables.emplace(name, value);
		}
		std::string resolution;
		std::getline(input, resolution);

		size_t y_index = resolution.find('Y');
		size_t x_index = resolution.find('X');
		std::string y_resolution = resolution.substr(y_index + 2, x_index - y_index - 4);
		std::string x_resolution = resolution.substr(x_index + 2);

		uint64_t y_resolution_value = std::stoi(y_resolution);
		uint64_t x_resolution_value = std::stoi(x_resolution);

		raster_.resize(y_resolution_value, x_resolution_value);

		uint64_t read_rows = 0;
		uint64_t read_columns = 0;

		// Read raster data
		while (read_rows < y_resolution_value) {

			read_columns = 0;

			uint8_t scanline_byte1 = raw_read<uint8_t>(input);
			uint8_t scanline_byte2 = raw_read<uint8_t>(input);
			uint16_t number_of_columns = raw_read<uint8_t>(input) << 8 | raw_read<uint8_t>(input);

			std::vector<uint8_t> line_data;
			line_data.reserve(number_of_columns);

			while (read_columns < static_cast<uint64_t>(number_of_columns) * 4) {
				uint8_t command = raw_read<uint8_t>(input);

				if (command <= 127) {

					// copy		
					for (uint8_t i = 0; i < command; ++i) {
						uint8_t command = raw_read<uint8_t>(input);
						line_data.push_back(command);
					}

					read_columns += command;
				}
				else {

					//run
					uint8_t repeats = command - 128;
					uint8_t symbol = raw_read<uint8_t>(input);

					for (uint8_t i = 0; i < repeats; ++i) {
						line_data.push_back(symbol);
					}

					read_columns += repeats;
				}
			}

			// Copy line data into the raster
			for (uint64_t col = 0; col < number_of_columns; ++col) {
				uint8_t r = line_data[col];
				uint8_t g = line_data[col + static_cast<uint64_t>(number_of_columns)];
				uint8_t b = line_data[col + static_cast<uint64_t>(number_of_columns) * 2];
				uint8_t e = line_data[col + static_cast<uint64_t>(number_of_columns) * 3];
				raster_(read_rows, col) = { reconstruct_value(r, e), reconstruct_value(g, e), reconstruct_value(b, e) };
			}

			read_rows++;
		}
	}

	matrix<vec3b> compute_global_tone_mapping() {
		matrix<vec3b> data(raster_.rows(), raster_.cols());

		float min_value = std::numeric_limits<float>::max();;
		float max_value = 0;

		for (const auto& item : raster_) {
			if (item[0] < min_value) {
				min_value = item[0];
			}
			if (item[1] < min_value) {
				min_value = item[1];
			}
			if (item[2] < min_value) {
				min_value = item[2];
			}
			if (item[0] > max_value) {
				max_value = item[0];
			}
			if (item[1] > max_value) {
				max_value = item[1];
			}
			if (item[2] > max_value) {
				max_value = item[2];
			}
		}

		float delta = max_value - min_value;

		for (uint64_t row = 0; row < data.rows(); ++row) {
			for (uint64_t col = 0; col < data.cols(); ++col) {
				const auto& value = raster_(row, col);

				data(row, col) = {
					map_tone(value[0], min_value, delta),
					map_tone(value[1], min_value, delta),
					map_tone(value[2], min_value, delta)
				};
			}
		}

		return data;
	}
};

class pam {
private:
	matrix<vec3b> raster_;

public:
	pam(const matrix<vec3b>& data) {
		raster_ = data;
	}

	void write(std::ofstream& output) {

		output << "P7" << std::endl;
		output << "WIDTH " << raster_.cols() << std::endl;
		output << "HEIGHT " << raster_.rows() << std::endl;
		output << "DEPTH " << "3" << std::endl;
		output << "MAXVAL " << "255" << std::endl;
		output << "TUPLTYPE " << "RGB" << std::endl;
		output << "ENDHDR" << std::endl;

		for (uint64_t row = 0; row < raster_.rows(); ++row) {
			for (uint64_t col = 0; col < raster_.cols(); ++col) {
				const auto& pixel = raster_(row, col);
				raw_write<uint8_t>(output, pixel[0]);
				raw_write<uint8_t>(output, pixel[1]);
				raw_write<uint8_t>(output, pixel[2]);
			}
		}
	}
};

int main(int argc, char* argv[]) {

	if (argc != 3) {
		return EXIT_FAILURE;
	}

	std::ifstream input(argv[1], std::ios::binary);

	if (!input) {
		return EXIT_FAILURE;
	}

	hdr hdr_image(input);

	matrix<vec3b> data = hdr_image.compute_global_tone_mapping();

	std::ofstream output(argv[2], std::ios::binary);

	if (!output) {
		return EXIT_FAILURE;
	}

	pam pam_image(data);

	pam_image.write(output);

	return EXIT_SUCCESS;
}