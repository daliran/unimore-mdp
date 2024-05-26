#include <fstream>
#include <iostream>
#include <cstdint>
#include <map>
#include <vector>
#include <sstream>
#include <array>
#include <cmath>

using pixel = std::array<uint8_t, 3>;

class z85 {
private:

	std::vector<uint8_t> index_to_symbol_map_ = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
		'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
		'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D',
		'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
		'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
		'Y', 'Z', '.', '-', ':', '+', '=', '^', '!', '/',
		'*', '?', '&', '<', '>', '(', ')', '[', ']', '{',
		'}', '@', '%', '$', '#'
	};

	std::map<uint8_t, uint8_t> symbols_to_index_map_{
		std::pair<uint8_t, uint32_t>('0', 0),
		std::pair<uint8_t, uint32_t>('1', 1),
		std::pair<uint8_t, uint32_t>('2', 2),
		std::pair<uint8_t, uint32_t>('3', 3),
		std::pair<uint8_t, uint32_t>('4', 4),
		std::pair<uint8_t, uint32_t>('5', 5),
		std::pair<uint8_t, uint32_t>('6', 6),
		std::pair<uint8_t, uint32_t>('7', 7),
		std::pair<uint8_t, uint32_t>('8', 8),
		std::pair<uint8_t, uint32_t>('9', 9),

		std::pair<uint8_t, uint32_t>('a', 10),
		std::pair<uint8_t, uint32_t>('b', 11),
		std::pair<uint8_t, uint32_t>('c', 12),
		std::pair<uint8_t, uint32_t>('d', 13),
		std::pair<uint8_t, uint32_t>('e', 14),
		std::pair<uint8_t, uint32_t>('f', 15),
		std::pair<uint8_t, uint32_t>('g', 16),
		std::pair<uint8_t, uint32_t>('h', 17),
		std::pair<uint8_t, uint32_t>('i', 18),
		std::pair<uint8_t, uint32_t>('j', 19),

		std::pair<uint8_t, uint32_t>('k', 20),
		std::pair<uint8_t, uint32_t>('l', 21),
		std::pair<uint8_t, uint32_t>('m', 22),
		std::pair<uint8_t, uint32_t>('n', 23),
		std::pair<uint8_t, uint32_t>('o', 24),
		std::pair<uint8_t, uint32_t>('p', 25),
		std::pair<uint8_t, uint32_t>('q', 26),
		std::pair<uint8_t, uint32_t>('r', 27),
		std::pair<uint8_t, uint32_t>('s', 28),
		std::pair<uint8_t, uint32_t>('t', 29),

		std::pair<uint8_t, uint32_t>('u', 30),
		std::pair<uint8_t, uint32_t>('v', 31),
		std::pair<uint8_t, uint32_t>('w', 32),
		std::pair<uint8_t, uint32_t>('x', 33),
		std::pair<uint8_t, uint32_t>('y', 34),
		std::pair<uint8_t, uint32_t>('z', 35),
		std::pair<uint8_t, uint32_t>('A', 36),
		std::pair<uint8_t, uint32_t>('B', 37),
		std::pair<uint8_t, uint32_t>('C', 38),
		std::pair<uint8_t, uint32_t>('D', 39),

		std::pair<uint8_t, uint32_t>('E', 40),
		std::pair<uint8_t, uint32_t>('F', 41),
		std::pair<uint8_t, uint32_t>('G', 42),
		std::pair<uint8_t, uint32_t>('H', 43),
		std::pair<uint8_t, uint32_t>('I', 44),
		std::pair<uint8_t, uint32_t>('J', 45),
		std::pair<uint8_t, uint32_t>('K', 46),
		std::pair<uint8_t, uint32_t>('L', 47),
		std::pair<uint8_t, uint32_t>('M', 48),
		std::pair<uint8_t, uint32_t>('N', 49),

		std::pair<uint8_t, uint32_t>('O', 50),
		std::pair<uint8_t, uint32_t>('P', 51),
		std::pair<uint8_t, uint32_t>('Q', 52),
		std::pair<uint8_t, uint32_t>('R', 53),
		std::pair<uint8_t, uint32_t>('S', 54),
		std::pair<uint8_t, uint32_t>('T', 55),
		std::pair<uint8_t, uint32_t>('U', 56),
		std::pair<uint8_t, uint32_t>('V', 57),
		std::pair<uint8_t, uint32_t>('W', 58),
		std::pair<uint8_t, uint32_t>('X', 59),

		std::pair<uint8_t, uint32_t>('Y', 60),
		std::pair<uint8_t, uint32_t>('Z', 61),
		std::pair<uint8_t, uint32_t>('.', 62),
		std::pair<uint8_t, uint32_t>('-', 63),
		std::pair<uint8_t, uint32_t>(':', 64),
		std::pair<uint8_t, uint32_t>('+', 65),
		std::pair<uint8_t, uint32_t>('=', 66),
		std::pair<uint8_t, uint32_t>('^', 67),
		std::pair<uint8_t, uint32_t>('!', 68),
		std::pair<uint8_t, uint32_t>('/', 69),

		std::pair<uint8_t, uint32_t>('*', 70),
		std::pair<uint8_t, uint32_t>('?', 71),
		std::pair<uint8_t, uint32_t>('&', 72),
		std::pair<uint8_t, uint32_t>('<', 73),
		std::pair<uint8_t, uint32_t>('>', 74),
		std::pair<uint8_t, uint32_t>('(', 75),
		std::pair<uint8_t, uint32_t>(')', 76),
		std::pair<uint8_t, uint32_t>('[', 77),
		std::pair<uint8_t, uint32_t>(']', 78),
		std::pair<uint8_t, uint32_t>('{', 79),

		std::pair<uint8_t, uint32_t>('}', 80),
		std::pair<uint8_t, uint32_t>('@', 81),
		std::pair<uint8_t, uint32_t>('%', 82),
		std::pair<uint8_t, uint32_t>('$', 83),
		std::pair<uint8_t, uint32_t>('#', 84)
	};

	std::vector<uint8_t> convert_to_base85(uint32_t value) {

		std::vector<uint8_t> result(5);

		int index = 4;

		while (value > 0) {
			uint32_t div = value / 85;
			uint32_t mod = value % 85;
			result[index] = mod;
			index--;
			value = div;
		}

		return result;
	}

	uint32_t convert_from_base85(const std::vector<uint8_t>& values) {
		
		uint32_t value = 0;

		for (size_t i = 0; i < values.size(); ++i) {
			value += std::pow(85, values.size() - 1 - i) * values[i];
		}

		return value;
	}

