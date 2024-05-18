#include "hufstr.h" 

#include <fstream>
#include <sstream>
//#include <exception>

template<typename T>
std::pair<T, bool> raw_read(std::istream& input) {
	T buffer = 0;
	input.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));

	if (input.eof()) {
		return std::pair<T, bool>(0, false);
	}

	return std::pair<T, bool>(buffer, true);
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
	bit_reader(std::istream& input) : input_(input) {};
	
	std::pair<uint8_t, bool> read_bit() {

		if (bits_in_buffer_ == 0) {
			auto read_buffer = raw_read <uint8_t>(input_);

			if (!read_buffer.second) {
				return std::pair<bool, bool>(false, false);
			}

			buffer_ = read_buffer.first;
			bits_in_buffer_ = 8;
		}

		uint8_t value = (buffer_ >> (bits_in_buffer_ - 1)) & 1;
		bits_in_buffer_--;

		return std::pair<uint8_t, bool>(value, true);
	}

	std::pair<uint64_t, bool> read_number(uint8_t bits_to_read) {

		uint64_t value = 0;

		while (bits_to_read > 0) {
			auto bit_value = read_bit();

			if (!bit_value.second) {
				return std::pair<uint64_t, bool>(0, false);
			}

			value <<= 1;
			value |= bit_value.first;
			bits_to_read--;
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
	bit_writer(std::ostream& output) : output_(output) {};

	void write_bit(uint8_t value) {

		buffer_ <<= 1;
		buffer_ |= value;
		bits_in_buffer_++;

		if (bits_in_buffer_ == 8) {
			raw_write<uint8_t>(output_, buffer_);
			buffer_ = 0;
			bits_in_buffer_ = 0;
		}
	}

	void write_number(uint64_t value, uint8_t bits_to_write) {

		while (bits_to_write > 0) {
			uint8_t bit = (value >> (bits_to_write - 1)) & 1;
			write_bit(bit);
			bits_to_write--;
		}
	}

	~bit_writer() {
		while (bits_in_buffer_ > 0) {
			write_bit(1); // padding with 1 instead of 0
		}
	}
};

hufstr::hufstr() {

	std::ifstream table("table.bin", std::ios::binary);

	if (!table) {
		//throw std::exception("Cannot find table.bin");
		return;
	}

	bit_reader reader(table);

	auto table_size = raw_read<size_t>(table);

	if (!table_size.second) {
		//throw std::exception("Failed to read table size");
		return;
	}

	for (size_t i = 0; i < table_size.first; ++i) {

		auto symbol = raw_read<uint8_t>(table);

		if (!symbol.second) {
			//throw std::exception("Failed to read symbol");
			return;
		}

		auto length = raw_read<uint8_t>(table);

		if (!length.second) {
			//throw std::exception("Failed to read code length");
			return;
		}

		auto& symbol_data = symbols_data_[symbol.first];
		symbol_data.sym = symbol.first;
		symbol_data.len = length.first;	

		sorted_symbol_data_.push_back(symbol_data);
	}

	uint8_t len = 0;
	uint32_t code = 0;
	for (auto& item : sorted_symbol_data_) {
		auto& symbol_data = symbols_data_[item.get().sym];
		code <<= (symbol_data.len - len);
		len = symbol_data.len;
		symbol_data.code = code;
		++code;
	}
}

std::vector<uint8_t> hufstr::compress(const std::string& s) const {

	std::ostringstream writer;

	{
		bit_writer bit_writer(writer);

		for (const auto& symbol : s) {
			auto const& symbol_data = symbols_data_.at(symbol);
			bit_writer.write_number(symbol_data.code, symbol_data.len);
		}
	}
	
	std::vector<uint8_t> data;

	for (const auto& item : writer.str()) {
		data.push_back(item);
	}

	return data;
}

std::string hufstr::decompress(const std::vector<uint8_t>& v) const {

	std::string out_string;

	std::string buffer(v.begin(), v.end());
	std::istringstream reader(buffer);
	bit_reader bit_reader(reader);

	uint32_t read_code = 0;
	uint8_t bits_in_code = 0;

	while (true) {
		auto read_bit = bit_reader.read_bit();

		if (!read_bit.second) {
			break;
		}

		read_code <<= 1;
		read_code |= read_bit.first;
		bits_in_code++;

		for (auto& item : sorted_symbol_data_) {

			const auto& symbol_data = item.get();

			if (read_code == symbol_data.code && bits_in_code == symbol_data.len) {
				out_string.push_back(symbol_data.sym);
				read_code = 0;
				bits_in_code = 0;
				break;
			}
		}
	}

	return out_string;
}