#include <fstream>
#include <string>
#include <array>
#include <cmath>
#include <vector>
#include <algorithm>
#include <istream>
#include <optional>

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

template<typename T>
class matrix {
private:
	uint64_t rows_ = 0;
	uint64_t columns_ = 0;
	std::vector<T> data_;
public:
	matrix() {}

	matrix(uint64_t rows, uint64_t columns) {
		resize(rows, columns);
	}

	void resize(uint64_t rows, uint64_t columns) {
		rows_ = rows;
		columns_ = columns;
		data_.resize(rows_ * columns_);
	}

	T& operator() (uint64_t row, uint64_t column) {
		return data_[row * columns_ + column];
	}

	const T& operator() (uint64_t row, uint64_t column) const {
		return data_[row * columns_ + column];
	}

	std::optional<T> get_value_with_check(int64_t row, int64_t column) const {

		if (row < 0 || row > static_cast<int64_t>(rows_ - 1) || column < 0 || column > static_cast<int64_t>(columns_) - 1) {
			return std::nullopt;
		}

		return data_[row * columns_ + column];
	}

	auto begin() {
		return data_.begin();
	}

	auto end() {
		return data_.end();
	}

	const auto begin() const {
		return data_.begin();
	}

	const auto end() const {
		return data_.end();
	}

	uint64_t rows() const {
		return rows_;
	}

	uint64_t columns() const {
		return columns_;
	}

};

template<typename T>
class pgm {
private:
	std::string magic_number_;
	uint64_t width_;
	uint64_t height_;
	uint64_t max_value_;
	matrix<T> raster_;

public:
	pgm(std::ifstream& input) {

		std::getline(input, magic_number_);

		std::string width;
		input >> width >> std::ws;
		width_ = std::stoi(width);

		std::string height;
		input >> height >> std::ws;
		height_ = std::stoi(height);

		std::string max_value;
		std::getline(input, max_value);
		max_value_ = std::stoi(max_value);

		raster_.resize(height_, width_);

		for (uint64_t row = 0; row < height_; ++row) {
			for (uint64_t col = 0; col < width_; ++col) {

				T pixel_value = 0;

				if (max_value_ < 256) {
					pixel_value = raw_read<uint8_t>(input);
				}
				else {
					uint8_t byte1 = raw_read<uint8_t>(input);
					uint8_t byte2 = raw_read<uint8_t>(input);
					pixel_value = (byte1 << 8) | byte2;
				}

				raster_(row, col) = pixel_value;
			}
		}
	}

	pgm(const matrix<T>& raster) {
		magic_number_ = "P5";
		raster_ = raster;
		width_ = raster_.columns();
		height_ = raster_.rows();
		max_value_ = static_cast<uint64_t>(std::pow(2, sizeof(T) * 8)) - 1;
	}

	uint64_t width() const {
		return width_;
	}

	uint64_t height() const {
		return height_;
	}

	const matrix<T>& raster() const {
		return raster_;
	}

	void write(std::ofstream& output) {
		output << magic_number_ << std::endl;
		output << width_ << std::endl;
		output << height_ << std::endl;
		output << max_value_ << std::endl;

		for (uint64_t row = 0; row < height_; ++row) {
			for (uint64_t col = 0; col < width_; ++col) {

				const auto& pixel = raster_(row, col);

				if (max_value_ < 256) {
					raw_write(output, raster_(row, col));
				}
				else {
					uint16_t casted_pixel = static_cast<uint16_t>(pixel);
					uint8_t byte1 = (casted_pixel >> 8) & 0b11111111;
					uint8_t byte2 = (casted_pixel >> 0) & 0b11111111;
					raw_write(output, byte1);
					raw_write(output, byte2);
				}
			}
		}
	}
};

class ppm {
private:
	std::string magic_number_;
	uint64_t width_;
	uint64_t height_;
	uint64_t max_value_;
	matrix<vec3b> raster_;

public:
	ppm(const matrix<vec3b>& raster) {
		magic_number_ = "P6";
		raster_ = raster;
		max_value_ = 255; // assuming 8bit per pixel
		width_ = raster_.columns();
		height_ = raster_.rows();
	}

