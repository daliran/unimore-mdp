#include <fstream>
#include <iostream>
#include <unordered_map>
#include <list>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <memory>
//#include <crtdbg.h>

class bit_writer {
private:
	std::ostream& output_;
	uint8_t buffer_ = 0;
	uint8_t bits_in_buffer_ = 0;

public:
	bit_writer(std::ostream& output) : output_(output) { }

	void write_bit(bool bit_value) {

		if (bit_value) {
			buffer_ |= 1;
		}

		bits_in_buffer_++;

		if (bits_in_buffer_ == 8) {
			output_.write(reinterpret_cast<char*>(&buffer_), sizeof(buffer_));
			buffer_ = 0;
			bits_in_buffer_ = 0;
		}
		else {
			buffer_ <<= 1;
		}
	}

	void write_number(uint64_t number, uint8_t bits_to_write) {
		for (uint8_t i = 0; i < bits_to_write; ++i) {
			bool bit = (number >> (bits_to_write - i - 1)) & 1;
			write_bit(bit);
		}
	}

	~bit_writer() {
		while (bits_in_buffer_ > 0) {
			write_bit(0);
		}
	}

};

class bit_reader {
private:
	std::istream& input_;
	uint8_t buffer_ = 0;
	uint8_t bits_in_buffer_ = 0;

public:
	bit_reader(std::istream& input) : input_(input) { }

	std::pair<bool, bool> read_bit() {

		if (bits_in_buffer_ == 0) {

			input_.read(reinterpret_cast<char*>(&buffer_), sizeof(buffer_));

			if (input_.eof()) {
				return std::pair<bool, bool>(false, false);
			}

			bits_in_buffer_ = 8;
		}

		bool bit = (buffer_ >> (bits_in_buffer_ - 1)) & 1;

		bits_in_buffer_--;

		return std::pair<bool, bool>(bit, true);
	}

	std::pair<uint64_t, bool> read_number(uint8_t bits_to_read) {

		uint64_t value = 0;
		uint8_t bits_in_value = 0;

		while (bits_to_read - bits_in_value > 0) {

			std::pair<bool, bool> result = read_bit();

			if (!result.second) {
				return std::pair<bool, bool>(false, false);
			}

			value <<= 1;

			if (result.first) {
				value |= 1;
			}

			bits_in_value++;
		}

		return std::pair<uint64_t, bool>(value, true);
	}
};

struct symbol_data {
	uint8_t symbol;
	uint32_t frequency;
	uint8_t code_length;
	uint32_t code;
};

struct huffman_code_calculation_node {
	std::vector<uint8_t> symbols;
	uint32_t frequency = 0;
	std::shared_ptr<huffman_code_calculation_node> branch_a = nullptr;
	std::shared_ptr<huffman_code_calculation_node> branch_b = nullptr;

	huffman_code_calculation_node(
		std::vector<uint8_t> symbols,
		uint32_t frequency,
		std::shared_ptr<huffman_code_calculation_node> branch_a = nullptr,
		std::shared_ptr<huffman_code_calculation_node> branch_b = nullptr
	) : symbols(symbols), frequency(frequency), branch_a(branch_a), branch_b(branch_b) { }

	huffman_code_calculation_node(
		const std::shared_ptr<huffman_code_calculation_node>& first,
		const std::shared_ptr<huffman_code_calculation_node>& second
	) {
		frequency = first->frequency + second->frequency;

		branch_a = first; // lower frequency
		branch_b = second; // higher frequency

		std::copy(first->symbols.begin(), first->symbols.end(), std::back_inserter(symbols));
		std::copy(second->symbols.begin(), second->symbols.end(), std::back_inserter(symbols));
	}
};

class base_huffman {

protected:
	std::unordered_map<uint8_t, symbol_data> symbols_data_;

	std::vector<std::reference_wrapper<symbol_data>> create_sorted_symbols_data() {

		std::vector<std::reference_wrapper<symbol_data>> sorted_symbol_data;

		for (auto& item : symbols_data_) {
			sorted_symbol_data.push_back(std::reference_wrapper<symbol_data>(item.second));
		}

		// the calculation is done by sorting first on the length, then the symbol
		std::sort(sorted_symbol_data.begin(), sorted_symbol_data.end(), [](const std::reference_wrapper<symbol_data>& a, const std::reference_wrapper<symbol_data>& b) {
			if (a.get().code_length == b.get().code_length) {
				return a.get().symbol < b.get().symbol;
			}
			else {
				return a.get().code_length < b.get().code_length;
			}
			});

		return sorted_symbol_data;
	}

