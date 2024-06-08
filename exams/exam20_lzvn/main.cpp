#include <fstream>
#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>

class lzvn_decoder {
private:

	std::ifstream& input_;
	std::ofstream& output_;
	std::vector<uint8_t> dictionary_;

	uint16_t last_distance_ = 0;

	enum class opcode_type {
		sml_d,
		med_d,
		lrg_d,
		pre_d,
		sml_m,
		lrg_m,
		sml_l,
		lrg_l,
		nop,
		eos,
		udef
	};

	opcode_type detect_opcode(uint8_t byte) {

		// Decode 8 explicit bits
		if (byte == 0b11110000) {
			return opcode_type::lrg_m;
		}
		else if (byte == 0b11100000) {
			return opcode_type::lrg_l;
		}
		else if (byte == 00001110 || byte == 00010110) {
			return opcode_type::nop;
		}
		else if (byte == 0b00000110) {
			return opcode_type::eos;
		}
		else if (byte == 0b00011110 || byte == 0b00100110 || byte == 0b00101110
			|| byte == 0b00110110 || byte == 0b00111110) {
			return opcode_type::udef;
		}
		else {

			// decode 4 explicit bits
			uint8_t upper_bits = (byte >> 4);

			if (upper_bits == 0b1101 || upper_bits == 0b0111) {
				return opcode_type::udef;
			}
			else if (upper_bits == 0b1110) {
				return opcode_type::sml_l;
			}
			else if (upper_bits == 0b1111) {
				return opcode_type::sml_m;
			}
			else {
				if ((byte >> 5) == 0b101) {
					return opcode_type::med_d;
				}
				else if ((byte & 0b00000111) == 0b111) {
					return opcode_type::lrg_d;
				}
				else if ((byte & 0b00000111) == 0b110) {
					return opcode_type::pre_d;
				}
				else {
					return opcode_type::sml_d;
				}
			}
		}
	}

	void execute_match(uint16_t match_distance, uint16_t match_length) {
		if (match_distance == 0) {
			return;
		}

		std::vector<uint8_t>::iterator begin = std::prev(dictionary_.end(), match_distance);
		std::vector<uint8_t>::iterator end = dictionary_.end();
		std::vector<uint8_t>::iterator current = begin;

		std::vector<uint8_t> buffer(match_length);

		for (uint16_t i = 0; i < match_length; ++i) {
			buffer[i] = *current;

			current++;

			if (current == end) {
				current = begin;
			}
		}

		std::copy(buffer.begin(), buffer.end(), std::back_inserter(dictionary_));
	}

	void read_literal(uint16_t literal_length) {
		if (literal_length > 0) {
			std::vector<uint8_t> buffer(literal_length);
			input_.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
			std::copy(buffer.begin(), buffer.end(), std::back_inserter(dictionary_));
		}
	}

	void execute_sml_d(uint8_t byte) {
		uint8_t literal_length = (byte & 0b11000000) >> 6;
		uint8_t match_length = ((byte & 0b00111000) >> 3) + 3; // bias
		uint8_t byte2 = 0;
		input_.read(reinterpret_cast<char*>(&byte2), sizeof(byte2));
		uint16_t match_distance = static_cast<uint16_t>((byte & 0b00000111)) << 8 | byte2;

		last_distance_ = match_distance;

		// read literal
		read_literal(literal_length);

		// execute match
		execute_match(match_distance, match_length);
	}

	void execute_med_d(uint8_t byte) {
		uint8_t literal_length = (byte & 0b00011000) >> 3;

		uint8_t byte2 = 0;
		input_.read(reinterpret_cast<char*>(&byte2), sizeof(byte2));

		uint8_t match_length = ((byte & 0b00000111) << 2 | (byte2 & 0b00000011)) + 3; // bias

		uint8_t byte3 = 0;
		input_.read(reinterpret_cast<char*>(&byte3), sizeof(byte3));

		uint16_t match_distance = (static_cast<uint16_t>(byte3) << 6) | ((byte2 & 0b11111100) >> 2);

		last_distance_ = match_distance;

		// read literal
		read_literal(literal_length);

		// execute match
		execute_match(match_distance, match_length);
	}

	void execute_lrg_d(uint8_t byte) {
		uint8_t literal_length = (byte & 0b11000000) >> 6;
		uint8_t match_length = ((byte & 0b00111000) >> 3) + 3; // bias

		uint8_t byte2 = 0;
		input_.read(reinterpret_cast<char*>(&byte2), sizeof(byte2));

		uint8_t byte3 = 0;
		input_.read(reinterpret_cast<char*>(&byte3), sizeof(byte3));

		uint16_t match_distance = (static_cast<uint16_t>(byte3) << 8) | byte2;

		last_distance_ = match_distance;

		// read literal
		read_literal(literal_length);

		// execute match
		execute_match(match_distance, match_length);
	}

	void execute_pre_d(uint8_t byte) {
		uint8_t literal_length = (byte & 0b11000000) >> 6;
		uint8_t match_length = ((byte & 0b00111000) >> 3) + 3; // bias

		// read literal
		read_literal(literal_length);

		// execute match
		execute_match(last_distance_, match_length);
	}