	uint64_t width() const {
		return width_;
	}

	uint64_t height() const {
		return height_;
	}

	const matrix<vec3b>& raster() const {
		return raster_;
	}

	void write(std::ofstream& output) {
		output << magic_number_ << std::endl;
		output << width_ << std::endl;
		output << height_ << std::endl;
		output << max_value_ << std::endl;

		for (uint64_t row = 0; row < height_; ++row) {
			for (uint64_t col = 0; col < width_; ++col) {
				const auto& pixel = raster_(row, col);
				raw_write(output, pixel[0]);
				raw_write(output, pixel[1]);
				raw_write(output, pixel[2]);
			}
		}
	}
};

enum class bayer_color {
	red,
	green,
	blue,
	unkwnown
};

static bayer_color get_bayer_color(int64_t row, int64_t col) {

	if (row < 0 || col < 0) {
		return bayer_color::unkwnown;
	}
	else if (row % 2 == 0 && col % 2 == 0) {
		return bayer_color::red;
	}
	else if (row % 2 == 0 && col % 2 != 0) {
		return bayer_color::green;
	}
	else if (row % 2 != 0 && col % 2 == 0) {
		return bayer_color::green;
	}
	else {
		return bayer_color::blue;
	}
}

static matrix<vec3b> create_bayer(matrix<uint8_t> source) {

	matrix<vec3b> dest(source.rows(), source.columns());

	for (uint64_t row = 0; row < source.rows(); ++row) {
		for (uint64_t col = 0; col < source.columns(); ++col) {

			const auto& pixel = source(row, col);
			auto bayer_color = get_bayer_color(row, col);

			if (bayer_color == bayer_color::red) {
				dest(row, col)[0] = pixel;
			}
			else if (bayer_color == bayer_color::green) {
				dest(row, col)[1] = pixel;
			}
			else if (bayer_color == bayer_color::blue) {
				dest(row, col)[2] = pixel;
			}
		}
	}

	return dest;
}

static vec3b resolve_value(const std::optional<vec3b>& pixel) {

	if (!pixel.has_value()) {
		return { 0,0,0 };
	}

	return pixel.value();
}

static uint8_t clamp(int16_t value) {
	if (value < 0) {
		return 0;
	}
	else if (value > 255) {
		return 255;
	}
	else {
		return static_cast<uint8_t>(value);
	}
}

