#include <fstream>
#include <cstdint>
#include <optional>
#include <vector>
#include <algorithm>

template<typename T>
T raw_read(std::ifstream& input, size_t size = sizeof(T)) {
	T buffer = 0;
	input.read(reinterpret_cast<char*>(&buffer), size);
	return buffer;
}

template<typename T>
void raw_write(std::ofstream& output, T value) {
	output.write(reinterpret_cast<char*>(&value), sizeof(value));
}

static uint64_t read_big_endian(std::ifstream& input, uint8_t number_of_bytes) {
	uint64_t value = 0;

	for (uint8_t i = 0; i < number_of_bytes; ++i) {
		uint8_t byte = raw_read<uint8_t>(input);
		value = value << 8 | byte;
	}

	return value;
}

class lz4_decoder {
private:
	std::ifstream& input_;
	std::ofstream& output_;

	struct lz4_header {
		uint32_t magic_number = 0;
		uint32_t uncompressed_length = 0;
		uint32_t constant = 0;
	};

	lz4_header header_;

	uint64_t current_block_size_ = 0;
	uint64_t current_block_read_bytes_ = 0;
	std::vector<uint8_t> dictionary_;

	std::optional<lz4_header> read_and_validate_header() {

		lz4_header header;

		// Check the magic number
		header.magic_number = static_cast<uint32_t>(read_big_endian(input_, 4));

		if (header.magic_number != 0x03214C18) {
			return std::nullopt;
		}

		header.uncompressed_length = raw_read<uint32_t>(input_);

		header.constant = static_cast<uint32_t>(read_big_endian(input_, 4));

		// Check the constant
		if (header.constant != 0x0000004D) {
			return std::nullopt;
		}

		return header;
	}

	uint64_t read_token_element_length(uint8_t token_length, uint8_t bias) {

		uint64_t literal_length = token_length;
		literal_length += bias;

		if (token_length == 15) {
			while (true) {
				uint8_t extra_byte = raw_read<uint8_t>(input_);
				current_block_read_bytes_++;

				literal_length += extra_byte;

				if (extra_byte < 255) {
					break;
				}
			}
		}

		return literal_length;
	}

	void handle_literal(uint64_t literal_length) {
		std::vector<uint8_t> buffer(literal_length);
		input_.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
		std::copy(buffer.begin(), buffer.end(), std::back_inserter(dictionary_));
		output_.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
		current_block_read_bytes_ += literal_length;
	}

	void handle_match(uint16_t offset, uint64_t match_length) {

		std::vector<uint8_t>::iterator start_it = dictionary_.end() - offset;
		std::vector<uint8_t>::iterator end_it = dictionary_.end();
		std::vector<uint8_t>::iterator current_it = start_it;

		// This is necessary to avoid the iterator invalidation
		std::vector<uint8_t> buffer(match_length);

		for (uint64_t i = 0; i < match_length; ++i) {

			const uint8_t& byte = *current_it;
			buffer[i] = byte;

			current_it++;

			if (current_it == end_it) {
				current_it = start_it;
			}
		}

		std::copy(buffer.begin(), buffer.end(), std::back_inserter(dictionary_));
		output_.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
	}

	void read_block() {
		uint8_t token = raw_read<uint8_t>(input_);
		current_block_read_bytes_ += 1;

		uint8_t token_length = (token & 0b11110000) >> 4;
		uint64_t literal_length = read_token_element_length(token_length, 0);

		if (literal_length > 0) {
			handle_literal(literal_length);
		}

		if (current_block_read_bytes_ == current_block_size_) {
			return;
		}

		uint16_t offset = raw_read<uint16_t>(input_);
		current_block_read_bytes_ += 2;

		// Invalid value
		if (offset == 0) {
			return;
		}

		uint8_t token_match = token & 0b00001111;
		uint64_t match_length = read_token_element_length(token_match, 4);

		if (match_length > 0) {
			handle_match(offset, match_length);
		}
	}

public:

	lz4_decoder(std::ifstream& input, std::ofstream& output) : input_(input), output_(output) { }

	bool decompress() {

		auto header = read_and_validate_header();

		// Validation error
		if (!header.has_value()) {
			return false;
		}

		header_ = header.value();

		dictionary_.reserve(header_.uncompressed_length);

		while (true) {

			if (current_block_read_bytes_ >= current_block_size_) {
				current_block_size_ = raw_read<uint32_t>(input_);
				current_block_read_bytes_ = 0;
			}

			read_block();

			if (input_.eof()) {
				break;
			}
		}

		return true;
	}
};

int main(int argc, char* argv[]) {

	if (argc != 3) {
		return EXIT_FAILURE;
	}

	std::ifstream input(argv[1], std::ios::binary);

	if (!input) {
		return EXIT_FAILURE;
	}

	std::ofstream output(argv[2], std::ios::binary);

	if (!output) {
		return EXIT_FAILURE;
	}

	lz4_decoder decoder(input, output);

	if (!decoder.decompress()) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}