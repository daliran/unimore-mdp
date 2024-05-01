#include <iostream>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>
#include <cstdint>

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
};

struct pam_header {
	std::string magic_number;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t depth = 0;
	uint32_t max_value = 0;
	std::string tuple_type;
};

class grayscale_pam {
private:
	std::string file_name_;
	pam_header header_;
	matrix<uint8_t> data_;

public:
	grayscale_pam(const std::string& file_name) : file_name_(file_name) { }

	bool load() {
		std::ifstream input(file_name_, std::ios::binary);

		if (!input) {
			return false;
		}

		input >> header_.magic_number;

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

		std::string end_header;
		input >> end_header;

		input.ignore(1);

		data_.resize(header_.height, header_.width);

		for (uint32_t row = 0; row < header_.height; ++row) {
			for (uint32_t column = 0; column < header_.width; ++column) {

				auto pixel_result = raw_read<uint8_t>(input);

				if (!pixel_result.second) {
					return false;
				}

				data_.set_value(row, column, pixel_result.first);
			}
		}

		return true;
	}

	const pam_header& header() const {
		return header_;
	}

	const matrix<uint8_t>& data() const {
		return data_;
	}
};

class pam_combiner {
private:
	const grayscale_pam& red_channel_;
	const grayscale_pam& green_channel_;
	const grayscale_pam& blue_channel_;
public:
	pam_combiner(const grayscale_pam& red_channel, const grayscale_pam& green_channel, const grayscale_pam& blue_channel)
		: red_channel_(red_channel), green_channel_(green_channel), blue_channel_(blue_channel) {}

	bool combine(const std::string& file_name) {
		std::ofstream output(file_name + "_reconstructed.pam", std::ios::binary);

		if (!output) {
			return false;
		}

		const pam_header& red_header = red_channel_.header();

		output << "P7" << std::endl;
		output << "WIDTH " << red_header.width << std::endl;
		output << "HEIGHT " << red_header.height << std::endl;
		output << "DEPTH " << 3 << std::endl;
		output << "MAXVAL " << red_header.max_value << std::endl;
		output << "TUPLTYPE " << "RGB" << std::endl;
		output << "ENDHDR" << std::endl;

		for (uint32_t row = 0; row < red_header.height; ++row) {
			for (uint32_t column = 0; column < red_header.width; ++column) {
				auto red = red_channel_.data().get_value(row, column);
				auto green = green_channel_.data().get_value(row, column);
				auto blue = blue_channel_.data().get_value(row, column);
				raw_write(output, red);
				raw_write(output, green);
				raw_write(output, blue);	
			}
		}

		return true;
	}
};

int main(int argc, char* argv[]) {

	if (argc != 2) {
		std::cerr << "Invalid number of arguments" << std::endl;
		return EXIT_FAILURE;
	}

	std::string file_name(argv[1]);

	grayscale_pam red_pam(file_name + "_R.pam");

	if (!red_pam.load()) {
		std::cerr << "Failed to load the red pam file" << std::endl;
		return EXIT_FAILURE;
	}

	grayscale_pam green_pam(file_name + "_G.pam");

	if (!green_pam.load()) {
		std::cerr << "Failed to load the green pam file" << std::endl;
		return EXIT_FAILURE;
	}

	grayscale_pam blue_pam(file_name + "_B.pam");

	if (!blue_pam.load()) {
		std::cerr << "Failed to load the blue pam file" << std::endl;
		return EXIT_FAILURE;
	}

	pam_combiner combiner(red_pam, green_pam, blue_pam);

	if (!combiner.combine(file_name)) {
		std::cerr << "Failed to combine the pam files" << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}