static matrix<vec3b> interpolate_green(matrix<vec3b> data) {

	for (int64_t row = 0; row < static_cast<int64_t>(data.rows()); ++row) {
		for (int64_t col = 0; col < static_cast<int64_t>(data.columns()); ++col) {

			const auto& current_pixel = data(row, col);

			/*if (row < 3 || col < 3 || row > data.rows() - 4 || col > data.columns() - 4) {
				continue;
			}*/

			auto color = get_bayer_color(row, col);

			// If this is a green pixel skip
			if (color == bayer_color::green) {
				continue;
			}

			vec3b center_pixel = resolve_value(data.get_value_with_check(row, col));
			vec3b top_2_pixel = resolve_value(data.get_value_with_check(row - 2, col));
			vec3b left_2_pixel = resolve_value(data.get_value_with_check(row, col - 2));
			vec3b right_2_pixel = resolve_value(data.get_value_with_check(row, col + 2));
			vec3b bottom_2_pixel = resolve_value(data.get_value_with_check(row + 2, col));

			int16_t center_pixel_value = color == bayer_color::red ? center_pixel[0] : center_pixel[2];
			int16_t top_2_pixel_value = color == bayer_color::red ? top_2_pixel[0] : top_2_pixel[2];
			int16_t left_2_pixel_value = color == bayer_color::red ? left_2_pixel[0] : left_2_pixel[2];
			int16_t right_2_pixel_value = color == bayer_color::red ? right_2_pixel[0] : right_2_pixel[2];
			int16_t bottom_2_pixel_value = color == bayer_color::red ? bottom_2_pixel[0] : bottom_2_pixel[2];

			// Green pixels
			int16_t top_1_pixel_value = resolve_value(data.get_value_with_check(row - 1, col))[1];
			int16_t left_1_pixel_value = resolve_value(data.get_value_with_check(row, col - 1))[1];
			int16_t bottom_1_pixel_value = resolve_value(data.get_value_with_check(row + 1, col))[1];
			int16_t right_1_pixel_value = resolve_value(data.get_value_with_check(row, col + 1))[1];

			int16_t dh = std::abs(left_1_pixel_value - right_1_pixel_value) +
				std::abs(center_pixel_value - left_2_pixel_value + center_pixel_value - right_2_pixel_value);

			int16_t dv = std::abs(top_1_pixel_value - bottom_1_pixel_value) +
				std::abs(center_pixel_value - top_2_pixel_value + center_pixel_value - bottom_2_pixel_value);

			int16_t green_value = 0;

			if (dh < dv) {
				green_value = (left_1_pixel_value + right_1_pixel_value) / 2 +
					(center_pixel_value - left_2_pixel_value + center_pixel_value - right_2_pixel_value) / 4;
			}
			else if (dh > dv) {
				green_value = (top_1_pixel_value + bottom_1_pixel_value) / 2 +
					(center_pixel_value - top_2_pixel_value + center_pixel_value - bottom_2_pixel_value) / 4;
			}
			else {
				green_value = (top_1_pixel_value + left_1_pixel_value + right_1_pixel_value + bottom_1_pixel_value) / 4 +
					(center_pixel_value - top_2_pixel_value + center_pixel_value - left_2_pixel_value +
						center_pixel_value - right_2_pixel_value + center_pixel_value - bottom_2_pixel_value) / 8;
			}

			data(row, col) = { current_pixel[0] , clamp(green_value), current_pixel[2] };
		}
	}

	return data;
}