	void execute_sml_m(uint8_t byte) {
		uint8_t match_length = byte & 0b00001111;

		// execute match
		execute_match(last_distance_, match_length);
	}

	void execute_lrg_m() {
		uint8_t byte2 = 0;
		input_.read(reinterpret_cast<char*>(&byte2), sizeof(byte2));

		uint16_t match_length = static_cast<uint16_t>(byte2) + 16;

		// execute match
		execute_match(last_distance_, match_length);
	}

	void execute_sml_l(uint8_t byte) {
		uint8_t literal_length = byte & 0b00001111;
		read_literal(literal_length);
	}

	void execute_lrg_l() {
		uint8_t byte2 = 0;
		input_.read(reinterpret_cast<char*>(&byte2), sizeof(byte2));

		uint16_t literal_length = static_cast<uint16_t>(byte2) + 16;
		read_literal(literal_length);
	}

	bool handle_bvxn() {

		uint32_t output_size = 0;
		input_.read(reinterpret_cast<char*>(&output_size), sizeof(output_size));

		std::cout << "bvxn output_size: " << output_size << std::endl;

		uint32_t block_size = 0;
		input_.read(reinterpret_cast<char*>(&block_size), sizeof(block_size));

		std::cout << "bvxn block_size: " << block_size << std::endl;

		// For each opcode
		while (true) {

			uint8_t opcode_byte = 0;
			input_.read(reinterpret_cast<char*>(&opcode_byte), sizeof(opcode_byte));

			opcode_type parsed_opcode = detect_opcode(opcode_byte);

			if (parsed_opcode == opcode_type::sml_d) {
				//std::cout << "bvxn sml_d" << std::endl;
				execute_sml_d(opcode_byte);
			}
			else if (parsed_opcode == opcode_type::med_d) {
				//std::cout << "bvxn med_d" << std::endl;
				execute_med_d(opcode_byte);
			}
			else if (parsed_opcode == opcode_type::lrg_d) {
				//std::cout << "bvxn lrg_d" << std::endl;
				execute_lrg_d(opcode_byte);
			}
			else if (parsed_opcode == opcode_type::pre_d) {
				//std::cout << "bvxn pre_d" << std::endl;
				execute_pre_d(opcode_byte);
			}
			else if (parsed_opcode == opcode_type::sml_m) {
				//std::cout << "bvxn sml_m" << std::endl;
				execute_sml_m(opcode_byte);
			}
			else if (parsed_opcode == opcode_type::lrg_m) {
				//std::cout << "bvxn lrg_m" << std::endl;
				execute_lrg_m();
			}
			else if (parsed_opcode == opcode_type::sml_l) {
				//std::cout << "bvxn sml_l" << std::endl;
				execute_sml_l(opcode_byte);
			}
			else if (parsed_opcode == opcode_type::lrg_l) {
				//std::cout << "bvxn lrg_l" << std::endl;
				execute_lrg_l();
			}
			else if (parsed_opcode == opcode_type::eos) {
				//std::cout << "bvxn eos" << std::endl;

				for (uint8_t i = 0; i < 7; ++i) {
					uint8_t null_byte = 0;
					input_.read(reinterpret_cast<char*>(&null_byte), sizeof(null_byte));
				}

				break;
			}
			else if (parsed_opcode == opcode_type::nop) {
				//std::cout << "bvxn nop" << std::endl;
				// Do nothing
			}
			else if (parsed_opcode == opcode_type::udef) {
				std::cerr << "bvxn udef opcode" << std::endl;
				return false;
			}
			else {
				std::cerr << "bvxn unknown opcode" << std::endl;
				return false;
			}
		}

		return true;
	}

public:

	lzvn_decoder(std::ifstream& input, std::ofstream& output) : input_(input), output_(output) {

	}

	bool decode() {

		// For each block
		while (true) {

			std::string magic_number(4, 0);
			input_.read(reinterpret_cast<char*>(magic_number.data()), magic_number.size());

			if (magic_number == "bvxn") {
				std::cout << "[bvxn block]" << std::endl;
				if (!handle_bvxn()) {
					return false;
				}
			}
			else if (magic_number == "bvx$") {
				std::cout << "[bvx$ block]" << std::endl;
				break;
			}
			else {
				std::cerr << "Not supported block: " << magic_number << std::endl;
				return false;
			}
		}

		std::cout << "Writing to output file" << std::endl;

		// Write decoded output to file
		output_.write(reinterpret_cast<char*>(dictionary_.data()), dictionary_.size());

		return true;
	}
};

int main(int argc, char* argv[]) {

	if (argc != 3) {
		std::cerr << "Invalid number of arguments" << std::endl;
		return EXIT_FAILURE;
	}

	std::ifstream input(argv[1], std::ios::binary);

	if (!input) {
		std::cerr << "Failed to open the input file" << std::endl;
		return EXIT_FAILURE;
	}

	std::ofstream output(argv[2], std::ios::binary);

	if (!output) {
		std::cerr << "Failed to open the output file" << std::endl;
		return EXIT_FAILURE;
	}

	lzvn_decoder decoder(input, output);

	if (!decoder.decode()) {
		return EXIT_FAILURE;
	}

	std::cout << "Decode completed successfully" << std::endl;

	return EXIT_SUCCESS;
}