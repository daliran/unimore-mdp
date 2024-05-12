#include "pbm.h"

Image BinaryImageToImage(const BinaryImage& bimg) {

	uint64_t div = bimg.W / 8;
	uint64_t mod = bimg.W % 8;
	uint64_t bytes_per_row = mod != 0 ? div + 1 : div;

	Image image;
	image.H = bimg.H;
	image.W = bimg.W;

	for (uint64_t row = 0; row < static_cast<uint64_t>(bimg.H); ++row) {
		for (uint64_t col = 0; col < bytes_per_row; ++col) {

			uint8_t value = bimg.ImageData[row * bytes_per_row + col];

			uint64_t bits_to_write = 8;

			if (col == bytes_per_row - 1) {
				bits_to_write = mod;
			}

			for (uint64_t i = 0; i < bits_to_write; ++i) {
				bool bit = (value >> (7 - i)) & 1;
				uint8_t to_write = bit ? 0 : 255;
				image.ImageData.push_back(to_write);
			}
		}
	}

	return image;
}