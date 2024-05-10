#include <fstream>
#include <iostream>
#include <string>
#include <cstdint>
#include <iomanip>

enum class context_type {
	none,
	list,
	dictionary,
	dictionary_key,
	pieces
};

static void write_tabs(int level) {
	for (int i = 0; i < level; ++i) {
		std::cout << '\t';
	}
}

static int64_t read_integer(std::istream& input) {
	std::string read_integer(21, 0); // max number of digits of a x64 number, including the sign
	input.getline(read_integer.data(), read_integer.size(), 'e');
	return std::stoi(read_integer);
}

static std::string read_string(std::istream& input, context_type context) {
	std::string read_string_size(20, 0); // max number of digits of a x64 number
	input.getline(read_string_size.data(), read_string_size.size(), ':');

	if (read_string_size[0] == 0) {
		return std::string();
	}

	int64_t string_size = std::stoi(read_string_size);
	std::string read_string(string_size, 0);

	if (string_size > 0) {
		input.read(read_string.data(), string_size);

		if (context != context_type::pieces) {
			// all bytes with a value less than 32 or greater than 126 must be displayed with the character "."
			for (int i = 0; i < read_string.size(); ++i) {
				if (read_string[i] < 32 || read_string[i] > 126) {
					read_string[i] = '.';
				}
			}
		}
	}

	return read_string;
}

static void print_integer(const int64_t& value, context_type context, int indentation_level) {
	if (context == context_type::dictionary_key) {
		// Dictionary value
		std::cout << " => " << value << std::endl;
	}
	else {
		// List value
		write_tabs(indentation_level);
		std::cout << value << std::endl;
	}
}

static void print_string(const std::string& value, context_type context, int indentation_level) {

	if (context == context_type::dictionary) {
		// Dictionary key
		write_tabs(indentation_level);
		std::cout << "\"" << value << "\"";
	}
	else if (context == context_type::dictionary_key) {
		// Dictionary value
		std::cout << " => \"" << value << "\"" << std::endl;
	}
	else if (context == context_type::pieces) {

		std::cout << " => " << std::endl;

		for (int i = 0; i < value.size(); i += 20) {
			auto piece = value.substr(i, 20);
			write_tabs(indentation_level);

			for (int j = 0; j < piece.size(); ++j) {
				int byte = piece[j];
				byte &= 0b11111111;
				std::cout << std::setw(2) << std::setfill('0') << std::hex << byte;
			}

			std::cout << std::endl;
		}
	}
	else {

		if (value.empty()) {
			return;
		}

		// List value
		write_tabs(indentation_level);
		std::cout << "\"" << value << "\"" << std::endl;
	}
}

static void print_start_of_structure(context_type context, bool starting_dictionary, int indentation_level){

	if (context == context_type::dictionary_key) {
		std::cout << " => ";
	}
	else {
		write_tabs(indentation_level);
	}

	if (starting_dictionary) {		
		std::cout << "{";	
	}
	else {
		std::cout << "[";
	}

	std::cout << std::endl;
}

static void print_end_of_structure(context_type context, int indentation_level) {
	
	write_tabs(indentation_level - 1);

	if (context == context_type::dictionary) {
		std::cout << "}" << std::endl;
	}
	else if (context == context_type::list) {
		std::cout << "]" << std::endl;
	}
}

static void parse_bencode(std::istream& input, int level, context_type context) {

	while (true) {

		char token = input.peek();

		// end of exploration
		if (token == EOF) {
			return;
		}

		switch (token) {
			case 'i': {			

				input.ignore(1);	

				auto value = read_integer(input);

				print_integer(value, context, level);

				if (context == context_type::dictionary_key) {
					// this integer is a dictionary value, exiting the context
					return;
				}
		
				break;
			}
			case 'd': { 
				
				input.ignore(1);
				print_start_of_structure(context, true, level);

				// starting a new context
				int new_level = context == context_type::dictionary_key ? level : level + 1;
				parse_bencode(input, new_level , context_type::dictionary);

				if (context == context_type::dictionary_key) {			
					// this is a dictionary value, exiting the context after the exploration
					return;
				}
	
				break;
			}
			case 'l': {
				input.ignore(1);		
				print_start_of_structure(context, false, level);

				// starting a new context
				int new_level = context == context_type::dictionary_key ? level : level + 1;
				parse_bencode(input, new_level, context_type::list);

				if (context == context_type::dictionary_key) {
					// this is a dictionary value, exiting the context after the exploration
					return;
				}

				break;
			}
			case 'e': {
				input.ignore(1);
				print_end_of_structure(context, level);

				if (context != context_type::dictionary && context != context_type::list){
					std::cerr << "Unexeptected end of list or dictionary" << std::endl;
					exit(1);
				}

				// exiting the context
				return;
			}
			default: {

				auto value = read_string(input, context);

				print_string(value, context, level);

				if (context == context_type::dictionary) {
					// this string is a dictionary key, let's get the value by starting a new context
					auto new_context = value == "pieces" ? context_type::pieces : context_type::dictionary_key;
					parse_bencode(input, level + 1, new_context);
				}
				else if (context == context_type::dictionary_key || context == context_type::pieces) {
					// this string is a dictionary value, exiting the context
					return;
				}

				break;
			}
		}
	}
}

int main(int argc, char* argv[]) {

	if (argc != 2) {
		std::cerr << "Invalid number of arguments" << std::endl;
		return EXIT_FAILURE;
	}

	std::ifstream input(argv[1], std::ios::binary);

	if (!input) {
		std::cerr << "Failed to open the file" << std::endl;
		return EXIT_FAILURE;
	}

	parse_bencode(input, 0, context_type::none);

	return EXIT_SUCCESS;
}