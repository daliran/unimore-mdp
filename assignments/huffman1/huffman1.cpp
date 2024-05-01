#include <fstream>
#include <iostream>
#include <cctype>
#include <map>
#include <list>
#include <algorithm>
#include <vector>
#include <memory>
#include <cstdint>
//#include <crtdbg.h>

struct huffman_node {
	uint8_t frequency = 0;
	uint32_t code = 0;
	uint8_t code_length = 0;
	std::vector<uint8_t> symbols;
	std::vector<std::shared_ptr<huffman_node>> children;
};

struct symbol_data {
	uint8_t frequency = 0;
	uint8_t length = 0;
	uint32_t code = 0;
};

class bit_writer {

private:
	std::ostream& output_;
	uint8_t buffer_ = 0;
	uint8_t bits_in_buffer = 0;

public:
	bit_writer(std::ostream& output) : output_(output) { }

	template<typename T>
	void write_number(T number, uint8_t bits_to_write) {
		for (uint8_t i = 0; i < bits_to_write; i++) {
			bool bit_to_write = (number >> (bits_to_write - i - 1)) & 1;
			write_bit(bit_to_write);
		}
	}

	void write_bit(bool value) {

		if (value) {
			buffer_ |= 1;
		}

		bits_in_buffer++;

		if (bits_in_buffer == 8) {
			output_.write(reinterpret_cast<char*>(&buffer_), sizeof(buffer_));
			buffer_ = 0;
			bits_in_buffer = 0;
		}
		else {
			buffer_ <<= 1;
		}
	}

	void flush_buffer() {
		while (bits_in_buffer > 0) {
			write_bit(0);
		}
	}

	~bit_writer() {
		flush_buffer();
	}

};

class huffman_encoder {
private:
	std::istream& input_;
	bit_writer bit_writer_;
	std::list<uint8_t> raw_data_;
	std::map<uint8_t, symbol_data> symbols_data_;

	void read_data() {
		while (true) {
			uint8_t buffer = 0;
			input_.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));

			if (input_.eof()) {
				break;
			}

