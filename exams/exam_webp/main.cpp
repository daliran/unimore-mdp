#include <fstream>
#include <iostream>
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <iterator>

template<typename T>
T raw_read(std::ifstream& input, size_t size = sizeof(T)) {
	T value = 0;
	input.read(reinterpret_cast<char*>(&value), size);
	return value;
}

template<typename T>
void raw_write(std::ofstream& output, T value) {
	output.write(reinterpret_cast<char*>(&value), sizeof(value));
}

class bit_reader {
private:
	std::ifstream& input_;
	uint8_t buffer_ = 0;
	uint8_t read_bits_ = 8;

public:
	bit_reader(std::ifstream& input) : input_(input) {}

	// Read from the least significant to the most significant
	uint64_t read_bit() {

		if (read_bits_ == 8) {
			buffer_ = raw_read<uint8_t>(input_);
			read_bits_ = 0;
		}

		uint64_t value = (buffer_ >> read_bits_) & 1;
		read_bits_++;

		return value;
	}

	uint64_t read_number(uint64_t bits_to_read) {
		uint64_t value = 0;

		uint64_t read_bits = 0;

		while (read_bits < bits_to_read) {
			// Least significant bit
			uint64_t bit = read_bit();
			value = value | (bit << read_bits);
			read_bits++;
		}

		return value;
	}
};

using argb = std::array<uint8_t, 4>;

template<typename T>
class matrix {
private:
	uint64_t rows_ = 0;
	uint64_t cols_ = 0;
	std::vector<T> data_;

public:
	matrix(uint64_t rows, uint64_t cols)
		: rows_(rows), cols_(cols), data_(rows_* cols_) {

	}

	T& operator()(uint64_t row, uint64_t col) {
		return data_[row * cols_ + col];
	}

	const T& operator()(uint64_t row, uint64_t col) const {
		return data_[row * cols_ + col];
	}

	uint64_t rows() const {
		return rows_;
	}

	uint64_t cols() const {
		return cols_;
	}

	auto begin() {
		return data_.begin();
	}

	auto end() {
		return data_.end();
	}
};

struct symbol_data {
	uint64_t symbol = 0;
	uint64_t length = 0;
	uint64_t code = 0;
};

class huffman {
private:
	std::unordered_map<uint64_t, symbol_data> symbols_data_;
	std::vector<symbol_data> sorted_symbols_data_;

	std::vector<symbol_data> create_sorted_table() {

		std::vector<symbol_data> sorted_data;

		for (const auto& item : symbols_data_) {
			sorted_data.push_back(item.second);
		}

		std::sort(sorted_data.begin(), sorted_data.end(), [](const symbol_data& first, const symbol_data& second) {
			if (first.length == second.length) {
				return first.symbol < second.symbol;
			}

			return first.length < second.length;
			});

		return sorted_data;
	}

	static uint64_t default_decode_symbol(bit_reader& bitstream, const symbol_data& read_symbol,
		const uint64_t& current_index, std::vector<uint64_t>& decoded_output) {
		uint64_t read_symbols = 1;
		decoded_output[current_index] = read_symbol.symbol;
		return read_symbols;
	}

	void create_canonical_code() {

		// Create symbols vector sorted by lenght then by symbol
		sorted_symbols_data_ = create_sorted_table();

		uint64_t code = 0;
		uint64_t current_length = 0;

		// Calculate canonical code
		for (const auto& item : sorted_symbols_data_) {
			uint64_t shifts = item.length - current_length;
			current_length = item.length;
			code = code << shifts;
			symbols_data_[item.symbol].code = code;
			code++;
		}

		// Refresh the vector with the new data
		sorted_symbols_data_ = create_sorted_table();
	}

public:
	huffman() {}

	huffman(const std::vector<uint64_t>& lenghts) {

		// Create symbols map
		for (uint64_t i = 0; i < lenghts.size(); ++i) {

			// Ignore symbols with lenght = 0
			if (lenghts[i] == 0) {
				continue;
			}

			symbol_data& symbol = symbols_data_[i];
			symbol.symbol = i;
			symbol.length = lenghts[i];
		}

		create_canonical_code();
	}

	huffman(const std::vector<symbol_data>& symbols) {

		// Create symbols map
		for (uint64_t i = 0; i < symbols.size(); ++i) {

			// Ignore symbols with lenght = 0
			if (symbols[i].length == 0) {
				continue;
			}

			symbol_data& symbol = symbols_data_[symbols[i].symbol];
			symbol.symbol = symbols[i].symbol;
			symbol.length = symbols[i].length;
		}

		create_canonical_code();
	}

