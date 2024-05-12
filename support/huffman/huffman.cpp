#include <iostream>
#include <fstream>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>

template<typename T>
std::pair<T, bool> raw_read(std::istream& input) {
	T buffer = 0;
	input.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));
	return std::pair<T, bool>(buffer, !input.eof());
}

template<typename T>
void raw_write(std::ostream& output, T value) {
	output.write(reinterpret_cast<char*>(&value), sizeof(value));
}

class bit_reader {
private:
	std::istream& input_;
	uint8_t buffer_ = 0;
	uint8_t bits_in_buffer_ = 0;

public:

	bit_reader(std::istream& input) : input_(input) {}

	std::pair<bool, bool> read_bit() {

		if (bits_in_buffer_ == 0) {
			auto read_byte = raw_read<uint8_t>(input_);

			if (!read_byte.second) {
				return std::pair<bool, bool>(false, false);
			}

			buffer_ = read_byte.first;
			bits_in_buffer_ = 8;
		}

		bool bit = (buffer_ >> (bits_in_buffer_ - 1)) & 1;
		bits_in_buffer_--;

		return std::pair<bool, bool>(bit, true);
	}

	std::pair<uint64_t, bool> read_number(uint64_t number_of_bits) {
		uint64_t value = 0;

		while (number_of_bits > 0) { // TODO: try to avoid negative unsigned issues

			auto read_bit_value = read_bit();

			if (!read_bit_value.second) {
				return std::pair<uint64_t, bool>(0, false);
			}

			value <<= 1;
			value |= static_cast<uint64_t>(read_bit_value.first);
			number_of_bits--;
		}

		return std::pair<uint64_t, bool>(value, true);
	}
};

class bit_writer {
private:
	std::ostream& output_;
	uint8_t buffer_ = 0;
	uint8_t bits_in_buffer_ = 0;

public:
	bit_writer(std::ostream& output) : output_(output) {}

	void write_bit(bool value) {

		buffer_ <<= 1;
		buffer_ |= static_cast<uint8_t>(value);
		bits_in_buffer_++;

		if (bits_in_buffer_ == 8) {
			raw_write(output_, buffer_);
			buffer_ = 0;
			bits_in_buffer_ = 0;
		}
	}

	void write_number(uint64_t value, uint64_t bits_to_write) {
		while (bits_to_write > 0) { // TODO: try to avoid negative unsigned issues
			bool bit = (value >> (bits_to_write - 1)) & 1;
			write_bit(bit);
			bits_to_write--;
		}
	}

	~bit_writer() {
		while (bits_in_buffer_ > 0) {
			write_bit(false);
		}
	}

};

template<typename Symbol>
class frequency_counter {
private:
	std::unordered_map<Symbol, uint64_t> data_;

public:
	void operator() (const Symbol& value) {
		data_[value]++;
	}

	auto begin() const {
		return data_.begin();
	}

	auto end() const {
		return data_.end();
	}

	auto size() const {
		return data_.size();
	}
};

template<typename Symbol>
class huffman_code {
private:

	struct node {
		std::optional<Symbol> symbol;
		uint64_t frequency = 0;
		std::unique_ptr<node> low_frequency_node = nullptr;
		std::unique_ptr<node> high_frequency_node = nullptr;

		node(std::optional<Symbol> symbol, uint64_t frequency) : symbol(symbol), frequency(frequency) {}

		node(std::unique_ptr<node>& low_frequency_node, std::unique_ptr<node>& high_frequency_node)
		{
			this->frequency = low_frequency_node->frequency + high_frequency_node->frequency;
			this->low_frequency_node = std::move(low_frequency_node);
			this->high_frequency_node = std::move(high_frequency_node);
		}
	};

	struct symbol_data {
		Symbol symbol = 0;
		uint64_t code = 0;
		uint64_t code_length = 0;
	};

	std::unordered_map<Symbol, symbol_data> symbols_map_;
	std::vector<std::reference_wrapper<symbol_data>> sorted_symbols_data_;

	void visit_node(const std::unique_ptr<node>& node, uint64_t code, uint64_t depth) {

		if (node->low_frequency_node == nullptr && node->high_frequency_node == nullptr) {

			symbols_map_[node->symbol.value()] = { node->symbol.value(), code, depth };
			return;
		}

		visit_node(node->low_frequency_node, (code << 1) | 0, depth + 1);
		visit_node(node->high_frequency_node, (code << 1) | 1, depth + 1);
	}

	void create_sorted_symbols() {
		for (auto& data : symbols_map_) {
			sorted_symbols_data_.push_back(std::reference_wrapper<symbol_data>(data.second));
		}

		std::sort(sorted_symbols_data_.begin(), sorted_symbols_data_.end(),
			[](const std::reference_wrapper<symbol_data>& first_ref, const std::reference_wrapper<symbol_data>& second_ref) {
				const symbol_data& first = first_ref.get();
				const symbol_data& second = second_ref.get();

				if (first.code_length == second.code_length) {
					return first.symbol < second.symbol;
				}
				else {
					return first.code_length < second.code_length;
				}
			});
	}

public:

	huffman_code(const std::unordered_map<Symbol, uint64_t>& map) {

		for (const auto& symbol : map) {
			symbols_map_[symbol.first] = { symbol.first, 0, symbol.second };
		}

		create_sorted_symbols();
	}

