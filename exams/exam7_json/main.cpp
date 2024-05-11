#include <string>

std::string JSON(const std::string& filename);

int main(int arg, char* argv[]) {
	std::string json = JSON(argv[1]);
	return EXIT_SUCCESS;
}