			raw_data_.push_back(buffer);
		}
	}

	void calculate_frequency() {

		for (const auto& item : raw_data_) {
			symbols_data_[item].frequency += 1;
		}
	}

	void compute_huffman_code() {

		std::vector<std::shared_ptr<huffman_node>> nodes_to_be_checked;

		for (const auto& item : symbols_data_) {

			std::shared_ptr<huffman_node> node = std::make_shared<huffman_node>();
			node->frequency = item.second.frequency;
			node->symbols.push_back(item.first);
			nodes_to_be_checked.push_back(node);
		}

		// COMMENT: Adding the single node having all symbols is not really necessary, it can be skipped
		// This takes care also of cases where there is only one symbol
		while (nodes_to_be_checked.size() != 1) {

			// COMMENT: When a should be put before b? Never put an = sign in sorting.

			// TODO: check how to sort always on top or bottom in case of frequencies with the same value
			// COMMENT: With this solution is not possible to always add on the top or always add to the bottom because nodes with the same frequencies cannot be distinguish
			// the solution is to decided manually where to add the merged node
			std::sort(nodes_to_be_checked.begin(), nodes_to_be_checked.end(),
				[](const std::shared_ptr<huffman_node> a, const std::shared_ptr<huffman_node> b) { return a->frequency > b->frequency; }); 

			// COMMENT: Instead of resorting the array all the time the merged point can be put into the right place directly using the vector insert function, using an iterator
			// when inserting the iterator becomes invalid, so the loop must be interruped
			// A better solution is to use the lower_bound algorithm to find the first match

			std::shared_ptr<huffman_node> first = nodes_to_be_checked[nodes_to_be_checked.size() - 1];
			std::shared_ptr<huffman_node> second = nodes_to_be_checked[nodes_to_be_checked.size() - 2];

			std::shared_ptr<huffman_node> merged = std::make_shared<huffman_node>();

			// COMMENT: The merging could be handled by a constructor or a function inside the huffman_node struct

			merged->frequency = first->frequency + second->frequency;

			std::copy(first->symbols.begin(), first->symbols.end(), std::back_inserter(merged->symbols));
			std::copy(second->symbols.begin(), second->symbols.end(), std::back_inserter(merged->symbols));

			merged->children.push_back(first);
			merged->children.push_back(second);

			nodes_to_be_checked.resize(nodes_to_be_checked.size() - 2);

			nodes_to_be_checked.push_back(merged);
		}

		// COMMENT: The first element of an array can be retrieved using the back function
		// The symbol with the lowest probability (first item of the array) gets the 0, the other the 1
		std::shared_ptr<huffman_node> root = nodes_to_be_checked[0];
		tree_navigation(root->children[0], 0, 1);
		tree_navigation(root->children[1], 1, 1);
	}

	void tree_navigation(std::shared_ptr<huffman_node> node, uint32_t code, uint8_t code_length) {

		node->code = code;
		node->code_length = code_length;

		if (node->children.empty()) {
			// Store the resulting coding
			symbol_data& data = symbols_data_[node->symbols[0]];
			data.code = code;
			data.length = code_length;
			return;
		}

		// The symbol with the lowest probability (first item of the array) gets the 0, the other the 1
		tree_navigation(node->children[0], code << 1, code_length + 1);
		tree_navigation(node->children[1], (code << 1) | 1, code_length + 1);
	}

	void encode_input_data() {

		// COMMENT: This can also be achieved by using the << operator in the out stream
		std::string magic_number("HUFFMAN1");

		for (const char& character : magic_number) {
			bit_writer_.write_number(character, 8);
		}

		if (symbols_data_.size() == 256) {
			bit_writer_.write_number(0, 8);
		}
		else {
			bit_writer_.write_number(symbols_data_.size(), 8);
		}

		for (const auto& item : symbols_data_) {
			bit_writer_.write_number(item.first, 8);
			bit_writer_.write_number(item.second.length, 5);
			bit_writer_.write_number(item.second.code, item.second.length);
		}

		std::list<std::pair<uint32_t, uint8_t>> encoded_data;

		// COMMENT: This can also be achieved by summing all frequencies in the frequency map, no need to pre store the raw data
		bit_writer_.write_number(raw_data_.size(), 32);

		// COMMENT: If the raw data is not prestored, we need to clear the state of the stream and seek to the beginning
		for (const auto& item : raw_data_) {
			const symbol_data& symbol_data = symbols_data_[item];
			bit_writer_.write_number(symbol_data.code, symbol_data.length);
		}
	}

public:
	huffman_encoder(std::istream& input, std::ostream& output) : input_(input), bit_writer_(output) { }

	void encode() {

		// Reading data must be done upfront because the output format requires to know the number of encoded symbols in advance
		read_data();

		// If there are no items then skip
		if (raw_data_.size() == 0) {
			return;
		}

		calculate_frequency();
		compute_huffman_code();
		encode_input_data();
	}
};

class bit_reader {
private:
	std::istream& input_;
	uint8_t buffer_ = 0;
	uint8_t bits_in_buffer = 0;

public:
	bit_reader(std::istream& input) : input_(input) { }

	std::pair<bool, bool> read_bit() {

		if (bits_in_buffer == 0) {
			input_.read(reinterpret_cast<char*>(&buffer_), sizeof(buffer_));
			bits_in_buffer = 8;

			if (input_.eof()) {
				return std::pair<bool, bool>(false, false);
			}
		}

		bool bit = buffer_ >> (bits_in_buffer - 1) & 1;
		bits_in_buffer--;

		return std::pair<bool, bool>(bit, true);
	}

