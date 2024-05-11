#include <string>
#include <cstdint>
#include "ppm.h"
#include "math.h"


bool LoadPPM(const std::string& filename, mat<vec3b>& img);
void SplitRGB(const mat<vec3b>& img, mat<uint8_t>& img_r, mat<uint8_t>& img_g, mat<uint8_t>& img_b);

int main(int argc, char* argv[]) {

	mat<vec3b> img;

	if (!LoadPPM(argv[1], img)) {
		return EXIT_FAILURE;
	}

	mat<uint8_t> r;
	mat<uint8_t> g;
	mat<uint8_t> b;

	SplitRGB(img, r, g, b);

	return EXIT_SUCCESS;
}