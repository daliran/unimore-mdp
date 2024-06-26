#include <fstream>
#include <cstdint>
#include <string>
#include <array>
#include <optional>
#include <iostream>
#include <vector>
#include <cmath>

template<typename T>
T raw_read(std::istream& input, size_t size = sizeof(T)) {
	T value = 0;
	input.read(reinterpret_cast<char*>(&value), size);
	return value;
}

template<typename T>
void raw_write(std::ostream& output, T value, size_t size = sizeof(T)) {
	output.write(reinterpret_cast<char*>(&value), size);
}

using vec3b = std::array<uint8_t, 3>;

template<typename T>
class matrix {
private:
	uint64_t rows_ = 0;
	uint64_t cols_ = 0;
	std::vector <T> data_;

public:
	matrix() {}

	matrix(uint64_t rows, uint64_t cols) {
		resize(rows, cols);
	}

	void resize(uint64_t rows, uint64_t cols) {
		rows_ = rows;
		cols_ = cols;
		data_.resize(rows_ * cols_);
	}

	T& operator () (uint64_t row, uint64_t col) {
		return data_[row * cols_ + col];
	}

	const T& operator () (uint64_t row, uint64_t col) const {
		return data_[row * cols_ + col];
	}

	// Get with bounduary checks
	T safe_get(int64_t row, int64_t col) const {
		if (row < 0 || col < 0) {
			return T();
		}

		if (static_cast<uint64_t>(row) > (rows_ - 1) || static_cast<uint64_t>(col) > (cols_ - 1)) {
			return T();
		}

		return data_[row * cols_ + col];
	}

	uint64_t rows() const {
		return rows_;
	}

	uint64_t cols() const {
		return cols_;
	}

};

static std::optional<matrix<uint8_t>> load_bayer_pgm(std::string filename) {

	std::ifstream input(filename, std::ios::binary);

	if (!input) {
		return std::nullopt;
	}

	std::string magic_number;
	input >> magic_number >> std::ws;

	if (input.peek() == '#') {
		std::string comment;
		std::getline(input, comment);
	}

	std::string width_str;
	input >> width_str >> std::ws;
	uint64_t width = std::stoi(width_str);

	std::string height_str;
	input >> height_str >> std::ws;
	uint64_t height = std::stoi(height_str);

	std::string max_value_str;
	input >> max_value_str >> std::ws;
	uint64_t max_value = std::stoi(max_value_str);

	matrix<uint8_t> raster(height, width);

	for (uint64_t row = 0; row < raster.rows(); ++row) {
		for (uint64_t col = 0; col < raster.cols(); ++col) {

			uint16_t byte1 = raw_read<uint8_t>(input);

			if (max_value < 256) {
				raster(row, col) = static_cast<uint8_t>(byte1);
			}
			else {
				uint16_t byte2 = raw_read<uint8_t>(input);
				uint16_t pixel = (byte1 << 8) | byte2;
				uint8_t scaled_pixel = static_cast<uint8_t>(pixel / 256);
				raster(row, col) = scaled_pixel;
			}
		}
	}

	return raster;
}

static bool write_pgm(std::string filename, const matrix<uint8_t>& raster) {

	std::ofstream output(filename, std::ios::binary);

	if (!output) {
		return false;
	}

	output << "P5" << std::endl;
	output << raster.cols() << " " << raster.rows() << std::endl;
	output << 255 << std::endl;

	for (uint64_t row = 0; row < raster.rows(); ++row) {
		for (uint64_t col = 0; col < raster.cols(); ++col) {
			uint8_t pixel = raster(row, col);
			raw_write<uint8_t>(output, pixel);
		}
	}

	return true;
}

static bool write_ppm(std::string filename, const matrix<vec3b>& raster) {

	std::ofstream output(filename, std::ios::binary);

	if (!output) {
		return false;
	}

	output << "P6" << std::endl;
	output << raster.cols() << " " << raster.rows() << std::endl;
	output << 255 << std::endl;

	for (uint64_t row = 0; row < raster.rows(); ++row) {
		for (uint64_t col = 0; col < raster.cols(); ++col) {
			vec3b pixel = raster(row, col);
			raw_write<uint8_t>(output, pixel[0]);
			raw_write<uint8_t>(output, pixel[1]);
			raw_write<uint8_t>(output, pixel[2]);
		}
	}

	return true;
}