	std::vector<uint64_t> read_from_bitstream(bit_reader& bitstream, uint32_t max_symbols,
		std::function <uint64_t(
			bit_reader& bitstream,
			const symbol_data& read_symbol,
			const uint64_t& current_index,
			std::vector<uint64_t>& decoded_output)> symbol_decode_func = default_decode_symbol) {

		uint64_t current_code = 0;
		uint64_t current_code_length = 0;
		uint64_t read_symbols = 0;

		std::vector<uint64_t> decoded_output(max_symbols);

		// If there is only one symbol then don't read from the stream
		if (sorted_symbols_data_.size() == 1) {
			decoded_output.push_back(sorted_symbols_data_.back().symbol);
			return decoded_output;
		}

		// For each symbol to be read
		while (read_symbols < max_symbols) {
			uint64_t bit = bitstream.read_bit();
			current_code = (current_code << 1) | bit;
			current_code_length++;

			// For each code in the codes table
			for (const auto& symbol_data : sorted_symbols_data_) {

				if (symbol_data.length != current_code_length) {
					continue;
				}

				// Found a match
				if (current_code == symbol_data.code) {

					uint64_t parsed_symbols = symbol_decode_func(bitstream, symbol_data,
						read_symbols, decoded_output);

					read_symbols += parsed_symbols;

					current_code = 0;
					current_code_length = 0;
					break;
				}
			}
		}

		return decoded_output;
	}
};

static uint64_t decode_symbol(bit_reader& bitstream, const symbol_data& read_symbol,
	const uint64_t& current_index, std::vector<uint64_t>& decoded_output) {

	if (read_symbol.symbol < 16) {
		uint64_t read_symbols = 1;
		decoded_output[current_index] = read_symbol.symbol;
		return read_symbols;
	}
	else if (read_symbol.symbol == 16) {
		uint64_t repeat = 3 + bitstream.read_number(2);

		uint64_t previous_value = decoded_output[current_index - 1u];
		uint64_t value_to_repeat = previous_value != 0 ? previous_value : 8;

		for (uint64_t i = 0; i < repeat; ++i) {
			decoded_output[current_index + i] = value_to_repeat;
		}

		return repeat;
	}
	else if (read_symbol.symbol == 17) {
		uint64_t repeat = 3 + bitstream.read_number(3);

		for (uint64_t i = 0; i < repeat; ++i) {
			decoded_output[current_index + i] = 0;
		}

		return repeat;
	}
	else if (read_symbol.symbol == 18) {
		uint64_t repeat = 11 + bitstream.read_number(7);

		for (uint32_t i = 0; i < repeat; ++i) {
			decoded_output[current_index + i] = 0;
		}

		return repeat;
	}
	else {
		return uint64_t(0);
	}

}

static huffman read_prefix_code(bit_reader& bitstream, uint32_t index) {

	uint64_t kCodeLengthCodes = 19;
	uint64_t type = bitstream.read_bit();

	if (type == 1) { // Simple
		uint64_t num_symbols = bitstream.read_bit() + 1;
		uint8_t is_first_8bits = static_cast<uint8_t>(bitstream.read_bit());
		uint64_t symbol0 = bitstream.read_number(1 + 7 * is_first_8bits);

		std::vector<symbol_data> symbols_data(kCodeLengthCodes);

		symbols_data[0].symbol = symbol0;
		symbols_data[0].length = 1;

		if (num_symbols == 2) {
			uint64_t symbol1 = bitstream.read_number(8);
			symbols_data[1].symbol = symbol1;
			symbols_data[1].length = 1;
		}

		huffman h(symbols_data);

		return h;
	}
	else if (type == 0) { // Normal

		uint64_t num_code_lengths = 4 + bitstream.read_number(4);

		int kCodeLengthCodeOrder[] = {
			17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
		};

		std::vector<uint64_t> code_lengths_lengths(kCodeLengthCodes);

		// Read the symbols lengths of the code lenghts. If a symbol length is not read the value is assumed to be 0
		for (uint64_t i = 0; i < num_code_lengths; ++i) {
			code_lengths_lengths[kCodeLengthCodeOrder[i]] = bitstream.read_number(3);
		}

		// Create the huffman code of the code lengths
		huffman h1(code_lengths_lengths);

		if (bitstream.read_bit()) {
			std::cerr << "The bit for the prefix codes max symbols set to 1, this is not supposed to be part of the exam." << std::endl;
			return huffman();
		}

		// Max symbols defined as a static (simplification for the exam)
		std::vector<uint32_t> max_symbols = {
			280, // Green
			256, // Red
			256, // Blue
			256, // Alpha
			40  // Distance
		};

		std::vector<uint64_t> code_lenghts = h1.read_from_bitstream(bitstream, max_symbols[index], decode_symbol);

		huffman h2(code_lenghts);

		return h2;
	}
}

static uint64_t get_length_or_distance(uint64_t value, bit_reader& bitstream) {
	if (value < 4) {
		return value + 1;
	}
	else {
		uint8_t extra_bits = (value - 2) >> 1;
		uint64_t offset = (2 + (value & 1)) << extra_bits;
		return offset + bitstream.read_number(extra_bits) + 1;
	}
}

