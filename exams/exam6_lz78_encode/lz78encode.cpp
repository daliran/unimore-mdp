#include <string>
#include <fstream>
#include <vector>
#include <cstdint>
#include <optional>
#include <cmath>

class bit_writer {  
private:
	std::ostream& output_;
	uint8_t buffer_ = 0;
	uint8_t bits_in_buffer_ = 0;

public:
	bit_writer(std::ostream& output) : output_(output){ }

	void write_bit(uint8_t bit) {
		buffer_ <<= 1;
		buffer_ |= bit;
		bits_in_buffer_++;

		if (bits_in_buffer_ == 8) {
			output_.write(reinterpret_cast<char*>(&buffer_), sizeof(buffer_));
			bits_in_buffer_ = 0;
			buffer_ = 0;
		}
	}

	void write_number(uint64_t number, uint8_t bits_to_write) {
		while (bits_to_write > 0) {
			uint8_t bit = (number >> (bits_to_write - 1)) & 1;
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

template<typename T>
class dictionary {
private:
	struct dictionary_node {
		T value;
		uint64_t key;
		std::vector<dictionary_node> children;
	};

	std::vector<dictionary_node> children;
	uint64_t last_key_ = 0;
	uint64_t max_allowed_key_;

	std::optional<uint64_t> inner_search(dictionary_node& node,
		const std::vector<T>& value, uint64_t level, bool add) {

		if (node.value == value[level]) {

			if (level == value.size() - 1) {

				if (add) {
					last_key_++;
					node.key = last_key_;
				}

				return node.key;
			}

			std::optional<uint64_t> found_result;

			for (auto& node : node.children) {
				auto result = inner_search(node, value, level + 1, add);

				if (result.has_value()) {
					found_result = result;
					break;
				}
			}

			if (found_result.has_value()) {
				return found_result;
			}
			else if (add) {
				node.children.emplace_back(value[level + 1]);
				auto result = inner_search(node.children.back(), value, level + 1, add);
				return result;
			}
		}

		return std::optional<uint64_t>();
	}

public:

	dictionary(uint64_t max_allowed_key) : max_allowed_key_(max_allowed_key) {}

	std::optional<uint64_t> search_key(const std::vector<T>& value) {

		for (auto& node : children) {
			auto result = inner_search(node, value, 0, false);

			if (result.has_value()) {
				return result;
			}
		}

		return std::optional<uint64_t>();
	}

	void add_key(const std::vector<T>& value) {
		bool found = false;


		for (auto& node : children) {
			auto result = inner_search(node, value, 0, true);

			if (result.has_value()) {
				found = true;
				break;
			}
		}

		if (!found) {
			children.emplace_back(value[0]);
			inner_search(children.back(), value, 0, true);
		}

		if (last_key_ >= max_allowed_key_) {
			clear();
		}
	}

	uint64_t last_key() const {
		return last_key_;
	}

	void clear() {
		children.clear();
		last_key_ = 0;
	}
};

bool lz78encode(const std::string& input_filename, const std::string& output_filename, int maxbits) {

	std::ifstream input(input_filename, std::ios::binary);

	if (!input) {
		return false;
	}

	std::ofstream output(output_filename, std::ios::binary);

	if (!output) {
		return false;
	}
	
	bit_writer bit_writer(output);

	dictionary<uint8_t> dictionary(static_cast<uint64_t>(std::pow(2, maxbits)));

	output << "LZ78";
	bit_writer.write_number(static_cast<uint64_t>(maxbits), 5);

	// Main loop
	while (true) {
		std::vector<uint8_t> to_search;
		std::vector<uint64_t> found_keys;

		// Find best matching key
		while (true) {
			uint8_t buffer = 0;
			input.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));

			if (input.eof()) {
				break;
			}

			to_search.push_back(buffer);
			auto found_key = dictionary.search_key(to_search);
			if (!found_key.has_value()) {
				break;
			}
			else {
				found_keys.push_back(found_key.value());
			}
		}

		// Calculate bits to be used for the encoding
		auto last_key = dictionary.last_key();
		uint8_t bits_to_use_for_encoding = last_key == 0 ? 0 : static_cast<uint8_t>(std::log2(dictionary.last_key())) + 1;

		if (to_search.size() > 0) {
			dictionary.add_key(to_search);
		}

		uint64_t key_to_use = 0;
		bool ignore_symbol = false;

		// Prepare the key to use for the encoding
		if (!input.eof()) {

			if (found_keys.size() > 0) {
				key_to_use = found_keys.back();
			}
		}
		else if (input.eof()) {
			if (found_keys.size() > 1) {
				key_to_use = found_keys[found_keys.size() - 2];
			}
			else if (found_keys.size() > 0) {

				if (to_search.size() == 1) {
					key_to_use = 0;
				}
				else {
					key_to_use = found_keys.back();
					ignore_symbol = true;
				}
			}
		}

		// Write to the output stream
		if (key_to_use != 0 || to_search.size() > 0) {
			if (bits_to_use_for_encoding > 0) {
				bit_writer.write_number(key_to_use, bits_to_use_for_encoding);
			}

			if (!ignore_symbol && to_search.size() > 0) {
				bit_writer.write_number(to_search.back(), 8);
			}
		}
		
		// End of input stream
		if (input.eof()) {
			break;
		}
	}

	return true;
}