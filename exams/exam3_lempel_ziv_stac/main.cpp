#include "lzs.h"

int main(int argc, char* argv[]) {

	std::ifstream input(argv[1], std::ios::binary);

	if (!input) {
		return EXIT_FAILURE;
	}

	std::ofstream output(argv[2], std::ios::binary);

	if (!output) {
		return EXIT_FAILURE;
	}

	lzs_decompress(input, output);

	return EXIT_SUCCESS;
}