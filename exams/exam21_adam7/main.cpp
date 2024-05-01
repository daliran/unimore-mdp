#include <iostream>
#include <fstream>
#include <cstdint>
#include <string>
#include <vector>
#include <map>

template<typename T>
std::pair<T, bool> raw_read(std::istream& input) {
	T buffer = 0;

	input.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));

	if (input.eof()) {
		return std::pair<T, bool>(buffer, false);
	}

	return std::pair<T, bool>(buffer, true);
}

template<typename T>
void raw_write(std::ostream& output, T value) {
	output.write(reinterpret_cast<char*>(&value), sizeof(value));
}

template<typename T>
class matrix {
private:
	uint32_t rows_;
	uint32_t cols_;
	std::vector<T> data_;

public:
	matrix(uint32_t rows = 0, uint32_t columns = 0) : rows_(rows), cols_(columns) {
		resize(rows_, cols_);
	}

	void resize(uint32_t rows, uint32_t columns) {
		rows_ = rows;
		cols_ = columns;
		data_.resize(rows_ * cols_);
	}

	T get(uint32_t row, uint32_t col) const {
		return data_[row * cols_ + col];
	}

	void set(uint32_t row, uint32_t col, T value) {
		data_[row * cols_ + col] = value;
	}

	void set_row(uint32_t row, T values[]) {
		for (uint32_t col = 0; col < cols_; ++col) {
			data_[row * cols_ + col] = values[col];
		}
	}

	void print(uint32_t max_rows, uint32_t max_cols) {

		uint32_t rows_to_print = std::min(max_rows, rows_);
		uint32_t cols_to_print = std::min(max_cols, cols_);

		for (uint32_t row = 0; row < rows_to_print; ++row) {
			for (uint32_t col = 0; col < cols_to_print; ++col) {
				std::cout << get(row, col) << " ";
			}
			std::cout << std::endl << std::endl;
		}

		std::cout << std::endl << std::endl;
	}

	uint32_t rows() const {
		return rows_;
	}

	uint32_t cols() const {
		return cols_;
	}

	matrix<T> create_window(uint32_t reference_row, uint32_t reference_col, uint32_t rows, uint32_t cols) {
		matrix<T> window(rows, cols);

		for (uint32_t row = 0; row < rows; ++row) {
			for (uint32_t col = 0; col < cols; ++col) {
				T value = get(reference_row + row, reference_col + col);
				window.set(row, col, value);
			}
		}

		return window;
	}
};

class pmg_reader {
private:
	std::istream& input_;
	uint16_t width_ = 0;
	uint16_t heigth_ = 0;
	uint16_t max_value_ = 0;
	matrix<uint16_t> data_;

	// TODO: to test
	uint16_t little_ending_to_big_endian(uint16_t number) {

		uint16_t buffer = 0;

		// high byte to low byte
		for (uint16_t i = 0; i < 8; ++i) {
			uint8_t shifts = 16 - 1 - i;
			bool bit = (number >> shifts) & 1;
			uint16_t mask = 1 << (shifts - 8);
			buffer |= mask;
		}

		// low byte to high byte
		for (uint16_t i = 0; i < 8; ++i) {
			uint8_t shifts = 8 - 1 - i;
			bool bit = (number >> shifts) & 1;
			uint16_t mask = 1 << (shifts + 8);
			buffer |= mask;
		}

		return buffer;
	}

public:
	pmg_reader(std::istream& input) : input_(input) { }

	bool read() {
		std::string magic_number;
		input_ >> magic_number;
		input_ >> std::ws;

		char read_char = input_.peek();
		bool is_comment = read_char == '#';

		if (is_comment) {
			std::string comment;
			std::getline(input_, comment);
			input_ >> std::ws;
		}

		std::string raw_width;
		input_ >> raw_width;
		width_ = std::stoi(raw_width);

		std::string raw_heigth;
		input_ >> raw_heigth;
		heigth_ = std::stoi(raw_heigth);

		input_ >> std::ws;

		data_.resize(heigth_, width_);

		std::string raw_max_value;
		input_ >> raw_max_value;
		max_value_ = std::stoi(raw_max_value);

		input_ >> std::ws;

		for (uint16_t h = 0; h < heigth_; ++h) {
			for (uint16_t w = 0; w < width_; ++w) {

				uint16_t read_value = 0;

				if (max_value_ < 256) { // 1 byte

					auto result = raw_read<uint8_t>(input_);

					if (!result.second) {
						return false;
					}

					read_value = result.first;
				}
				else { // 2 bytes big endian

					auto result = raw_read<uint16_t>(input_);

					if (!result.second) {
						return false;
					}

					read_value = little_ending_to_big_endian(result.first);
				}

				data_.set(h, w, read_value);
			}
		}

		//data_.print(8, 24);

		return true;
	}

	uint32_t width() const {
		return width_;
	}

	uint32_t height() const {
		return heigth_;
	}

	uint32_t max_value() const {
		return max_value_;
	}

	const matrix<uint16_t> data() const {
		return data_;
	}
};

class mlt_writer {
private:
	std::ostream& output_;
	matrix<uint32_t> pattern_;
	std::map<uint32_t, std::vector<uint16_t>> levels_;

