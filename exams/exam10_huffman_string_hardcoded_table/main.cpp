#include "hufstr.h"

int main() 
{
	hufstr huffstr;
	auto compressed = huffstr.compress("aeio");
	auto original = huffstr.decompress(compressed);

	return EXIT_SUCCESS;
}