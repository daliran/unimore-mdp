#include <fstream>
#include <iostream>
#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <filesystem>
#include <algorithm>

using pixel = std::array<uint8_t, 4>;

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

	const T& operator() (uint64_t row, uint64_t column) const {
		return data_[row * columns_ + column];
	}

	T& operator() (uint64_t row, uint64_t column) {
		return data_[row * columns_ + column];
	}

	uint64_t rows() const { return rows_; }
	uint64_t columns() const { return columns_; }

	auto begin() { return data_.begin(); }
	auto end() { return data_.end(); }
	const auto begin() const { return data_.begin(); }
	const auto end() const { return data_.end(); }
};

class pam_image {
private:

	struct pam_header {
		std::string magic_number;
		uint64_t width = 0;
		uint64_t height = 0;
		uint64_t depth = 0;
		uint64_t max_value = 0;
		std::string tuple_type;
	};

	matrix<pixel> image_;
	pam_header header_;

public:
	pam_image(std::string& file_name) {

		std::ifstream input(file_name, std::ios::binary);

		input >> header_.magic_number;

		char comment_char = input.peek();

		if (comment_char == '#') {
			std::string comment;
			input >> comment;
		}

		std::string width;
		input >> width >> width;
		header_.width = std::stoi(width);

		std::string height;
		input >> height >> height;
		header_.height = std::stoi(height);

		std::string depth;
		input >> depth >> depth;
		header_.depth = std::stoi(depth);

		std::string max_value;
		input >> max_value >> max_value;
		header_.max_value = std::stoi(max_value);

		input >> header_.tuple_type >> header_.tuple_type;

		std::string end_of_header;
		input >> end_of_header;

		input.ignore(1);

		image_.resize(header_.height, header_.width);

		if (header_.max_value > 255) {
			std::cerr << "Max value greater than 255" << std::endl;
			return;
		}

		for (uint64_t row = 0; row < header_.height; ++row) {
			for (uint64_t column = 0; column < header_.width; ++column) {
				pixel pixel_data = {};
				input.read(reinterpret_cast<char*>(&pixel_data), header_.depth);

				if (header_.depth != 4) {
					// Set the pixel as opaque
					pixel_data[3] = static_cast<uint8_t>(header_.max_value);
				}
			
				image_(row, column) = pixel_data;
			}
		}
	}

	pam_image(const matrix<pixel>& raw_image, uint64_t depth) {

		header_.magic_number = "P7";
		header_.width = raw_image.columns();
		header_.height = raw_image.rows();
		header_.depth = depth;
		header_.max_value = 255;
		header_.tuple_type = depth == 3 ? "RGB" : "RGB_ALPHA";

		image_.resize(header_.height, header_.width);
		std::copy(raw_image.begin(), raw_image.end(), image_.begin());
	}

	bool write(std::string& file_name) {
		std::ofstream output(file_name, std::ios::binary);

		if (!output) {
			return false;
		}

		output << "P7" << std::endl;
		output << "WIDTH " << header_.width << std::endl;
		output << "HEIGHT " << header_.height << std::endl;
		output << "DEPTH " << header_.depth << std::endl;
		output << "MAXVAL " << header_.max_value << std::endl;
		output << "TUPLTYPE " << header_.tuple_type << std::endl;
		output << "ENDHDR" << std::endl;

		for (uint64_t row = 0; row < header_.height; ++row) {
			for (uint64_t column = 0; column < header_.width; ++column) {
				const auto& pixel = image_(row, column);
				output.write(reinterpret_cast<const char*>(&pixel), header_.depth);
			}
		}

		return true;
	}

	const pam_header& header() const { return header_; };

	const matrix<pixel>& image_data() const { return image_; }
};

struct image_with_offset {
	pam_image image;
	uint64_t x_offset = 0;
	uint64_t y_offset = 0;
};

template<typename FROM, typename TO>
static TO map(FROM value, FROM from_min, FROM from_max, TO to_min, TO to_max) {
	double pos_percentage = static_cast<double>(value) / (from_max - from_min);
	return static_cast<TO>((pos_percentage * to_max) - to_min);
}

template<typename T>
static T calculate_alpha_value(T alpha_a, T alpha_b, T max_value) {
	return alpha_a + (alpha_b * (max_value - alpha_a));
}

