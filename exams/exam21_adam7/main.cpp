#include <fstream>
#include <iostream>
#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <array>

uint8_t adam7_pattern[8][8]{
		{1, 6, 4, 6, 2, 6, 4, 6},
		{7, 7, 7, 7, 7, 7, 7, 7},
		{5, 6, 5, 6, 5, 6, 5, 6},
		{7, 7, 7, 7, 7, 7, 7, 7},
		{3, 6, 4, 6, 3, 6, 4, 6},
		{7, 7, 7, 7, 7, 7, 7, 7},
		{5, 6, 5, 6, 5, 6, 5, 6},
		{7, 7, 7, 7, 7, 7, 7, 7}
};

using coord = std::array<uint8_t, 2>;

std::map<uint8_t, std::vector<coord>> adam7_map = {
	std::make_pair<uint8_t, std::vector<coord>>(1, {{0, 0}}),
	std::make_pair<uint8_t, std::vector<coord>>(2, {{0, 4}}),
	std::make_pair<uint8_t, std::vector<coord>>(3, {{4, 0}, {4, 4}}),
	std::make_pair<uint8_t, std::vector<coord>>(4, {{0, 2}, {0, 6}, {4, 2}, {4, 6}}),
	std::make_pair<uint8_t, std::vector<coord>>(5, {{2, 0}, {2, 2}, {2, 4}, {2, 6}, {6, 0}, {6, 2}, {6, 4}, {6, 6}}),
	std::make_pair<uint8_t, std::vector<coord>>(6, {{0, 1}, {0, 3}, {0, 5}, {0, 7}, {2, 1}, {2, 3}, {2, 5}, {2, 7}, {4, 1}, {4, 3}, {4, 5}, {4, 7}, {6, 1}, {6, 3}, {6, 5}, {6, 7}}),
	std::make_pair<uint8_t, std::vector<coord>>(7, {{1, 0}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {3, 0}, {3, 1}, {3, 2}, {3, 3}, {3, 4}, {3, 5}, {3, 6}, {3, 7}, {5, 0}, {5, 1}, {5, 2}, {5, 3}, {5, 4}, {5, 5}, {5, 6}, {5, 7}, {7, 0}, {7, 1}, {7, 2}, {7, 3}, {7, 4}, {7, 5}, {7, 6}, {7, 7}})
};

template<typename T>
T raw_read(std::istream& input, size_t size = sizeof(T)) {
	T value = 0;
	input.read(reinterpret_cast<char*>(&value), size);
	return value;
}

template<typename T>
void raw_write(std::ostream& output, T value) {
	output.write(reinterpret_cast<char*>(&value), sizeof(value));
}

template <typename T>
class matrix {
private:
	uint32_t rows_ = 0;
	uint32_t cols_ = 0;
	std::vector<T> data_;

public:
	matrix() {}

	matrix(uint32_t rows, uint32_t cols) {
		resize(rows, cols);
	}

	void resize(uint32_t rows, uint32_t cols) {
		rows_ = rows;
		cols_ = cols;
		data_.resize(rows_ * cols_);
	}

	T& operator()(uint32_t row, uint32_t col) {
		return data_[row * cols_ + col];
	}

	const T& operator()(uint32_t row, uint32_t col) const {
		return data_[row * cols_ + col];
	}

	uint32_t rows() const { return rows_; }
	uint32_t cols() const { return cols_; }
	const auto begin() const { return data_.begin(); }
	const auto end() const { return data_.end(); }
};

static matrix<uint8_t> load_pgm(std::ifstream& input) {

	std::string magic_number;
	input >> magic_number >> std::ws;

	if (input.peek() == '#') {
		std::string comment;
		std::getline(input, comment);
	}

	std::string width;
	input >> width >> std::ws;

	std::string height;
	input >> height >> std::ws;

	std::string max_value;
	input >> max_value >> std::ws;

	matrix<uint8_t> raster(std::stoi(height), std::stoi(width));

	for (uint32_t row = 0; row < raster.rows(); ++row) {
		for (uint32_t col = 0; col < raster.cols(); ++col) {
			raster(row, col) = raw_read<uint8_t>(input);
		}
	}

	return raster;
}

static std::map<uint8_t, std::vector<uint8_t>> apply_adam7_pattern(const matrix<uint8_t>& image) {

	std::map<uint8_t, std::vector<uint8_t>> images;

	for (uint32_t row = 0; row < image.rows(); ++row) {
		for (uint32_t col = 0; col < image.cols(); ++col) {

			// Calculate the row and column in the pattern depeding on row and col in the image
			uint8_t pattern_row = row % 8;
			uint8_t pattern_col = col % 8;

			// Get the index in the images based on the pattern
			uint8_t vector_index = adam7_pattern[pattern_row][pattern_col];

			// Assign the pixel
			images[vector_index].push_back(image(row, col));
		}
	}

	return images;
}

static bool compress(const std::string& input_file, const std::string& output_file) {

	std::ifstream input(input_file, std::ios::binary);

	if (!input) {
		std::cerr << "Failed to open input file" << std::endl;
		return false;
	}

	matrix<uint8_t> pgm_image = load_pgm(input);
	std::map<uint8_t, std::vector<uint8_t>> adam7_images = apply_adam7_pattern(pgm_image);

	std::ofstream output(output_file, std::ios::binary);

	if (!output) {
		std::cerr << "Failed to open output file" << std::endl;
		return false;
	}

	output << "MULTIRES";
	raw_write(output, pgm_image.cols());
	raw_write(output, pgm_image.rows());

	for (const auto& image : adam7_images) {
		for (const auto& pixel : image.second) {
			raw_write(output, pixel);
		}
	}

	return true;
}