	void calculate_canonical_huffman() {

		const auto sorted_symbol_data = create_sorted_symbols_data();

		uint32_t currentNumber = 0;
		uint8_t currentLength = 1;

		for (auto& container_item : sorted_symbol_data) {

			symbol_data& data = container_item.get();
			currentNumber <<= (data.code_length - currentLength);
			data.code = currentNumber;
			currentNumber++;
			currentLength = data.code_length;
		}
	}
};

class canonical_huffman_encoder : public base_huffman {
private:
	std::istream& input_;
	bit_writer bit_writer_;

	void compute_frequencies() {
		while (true) {
			uint8_t buffer = 0;

			input_.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));

			if (input_.eof()) {
				return;
			}

			symbol_data& symbol_data = symbols_data_[buffer];
			symbol_data.symbol = buffer;
			symbol_data.frequency++;
		}
	}

	void calculate_code_length() {

		std::vector<std::shared_ptr<huffman_code_calculation_node>> tree;

		for (const auto& item : symbols_data_) {

			std::shared_ptr<huffman_code_calculation_node> tree_item =
				std::make_shared<huffman_code_calculation_node>(huffman_code_calculation_node({ item.first }, item.second.frequency));

			tree.push_back(tree_item);
		}

		std::sort(tree.begin(), tree.end(), [](const std::shared_ptr<huffman_code_calculation_node>& a, const std::shared_ptr<huffman_code_calculation_node>& b) {
			return a->frequency > b->frequency;
			});

		while (tree.size() > 1) {

			/*std::sort(tree.begin(), tree.end(), [](const std::shared_ptr<huffman_code_calculation_node>& a, const std::shared_ptr<huffman_code_calculation_node>& b) {
				return a->frequency > b->frequency;
				});*/

			std::shared_ptr<huffman_code_calculation_node> first = tree[tree.size() - 1]; // lower frequency
			std::shared_ptr<huffman_code_calculation_node> second = tree[tree.size() - 2]; // higher frequency

			std::shared_ptr<huffman_code_calculation_node> tree_item = std::make_shared<huffman_code_calculation_node>(huffman_code_calculation_node(first, second));

			auto insert_it = std::lower_bound(tree.begin(), tree.end(), tree_item, []
			(const std::shared_ptr<huffman_code_calculation_node>& a, const std::shared_ptr<huffman_code_calculation_node>& b) {
					return a->frequency > b->frequency;
			});

			tree.insert(insert_it, tree_item);

			tree.resize(tree.size() - 2);

			//tree.push_back(tree_item);
		}

		tree_exploration(tree.front(), 0);
	}

	void tree_exploration(std::shared_ptr<huffman_code_calculation_node> node, uint32_t depth) {

		if (node->branch_a == nullptr && node->branch_b == nullptr) {

			symbol_data& data = symbols_data_[node->symbols[0]];
			data.code_length = depth;
			return;
		}

		tree_exploration(node->branch_a, depth + 1);
		tree_exploration(node->branch_b, depth + 1);
	}

	void encode_and_write() {

		std::string magic_number = "HUFFMAN2";

		for (auto& item : magic_number) {
			bit_writer_.write_number(item, 8);
		}

		if (symbols_data_.size() == 256) {
			bit_writer_.write_number(0, 8);
		}
		else {
			bit_writer_.write_number(symbols_data_.size(), 8);
		}

		const auto sorted_symbols_data = create_sorted_symbols_data();

		for (auto& container_item : sorted_symbols_data) {

			const symbol_data& data = container_item.get();

			bit_writer_.write_number(data.symbol, 8);
			bit_writer_.write_number(data.code_length, 5);
		}

		int number_of_symbols = std::accumulate(symbols_data_.begin(), symbols_data_.end(), 0,
			[](const int& acc, const std::pair<uint8_t, symbol_data>& element) {
				return acc + element.second.frequency;
			});

		bit_writer_.write_number(number_of_symbols, 32);

		while (true) {
			uint8_t buffer = 0;

			input_.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));

			if (input_.eof()) {
				return;
			}

			const symbol_data& data = symbols_data_[buffer];

			bit_writer_.write_number(data.code, data.code_length);
		}
	}

