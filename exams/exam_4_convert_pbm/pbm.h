#pragma once

#include <vector>
#include <cstdint>

struct BinaryImage {
	int W = 0;
	int H = 0;
	std::vector<uint8_t> ImageData;
};

struct Image {
	int W = 0;
	int H = 0;
	std::vector<uint8_t> ImageData;
};

Image BinaryImageToImage(const BinaryImage& bimg);