template<typename T>
static T calculate_color_value(T color_a, T alpha_a, T color_b,
	T alpha_b, T alpha, T max_value) {

	return ((color_a * alpha_a) + (color_b * alpha_b * (max_value - alpha_a))) / alpha;
}

static pam_image combine_images(const std::vector<image_with_offset>& images) {

	uint64_t max_width = 0;
	uint64_t max_height = 0;

	for (const auto& item : images) {
		const auto& header = item.image.header();

		uint64_t calculated_width = header.width + item.x_offset;

		if (calculated_width > max_width) {
			max_width = calculated_width;
		}

		uint64_t calculated_height = header.height + item.y_offset;

		if (calculated_height > max_height) {
			max_height = calculated_height;
		}
	}

	// This is by default a dark background
	matrix<pixel> raw_image(max_height, max_width);

	// Combine images
	for (uint64_t i = 0; i < images.size(); ++i) {

		const auto& current_image = images[i];
		const auto& image_header = current_image.image.header();
		const auto& image_data = current_image.image.image_data();

		uint8_t casted_max_value = static_cast<uint8_t>(image_header.max_value);

		for (uint64_t row = 0; row < max_height; ++row) {
			for (uint64_t column = 0; column < max_width; ++column) {

				int64_t image_row = row - current_image.y_offset;
				int64_t image_column = column - current_image.x_offset;

				if (row >= current_image.y_offset && column >= current_image.x_offset && 
					image_row < image_data.rows() && image_column < image_data.columns()) {

					double red_a = image_data(image_row, image_column)[0];
					double red_b = raw_image(row, column)[0];

					double green_a = image_data(image_row, image_column)[1];
					double green_b = raw_image(row, column)[1];

					double blue_a = image_data(image_row, image_column)[2];
					double blue_b = raw_image(row, column)[2];
					
					double alpha_a = map<uint8_t, double>(image_data(image_row, image_column)[3], 0, casted_max_value, 0, 1);
					double alpha_b = map<uint8_t, double>(raw_image(row, column)[3], 0, casted_max_value, 0, 1);					

					double alpha = calculate_alpha_value<double>(alpha_a, alpha_b, 1.0);
					double red = calculate_color_value<double>(red_a, alpha_a, red_b, alpha_b, alpha, 1.0);
					double green = calculate_color_value<double>(green_a, alpha_a, green_b, alpha_b, alpha, 1.0);
					double blue = calculate_color_value<double>(blue_a, alpha_a, blue_b, alpha_b, alpha, 1.0);

					uint8_t calculated_red = static_cast<uint8_t>(red);
					uint8_t calculated_green = static_cast<uint8_t>(green);
					uint8_t calculated_blue = static_cast<uint8_t>(blue);

					uint8_t calculated_alpha = map<double, uint8_t>(alpha, 0, 1, 0, casted_max_value);

					raw_image(row, column) = { calculated_red, calculated_green, calculated_blue, calculated_alpha };
				}
			}
		}
	}

	return pam_image(raw_image, 4);
}

int main(int argc, char* argv[]) {

	if (argc < 3) {
		std::cerr << "At least an output image and an input image are required";
		return EXIT_FAILURE;
	}

	std::string output_file(argv[1]);
	output_file += ".pam";

	std::vector<image_with_offset> read_images;

	for (int i = 0; i < argc - 2;) {

		uint32_t current_param_index = i + 2;

		std::string param(argv[current_param_index]);

		if (param == "-p") {
			std::string x_offset(argv[current_param_index + 1]);
			std::string y_offset(argv[current_param_index + 2]);

			std::string input_file(argv[current_param_index + 3]);
			input_file += ".pam";

			if (!std::filesystem::exists(std::filesystem::path(input_file))) {
				std::cerr << "file: " << input_file << " not found" << std::endl;
				return EXIT_FAILURE;
			}

			read_images.emplace_back(pam_image(input_file), std::stoi(x_offset), std::stoi(y_offset));
			i += 4;
		}
		else {
			param += ".pam";
			read_images.emplace_back(pam_image(param));
			i += 1;
		}
	}

	pam_image combined_image = combine_images(read_images);
	combined_image.write(output_file);

	return EXIT_SUCCESS;
}