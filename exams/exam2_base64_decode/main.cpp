#include<iostream>
#include "base64.h"

int main(int argc, char* argv[]) {
	std::string test = "UHJldHR5IGxvbmcgdGV4dCB3aGljaCByZXF1aXJlcyBtb3JlIHRoYW4gNzYgY2hhcmFjdGVycyB0\nbyBlbmNvZGUgaXQgY29tcGxldGVseS4=";
	std::string output = base64_decode(test);
	std::cout << "Decoded string: " << output << std::endl;
	return EXIT_SUCCESS;
}