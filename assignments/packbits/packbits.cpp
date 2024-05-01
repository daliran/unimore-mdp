#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <iterator>
//#include <crtdbg.h>

static std::pair<uint8_t, bool> raw_read(std::istream& input) {
	uint8_t buffer = 0;
	input.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));
	return std::pair<uint8_t, bool>(buffer, !input.eof());
}

static void raw_write(const uint8_t& value, std::ostream& output) {
	output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

class packbits_encoder {
private:
	std::istream& input_;
	std::ostream& output_;
	std::vector<uint8_t> buffer_;

	bool run_mode = true;

	void write_buffer_as_run() {

		if (buffer_.size() == 0) {
			return;
		}

		// Only take up to 128 bytes from the buffer
		uint8_t bytes_in_buffer = std::min<uint8_t>(static_cast<uint8_t>(buffer_.size()), 128);

		uint8_t run_length_byte = 257 - bytes_in_buffer;
		raw_write(run_length_byte, output_);

		uint8_t value = buffer_.back();
		raw_write(value, output_);

		//std::cout << "RUN (" << buffer_.size() << ", " << value << ")" << std::endl;

		buffer_.clear();
	}

	void write_buffer_as_copy() {

		if (buffer_.size() == 0) {
			return;
		}

		// Only take up to 128 bytes from the buffer
		uint8_t bytes_in_buffer = std::min<uint8_t>(static_cast<uint8_t>(buffer_.size()), 128);

		// COPY is 0 based
		uint8_t elements_to_copy = bytes_in_buffer - 1; 

		raw_write(elements_to_copy, output_);

		for (const auto& container_item : buffer_) {
			raw_write(container_item, output_);
		}

		//std::cout << "COPY (" << buffer_.size() << ", " << std::string(buffer_.begin(), buffer_.end()) << ")" << std::endl;

		buffer_.clear();
	}

	void write_eod() {
		raw_write(128, output_);
		//std::cout << "EOD" << std::endl;
	}

	void write_buffer() {
		if (run_mode) {
			write_buffer_as_run();
		}
		else {
			write_buffer_as_copy();
		}
	}

public:
	packbits_encoder(std::istream& input, std::ostream& output) : input_(input), output_(output) {}

	void encode() {
		for (std::istream_iterator<uint8_t> it(input_); it != std::istream_iterator<uint8_t>(); ++it) {

			uint8_t value = *it;

			if (!buffer_.empty()) {

				uint8_t last_buffer_value = buffer_.back();

				if (run_mode) {
					if (last_buffer_value != value) {
						if (buffer_.size() > 1) {
							// write runs only if there are at least 2 elements in the buffer
							write_buffer_as_run();
						}
						else {
							run_mode = false;
						}
					}
					else if (buffer_.size() >= 128) {
						// Write the buffer if the run exceeds 128 repetitions
						write_buffer();
					}
				}
				else if (!run_mode) {
					if (last_buffer_value == value) {
						// Remove the last element from the array because it belongs to the run
						buffer_.pop_back();
						write_buffer_as_copy();
						buffer_.push_back(last_buffer_value);
						run_mode = true;
					}
					else if (buffer_.size() >= 128) {
						// Write the buffer if the copy exceeds 128 elements
						write_buffer();
					}	
				}
			}

			// In theory for runs we don't need to store all the values, but just the symbol and the count of symbols found
			buffer_.push_back(value);
		}

		write_buffer();
		write_eod();
	}
};

class packbits_decoder {
private:
	std::istream& input_;
	std::ostream& output_;

	void handle_run(uint8_t value) {
		uint8_t repetitions = 257 - value;

		auto symbol = raw_read(input_);

		if (!symbol.second) {
			return;
		}

		for (int i = 0; i < repetitions; ++i) {
			raw_write(symbol.first, output_);
		}
	}

	void handle_copy(uint8_t value) {
		uint8_t charaters_to_read = value + 1;

		for (int i = 0; i < charaters_to_read; ++i) {

			auto symbol = raw_read(input_);

			if (!symbol.second) {
				return;
			}

			raw_write(symbol.first, output_);
		}
	}

public:
	packbits_decoder(std::istream& input, std::ostream& output) : input_(input), output_(output) {}

	void decode() {

		while (true) {

			auto next_command = raw_read(input_);

			if (!next_command.second) {
				break;
			}

			if (next_command.first < 128) {
				handle_copy(next_command.first);
			}
			else if (next_command.first == 128) { 
				break; // EOD
			}
			else {
				handle_run(next_command.first);
			}
		}
	}
};

int main(int argc, char* argv[]) {
	{
		if (argc != 4) {
			std::cout << "Wrong arguments number" << std::endl;
			return EXIT_FAILURE;
		}

		std::string mode(argv[1]);

		if (!(mode == "c" || mode == "d")) {
			std::cout << "The mode must be only one characted, either c or d" << std::endl;
			return EXIT_FAILURE;
		}

		bool compress = mode == "c";

		std::ifstream input(argv[2], std::ios::binary);

		if (!input) {
			std::cout << "Cannot open the input file" << std::endl;
			return EXIT_FAILURE;
		}

		// Tells the stream to avoid skipping whitespaces
		std::noskipws(input);

		std::ofstream output(argv[3], std::ios::binary);

		if (!output) {
			std::cout << "Cannot open the output file" << std::endl;
			return EXIT_FAILURE;
		}

		if (compress) {
			packbits_encoder encoder(input, output);
			encoder.encode();
		}
		else {
			packbits_decoder decoder(input, output);
			decoder.decode();
		}	
	}

	//_CrtDumpMemoryLeaks();

	return EXIT_SUCCESS;
}