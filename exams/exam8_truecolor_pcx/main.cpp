#include "pcx.h"

int main(int argc, char* argv[]) {

	mat<vec3b> img;

	if(!load_pcx(argv[1], img)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}