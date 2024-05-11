#include <string>

bool lz78encode(const std::string& input_filename, const std::string& output_filename, int maxbits);

int main(int argc, char* argv[]) {

	if (argc != 4) {
		return EXIT_FAILURE;
	}

	int max_bits = std::stoi(argv[3]);

	if (!lz78encode(argv[1], argv[2], max_bits)){
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}