public:

	std::string encode(const std::vector<uint8_t>& binary_frame, int32_t N) {

		std::ostringstream output_string;

		// Encoding binary frame
		for (size_t i = 0; i < binary_frame.size(); i += 4) {

			uint32_t value = binary_frame[i] << 24 |
				binary_frame[i + 1] << 16 |
				binary_frame[i + 2] << 8 |
				binary_frame[i + 3] << 0;

			auto converted_value_digits = convert_to_base85(value);

			for (auto& digit : converted_value_digits) {
				output_string << index_to_symbol_map_[digit];
			}
		}

		std::string encoded_string = output_string.str();

		// Encryption
		for (size_t i = 0; i < encoded_string.size(); ++i) {

			uint8_t symbol = encoded_string[i];
			int symbol_index = symbols_to_index_map_[symbol];
			int new_symbol_index = symbol_index - (N * i);

			int adapted_new_symbol_index = new_symbol_index;

			if (new_symbol_index < 0) {
				int offset = std::abs(new_symbol_index) % index_to_symbol_map_.size();

				if (offset == 0) {
					adapted_new_symbol_index = 0;
				}
				else {
					adapted_new_symbol_index = index_to_symbol_map_.size() - offset;
				}	
			}

			uint8_t new_symbol = index_to_symbol_map_[static_cast<uint8_t>(adapted_new_symbol_index)];
			encoded_string[i] = new_symbol;
		}

		return encoded_string;
	}

	std::vector<uint8_t> decode(const std::string& data, int32_t N) {

		// Decryption
		std::string decrypted_string(data);

		for (size_t i = 0; i < decrypted_string.size(); ++i) {

			uint8_t symbol = decrypted_string[i];
			int symbol_index = symbols_to_index_map_[symbol];
			int new_symbol_index = symbol_index + (N * i);

			int adapted_new_symbol_index = new_symbol_index;

			if (new_symbol_index >= symbols_to_index_map_.size()) {
				int offset = std::abs(new_symbol_index) % index_to_symbol_map_.size();
				adapted_new_symbol_index = offset;
			}

			uint8_t new_symbol = index_to_symbol_map_[static_cast<uint8_t>(adapted_new_symbol_index)];
			decrypted_string[i] = new_symbol;
		}

		std::vector<uint8_t> binary_frame;

		// Decode binary frame
		for (size_t i = 0; i < decrypted_string.size(); i += 5) {

			std::vector<uint8_t> base85_digits = {
				symbols_to_index_map_[decrypted_string[i]],
				symbols_to_index_map_[decrypted_string[i + 1]],
				symbols_to_index_map_[decrypted_string[i + 2]],
				symbols_to_index_map_[decrypted_string[i + 3]],
				symbols_to_index_map_[decrypted_string[i + 4]]
			};
			
			uint32_t base10_value = convert_from_base85(base85_digits);

			uint8_t byte1 = (base10_value >> 24) & 0b11111111;
			binary_frame.push_back(byte1);

			uint8_t byte2 = (base10_value >> 16) & 0b11111111;
			binary_frame.push_back(byte2);

			uint8_t byte3 = (base10_value >> 8) & 0b11111111;
			binary_frame.push_back(byte3);

			uint8_t byte4 = (base10_value >> 0) & 0b11111111;
			binary_frame.push_back(byte4);
		}

		return binary_frame;
	}
};