	template<typename T>
	std::pair<T, bool> read_number(uint8_t bits_to_read) {

		T value = 0;

		while (bits_to_read > 0) {
			std::pair<bool, bool> bit = read_bit();

			if (!bit.second) {
				return std::pair<T, bool>(0, false);
			}

			value <<= 1;

			if (bit.first) {
				value |= 1;
			}

			bits_to_read--;
		}

		return std::pair <T, bool>(value, true);
	}
};

class huffman_decoder {
private:
	bit_reader bit_reader_;
	std::ostream& output_;	
	std::list<uint8_t> raw_data_;
	std::map<uint8_t, symbol_data> symbols_data_;

	void read_header() {

		std::string magic_number;

		for (int i = 0; i < 8; ++i) {
			std::pair<char, bool> magin_number_character = bit_reader_.read_number<char>(8);

			if (!magin_number_character.second) {
				return;
			}

			magic_number.push_back(magin_number_character.first);
		}	

		std::pair<uint8_t, bool> table_entries_raw = bit_reader_.read_number<uint8_t>(8);

		if (!table_entries_raw.second) {
			return;
		}

		uint32_t table_entries = table_entries_raw.first > 0 ? table_entries_raw.first : 256;

		for (uint32_t i = 0; i < table_entries; ++i) {

			std::pair<uint8_t, bool> symbol_raw = bit_reader_.read_number<uint8_t>(8);

			if (!symbol_raw.second) {
				return;
			}

			std::pair<uint8_t, bool> symbol_length_raw = bit_reader_.read_number<uint8_t>(5);

			if (!symbol_length_raw.second) {
				return;
			}

			std::pair<uint32_t, bool> code_raw = bit_reader_.read_number<uint32_t>(symbol_length_raw.first);

			if (!code_raw.second) {
				return;
			}

			symbol_data& data = symbols_data_[symbol_raw.first];
			data.code = code_raw.first;
			data.length = symbol_length_raw.first;
		}
	}

	void read_write_data() {

		std::pair<uint32_t, bool> numb_symbols_raw = bit_reader_.read_number<uint32_t>(32);

		if (!numb_symbols_raw.second) {
			return;
		}
		
		uint32_t symbols_to_read = numb_symbols_raw.first;

		uint32_t read_code = 0;

		while (symbols_to_read > 0) {
			std::pair<bool, bool> bit = bit_reader_.read_bit();

			if (!bit.second) {
				return;
			}

			read_code <<= 1;

			if (bit.first) {
				read_code |= 1;
			}

			for (const auto& data : symbols_data_) {

				if (read_code == data.second.code) {
					read_code = 0;
					output_ << data.first;
					symbols_to_read--;
					break;
				}
			}
		}
	}

public:
	huffman_decoder(std::istream& input, std::ostream& output) : bit_reader_(input), output_(output) { }

	void decode() {
		read_header();
		read_write_data();
	}
};

int main(int argc, char* argv[]) {

	{
		if (argc < 4) {
			std::cout << "Invalid number of arguments";
			return EXIT_FAILURE;
		}

		std::string mode(argv[1]);

		if (mode.length() > 1) {
			std::cout << "The mode must be a single character";
			return EXIT_FAILURE;
		}

		if (mode.compare("c") != 0 && mode.compare("d") != 0) {
			std::cout << "The mode must either c or d";
			return EXIT_FAILURE;
		}

		bool compress = mode.compare("c") == 0;

		std::ifstream input(argv[2], std::ios::binary);

		if (!input) {
			std::cout << "Failed to open the input file";
			return EXIT_FAILURE;
		}

		std::ofstream output(argv[3], std::ios::binary);

		if (!output) {
			std::cout << "Failed to open the output file";
			return EXIT_FAILURE;
		}

		if (compress) {
			huffman_encoder encoder(input, output);
			encoder.encode();
		}
		else {
			huffman_decoder decoder(input, output);
			decoder.decode();
		}
	}

	//_CrtDumpMemoryLeaks();

	return EXIT_SUCCESS;
}