enum class bayer_color {
	red,
	green,
	blue
};

static bayer_color get_bayer_color(uint64_t row, uint64_t col) {
	//Bayer pattern:
	//RG
	//GB

	bool even_row = row % 2 == 0;
	bool even_col = col % 2 == 0;

	if (even_row && even_col) {
		return bayer_color::red;
	}
	else if (!even_row && !even_col) {
		return bayer_color::blue;
	}
	else {
		return bayer_color::green;
	}
}

static matrix<vec3b> split_bayer_components(const matrix<uint8_t>& raster) {

	matrix<vec3b> bayer(raster.rows(), raster.cols());

	for (uint64_t row = 0; row < raster.rows(); ++row) {
		for (uint64_t col = 0; col < raster.cols(); ++col) {

			bayer_color color = get_bayer_color(row, col);

			if (color == bayer_color::red) {
				bayer(row, col)[0] = raster(row, col);
			}
			else if (color == bayer_color::green) {
				bayer(row, col)[1] = raster(row, col);
			}
			else if (color == bayer_color::blue) {
				bayer(row, col)[2] = raster(row, col);
			}
		}
	}

	return bayer;
}

static uint8_t saturate_value(int32_t value) {
	if (value > 255) {
		return 255;
	}
	else if (value < 0) {
		return 0;
	}
	else {
		return static_cast<uint8_t>(value);
	}
}

static void reconstruct_green(matrix<vec3b>& raster) {

	for (uint64_t row = 0; row < raster.rows(); ++row) {
		for (uint64_t col = 0; col < raster.cols(); ++col) {

			/*vec3b pixel = raster(row, col);

			if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0) {
				continue;
			}*/

			bayer_color pixel_color = get_bayer_color(row, col);

			if (pixel_color == bayer_color::green) {
				continue;
			}

			uint8_t index = pixel_color == bayer_color::red ? 0 : 2;

			int32_t x5 = raster.safe_get(row, col)[index];

			int32_t g4 = raster.safe_get(row, col - 1)[1];
			int32_t x3 = raster.safe_get(row, col - 2)[index];

			int32_t g6 = raster.safe_get(row, col + 1)[1];
			int32_t x7 = raster.safe_get(row, col + 2)[index];

			int32_t g2 = raster.safe_get(row - 1, col)[1];
			int32_t x1 = raster.safe_get(row - 2, col)[index];

			int32_t g8 = raster.safe_get(row + 1, col)[1];
			int32_t x9 = raster.safe_get(row + 2, col)[index];

			int32_t dh_x = x5 - x3 + x5 - x7;
			int32_t dv_x = x5 - x1 + x5 - x9;
			int32_t dh = std::abs(g4 - g6) + std::abs(dh_x);
			int32_t dv = std::abs(g2 - g8) + std::abs(dv_x);

			int32_t green_component;

			if (dh < dv) {
				green_component = (g4 + g6) / 2 + (dh_x) / 4;
			}
			else if (dh > dv) {
				green_component = (g2 + g8) / 2 + (dv_x) / 4;
			}
			else {
				green_component = (g2 + g4 + g6 + g8) / 4 + (dh_x + dv_x) / 8;
			}

			raster(row, col)[1] = saturate_value(green_component);
		}
	}
}

