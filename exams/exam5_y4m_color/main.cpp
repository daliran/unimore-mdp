#include <string>
#include <vector>
#include <cstdint>

#include "mat.h"
#include "types.h"

bool y4m_extract_color(const std::string& filename, std::vector<mat<vec3b>>& frames);

int main(int argc, char* argv[]) {

	std::vector<mat<vec3b>> frames;

	if (!y4m_extract_color(argv[1], frames)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}