#include <string>
#include <vector>
#include <cstdint>
#include "mat.h"

bool y4m_extract_gray(const std::string& filename, std::vector<mat<uint8_t>>& frames);

int main(int argc, char* argv[]) {

	std::vector<mat<uint8_t>> frames;

	if (!y4m_extract_gray(argv[1], frames)){
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}