	huffman_code(const frequency_counter<Symbol>& frequency_map) {

		std::vector<std::unique_ptr<node>> nodes;

		for (const auto& symbol : frequency_map) {
			nodes.push_back(std::make_unique<node>(symbol.first, symbol.second));
		}

		std::sort(nodes.begin(), nodes.end(), [](const std::unique_ptr<node>& node1, const std::unique_ptr<node>& node2) {
			return node1->frequency > node2->frequency;
			});

		while (nodes.size() > 1) {
			auto merged_node = std::make_unique<node>(nodes[nodes.size() - 1], nodes[nodes.size() - 2]);

			nodes.resize(nodes.size() - 2);

			auto insert_it = std::lower_bound(nodes.begin(), nodes.end(), merged_node,
				[](const std::unique_ptr<node>& node1, const std::unique_ptr<node>& node2) {
					return node1->frequency > node2->frequency;
				});

			nodes.insert(insert_it, std::move(merged_node));	
		}

		visit_node(nodes.front(), 0, 0);

		create_sorted_symbols();
	}

	void make_canonical() {

		uint64_t code = 0;
		uint64_t previous_length = 0;

		for (auto& symbol_data_ref : sorted_symbols_data_) {

			symbol_data& data = symbol_data_ref.get();

			uint64_t shifts = data.code_length - previous_length;
			code <<= shifts;
			data.code = code;
			code++;
			previous_length = data.code_length;
		}
	}

	const auto& symbols_map() const {
		return symbols_map_;
	}

	const auto& sorted_symbols_data() const {
		return sorted_symbols_data_;
	}
};

static bool encode_data(std::string input_file, std::string output_file) {

	std::ifstream input(input_file, std::ios::binary);

	if (!input) {
		return false;
	}

	frequency_counter<uint8_t> frequency_counter;

	uint64_t symbols_to_encode = 0;

	while (true) {
		auto read_value = raw_read<uint8_t>(input);

		if (!read_value.second) {
			break;
		}

		frequency_counter(read_value.first);

		symbols_to_encode++;
	}

	input.clear(); // this must be put before the seek
	input.seekg(0);

	huffman_code<uint8_t> huffman(frequency_counter);

	huffman.make_canonical();

	std::ofstream output(output_file, std::ios::binary);

	if (!output) {
		return false;
	}

	bit_writer bit_writer(output);

	output << "HUFFMAN2";

	auto table_size = huffman.symbols_map().size();

	if (table_size == 256) {
		raw_write<uint8_t>(output, 0);
	}
	else {
		raw_write<uint8_t>(output, static_cast<uint8_t>(table_size));
	}

	for (const auto& symbol_data : huffman.sorted_symbols_data()) {
		bit_writer.write_number(symbol_data.get().symbol, 8);
		bit_writer.write_number(symbol_data.get().code_length, 5);
	}

	bit_writer.write_number(symbols_to_encode, 32);

	const auto& map = huffman.symbols_map();

	while (true) {
		auto read_value = raw_read<uint8_t>(input);

		if (!read_value.second) {
			break;
		}

		const auto& x = map.at(read_value.first);

		bit_writer.write_number(x.code, x.code_length);
	}

	return true;
}

static bool decode_data(std::string input_file, std::string output_file) {

	std::ifstream input(input_file, std::ios::binary);

	if (!input) {
		return false;
	}

	bit_reader bit_reader(input);

	std::string magic_number(8, ' ');
	input.read(magic_number.data(), 8);

	auto table_size = raw_read<uint8_t>(input);

	if (!table_size.second) {
		return false;
	}

	std::unordered_map<uint8_t, uint64_t> map;

	for (int i = 0; i < table_size.first; ++i) {
		auto read_symbol = bit_reader.read_number(8);

		if (!read_symbol.second) {
			return false;
		}

		auto read_code_length = bit_reader.read_number(5);

		if (!read_code_length.second) {
			return false;
		}

		map[static_cast<uint8_t>(read_symbol.first)] = read_code_length.first;
	}

	huffman_code<uint8_t> huffman(map);

	huffman.make_canonical();

	const auto& sorted_symbols = huffman.sorted_symbols_data();

	auto number_of_symbols = bit_reader.read_number(32);

	if (!number_of_symbols.second) {
		return false;
	}

	std::ofstream output(output_file, std::ios::binary);

	if (!output) {
		return false;
	}

	for (uint64_t i = 0; i < number_of_symbols.first; ++i) {

		uint64_t code = 0;
		uint64_t bits_in_code = 0;

		bool found = false;

		for (const auto& symbol_ref : sorted_symbols) {

			const auto& symbol = symbol_ref.get();

			while (bits_in_code < symbol.code_length) {

				auto bit = bit_reader.read_bit();

				if (!bit.second) {
					return false;
				}

				code <<= 1;
				code |= static_cast<uint64_t>(bit.first);
				bits_in_code++;
			}

			if (code == symbol.code) {
				raw_write<uint8_t>(output, symbol.symbol);
				found = true;
				break;
			}
		}

		if (!found) {
			return false;
		}
	}

	return true;
}

int main(int argc, char* argv[]) {

	if (argc != 4) {
		std::cerr << "Invalid number of arguments" << std::endl;
		return EXIT_FAILURE;
	}

	std::string mode(argv[1]);

	if (mode == "c") {
		if (!encode_data(argv[2], argv[3])) {
			std::cerr << "Encoding failed" << std::endl;
			return EXIT_FAILURE;
		}
	}
	else if (mode == "d") {
		if (!decode_data(argv[2], argv[3])) {
			std::cerr << "Decoding failed" << std::endl;
			return EXIT_FAILURE;
		}
	}
	else {
		std::cerr << "Mode can be either c or d" << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}