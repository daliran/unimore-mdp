#include <fstream>
#include <cstdint>
#include <array>
#include <vector>
#include <cmath>

using palette_item = std::array<uint8_t, 4>;
using pixel_data = std::array<uint8_t, 3>;

template<typename T>
void raw_write(std::ostream& output, T value) {
	output.write(reinterpret_cast<char*>(&value), sizeof(value));
}

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

	T& operator() (uint64_t row, uint64_t col) {
		return data_[row * cols_ + col];
	}

	const T& operator() (uint64_t row, uint64_t col) const {
		return data_[row * cols_ + col];
	}

	uint64_t rows() const {
		return rows_;
	}

	uint64_t cols() const {
		return cols_;
	}

	auto begin() const {
		return data_.begin();
	}

	auto end() const {
		return data_.end();
	}
};

#pragma pack(1)
struct bitmap_header {
	uint16_t magic_number;
	uint32_t size;
	uint16_t reserved1;
	uint16_t reserved2;
	uint32_t offset;
};

enum class bitmap_compression_method : uint32_t {
	BI_RGB = 0,
	BI_RLE8 = 1,
	BI_RLE4 = 2
};

#pragma pack(1)
struct bitmap_info_header {
	uint32_t size;
	int32_t width;
	int32_t height;
	uint16_t color_planes;
	uint16_t bits_per_pixel;
	bitmap_compression_method compression_method;
	uint32_t image_size;
	int32_t horizontal_resolution; // pixel per meter
	int32_t vertical_resolution; // pixel per meter
	uint32_t number_of_palette_colors;
	uint32_t number_of_important_colors_used;
};

static matrix<pixel_data> read_bpm8(std::ifstream& input) {
	bitmap_header header = {};
	input.read(reinterpret_cast<char*>(&header), sizeof(header));

	bitmap_info_header info_header = {};
	input.read(reinterpret_cast<char*>(&info_header), sizeof(info_header));

	// In bgr format
	std::vector<palette_item> palette;

	if (info_header.bits_per_pixel != 24) {
		uint64_t colors_in_palette = info_header.number_of_palette_colors;

		if (colors_in_palette == 0) {
			colors_in_palette = static_cast<uint64_t>(std::pow(2, info_header.bits_per_pixel));
		}

		palette.resize(colors_in_palette);
		input.read(reinterpret_cast<char*>(palette.data()), sizeof(palette_item) * palette.size());
	}

	uint64_t bits_of_paddings_per_row = 32 - (static_cast<uint64_t>(info_header.width) * info_header.bits_per_pixel) % 32;

	matrix<pixel_data> raster(info_header.height, info_header.width);

	for (uint64_t row = 0; row < raster.rows(); ++row) {

		for (uint64_t col = 0; col < raster.cols(); ++col) {
			uint8_t palette_index = 0;
			input.read(reinterpret_cast<char*>(&palette_index), sizeof(palette_index));

			palette_item color = palette[palette_index];
			raster(raster.rows() - 1 - row, col) = { color[0], color[1], color[2] };
		}

		// Ignore padding
		input.ignore(bits_of_paddings_per_row / 8);
	}

	return raster;
}

static void write_pam(std::ofstream& output, const matrix<pixel_data>& raster) {
	// The image data is stored in bgr format

	output << "P7" << std::endl;
	output << "WIDTH " << raster.cols() << std::endl;
	output << "HEIGHT " << raster.rows() << std::endl;
	output << "DEPTH " << 3 << std::endl;
	output << "MAXVAL " << 255 << std::endl;
	output << "TUPLTYPE " << "RGB" << std::endl;
	output << "ENDHDR" << std::endl;

	for (uint64_t row = 0; row < raster.rows(); ++row) {
		for (uint64_t col = 0; col < raster.cols(); ++col) {
			const auto& pixel = raster(row, col);
			raw_write(output, pixel[2]);
			raw_write(output, pixel[1]);
			raw_write(output, pixel[0]);
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

	matrix<pixel_data> bpm = read_bpm8(input);

	std::ofstream output(argv[2], std::ios::binary);

	if (!output) {
		return EXIT_FAILURE;
	}

	write_pam(output, bpm);

	return EXIT_SUCCESS;
}