static void write_pgm(std::string prefix, uint8_t index, const matrix<uint8_t>& image) {

	std::string file_name = prefix + "_" + std::to_string(index) + ".pgm";

	std::ofstream output(file_name, std::ios::binary);

	if (!output) {
		std::cerr << "Failed to open output file" << std::endl;
		return;
	}

	output << "P5" << std::endl;
	output << image.cols() << " ";
	output << image.rows() << std::endl;
	output << 255 << std::endl;

	for (uint32_t row = 0; row < image.rows(); ++row) {
		for (uint32_t col = 0; col < image.cols(); ++col) {
			uint8_t pixel = image(row, col);
			raw_write(output, pixel);
		}
	}
}

static matrix<uint8_t> compute_level(matrix<uint8_t> image, uint8_t level, std::vector<uint8_t>& level_data) {

	uint8_t level_rows = 0;

	switch (level) {
	case 1:
	case 2:
		level_rows = 8; break;
	case 3:
	case 4:
		level_rows = 4; break;
	case 5:
	case 6:
		level_rows = 2; break;
	case 7:
		level_rows = 1; break;
	}

	uint8_t level_cols = 0;

	switch (level) {
	case 1:
		level_cols = 8; break;
	case 2:
	case 3:
		level_cols = 4; break;
	case 4:
	case 5:
		level_cols = 2; break;
	case 6:
	case 7:
		level_cols = 1; break;
	}

	// TODO: missing image offset increate
	// TODO: don't increase the level_data index if the mapped coord is outside the image 

	// For every value stored for the given level
	for (size_t i = 0; i < level_data.size(); ++i) {

		size_t map_level_size = adam7_map[level].size();
		auto index = i % map_level_size;

		// Find the coordinate of the value in the adam7 matrix
		auto& coord = adam7_map[level][index];

		// Loop the window (offset from the coordinate)
		for (uint32_t row = 0; row < level_rows; ++row) {
			for (uint32_t col = 0; col < level_cols; ++col) {

				auto current_row = coord[0] + row;

				// Skip if outside the window
				if (current_row > image.rows() - 1) {
					continue;
				}

				auto current_col = coord[1] + col;

				// Skip if outside the window
				if (current_col > image.cols() - 1) {
					continue;
				}

				// Set the image value
				image(current_row, current_col) = level_data[index];
			}
		}
	}

	return image;
}

static std::vector<uint32_t> get_adam7_levels_from_size(uint32_t width, uint32_t height) {

	std::vector<uint32_t> levels(7);

	for (uint32_t row = 0; row < height; ++row) {
		for (uint32_t col = 0; col < width; ++col) {

			// Calculate the row and column in the pattern depeding on row and col in the image
			uint8_t pattern_row = row % 8;
			uint8_t pattern_col = col % 8;

			size_t adam7_value = adam7_pattern[pattern_row][pattern_col];

			// Increase the number of pixels
			levels[adam7_value - 1]++;
		}
	}

	return levels;
}

static bool decompress(const std::string& input_file, const std::string& prefix) {

	std::ifstream input(input_file, std::ios::binary);

	if (!input) {
		std::cerr << "Failed to open input file" << std::endl;
		return false;
	}

	std::string magic_string(8, 0);
	input.read(reinterpret_cast<char*>(magic_string.data()), magic_string.size());

	uint32_t width = raw_read<uint32_t>(input);
	uint32_t height = raw_read<uint32_t>(input);

	std::vector<uint32_t> levels = get_adam7_levels_from_size(width, height);

	std::map<uint8_t, std::vector<uint8_t>> levels_data;

	for (uint32_t i = 0; i < levels.size(); ++i) {
		for (uint32_t value = 0; value < levels[i]; ++value) {
			uint8_t pixel = raw_read<uint8_t>(input);
			levels_data[i + 1].push_back(pixel);
		}
	}

	matrix<uint8_t> image1 = compute_level(matrix<uint8_t>(height, width), 1, levels_data[1]);
	write_pgm(prefix, 1, image1);

	matrix<uint8_t> image2 = compute_level(image1, 2, levels_data[2]);
	write_pgm(prefix, 2, image2);

	matrix<uint8_t> image3 = compute_level(image2, 3, levels_data[3]);
	write_pgm(prefix, 3, image3);

	matrix<uint8_t> image4 = compute_level(image3, 4, levels_data[4]);
	write_pgm(prefix, 4, image4);

	matrix<uint8_t> image5 = compute_level(image4, 5, levels_data[5]);
	write_pgm(prefix, 5, image5);

	matrix<uint8_t> image6 = compute_level(image5, 6, levels_data[6]);
	write_pgm(prefix, 6, image6);

	matrix<uint8_t> image7 = compute_level(image6, 7, levels_data[7]);
	write_pgm(prefix, 7, image7);

	return true;
}

int main(int argc, char* argv[]) {

	if (argc != 4) {
		std::cerr << "Wrong number or arguments" << std::endl;
		return EXIT_FAILURE;
	}

	std::string mode(argv[1]);

	bool compress_mode = (mode == "c");

	if (compress_mode) {
		if (!compress(argv[2], argv[3])) {
			return EXIT_FAILURE;
		}
	}
	else {
		if (!decompress(argv[2], argv[3])) {
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}