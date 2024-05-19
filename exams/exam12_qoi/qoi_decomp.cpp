#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <string>

using rgba = std::array<uint8_t, 4>;

template<typename T>
T raw_read(std::istream& input) {
	T value = 0;
	input.read(reinterpret_cast<char*>(&value), sizeof(value));
	return value;
}

template<typename T>
class mat {

private:
	size_t rows_ = 0;
	size_t cols_ = 0;
	std::vector<T> data_;

public:
	mat() { }

	mat(size_t rows, size_t cols) {
		resize(rows, cols);
	}

	void resize(size_t rows, size_t cols) {
		rows_ = rows;
		cols_ = cols;
		data_.resize(rows * cols);
	}

	const T& operator[](size_t i) const { return data_[i]; }
	T& operator[](size_t i) { return data_[i]; }

	size_t size() const { return rows_ * cols_; }
	size_t rows() const { return rows_; }
	size_t cols() const { return cols_; }

	const char* rawdata() const {
		return reinterpret_cast<const char*>(data_.data());
	}

	size_t rawsize() const { return size() * sizeof(T); }
};

class qoi {
private:

#pragma pack(1) 
	struct qoi_header {
		char magic[4] = { 0 }; // magic bytes "qoif"
		uint32_t width = 0; // image width in pixels (BE)
		uint32_t height = 0; // image height in pixels (BE)
		uint8_t channels = 0; // 3 = RGB, 4 = RGBA
		uint8_t colorspace = 0; // 0 = sRGB with linear alpha, 1 = all channels linear
	};

	std::ifstream input_;
	qoi_header header_;
	mat<rgba> decoded_image_;
	std::array<rgba, 64> cache_ = { 0 };
	rgba previous_pixel_ = { 0, 0, 0, 255 };

	uint64_t current_pixel_index_ = 0;

	void add_pixel(const rgba& pixel) {
		decoded_image_[current_pixel_index_] = pixel;
		previous_pixel_ = pixel;
		cache_[calculate_pixel_hash(pixel)] = pixel;
		current_pixel_index_++;
	}

	void decode_QOI_OP_RGB(uint8_t byte1, uint8_t byte2, uint8_t byte3) {
		rgba pixel = { byte1, byte2, byte3, previous_pixel_[3] };
		add_pixel(pixel);
	}

	void decode_QOI_OP_RGBA(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
		rgba pixel = { byte1, byte2, byte3, byte4 };
		add_pixel(pixel);
	}

	void decode_QOI_OP_INDEX(uint8_t byte0) {
		uint8_t index = byte0 & 0b00111111;
		rgba pixel = cache_[index];
		add_pixel(pixel);
	}

	void decode_QOI_OP_DIFF(uint8_t byte0) {

		int8_t dr = ((byte0 & 0b00110000) >> 4) - 2;
		int8_t dg = ((byte0 & 0b00001100) >> 2) - 2;
		int8_t db = ((byte0 & 0b00000011) >> 0) - 2;

		// The operations must wrap around
		uint8_t r = previous_pixel_[0] + dr;
		uint8_t g = previous_pixel_[1] + dg;
		uint8_t b = previous_pixel_[2] + db;

		rgba pixel = { r, g, b, previous_pixel_[3] };
		add_pixel(pixel);
	}

	void decode_QOI_OP_LUMA(uint8_t byte0, uint8_t byte1) {
		int8_t dg = (byte0 & 0b00111111) - 32;
		int8_t dr_minus_dg = ((byte1 & 0b11110000) >> 4) - 8;
		int8_t db_minus_dg = (byte1 & 0b00001111) - 8;

		int8_t dr = dr_minus_dg + dg;
		int8_t db = db_minus_dg + dg;

		// The operations must wrap around
		uint8_t r = previous_pixel_[0] + dr;
		uint8_t g = previous_pixel_[1] + dg;
		uint8_t b = previous_pixel_[2] + db;

		rgba pixel = { r, g, b, previous_pixel_[3] };
		add_pixel(pixel);
	}

