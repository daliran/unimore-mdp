#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <cmath>

template<typename T>
std::pair<T, bool> raw_read(std::istream& input) {
	T buffer = 0;

	input.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));

	if (input.eof()) {
		return std::pair<T, bool>(buffer, false);
	}

	return std::pair<T, bool>(buffer, true);
}

template<typename T>
void raw_write(std::ostream& output, T value) {
	output.write(reinterpret_cast<char*>(&value), sizeof(value));
}

class bit_writer {
private:
	std::ostream& output_;
	uint8_t buffer_ = 0;
	uint8_t bits_in_buffer = 0;

public:
	bit_writer(std::ostream& output) : output_(output) {}

	void write_bit(bool bit) {

		if (bit) {
			buffer_ |= 1;
		}

		bits_in_buffer++;

		if (bits_in_buffer == 8) {
			raw_write(output_, buffer_);
			buffer_ = 0;
			bits_in_buffer = 0;
		}
		else {
			buffer_ <<= 1;
		}
	}

	void write_number(uint64_t value, uint8_t number_of_bits) {
		for (uint8_t i = 0; i < number_of_bits; ++i) {
			bool bit = (value >> (number_of_bits - i - 1)) & 1;
			write_bit(bit);
		}
	}

	~bit_writer() {
		while (bits_in_buffer > 0) {
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
	bit_reader(std::istream& input) : input_(input){}

	std::pair<bool, bool> read_bit() {

		if (bits_in_buffer_ == 0) {
			auto read_buffer_result = raw_read<uint8_t>(input_);

			if (!read_buffer_result.second) {
				return std::pair<bool, bool>(false, false);
			}

			buffer_ = read_buffer_result.first;
			bits_in_buffer_ = 8;
		}

		bool bit = (buffer_ >> (bits_in_buffer_  - 1)) & 1;
		bits_in_buffer_--;

		return std::pair<bool, bool>(bit, true);
	}

	std::pair<uint64_t, bool> read_number(uint8_t number_of_bits) {
		
		uint64_t buffer = 0;

		while (number_of_bits > 0) {

			auto read_bit_result = read_bit();

			if (!read_bit_result.second) {
				return std::pair<uint64_t, bool>(0, false);
			}

			buffer <<= 1;

			if (read_bit_result.first) {
				buffer |= 1;
			}

			number_of_bits--;
		}

		return std::pair<uint64_t, bool>(buffer, true);
	}

};

template<typename T>
class matrix {
private:
	uint32_t rows_;
	uint32_t columns_;
	std::vector<T> data_;
public:

	matrix(uint32_t rows = 0, uint32_t columns = 0) : rows_(rows), columns_(columns) {
		resize(rows_, columns_);
	}

	void resize(uint32_t rows, uint32_t columns) {
		rows_ = rows;
		columns_ = columns;
		data_.resize(rows_ * columns_);
	}

	T get_value(uint32_t row, uint32_t column) const {
		return data_[row * columns_ + column];
	}

	void set_value(uint32_t row, uint32_t column, T value) {
		data_[row * columns_ + column] = value;
	}

	const std::vector<T> data() const {
		return data_;
	}

	uint32_t rows() const {
		return rows_;
	}

	uint32_t columns() const {
		return columns_;
	}
};

struct pam_header {
	std::string magic_number;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t depth = 0;
	uint32_t max_value = 0;
	std::string tuple_type;
};

class pam {
private:
	pam_header header_;
	matrix<uint8_t> data_;

	uint8_t map_to_visible(int32_t value) const {

		if (value == 0) {
			return 128;
		}
		else {
			uint8_t result = static_cast<uint8_t>(value / 2.0 + 128);

			if (value > 0) {
				result += 1;
			}

			if (result > 255) {
				return 255;
			}
		}
	}

public:
	pam() {}

	bool load_from_file(const std::string& file_name) {

		std::ifstream input(file_name, std::ios::binary);

		if (!input) {
			return false;
		}

		input >> header_.magic_number;

		std::string width;
		input >> width >> width;
		header_.width = std::stoi(width);

		std::string height;
		input >> height >> height;
		header_.height = std::stoi(height);

		std::string depth;
		input >> depth >> depth;
		header_.depth = std::stoi(depth);

		std::string max_value;
		input >> max_value >> max_value;
		header_.max_value = std::stoi(max_value);

		input >> header_.tuple_type >> header_.tuple_type;

		std::string end_of_header;
		input >> end_of_header;

		input.ignore(1);

		data_.resize(header_.height, header_.width);

		for (uint32_t row = 0; row < header_.height; ++row) {
			for (uint32_t column = 0; column < header_.width; ++column) {

				auto read_pixel = raw_read<uint8_t>(input);

				if (!read_pixel.second) {
					return false;
				}

				data_.set_value(row, column, read_pixel.first);
			}
		}

		return true;
	}

	void load_from_raw_data(const matrix<uint8_t>& raw_data) {

		header_.magic_number = "P7";
		header_.width = raw_data.columns();
		header_.height = raw_data.rows();
		header_.depth = 1; // assume gray scale
		header_.max_value = 255; // assume 255 value
		header_.tuple_type = "GRAYSCALE"; // assume gray scale

		// TODO: use move constructor
		data_.resize(header_.height, header_.width);

		for (uint32_t row = 0; row < header_.height; ++row) {
			for (uint32_t column = 0; column < header_.width; ++column) {
				uint8_t value = raw_data.get_value(row, column);
				data_.set_value(row, column, value);
			}
		}
	}

	void load_and_decode_difference_image(const matrix<int32_t>& raw_data) {

		header_.magic_number = "P7";
		header_.width = raw_data.columns();
		header_.height = raw_data.rows();
		header_.depth = 1; // assume gray scale
		header_.max_value = 255; // assume 255 value
		header_.tuple_type = "GRAYSCALE"; // assume gray scale

		data_.resize(header_.height, header_.width);

		for (uint32_t row = 0; row < header_.height; ++row) {
			for (uint32_t column = 0; column < header_.width; ++column) {

				int32_t pixel = raw_data.get_value(row, column);

				if (row == 0 && column == 0) {
					// first pixel
					data_.set_value(row, column, pixel);
				}
				else if (row > 0 && column == 0) {
					// first column (except the first row)
					int32_t pixel_above = data_.get_value(row - 1, column);
					data_.set_value(row, column, pixel + pixel_above);
				}
				else {
					// all other positions excepts the first column
					int32_t pixel_on_the_left = data_.get_value(row, column - 1);
					data_.set_value(row, column, pixel + pixel_on_the_left);
				}
			}
		}
	}

	bool write_to_file(const std::string& file_name) {

		std::ofstream output(file_name, std::ios::binary);

		if (!output) {
			return false;
		}

		output << header_.magic_number << std::endl;
		output << "WIDTH " << header_.width << std::endl;
		output << "HEIGHT " << header_.height << std::endl;
		output << "DEPTH " << header_.depth << std::endl;
		output << "MAXVAL " << header_.max_value << std::endl;
		output << "TUPLTYPE " << header_.tuple_type << std::endl;
		output << "ENDHDR" << std::endl;

		for (uint32_t row = 0; row < header_.height; ++row) {
			for (uint32_t column = 0; column < header_.width; ++column) {
				uint8_t pixel = data_.get_value(row, column);
				raw_write(output, pixel);
			}
		}

		return true;
	}

	matrix<uint8_t> calculate_visible_difference_image() const {

		matrix<uint8_t> difference_image(header_.height, header_.width);

		for (uint32_t row = 0; row < header_.height; ++row) {
			for (uint32_t column = 0; column < header_.width; ++column) {

				int32_t pixel = data_.get_value(row, column);

				if (row == 0 && column == 0) {
					// first pixel
					difference_image.set_value(row, column, pixel);
				}
				else if (row > 0 && column == 0) {
					// first column (except the first row)
					int32_t pixel_above = data_.get_value(row - 1, column);
					difference_image.set_value(row, column, map_to_visible(pixel - pixel_above));
				}
				else {
					// all other positions excepts the first column
					int32_t pixel_on_the_left = data_.get_value(row, column - 1);
					difference_image.set_value(row, column, map_to_visible(pixel - pixel_on_the_left));
				}
			}
		}

		return difference_image;
	}

	matrix<int32_t> calculate_difference_image() const {

		matrix<int32_t> difference_image(header_.height, header_.width);

		for (uint32_t row = 0; row < header_.height; ++row) {
			for (uint32_t column = 0; column < header_.width; ++column) {

				int32_t pixel = data_.get_value(row, column);

				if (row == 0 && column == 0) {
					// first pixel
					difference_image.set_value(row, column, pixel);
				}
				else if (row > 0 && column == 0) {
					// first column (except the first row)
					int32_t pixel_above = data_.get_value(row - 1, column);
					difference_image.set_value(row, column, pixel - pixel_above);
				}
				else {
					// all other positions excepts the first column
					int32_t pixel_on_the_left = data_.get_value(row, column - 1);
					difference_image.set_value(row, column, pixel - pixel_on_the_left);
				}
			}
		}

		return difference_image;
	}

	const pam_header& header() const {
		return header_;
	}

	const matrix<uint8_t>& data() const {
		return data_;
	}
};

struct symbol_data {
	int32_t symbol = 0;
	uint32_t frequency = 0;
	uint32_t code = 0;
	uint8_t code_length = 0;
};

struct huffman_node {
	uint32_t frequency;
	std::vector<int32_t> symbols;
	std::shared_ptr<huffman_node> low_frequency_node;
	std::shared_ptr<huffman_node> high_frequency_node;

	huffman_node(
		uint32_t frequency,
		std::vector<int32_t> symbols,
		std::shared_ptr<huffman_node> low_frequency_node = nullptr,
		std::shared_ptr<huffman_node> high_frequency_node = nullptr
	) {
		this->frequency = frequency;
		this->symbols = symbols;
		this->low_frequency_node = low_frequency_node;
		this->high_frequency_node = high_frequency_node;
	}

	huffman_node(const std::shared_ptr<huffman_node>& low_frequency_node, const std::shared_ptr <huffman_node>& high_frequency_node) {
		this->frequency = low_frequency_node->frequency + high_frequency_node->frequency;
		this->low_frequency_node = low_frequency_node;
		this->high_frequency_node = high_frequency_node;
		std::copy(low_frequency_node->symbols.begin(), low_frequency_node->symbols.end(), std::back_inserter(symbols));
		std::copy(high_frequency_node->symbols.begin(), high_frequency_node->symbols.end(), std::back_inserter(symbols));
	}
};

class huffman {
private:
	void calculate_frequencies(const matrix<int32_t>& raw_data, std::map<uint8_t, symbol_data>& symbols_data) {
		for (auto& symbol : raw_data.data()) {
			symbol_data& symbol_data = symbols_data[symbol];
			symbol_data.symbol = symbol;
			symbol_data.frequency += 1;
		}
	}

	void calculate_code_length(std::map<uint8_t, symbol_data>& symbols_data, std::shared_ptr<huffman_node> node, int depth) {
		
		if (node->low_frequency_node == nullptr && node->high_frequency_node == nullptr) {
			symbol_data& symbol_data = symbols_data[node->symbols.front()];
			symbol_data.code_length = depth;
			return;
		}

		calculate_code_length(symbols_data, node->low_frequency_node, depth + 1);
		calculate_code_length(symbols_data, node->high_frequency_node, depth + 1);
	}

	std::vector<symbol_data> get_symbols_sorted_by_code_length(const std::map<uint8_t, symbol_data>& symbols_data) {

		std::vector<symbol_data> sorted_symbols;

		for (auto& symbol_data : symbols_data) {
			sorted_symbols.push_back(symbol_data.second);
		}

		std::sort(sorted_symbols.begin(), sorted_symbols.end(),
			[](const symbol_data& first, const symbol_data& second) {
				if (first.code_length == second.code_length) {
					return first.symbol < second.symbol;
				}
				else {
					return first.code_length < second.code_length;
				}
			});

		return sorted_symbols;
	}

	void generate_canonical_code_from_length(std::map<uint8_t, symbol_data>& symbols_data) {

		auto sorted_symbols_data = get_symbols_sorted_by_code_length(symbols_data);

		uint32_t previous_length = 1;
		uint32_t current_value = 0;

		for (auto& symbol_data : sorted_symbols_data) {
			uint32_t shifts = symbol_data.code_length - previous_length;
			current_value <<= shifts;
			symbols_data[symbol_data.symbol].code = current_value;
			current_value += 1;
			previous_length = symbol_data.code_length;
		}
	}

	void calculate_canonical_code(const matrix<int32_t>& raw_data, std::map<uint8_t, symbol_data>& symbols_data) {

		calculate_frequencies(raw_data, symbols_data);

		std::vector<std::shared_ptr<huffman_node>> tree;

		for (auto& symbol_data : symbols_data) {
			auto node = std::make_shared<huffman_node>(huffman_node(symbol_data.second.frequency, { symbol_data.second.symbol }));
			tree.push_back(node);
		}

		std::sort(tree.begin(), tree.end(),
			[](const std::shared_ptr<huffman_node>& first, const std::shared_ptr<huffman_node>& second) {
				return first->frequency > second->frequency;
			});

		while (tree.size() > 1) {

			// Merge two smallest frequency nodes
			auto& low_frequency_node = tree[tree.size() - 1];
			auto& high_frequency_node = tree[tree.size() - 2];
			auto merged_node = std::make_shared<huffman_node>(huffman_node(low_frequency_node, high_frequency_node));

			auto lower_bound_it = std::lower_bound(tree.begin(), tree.end(), merged_node,
				[](const std::shared_ptr<huffman_node>& first, const std::shared_ptr<huffman_node>& second) {
					return first->frequency > second->frequency;
				});

			tree.insert(lower_bound_it, merged_node);
			tree.resize(tree.size() - 2);
		}

		auto& root = tree.front();

		calculate_code_length(symbols_data, root, 0);

		generate_canonical_code_from_length(symbols_data);
	}

	int32_t fix_negative_number(uint64_t number, uint8_t number_of_bits) {

		bool negative_sign = (number >> (number_of_bits - 1)) & 1;

		if (negative_sign) {
			auto x = static_cast<int32_t>(number  + (-std::pow(2, number_of_bits)));
			return x;
		}
		else {
			return static_cast<int32_t>(number);
		}
	}

public:
	
	bool encode_data(const std::string& file_name, const matrix<int32_t>& raw_data) {

		std::map<uint8_t, symbol_data> symbols_data;

		calculate_canonical_code(raw_data, symbols_data);

		std::ofstream output(file_name, std::ios::binary);

		if (!output) {
			return false;
		}

		bit_writer bit_writer(output);

		output << "HUFFDIFF";

		raw_write(output, raw_data.columns());
		raw_write(output, raw_data.rows());

		bit_writer.write_number(symbols_data.size(), 9);

		auto sorted_symbols_data = get_symbols_sorted_by_code_length(symbols_data);

		for (auto& symbol_data : sorted_symbols_data) {

			bit_writer.write_number(symbol_data.symbol, 9);
			bit_writer.write_number(symbol_data.code_length, 5);
		}

		for (auto& symbol : raw_data.data()) {
			auto& symbol_data = symbols_data[symbol];
			bit_writer.write_number(symbol_data.code, symbol_data.code_length);
		}

		return true;
	}

	std::pair<matrix<int32_t>, bool> decode_data(const std::string& file_name) {
		
		matrix<int32_t> raw_data;

		std::ifstream input(file_name, std::ios::binary);

		if (!input) {
			return std::pair<matrix<int32_t>, bool>(raw_data, false);
		}
		
		bit_reader bit_reader(input);

		std::string magic_number;
		for (uint8_t i = 0; i < 8; ++i) {
			auto read_result = raw_read<uint8_t>(input);

			if (!read_result.second) {
				return std::pair<matrix<int32_t>, bool>(raw_data, false);
			}

			magic_number.push_back(read_result.first);
		}

		auto width_read_result = raw_read<uint32_t>(input);

		if (!width_read_result.second) {
			return std::pair<matrix<int32_t>, bool>(raw_data, false);
		}

		auto height_read_result = raw_read<uint32_t>(input);
		
		if (!height_read_result.second) {
			return std::pair<matrix<int32_t>, bool>(raw_data, false);
		}

		auto number_of_elements_result = bit_reader.read_number(9);

		if (!number_of_elements_result.second) {
			return std::pair<matrix<int32_t>, bool>(raw_data, false);
		}
	
		raw_data.resize(height_read_result.first, width_read_result.first);

		std::map<uint8_t, symbol_data> symbols_data;

		for (uint32_t i = 0; i < number_of_elements_result.first; ++i) {

			auto symbol_result = bit_reader.read_number(9);

			if (!symbol_result.second) {
				return std::pair<matrix<int32_t>, bool>(raw_data, false);
			}

			auto code_length_result = bit_reader.read_number(5);

			if (!code_length_result.second) {
				return std::pair<matrix<int32_t>, bool>(raw_data, false);
			}

			int32_t symbol = fix_negative_number(symbol_result.first, 9);

			symbol_data& symbol_data = symbols_data[symbol];
			symbol_data.symbol = symbol;
			symbol_data.code_length = static_cast<uint8_t>(code_length_result.first);
		}

		generate_canonical_code_from_length(symbols_data);

		uint64_t symbols_to_read = static_cast<uint64_t>(width_read_result.first) * static_cast<uint64_t>(height_read_result.first);

		auto sorted_symbols_data = get_symbols_sorted_by_code_length(symbols_data);

		uint32_t code = 0;
		uint32_t bits_in_code = 0;

		uint32_t row = 0;
		uint32_t column = 0;

		while (symbols_to_read > 0) {

			auto read_bit = bit_reader.read_bit();

			if (!read_bit.second) {
				break;
			}

			code <<= 1;

			if (read_bit.first) {
				code |= 1;
			}

			bits_in_code++;

			for (const auto& symbol_data : sorted_symbols_data) {

				if (symbol_data.code_length == bits_in_code && symbol_data.code == code) {

					raw_data.set_value(row, column, symbol_data.symbol);

					symbols_to_read--;
					code = 0;
					bits_in_code = 0;

					if (column < width_read_result.first - 1) {
						column++;
					}
					else {
						column = 0;
						row++;
					}

					break;
				}
			}
		}

		return std::pair<matrix<int32_t>, bool>(raw_data, true);
	}
};

int main(int argc, char* argv[]) {

	if (argc != 4) {
		std::cerr << "Invalid number of arguments" << std::endl;
		return EXIT_FAILURE;
	}

	std::string mode(argv[1]);

	if (!(mode == "c" || mode == "d")) {
		std::cerr << "The mode can only be c or d" << std::endl;
		return EXIT_FAILURE;
	}

	std::string input_file(argv[2]);

	std::string output_file(argv[3]);

	bool compress = mode == "c";

	if (compress) {
		pam loaded_image;

		if (!loaded_image.load_from_file(input_file)) {
			std::cerr << "Failed to read pam file" << std::endl;
			return EXIT_FAILURE;
		}

		/*auto image_difference_data = loaded_image.calculate_visible_difference_image();
		pam image_difference;
		image_difference.load_from_raw_data(image_difference_data);
		image_difference.write_to_file("difference.pam");*/

		auto image_difference = loaded_image.calculate_difference_image();
		huffman encoder;
		encoder.encode_data(output_file, image_difference);
	}
	else {

		huffman decoder;
		auto decode_result = decoder.decode_data(input_file);

		if (!decode_result.second) {
			std::cerr << "Failed to decode the huffdiff file" << std::endl;
			return EXIT_FAILURE;
		}

		pam decoded_image;
		decoded_image.load_and_decode_difference_image(decode_result.first);

		if (!decoded_image.write_to_file(output_file)) {
			std::cerr << "Failed to write the pam file from the decoded huffdiff file" << std::endl;
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}