	void apply_pattern_over_image(matrix<uint16_t> pmg_data) {

		for (uint32_t current_row_pixel = 0; current_row_pixel < pmg_data.rows();) {

			uint32_t remaining_row_pixels = pmg_data.rows() - current_row_pixel;
			uint32_t pixels_in_row = std::min<uint32_t>(8, remaining_row_pixels);

			for (uint32_t current_column_pixel = 0; current_column_pixel < pmg_data.cols();) {

				uint32_t remaining_column_pixels = pmg_data.cols() - current_column_pixel;
				uint32_t pixels_in_column = std::min<uint32_t>(8, remaining_column_pixels);

				matrix<uint16_t> t = pmg_data.create_window(current_row_pixel, current_column_pixel, pixels_in_row, pixels_in_column);

				apply_pattern_over_window(t);

				current_column_pixel += pixels_in_column;
			}

			current_row_pixel += pixels_in_row;
		}
	}

	void apply_pattern_over_window(matrix<uint16_t>& window) {

		for (uint32_t row = 0; row < window.rows(); ++row) {
			for (uint32_t col = 0; col < window.cols(); ++col) {
				uint32_t level = pattern_.get(row, col);
				uint16_t pixel = window.get(row, col);
				levels_[level].push_back(pixel);
			}
		}
	}

public:
	mlt_writer(std::ostream& output) : output_(output) {

		pattern_.resize(8, 8);

		uint32_t row0[] = { 1, 6, 4, 6, 2, 6, 4, 6 };
		pattern_.set_row(0, row0);

		uint32_t row1[] = { 7, 7, 7, 7, 7, 7, 7, 7 };
		pattern_.set_row(1, row1);

		uint32_t row2[] = { 5, 6, 5, 6, 5, 6, 5, 6 };
		pattern_.set_row(2, row2);

		uint32_t row3[] = { 7, 7, 7, 7, 7, 7, 7, 7 };
		pattern_.set_row(3, row3);

		uint32_t row4[] = { 3, 6, 4, 6, 3, 6, 4, 6 };
		pattern_.set_row(4, row4);

		uint32_t row5[] = { 7, 7, 7, 7, 7, 7, 7, 7 };
		pattern_.set_row(5, row5);

		uint32_t row6[] = { 5, 6 ,5, 6, 5, 6, 5, 6 };
		pattern_.set_row(6, row6);

		uint32_t row7[] = { 7, 7, 7, 7, 7, 7, 7, 7 };
		pattern_.set_row(7, row7);
	}

	void write_pmg(const pmg_reader& pmg) {

		output_ << "MULTIRES";

		raw_write<uint32_t>(output_, pmg.width());
		raw_write<uint32_t>(output_, pmg.height());

		apply_pattern_over_image(pmg.data());

		for (auto& level : levels_) {
			for (auto& level_item : level.second) {

				if (pmg.max_value() > 255) {
					raw_write<uint16_t>(output_, level_item);
				}
				else {
					raw_write<uint8_t>(output_, level_item);
				}
			}
		}
	}
};

class mlt_reader {
private:
	std::istream& input_;
	matrix<uint16_t> data_; 

public:
	mlt_reader(std::istream& input) : input_(input) { }

	bool read() {

		for (uint8_t i = 0; i < 8; ++i) {
			auto char_result = raw_read<uint8_t>(input_);

			if (!char_result.second) {
				return false;
			}
		}

		auto width_result = raw_read<uint32_t>(input_);

		if (!width_result.second) {
			return false;
		}

		auto height_result = raw_read<uint32_t>(input_);

		if (!height_result.second) {
			return false;
		}

		uint32_t number_of_blocks = (width_result.first * height_result.first) / 8;
		uint32_t partial_vertical_block = width_result.first % 8;
		uint32_t partial_horizontal_block = height_result.first % 8;

		// TODO: final bottom right corner



		// Assumption: the reading will only have pixels of 8 bits
		auto level_value = raw_read<uint8_t>(input_);





		// TODO: read the levels

		return true;
	}

};

class pmg_writer {
private:
	std::string prefix_;

public:
	pmg_writer(std::string prefix) : prefix_(prefix) {}

	void write(const mlt_reader& mlt) {

	}
};

int main(int argc, char* argv[]) {

	if (argc != 4) {
		std::cerr << "Invalid number of arguments" << std::endl;
		return EXIT_FAILURE;
	}

	std::string mode(argv[1]);

	if (!(mode == "c" || mode == "d")) {
		std::cerr << "The mode must be a single character with value either c or d" << std::endl;
		return EXIT_FAILURE;
	}

	bool compress = mode == "c";

	std::ifstream input(argv[2], std::ios::binary);

	if (!input) {
		std::cerr << "Failed to open the input file" << std::endl;
		return EXIT_FAILURE;
	}

	if (compress) { // pgm to mlt

		std::ofstream output(argv[3], std::ios::binary);

		if (!output) {
			std::cerr << "Failed to open the output file" << std::endl;
			return EXIT_FAILURE;
		}

		pmg_reader reader(input);

		if (!reader.read()) {
			std::cerr << "Failed to read the input file" << std::endl;
			return EXIT_FAILURE;
		}

		mlt_writer writer(output);
		writer.write_pmg(reader);
	}
	else { // mlt to 7 pgm files using the prefix

		mlt_reader reader(input);

		if (!reader.read()) {
			std::cerr << "Failed to read the input file" << std::endl;
			return EXIT_FAILURE;
		}

		pmg_writer writer(argv[3]);
		writer.write(reader);
	}

	return EXIT_SUCCESS;
}