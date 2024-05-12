#include "pcx.h"
#include "pgm.h"

int main(int argc, char* argv[]) {

	mat<uint8_t> img;

	if(!load_pcx(argv[1], img)) {
		return EXIT_FAILURE;
	}

	if (!save_pgm(argv[2], img)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}