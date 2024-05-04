#include "lzs.h"
#include <cstdint>
#include <unordered_map>
#include <algorithm>
#include <list>
#include <iterator>

class bit_reader {
private:
	std::istream& input_;
	uint8_t buffer_ = 0;
	uint8_t bits_in_buffer = 0;

public:
	bit_reader(std::istream& input) : input_(input) {}

	std::pair<bool, bool> read_bit() {

		if (bits_in_buffer == 0) {
			input_.read(reinterpret_cast<char*>(&buffer_), sizeof(buffer_));

			if (input_.eof()) {
				return std::pair<bool, bool>(false, false);
			}

			bits_in_buffer = 8;
		}

		bool bit = (buffer_ >> (bits_in_buffer - 1)) & 1;
		bits_in_buffer--;

		return std::pair<bool, bool>(bit, true);
	}

	std::pair<uint64_t, bool> read_number(uint64_t number_of_bits) {

		uint64_t value = 0;
		uint64_t read_bits = 0;

		while (read_bits != number_of_bits) {

			auto bit = read_bit();

			if (!bit.second) {
				return std::pair<uint64_t, bool>(0, false);
			}

			value <<= 1;
			value |= static_cast<uint64_t>(bit.first);
			read_bits++;
		}

		return std::pair<uint64_t, bool>(value, true);
	}

};

class lzs_decoder {
private:
	bit_reader bit_reader_;
	std::ostream& output_;
	std::list<uint8_t> buffer_;

	bool read_literal_byte() {
		auto literal_byte = bit_reader_.read_number(8);

		if (!literal_byte.second) {
			return false;
		}

		uint8_t casted = static_cast<uint8_t>(literal_byte.first);
		buffer_.push_back(casted);
		output_ << casted;

		return true;
	}

	bool read_offset_length() {

		auto offset_type = bit_reader_.read_bit();

		if (!offset_type.second) {
			return false;
		}

		uint8_t bits_to_read = offset_type.first ? 7 : 11;

		auto read_offset = bit_reader_.read_number(bits_to_read);

		if (!read_offset.second) {
			return false;
		}

		uint64_t offset = read_offset.first;

		// An end of marker can be seen as an offset of 0
		if (offset == 0) {
			return true;
		}

		auto length_part1 = bit_reader_.read_number(2);

		if (!length_part1.second) {
			return false;
		}

		uint64_t length_code = 0;
		length_code = length_part1.first;
		uint64_t length = length_part1.first + 2;

		if (length_code > 2) {
			auto length_part2 = bit_reader_.read_number(2);

			if (!length_part2.second) {
				return false;
			}

			length_code = (length_part1.first << 2) | length_part2.first;
			length = length_code - 7;
		}

		if (length_code == 15) {

			uint32_t n = 1;

			while (true) {
				auto extra_bits = bit_reader_.read_number(4);

				if (!extra_bits.second) {
					return false;
				}

				if (extra_bits.first == 15) {
					n++;
				}
				else {
					length = extra_bits.first + (n * 15) - 7;
					break;
				}
			}
		}

		if (offset > buffer_.size()) {
			return false;
		}

		std::list<uint8_t>::iterator start_it = std::next(buffer_.end(), -static_cast<int64_t>(offset));
		std::list<uint8_t>::iterator current_it = start_it;

		for (size_t i = 0; i < length; ++i) {

			const auto& value = *current_it;
			buffer_.push_back(value);
			output_ << value;
			current_it++;

			if (current_it == buffer_.end()) {
				current_it = start_it;
			}
		}

		return true;
	}

public:
	lzs_decoder(std::istream& input, std::ostream& output) : bit_reader_(input), output_(output) {}

	void decode() {
		while (true) {

			auto bit = bit_reader_.read_bit();

			if (!bit.second) {
				return;
			}

			if (bit.first) {
				if (!read_offset_length()) {
					return;
				}
			}
			else {
				if (!read_literal_byte()) {
					return;
				}
			}
		}
	}
};

void lzs_decompress(std::istream& is, std::ostream& os) {
	lzs_decoder decoder(is, os);
	decoder.decode();
}