#include <fstream>
#include <cstdint>
#include <vector>
#include <iterator>
#include <iostream>
#include <iterator>
#include <algorithm>

template<typename T>
T raw_read(std::istream& input, size_t size = sizeof(T)) {
	T buffer = 0;
	input.read(reinterpret_cast<char*>(&buffer), size);
	return buffer;
}

template<typename T>
void raw_write(std::ostream& output, T value) {
	output.write(reinterpret_cast<char*>(&value), sizeof(value));
}

class snappy_decoder {
private:
	std::ifstream input_;
	std::ofstream output_;
	uint64_t original_size_ = 0;
	std::vector<uint8_t> dictionary_;

	void read_preamble() {
		bool read_byte = true;
		for (int i = 0; read_byte; ++i) {
			uint64_t byte = raw_read<uint64_t>(input_, sizeof(uint8_t));
			read_byte = (byte >> 7) & 1;
			original_size_ |= ((byte & 0b01111111) << 7 * i);
		}
	}

	void read_element() {
		uint8_t byte_tag = raw_read<uint8_t>(input_);
		uint8_t type = byte_tag & 0b00000011;

		switch (type) {
		case 0b00:
			read_literal(byte_tag);
			break;
		case 0b01:
			read_copy(byte_tag, 1);
			break;
		case 0b10:
			read_copy(byte_tag, 2);
			break;
		case 0b11:
			read_copy(byte_tag, 4);
			break;
		}
	}

	void read_literal(uint8_t byte_tag) {

		uint8_t value = (byte_tag & 0b11111100) >> 2;

		uint64_t length = 0;

		if (value < 60) {
			length = static_cast<uint64_t>(value) + 1;
		}
		else {
			uint8_t bytes_to_read = value - 59;

			for (int i = 0; i < bytes_to_read; ++i) {
				uint64_t byte = raw_read<uint8_t>(input_);
				length |= (byte << (8 * i));
			}

			length += 1;
		}

		std::vector<uint8_t> read_buffer(length);
		input_.read(reinterpret_cast<char*>(read_buffer.data()), read_buffer.size());
		std::copy(read_buffer.begin(), read_buffer.end(), std::back_inserter(dictionary_));
		std::copy(read_buffer.begin(), read_buffer.end(), std::ostream_iterator<uint8_t>(output_));
	}

	void read_copy(uint8_t byte_tag, uint8_t byte_offset) {

		uint64_t offset = 0;
		uint64_t length = 0;

		switch (byte_offset)
		{
		case 1: {
			length = static_cast<uint64_t>((byte_tag & 0b00011100) >> 2) + 4;

			uint8_t next_offset_byte = raw_read<uint8_t>(input_);
			uint64_t most_significant_offset_bits = ((byte_tag & 0b11100000) >> 5);
			offset = (most_significant_offset_bits << 8) | next_offset_byte;
			break;
		}

		case 2: {
			length = static_cast<uint64_t>((byte_tag & 0b11111100) >> 2) + 1;
			offset = raw_read<uint16_t>(input_);
			break;
		}

		case 4: {
			length = static_cast<uint64_t>((byte_tag & 0b11111100) >> 2) + 1;
			offset = raw_read<uint32_t>(input_);
			break;
		}
		}

		std::vector<uint8_t>::iterator start_it = std::prev(dictionary_.end(), offset);
		std::vector<uint8_t>::iterator end_it = dictionary_.end();	
		std::vector<uint8_t>::iterator current_it = start_it;

		std::vector<uint8_t> copy_buffer;

		for (uint64_t i = 0; i < length; ++i) {

			uint8_t value = *current_it;

			copy_buffer.push_back(value);

			current_it++;

			if (current_it == end_it) {
				current_it = start_it;
			}
		}

		std::copy(copy_buffer.begin(), copy_buffer.end(), std::back_inserter(dictionary_));
		std::copy(copy_buffer.begin(), copy_buffer.end(), std::ostream_iterator<uint8_t>(output_));
	}

	void print_progress() {

		static int previous_percentage = 0;

		int percentage = static_cast<int>(dictionary_.size() / static_cast<double>(original_size_) * 100);

		if (percentage != previous_percentage && percentage % 5 == 0) {
			std::cout << "Current progress: " << percentage << std::endl;
			previous_percentage = percentage;
		}
	}

public:
	snappy_decoder(const std::string& input_file, const std::string& output_file)
		: input_(input_file, std::ios::binary), output_(output_file, std::ios::binary) { }

	bool decode() {

		if (!input_ || !output_) {
			return false;
		}

		read_preamble();

		while (true) {
			read_element();

			//print_progress();

			if (input_.peek() == EOF) {
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

	snappy_decoder snappy(argv[1], argv[2]);

	if (!snappy.decode()) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}