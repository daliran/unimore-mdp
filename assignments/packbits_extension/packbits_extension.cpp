#include <fstream>
#include <iostream>
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

class packbits {
private:

	void write_copy(std::vector<uint8_t>& buffer, std::ostream& output) {

		if (buffer.size() == 0) {
			return;
		}

		// Only take up to 128 bytes from the buffer
		uint8_t bytes_in_buffer = std::min<uint8_t>(static_cast<uint8_t>(buffer.size()), 128);

		// The copy is 0 based
		uint8_t encoded_bytes_to_write = bytes_in_buffer - 1;

		raw_write(encoded_bytes_to_write, output);

		for (const auto& container_item : buffer) {
			raw_write(container_item, output);
		}

		//std::cout << "COPY (" << buffer.size() << ", " << std::string(buffer.begin(), buffer.end()) << ")" << std::endl;

		buffer.clear();
	}

	void write_run(std::vector<uint8_t>& buffer, std::ostream& output) {

		if (buffer.size() == 0) {
			return;
		}

		// Only take up to 128 bytes from the buffer
		uint8_t bytes_in_buffer = std::min<uint8_t>(static_cast<uint8_t>(buffer.size()), 128);

		uint8_t encoded_number_of_repetitions = 257 - bytes_in_buffer;

		raw_write(encoded_number_of_repetitions, output);

		uint8_t symbol = buffer.back();

		raw_write(symbol, output);

		//std::cout << "RUN (" << buffer.size() << ", " << symbol << ")" << std::endl;

		buffer.clear();
	}

	void write_buffer(bool read_state_run, std::vector<uint8_t>& buffer, std::ostream& output) {
		if (buffer.size() > 0) {
			if (read_state_run) {
				write_run(buffer, output);
			}
			else {
				write_copy(buffer, output);
			}
		}
	}

	void execute_copy(uint8_t command, std::istream& input, std::ostream& output) {
		uint8_t bytes_to_read = command + 1;

		for (int i = 0; i < bytes_to_read; ++i) {
			auto read_byte = raw_read(input);

			if (!read_byte.second) {
				return;
			}

			raw_write(read_byte.first, output);
		}
	}

	void execute_run(uint8_t command, std::istream& input, std::ostream& output) {

		uint8_t repetitons = 257 - command;

		auto read_symbol = raw_read(input);

		if (!read_symbol.second) {
			return;
		}

		for (int i = 0; i < repetitons; ++i) {
			raw_write(read_symbol.first, output);
		}
	}

public:
	void encode(std::istream& input, std::ostream& output) {

		std::vector<uint8_t> buffer;
		bool read_state_run = true;

		for (std::istream_iterator<uint8_t> it(input); it != std::istream_iterator<uint8_t>(); ++it) {

			uint8_t value = *it;

			if (buffer.size() != 0) {

				uint8_t last_buffer_value = buffer.back();

				if (read_state_run) {
					if (last_buffer_value != value) {
						if (buffer.size() > 1) {
							// write runs only if there are at least 2 elements in the buffer
							write_run(buffer, output);
						}
						else {
							read_state_run = false;
						}
					}
					else if (buffer.size() >= 128) {
						// Write the buffer if the run exceeds 128 repetitions
						write_run(buffer, output);
					}
				}
				else if (!read_state_run) {

					if (buffer.size() > 1) {
						uint8_t last_buffer_value_minus_1 = buffer[buffer.size() - 2];

						if (last_buffer_value_minus_1 == last_buffer_value || value == last_buffer_value) {
							// This is a candidate run to be integrated

							if (last_buffer_value_minus_1 == last_buffer_value && value == last_buffer_value) {
								// The doesn't need to be integrated, separating the run fron the copy
								buffer.pop_back();
								buffer.pop_back();
								write_copy(buffer, output);

								buffer.push_back(last_buffer_value_minus_1);
								buffer.push_back(last_buffer_value);
								read_state_run = true;
							}
							// If we are not sure yet if the run can be integrated, let's wait for the next value
						}	
						else if (buffer.size() >= 128) {
							// Write the buffer if the copy exceeds 128 elements
							write_copy(buffer, output);
						}
					}
				}
			}

			buffer.push_back(value);
		}

		write_buffer(read_state_run, buffer, output);

		// Write EOD
		raw_write(128, output);
	}

	void decode(std::istream& input, std::ostream& output) {

		while (true) {
			auto read_command = raw_read(input);

			if (!read_command.second) {
				break;
			}

			if (read_command.first < 128) {
				execute_copy(read_command.first, input, output);
			}
			else if (read_command.first == 128) {
				break; // EOD
			}
			else {
				execute_run(read_command.first, input, output);
			}
		}
	}
};

int main(int argc, char* argv[]) {
	{
		if (argc != 4) {
			std::cerr << "Invalid number of arguments";
			return EXIT_FAILURE;
		}

		std::string mode(argv[1]);

		if (!(mode == "c" || mode == "d")) {
			std::cerr << "Mode must be a single charatecter, either c or d";
			return EXIT_FAILURE;
		}

		bool compress = mode == "c";

		std::ifstream input(argv[2], std::ios::binary);

		if (!input) {
			std::cerr << "Failed to open the input file";
			return EXIT_FAILURE;
		}

		// Tells the stream to avoid skipping whitespaces
		std::noskipws(input);

		std::ofstream output(argv[3], std::ios::binary);

		if (!output) {
			std::cerr << "Failed to open the output file";
			return EXIT_FAILURE;
		}

		packbits packbits;

		if (compress) {
			packbits.encode(input, output);
		}
		else {
			packbits.decode(input, output);
		}
	}

	//_CrtDumpMemoryLeaks();

	return EXIT_SUCCESS;
}