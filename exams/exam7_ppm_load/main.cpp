#include <string>
#include "ppm.h"
#include "math.h"

bool LoadPPM(const std::string& filename, mat<vec3b>& img);

int main(int argc, char* argv[]) {

	mat<vec3b> img;

	if (!LoadPPM(argv[1], img)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}