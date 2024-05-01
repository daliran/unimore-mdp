#include "pgm16.h"

// time: 1:15 hh:mm

int main(int argc, char* argv[]) {

	mat<uint16_t> img;
	uint16_t max_value;

	std::string filename(argv[1]);
	bool result = load(filename, img, max_value);
	return result ? EXIT_SUCCESS : EXIT_FAILURE;
}