	void decode_QOI_OP_RUN(uint8_t byte0) {
		uint8_t run = (byte0 & 0b00111111) + 1;

		// 63 and 64 are illegal values
		if (run == 63 || run == 64) {
			std::cerr << "Invalid run" << std::endl;
			return;
		}

		for (uint8_t i = 0; i < run; ++i) {
			add_pixel(previous_pixel_);
		}
	}

	uint32_t little_endian_to_big_endian(uint32_t little_endian) {
		uint32_t byte1 = ((little_endian >> 0) & 0xFF) << 24;
		uint32_t byte2 = ((little_endian >> 8) & 0xFF) << 16;
		uint32_t byte3 = ((little_endian >> 16) & 0xFF) << 8;
		uint32_t byte4 = ((little_endian >> 24) & 0xFF) << 0;
		return byte1 | byte2 | byte3 | byte4;
	}

	void read_header() {
		input_.read(reinterpret_cast<char*>(&header_), sizeof(header_));
		header_.width = little_endian_to_big_endian(header_.width);
		header_.height = little_endian_to_big_endian(header_.height);
		decoded_image_.resize(header_.height, header_.width);
	}

	uint8_t calculate_pixel_hash(const rgba& pixel) {
		return (pixel[0] * 3 + pixel[1] * 5 + pixel[2] * 7 + pixel[3] * 11) % 64;
	}

public:
	qoi(const std::string& input_file) : input_(input_file, std::ios::binary) {

		read_header();

		while (true) {
			uint8_t byte0 = raw_read<uint8_t>(input_);

			if (input_.eof()) {
				break;
			}

			uint8_t tag = byte0;

			auto peeked_byte = input_.peek();

			if (tag == 0b00000000 && peeked_byte == 0b00000000) {
				for (uint8_t i = 0; i < 6; ++i) {
					uint8_t zero_byte = raw_read<uint8_t>(input_);
				}
				uint8_t one_byte = raw_read<uint8_t>(input_);
			}
			else {
				if (tag == 0b11111110) {
					uint8_t byte1 = raw_read<uint8_t>(input_);
					uint8_t byte2 = raw_read<uint8_t>(input_);
					uint8_t byte3 = raw_read<uint8_t>(input_);
					decode_QOI_OP_RGB(byte1, byte2, byte3);
				}
				else if (tag == 0b11111111) {
					uint8_t byte1 = raw_read<uint8_t>(input_);
					uint8_t byte2 = raw_read<uint8_t>(input_);
					uint8_t byte3 = raw_read<uint8_t>(input_);
					uint8_t byte4 = raw_read<uint8_t>(input_);
					decode_QOI_OP_RGBA(byte1, byte2, byte3, byte4);
				}
				else {

					// Get the 2 most significant digits of the byte
					tag >>= 6;

					if (tag == 0b00) {
						decode_QOI_OP_INDEX(byte0);
					}
					else if (tag == 0b01) {
						decode_QOI_OP_DIFF(byte0);
					}
					else if (tag == 0b10) {
						uint8_t byte1 = raw_read<uint8_t>(input_);
						decode_QOI_OP_LUMA(byte0, byte1);
					}
					else if (tag == 0b11) {
						decode_QOI_OP_RUN(byte0);
					}
					else {
						std::cerr << "Invalid tag" << std::endl;
					}
				}
			}
		}
	}

	const mat<rgba>& image() const {
		return decoded_image_;
	}
};

static void write_pam(const mat<rgba>& image, std::ostream& output) {

	output << "P7\nWIDTH " << image.cols() << "\nHEIGHT " << image.rows() <<
		"\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";

	output.write(image.rawdata(), image.rawsize());
}

int main(int argc, char* argv[])
{
	if (argc != 3) {
		return EXIT_FAILURE;
	}

	qoi qoi(argv[1]);
	const auto& image = qoi.image();

	std::ofstream output(argv[2], std::ios::binary);

	if (!output) {
		return EXIT_FAILURE;
	}

	write_pam(image, output);

	return EXIT_SUCCESS;
}