public:
	canonical_huffman_encoder(std::istream& input, std::ostream& output) : input_(input), bit_writer_(output) { }

	void encode() {
		compute_frequencies();

		calculate_code_length();
		calculate_canonical_huffman();

		// Clear the EOF flag and seek to the beginning of the file
		input_.clear();
		input_.seekg(0);

		encode_and_write();
	}
};

class canonical_huffman_decoder : public base_huffman {
private:
	bit_reader bit_reader_;
	std::ostream& output_;

	void read_table() {

		std::string magic_number;

		for (uint8_t i = 0; i < 8; ++i) {

			auto result = bit_reader_.read_number(8);

			if (!result.second) {
				return;
			}

			magic_number.push_back(static_cast<char>(result.first));
		}

		auto table_entries_raw = bit_reader_.read_number(8);

		if (!table_entries_raw.second) {
			return;
		}

		uint8_t table_entries = static_cast<uint8_t>(table_entries_raw.first);

		for (uint8_t i = 0; i < table_entries; ++i) {

			auto symbol_raw = bit_reader_.read_number(8);

			if (!symbol_raw.second) {
				return;
			}

			uint8_t symbol = static_cast<uint8_t>(symbol_raw.first);

			symbol_data& data = symbols_data_[symbol];
			data.symbol = symbol;

			auto code_length_raw = bit_reader_.read_number(5);

			if (!code_length_raw.second) {
				return;
			}

			data.code_length = static_cast<uint8_t>(code_length_raw.first);	
		}

		calculate_canonical_huffman();
	}

	void read_data() {

		auto number_of_symbols_raw = bit_reader_.read_number(32);

		if (!number_of_symbols_raw.second) {
			return;
		}

		uint32_t number_of_symbols = static_cast<uint32_t>(number_of_symbols_raw.first);

		uint32_t read_symbols = number_of_symbols;
		uint32_t code = 0;

		uint32_t read_bits = 0;

		// Optimization
		auto sorted_symbols = create_sorted_symbols_data();

		uint32_t min_code_length = sorted_symbols.front().get().code_length;

		while (read_symbols > 0) {
			auto bit_raw = bit_reader_.read_bit();

			// This exists in case the file reached eof
			if (!bit_raw.second) {
				return;
			}

			bool bit = bit_raw.first;

			code <<= 1;

			if (bit) {
				code |= 1;
			}

			read_bits++;

			// Optimization
			if ( read_bits < min_code_length) {
				continue;
			}

			for (const auto& container_item : sorted_symbols) {

				auto& symbol_reference = container_item.get();

				bool match = read_bits == symbol_reference.code_length && symbol_reference.code == code;

				if (match) {
					output_ << symbol_reference.symbol;
					read_symbols--;
					code = 0;
					read_bits = 0;
					break;
				}	
			}
		}
	}

public:
	canonical_huffman_decoder(std::istream& input, std::ostream& output) : bit_reader_(input), output_(output) { }

	void decode() {
		read_table();
		read_data();
	}
};

int main(int argc, char* argv[]) {

	{
		if (argc != 4) {
			std::cout << "Invalid number of arguments" << std::endl;
			return EXIT_FAILURE;
		}

		std::string mode(argv[1]);

		if (mode.length() > 1) {
			std::cout << "The mode must be a single character with value c or d" << std::endl;
			return EXIT_FAILURE;
		}

		// The operator != doens't work
		if (!(mode == "c" || mode == "d"))
		{
			std::cout << "Mode must be either c or d" << std::endl;
			return EXIT_FAILURE;
		}

		bool compress = mode == "c";

		std::ifstream input(argv[2], std::ios::binary);

		if (!input) {
			std::cout << "Cannot open the input file" << std::endl;
			return EXIT_FAILURE;
		}

		std::ofstream output(argv[3], std::ios::binary);

		if (!output) {
			std::cout << "Cannot open the output file" << std::endl;
			return EXIT_FAILURE;
		}

		if (compress) {
			canonical_huffman_encoder encoder(input, output);
			encoder.encode();
		}
		else {
			canonical_huffman_decoder decoder(input, output);
			decoder.decode();
		}
	}

	//_CrtDumpMemoryLeaks();

	return EXIT_SUCCESS;
}