static void reconstruct_red_blue(matrix<vec3b>& raster) {

	for (uint64_t row = 0; row < raster.rows(); ++row) {
		for (uint64_t col = 0; col < raster.cols(); ++col) {

			/*vec3b pixel = raster(row, col);

			if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0) {
				continue;
			}*/

			bayer_color pixel_color = get_bayer_color(row, col);

			if (pixel_color == bayer_color::green) {

				bayer_color right_side_pixel_color = get_bayer_color(row, col + 1);

				vec3b pixel2 = raster.safe_get(row - 1, col);
				vec3b pixel4 = raster.safe_get(row, col - 1);
				vec3b pixel6 = raster.safe_get(row, col + 1);
				vec3b pixel8 = raster.safe_get(row + 1, col);

				uint8_t index1 = right_side_pixel_color == bayer_color::red ? 0 : 2;
				uint8_t index2 = right_side_pixel_color == bayer_color::red ? 2 : 0;

				int32_t x2 = pixel2[index2];
				int32_t x4 = pixel4[index1];
				int32_t x6 = pixel6[index1];
				int32_t x8 = pixel8[index2];

				int32_t avg_h = (x4 + x6) / 2;
				int32_t avg_v = (x2 + x8) / 2;

				raster(row, col)[index1] = saturate_value(avg_h);
				raster(row, col)[index2] = saturate_value(avg_v);
			}
			else {

				uint8_t index = pixel_color == bayer_color::red ? 2 : 0;

				int32_t g5 = raster.safe_get(row, col)[1];

				vec3b pixel1 = raster.safe_get(row - 1, col - 1);
				int32_t x1 = pixel1[index];
				int32_t g1 = pixel1[1];

				vec3b pixel3 = raster.safe_get(row - 1, col + 1);
				int32_t x3 = pixel3[index];
				int32_t g3 = pixel3[1];

				vec3b pixel7 = raster.safe_get(row + 1, col - 1);
				int32_t x7 = pixel7[index];
				int32_t g7 = pixel7[1];

				vec3b pixel9 = raster.safe_get(row + 1, col + 1);
				int32_t x9 = pixel9[index];
				int32_t g9 = pixel9[1];

				int32_t dn_x = g5 - g1 + g5 - g9;
				int32_t dp_x = g5 - g3 + g5 - g7;

				int32_t dn = std::abs(x1 - x9) + std::abs(dn_x);
				int32_t dp = std::abs(x3 - x7) + std::abs(dp_x);

				int32_t x_component;

				if (dn < dp) {
					x_component = (x1 + x9) / 2 + (dn_x) / 4;
				}
				else if (dn > dp) {
					x_component = (x3 + x7) / 2 + (dp_x) / 4;
				}
				else {
					x_component = (x1 + x3 + x7 + x9) / 4 + (dn_x + dp_x) / 8;
				}

				raster(row, col)[index] = saturate_value(x_component);
			}
		}
	}
}

int main(int argc, char* argv[]) {

	if (argc != 3) {
		std::cerr << "Wrong number of arguments" << std::endl;
		return EXIT_FAILURE;
	}

	std::string prefix(argv[2]);

	std::optional<matrix<uint8_t>> grey_pgm = load_bayer_pgm(argv[1]);

	if (!grey_pgm.has_value()) {
		std::cerr << "Failed to load the pgm file" << std::endl;
		return EXIT_FAILURE;
	}

	std::string gray_file_name = prefix + "_gray.pgm";

	if (!write_pgm(gray_file_name, grey_pgm.value())) {
		std::cerr << "Failed to write " << gray_file_name << std::endl;
		return EXIT_FAILURE;
	}

	std::string bayer_file_name = prefix + "_bayer.ppm";
	matrix<vec3b> bayer_image = split_bayer_components(grey_pgm.value());

	if (!write_ppm(bayer_file_name, bayer_image)) {
		std::cerr << "Failed to write " << bayer_file_name << std::endl;
		return EXIT_FAILURE;
	}

	std::string green_file_name = prefix + "_green.ppm";
	reconstruct_green(bayer_image);

	if (!write_ppm(green_file_name, bayer_image)) {
		std::cerr << "Failed to write " << green_file_name << std::endl;
		return EXIT_FAILURE;
	}

	std::string interpolated_file_name = prefix + "_interp.ppm";
	reconstruct_red_blue(bayer_image);

	if (!write_ppm(interpolated_file_name, bayer_image)) {
		std::cerr << "Failed to write " << interpolated_file_name << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}