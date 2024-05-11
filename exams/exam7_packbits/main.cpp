#include <vector>
#include <cstdint>
#include "ppm.h"
#include "mat.h"

bool LoadPPM(const std::string& filename, mat<vec3b>& img);
void SplitRGB(const mat<vec3b>& img, mat<uint8_t>& img_r, mat<uint8_t>& img_g, mat<uint8_t>& img_b);
void PackBitsEncode(const mat<uint8_t>& img, std::vector<uint8_t>& encoded);

int main(int arg, char* argv[]) {

	mat<vec3b> img;

	if (!LoadPPM(argv[1], img)) {
		return EXIT_FAILURE;
	}

	mat<uint8_t> r;
	mat<uint8_t> g;
	mat<uint8_t> b;

	SplitRGB(img, r, g, b);

	std::vector<uint8_t> r_encoded;
	PackBitsEncode(r, r_encoded);

	std::vector<uint8_t> g_encoded;
	PackBitsEncode(g, g_encoded);

	std::vector<uint8_t> b_encoded;
	PackBitsEncode(b, b_encoded);

	return EXIT_SUCCESS;
}