#include <vector>
#include <cstdint>
#include <algorithm>
#include <string>
#include "mat.h"

static void write_run(std::vector<uint8_t>& buffer, std::vector<uint8_t>& encoded) {
	uint8_t run_byte = 257 - static_cast<uint8_t>(buffer.size());
	encoded.push_back(run_byte);
	encoded.push_back(buffer.front());
	buffer.clear();
}

static void write_copy(std::vector<uint8_t>& buffer, std::vector<uint8_t>& encoded) {
	uint8_t copy_byte = static_cast<uint8_t>(buffer.size()) - 1;
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

	encoded.push_back(128);
}

static uint8_t map_bits_to_char(uint8_t bits_group) {

	if (bits_group <= 25) {
		return 'A' + bits_group;
	}
	else if (bits_group >= 26 && bits_group <= 51) {
		return 'a' + bits_group - 26;
	}
	else if (bits_group >= 52 && bits_group <= 61) {
		return '0' + bits_group - 52;
	}
	else if (bits_group == 62) {
		return '+';
	}
	else if (bits_group == 63) {
		return '/';
	}
	else {
		return '?';
	}
}

std::string Base64Encode(const std::vector<uint8_t>& v) {

	std::vector<uint8_t> data(v.begin(), v.end());

	size_t remainder = v.size() % 3;

	size_t padding_bytes = remainder == 0 ? 0 : 3 - remainder;

	for (size_t i = 0; i < padding_bytes; ++i) {
		data.push_back(128);
	}

	std::string output;

	for (size_t i = 0; i < data.size(); i+=3) {
		uint32_t combined = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];

		uint8_t group1 = (combined >> 18) & 0b111111;
		uint8_t converted1 = map_bits_to_char(group1);
		output.push_back(converted1);

		uint8_t group2 = (combined >> 12) & 0b111111;
		uint8_t converted2 = map_bits_to_char(group2);
		output.push_back(converted2);

		uint8_t group3 = (combined >> 6) & 0b111111;
		uint8_t converted3 = map_bits_to_char(group3);
		output.push_back(converted3);

		uint8_t group4 = combined & 0b111111;
		uint8_t converted4 = map_bits_to_char(group4);
		output.push_back(converted4);
	}

	return output;
}