static matrix<argb> decode_webp(std::ifstream& input) {

	std::string riff(4, 0);
	input.read(reinterpret_cast<char*>(riff.data()), riff.size());

	int32_t chunk_length = raw_read<int32_t>(input);

	std::string riff_container(4, 0);
	input.read(reinterpret_cast<char*>(riff_container.data()), riff_container.size());

	std::string vp8l(4, 0);
	input.read(reinterpret_cast<char*>(vp8l.data()), vp8l.size());

	int32_t bytes_lossless_stream = raw_read<int32_t>(input);

	uint8_t signature = raw_read<uint8_t>(input);

	bit_reader bitstream_reader(input);
	uint64_t width = bitstream_reader.read_number(14) + 1;
	uint64_t height = bitstream_reader.read_number(14) + 1;

	matrix<argb> raster(height, width);

	uint64_t alpha_is_used = bitstream_reader.read_bit();
	uint64_t version_number = bitstream_reader.read_number(3);

	if (version_number) {
		std::cerr << "Version number is not 0" << std::endl;
		return raster;
	}

	uint8_t transform = static_cast<uint8_t>(bitstream_reader.read_bit());

	if (transform) {
		std::cerr << "Transform present, this is not supposed to be part of the exam." << std::endl;
		return raster;
	}

	uint64_t color_cache = bitstream_reader.read_bit();

	if (color_cache) {
		std::cerr << "Color cache present, this is not supposed to be part of the exam." << std::endl;
		return raster;
	}

	uint64_t meta_prefix = bitstream_reader.read_bit();

	if (meta_prefix) {
		// This means there is an entropy image to decode
		std::cerr << "Meta prefix present, this is not supposed to be part of the exam." << std::endl;
		return raster;
	}

	// Read the prefix codes (meta prefix group)
	// Prefix code 1 (index 0) = green, backward-reference length 
	// Prefix code 2 (index 1) = red
	// Prefix code 3 (index 2) = blue
	// Prefix code 4 (index 3) = alpha
	// Prefix code 5 (index 4) = backward-reference distance 
	std::vector<huffman> prefix_codes(5);

	// There is only one meta prefix group (simplification for the exam).
	for (uint8_t i = 0; i < 5; ++i) {
		prefix_codes[i] = read_prefix_code(bitstream_reader, i);
	}

	// Read image data
	for (std::vector<argb>::iterator it = raster.begin(); it < raster.end(); ++it) {

		std::vector<uint64_t> decoded_green = prefix_codes[0].read_from_bitstream(bitstream_reader, 1);

		// Value between 0 and 280
		uint64_t s = decoded_green.back();

		if (s < 256) {
			std::vector<uint64_t> decoded_red = prefix_codes[1].read_from_bitstream(bitstream_reader, 1);
			std::vector<uint64_t> decoded_blue = prefix_codes[2].read_from_bitstream(bitstream_reader, 1);
			std::vector<uint64_t> decoded_alpha = prefix_codes[3].read_from_bitstream(bitstream_reader, 1);

			*it = {
				static_cast<uint8_t>(decoded_alpha.back()),
				static_cast<uint8_t>(decoded_red.back()),
				static_cast<uint8_t>(decoded_green.back()),
				static_cast<uint8_t>(decoded_blue.back())
			};
		}
		else if (s >= 256 && s < 280) {

			uint64_t length_prefix_code = s - 256;
			uint64_t length = get_length_or_distance(length_prefix_code, bitstream_reader);

			std::vector<uint64_t> decoded_distance = prefix_codes[4].read_from_bitstream(bitstream_reader, 1);
			uint64_t distance = get_length_or_distance(decoded_distance.back(), bitstream_reader);

			std::vector<argb>::iterator begin_copy = std::prev(it, distance);
			std::vector<argb>::iterator current = begin_copy;

			for (uint64_t i = 0; i < length; ++i) {

				*it = *current;
				current++;

				if (current == raster.end()) {
					current = begin_copy;
				}

				it++;
			}
		}
		else if (s >= 280) {
			// No color cache (simplification for the exam)
			std::cerr << "Tried to read pixel from cache." << std::endl;
		}
	}

	return raster;
}

static void write_pam(std::ofstream& output, const matrix<argb>& image) {

	output << "P7" << std::endl;
	output << "WIDTH " << image.cols() << std::endl;
	output << "HEIGHT " << image.rows() << std::endl;
	output << "DEPTH " << 4 << std::endl;
	output << "MAXVAL " << 255 << std::endl;
	output << "TUPLTYPE " << "RGBA" << std::endl;
	output << "ENDHDR" << std::endl;

	for (uint64_t row = 0; row < image.rows(); ++row) {
		for (uint64_t col = 0; col < image.cols(); ++col) {
			argb pixel = image(row, col);
			raw_write(output, pixel[1]);
			raw_write(output, pixel[2]);
			raw_write(output, pixel[3]);
			raw_write(output, pixel[0]);
		}
	}
}

int main(int argc, char* argv[]) {

	if (argc != 3) {
		return EXIT_FAILURE;
	}

	std::ifstream input(argv[1], std::ios::binary);

	if (!input) {
		return EXIT_FAILURE;
	}

	matrix<argb> raster = decode_webp(input);

	std::ofstream output(argv[2], std::ios::binary);

	if (!output) {
		return EXIT_FAILURE;
	}

	write_pam(output, raster);

	return EXIT_SUCCESS;
};