static matrix<vec3b> interpolate_red_blue(matrix<vec3b> data) {

	for (int64_t row = 0; row < static_cast<int64_t>(data.rows()); ++row) {
		for (int64_t col = 0; col < static_cast<int64_t>(data.columns()); ++col) {

			const auto& current_pixel = data(row, col);

			/*if (row < 3 || col < 3 || row > data.rows() - 4 || col > data.columns() - 4) {
				continue;
			}*/

			auto color = get_bayer_color(row, col);

			if (color == bayer_color::red || color == bayer_color::blue) {
				// Red or blue pixel

				vec3b top_left = resolve_value(data.get_value_with_check(row - 1, col - 1));
				vec3b top_right = resolve_value(data.get_value_with_check(row - 1, col + 1));
				vec3b bottom_left = resolve_value(data.get_value_with_check(row + 1, col - 1));
				vec3b bottom_right = resolve_value(data.get_value_with_check(row + 1, col + 1));

				int16_t green_center = current_pixel[1];
				int16_t green_top_left = top_left[1];
				int16_t green_top_right = top_right[1];
				int16_t green_bottom_left = bottom_left[1];
				int16_t green_bottom_right = bottom_right[1];

				int16_t other_top_left = 0;
				int16_t other_top_right = 0;
				int16_t other_bottom_left = 0;
				int16_t other_bottom_right = 0;

				if (color == bayer_color::red) {
					// red pixel
					other_top_left = top_left[2];
					other_top_right = top_right[2];
					other_bottom_left = bottom_left[2];
					other_bottom_right = bottom_right[2];
				}
				else {
					// blue pixel
					other_top_left = top_left[0];
					other_top_right = top_right[0];
					other_bottom_left = bottom_left[0];
					other_bottom_right = bottom_right[0];
				}

				int16_t dn = std::abs(other_top_left - other_bottom_right) +
					std::abs(green_center - green_top_left + green_center - green_bottom_right);

				int16_t dp = std::abs(other_top_right - other_bottom_left) +
					std::abs(green_center - green_top_right + green_center - green_bottom_left);

				int16_t other = 0;

				if (dn < dp) {
					other = (other_top_left + other_bottom_right) / 2 +
						(green_center - green_top_left + green_center - green_bottom_right) / 4;
				}
				else if (dn > dp) {
					other = (other_top_right + other_bottom_left) / 2 +
						(green_center - green_top_right + green_center - green_bottom_left) / 4;
				}
				else {
					other = (other_top_left + other_top_right + other_bottom_left + other_bottom_right) / 4 +
						(green_center - green_top_left + green_center - green_top_right +
							green_center - green_bottom_left + green_center - green_bottom_right) / 8;
				}

				if (color == bayer_color::red) {
					// red pixel
					data(row, col) = { current_pixel[0], static_cast<uint8_t>(green_center), clamp(other) };
				}
				else {
					// blue pixel
					data(row, col) = { clamp(other), static_cast<uint8_t>(green_center), current_pixel[2] };
				}
			}
			else {
				// Green pixel			
				vec3b top = resolve_value(data.get_value_with_check(row - 1, col));
				vec3b right = resolve_value(data.get_value_with_check(row, col + 1));
				vec3b bottom = resolve_value(data.get_value_with_check(row + 1, col));
				vec3b left = resolve_value(data.get_value_with_check(row, col - 1));

				if (get_bayer_color(row -1, col) == bayer_color::red || get_bayer_color(row + 1, col) == bayer_color::red) {
					// red top-bottom, blue left-right
					int16_t red_value = (top[0] + bottom[0]) / 2;
					int16_t blue_value = (left[2] + right[2]) / 2;
					data(row, col) = { clamp(red_value) , current_pixel[1], clamp(blue_value) };
				}
				else {
					// blue top-bottom, red left_pixel-right
					int16_t blue_value = (top[2] + bottom[2]) / 2;
					int16_t red_value = (left[0] + right[0]) / 2;
					data(row, col) = { clamp(red_value) , current_pixel[1], clamp(blue_value) };
				}
			}
		}
	}

	return data;
}

int main(int argc, char* argv[]) {

	if (argc != 3) {
		return EXIT_FAILURE;
	}

	std::ifstream input(argv[1], std::ios::binary);

	if (!input) {
		return EXIT_FAILURE;
	}

	std::string prefix(argv[2]);

	pgm<uint16_t> source_pgm(input);

	matrix<uint8_t> converted_matrix(source_pgm.width(), source_pgm.height());

	std::transform(source_pgm.raster().begin(), source_pgm.raster().end(), converted_matrix.begin(), [](const uint16_t& value) {
		return value / 256;
		});

	pgm<uint8_t> gray_pgm(converted_matrix);

	std::ofstream gray_pgm_file(prefix + "_gray.pgm", std::ios::binary);
	gray_pgm.write(gray_pgm_file);

	matrix<vec3b> bayer_matrix = create_bayer(converted_matrix);

	ppm bayer_ppm(bayer_matrix);
	std::ofstream bayer_ppm_file(prefix + "_bayer.ppm", std::ios::binary);
	bayer_ppm.write(bayer_ppm_file);

	matrix<vec3b> green_interpolated_matrix = interpolate_green(bayer_matrix);

	ppm bayer_green_ppm(green_interpolated_matrix);
	std::ofstream green_ppm_file(prefix + "_green.ppm", std::ios::binary);
	bayer_green_ppm.write(green_ppm_file);

	matrix<vec3b> green_red_blue_interpolated_matrix = interpolate_red_blue(green_interpolated_matrix);
	ppm bayer_interpolated_ppm(green_red_blue_interpolated_matrix);
	std::ofstream interp_ppm_file(prefix + "_interp.ppm", std::ios::binary);
	bayer_interpolated_ppm.write(interp_ppm_file);

	return EXIT_SUCCESS;
}