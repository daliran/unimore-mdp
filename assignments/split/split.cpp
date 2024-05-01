#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <vector>
#include <tuple>

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
	uint32_t columns_;
	std::vector<T> data_;

public:
	matrix(uint32_t rows = 0, uint32_t columns = 0) : rows_(rows), columns_(columns) {
		resize(rows_, columns_);
	}

	void resize(uint32_t rows, uint32_t columns) {
		rows_ = rows;
		columns_ = columns;
		data_.resize(rows_ * columns_);
	}

	T get_value(uint32_t row, uint32_t column) const {
		return data_[row * columns_ + column];
	}

	void set_value(uint32_t row, uint32_t column, T value) {
		data_[row * columns_ + column] = value;
	}

	uint32_t rows() const {
		return rows_;
	}

	uint32_t columns() const {
		return columns_;
	}
};

struct pam_header {
	std::string magic_number;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t depth = 0;
	uint32_t max_value = 0;
	std::string tuple_type;
};

class pam {
private:
	std::string file_path_;
	pam_header header_;
	matrix<std::tuple<uint8_t, uint8_t, uint8_t>> data_;

	void write_header(std::ostream& output) const {
		output << header_.magic_number << std::endl;
		output << "WIDTH " << header_.width << std::endl;
		output << "HEIGHT " << header_.height << std::endl;
		output << "DEPTH " << 1 << std::endl;
		output << "MAXVAL " << header_.max_value << std::endl;
		output << "TUPLTYPE " << "GRAYSCALE" << std::endl;
		output << "ENDHDR" << std::endl;
	}

public:
	pam(const std::string& file_path) : file_path_(file_path) {}

	bool load() {

		std::ifstream input(file_path_, std::ios::binary);

		if (!input) {
			return false;
		}

		std::string magic_number;
		input >> magic_number;
		header_.magic_number = magic_number;

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

		std::string tuple_type;
		input >> tuple_type >> tuple_type;
		header_.tuple_type = tuple_type;

		std::string end_of_header;
		input >> end_of_header;

		input.ignore(1);

		data_.resize(header_.height, header_.width);

		for (uint32_t row = 0; row < header_.height; ++row) {
			for (uint32_t column = 0; column < header_.width; ++column) {

				auto r_result = raw_read<uint8_t>(input);

				if (!r_result.second) {
					return false;
				}

				auto g_result = raw_read<uint8_t>(input);

				if (!g_result.second) {
					return false;
				}

				auto b_result = raw_read<uint8_t>(input);

				if (!b_result.second) {
					return false;
				}

				std::tuple<uint8_t, uint8_t, uint8_t> rgb = std::make_tuple(r_result.first, g_result.first, b_result.first);
				data_.set_value(row, column, rgb);
			}
		}

		return true;
	}

	bool save_channels() {

		auto pos = file_path_.find_last_of('.');
		std::string prefix = file_path_.substr(0, pos);
		std::string extension = file_path_.substr(pos);

		std::ofstream r_output(prefix + "_R" + extension, std::ios::binary);

		if (!r_output) {
			return false;
		}

		write_header(r_output);

		std::ofstream g_output(prefix + "_G" + extension, std::ios::binary);

		if (!g_output) {
			return false;
		}

		write_header(g_output);

		std::ofstream b_output(prefix + "_B" + extension, std::ios::binary);

		if (!b_output) {
			return false;
		}

		write_header(b_output);

		for (uint32_t row = 0; row < header_.height; ++row) {
			for (uint32_t column = 0; column < header_.width; ++column) {

				auto rgb = data_.get_value(row, column);

				uint8_t r = std::get<0>(rgb);
				raw_write(r_output, r);

				uint8_t g = std::get<1>(rgb);
				raw_write(g_output, g);

				uint8_t b = std::get<2>(rgb);
				raw_write(b_output, b);
			}
		}

		return true;
	}
};


int main(int argc, char* argv[]) {

	if (argc < 2) {
		std::cerr << "Invalid number of arguments";
		return EXIT_FAILURE;
	}

	std::string file_path(argv[1]);

	pam p(file_path);

	if (!p.load()) {
		std::cerr << "Cannot load pam file";
		return EXIT_FAILURE;
	}

	if (!p.save_channels()) {
		std::cerr << "Cannot save channels";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}