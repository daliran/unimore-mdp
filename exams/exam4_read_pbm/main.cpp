#include "pbm.h"


int main(int argc, char* argv[]) {

	BinaryImage image;

	if (!image.ReadFromPBM(argv[1])) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}