struct ppm_header {
	std::string magic_number;
	uint64_t width = 0;
	uint64_t height = 0;
	uint64_t max_value = 0;
};

class ppm {
private:

	ppm_header header_;
	std::vector<uint8_t> data_;

public:
	ppm(std::istream& input) {

		std::getline(input, header_.magic_number);

		char comment_char = input.peek();

		if (comment_char == '#') {
			std::string comment;
			std::getline(input, comment);
		}

		std::string width;
		input >> width;
		header_.width = std::stoi(width);

		std::string height;
		input >> height;
		header_.height = std::stoi(height);

		std::string max_value;
		input >> max_value;
		header_.max_value = std::stoi(max_value);

		if (header_.max_value > 255) {
			std::cerr << "Unsupported max value" << std::endl;
		}

		input.ignore(1);

		data_.resize(header_.width * header_.height * sizeof(pixel));

		input.read(reinterpret_cast<char*>(data_.data()), sizeof(pixel) * data_.size());
	}

	ppm(const ppm_header& header, const std::vector<uint8_t>& data) : header_(header), data_(data) { }

	const ppm_header& header() const {
		return header_;
	}

	const std::vector<uint8_t>& data() const {
		return data_;
	}

	void write(std::ostream& output) {
		output << header_.magic_number << std::endl;
		output << header_.width << ' '; // space to make OLJ test pass
		output << header_.height << std::endl;
		output << header_.max_value << std::endl;
		output.write(reinterpret_cast<char*>(data_.data()), data_.size());
	}
};

int main(int argc, char* argv[]) {

	if (argc < 5) {
		return EXIT_FAILURE;
	}

	std::string mode(argv[1]);
	int32_t N = std::atoi(argv[2]);
	std::string input_file(argv[3]);
	std::string output_file(argv[4]);

	z85 z85;

	if (mode == "c") {

		std::ifstream input(input_file, std::ios::binary);

		if (!input) {
			std::cerr << "Failed to open the input file";
			return EXIT_FAILURE;
		}

		ppm image(input);

		std::vector<uint8_t> binary_frame(image.data());

		size_t mod = binary_frame.size() % 4;

		// Padding
		if (mod != 0) {
			for (uint8_t i = 0; i < 4 - mod; ++i) {
				binary_frame.push_back(0);
			}
		}

		std::string encoded = z85.encode(binary_frame, N);

		std::ofstream output(output_file);

		if (!output) {
			std::cerr << "Failed to open the output file";
			return EXIT_FAILURE;
		}

		output << image.header().width << "," << image.header().height << "," << encoded;
	}
	else if (mode == "d") {

		std::ifstream input(input_file);

		if (!input) {
			std::cerr << "Failed to open the input file";
			return EXIT_FAILURE;
		}

		ppm_header header;
		header.magic_number = "P6";
		header.max_value = 255;

		std::string width;
		std::getline(input, width, ',');
		header.width = std::stoi(width);

		std::string height;
		std::getline(input, height, ',');
		header.height = std::stoi(height);

		std::string encoded_data;
		input >> encoded_data;

		std::vector<uint8_t> binary_frame = z85.decode(encoded_data, N);

		// Remove padding
		size_t original_size = header.width * header.height * sizeof(pixel);

		if (binary_frame.size() != original_size) {
			binary_frame.resize(original_size);
		}

		ppm image(header, binary_frame);

		std::ofstream output(output_file, std::ios::binary);

		if (!output) {
			std::cerr << "Failed to open the output file";
			return EXIT_FAILURE;
		}

		image.write(output);
	}

	return EXIT_SUCCESS;
}