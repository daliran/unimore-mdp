#include <fstream>
#include <iostream>
#include <cstdint>
#include <vector>

template <typename T>
T raw_read(std::istream& input, size_t size = sizeof(T)) {
	T value = 0;
	input.read(reinterpret_cast<char*>(&value), size);
	return value;
}

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

	uint64_t rows() const { return rows_; }
	uint64_t columns() const { return columns_; }
};

static matrix<uint8_t> read_tiff(std::ifstream& input) {

	// For this exercise we only read little endian numbers
	uint16_t byte_order = raw_read<uint16_t>(input);

	uint16_t tiff_identifier = raw_read<uint16_t>(input);

	// From the beginning of the file
	uint32_t ifd_offset = raw_read<uint32_t>(input);

	// Move to the ifd offset
	input.seekg(ifd_offset, std::ios::beg);

	uint32_t image_width = 0; // width, cols
	uint32_t image_length = 0; // height, rows
	uint32_t bits_per_sample = 0;
	uint32_t compression = 10;
	uint32_t photometric_interpretation = 0;
	uint32_t rows_per_strip = 0;

	// For this exercise we don't consider arrays -> only one strip
	uint32_t strip_offsets = 0;
	uint32_t strip_byte_counts = 0;

	// For this exercise we only read the first ifd
	uint16_t number_of_directories = raw_read<uint16_t>(input);

	for (uint64_t i = 0; i < number_of_directories; ++i) {
		uint16_t tag = raw_read<uint16_t>(input);
		uint16_t field_type = raw_read<uint16_t>(input);
		uint32_t number_of_values = raw_read<uint32_t>(input, 4);
		uint32_t value_offset = raw_read<uint32_t>(input, 4);

		if (tag == 256) {
			image_width = value_offset;
		}
		else if (tag == 257) {
			image_length = value_offset;
		}
		else if (tag == 258) {
			bits_per_sample = value_offset;
		}
		else if (tag == 259) {
			compression = value_offset;
		}
		else if (tag == 262) {
			photometric_interpretation = value_offset;
		}
		else if (tag == 273) {
			strip_offsets = value_offset;
		}
		else if (tag == 278) {
			rows_per_strip = value_offset;
		}
		else if (tag == 279) {
			strip_byte_counts = value_offset;
		}
	}

	input.seekg(strip_offsets, std::ios::beg);

	matrix<uint8_t> raster(image_length, image_width);

	for (uint64_t row = 0; row < raster.rows(); ++row) {
		for (uint64_t col = 0; col < raster.columns(); ++col) {
			raster(row, col) = raw_read<uint8_t>(input);
		}
	}

	return raster;
}

static void write_pam(std::ofstream& output, matrix<uint8_t> raster) {

	output << "P7" << std::endl;
	output << "WIDTH " << raster.columns() << std::endl;
	output << "HEIGHT " << raster.rows() << std::endl;
	output << "DEPTH " << 1 << std::endl;
	output << "MAXVAL " << 255 << std::endl;
	output << "TUPLTYPE " << "GRAYSCALE" << std::endl;
	output << "ENDHDR" << std::endl;

	for (uint64_t row = 0; row < raster.rows(); ++row) {
		for (uint64_t col = 0; col < raster.columns(); ++col) {
			uint8_t pixel = raster(row, col);
			output.write(reinterpret_cast<char*>(&pixel), sizeof(pixel));
		}
	}
}

int main(int argc, char* argv[]) {

	if (argc != 3) {
		return EXIT_FAILURE;
	}

	std::ifstream input(argv[1], std::ios::binary);

	if (!input) {
		return EXIT_FAILURE;
	}

	matrix<uint8_t> tiff_data = read_tiff(input);

	std::ofstream output(argv[2], std::ios::binary);

	if (!output) {
		return EXIT_FAILURE;
	}

	write_pam(output, tiff_data);

	return EXIT_SUCCESS;
}