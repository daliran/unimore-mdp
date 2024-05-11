#include <vector>
#include <cstdint>
#include <algorithm>
#include "mat.h"

static void write_run(std::vector<uint8_t>& buffer, std::vector<uint8_t>& encoded) {
	uint8_t run_byte = 257 - buffer.size();
	encoded.push_back(run_byte);
	encoded.push_back(buffer.front());
	buffer.clear();
}

static void write_copy(std::vector<uint8_t>& buffer, std::vector<uint8_t>& encoded) {
	uint8_t copy_byte = buffer.size() - 1;
	encoded.push_back(copy_byte);
	std::copy(buffer.begin(), buffer.end(), std::back_inserter(encoded));
	buffer.clear();
}

void PackBitsEncode(const mat<uint8_t>& img, std::vector<uint8_t>& encoded) {

	bool is_run = true;

	std::vector<uint8_t> buffer;

	for (const auto& pixel : img) {

		if (buffer.size() > 0) {

			auto previous_value = buffer.back();

			if (is_run) {
				
				if (pixel != previous_value) {
					write_run(buffer, encoded);

					if (buffer.size() == 1) {
						is_run = false;
					}
				}
			}
			else {

				if (pixel == previous_value) {
					buffer.pop_back();
					write_copy(buffer, encoded);
					buffer.push_back(previous_value);
					is_run = true;
				}
			}
		}

		buffer.push_back(pixel);
	}

	if (buffer.size() > 0) {
		if (is_run) {
			write_run(buffer, encoded);
		}
		else {
			write_copy(buffer, encoded);
		}
	}

	encoded.push_back(128); // EOD
}