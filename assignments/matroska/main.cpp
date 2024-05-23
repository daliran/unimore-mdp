#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <bit>
#include <cmath>

enum class ebml_type {
	ebml_unknown,
	ebml_int,
	ebml_uint,
	ebml_float,
	ebml_string,
	ebml_utf8,
	ebml_date,
	ebml_master,
	ebml_binary
};

struct ebml_entry {
	ebml_type type;
	std::string name;
};

std::unordered_map<uint64_t, ebml_entry> ebml_table = {
	std::pair<uint64_t, ebml_entry>(0x1A45DFA3, ebml_entry { ebml_type::ebml_master, "EBML" }),
	std::pair<uint64_t, ebml_entry>(0x4286, ebml_entry { ebml_type::ebml_uint, "EBMLVersion" }),
	std::pair<uint64_t, ebml_entry>(0x42F7, ebml_entry { ebml_type::ebml_uint, "EBMLReadVersion" }),
	std::pair<uint64_t, ebml_entry>(0x42F2, ebml_entry { ebml_type::ebml_uint, "EBMLMaxIDLength" }),
	std::pair<uint64_t, ebml_entry>(0x42F3, ebml_entry { ebml_type::ebml_uint, "EBMLMaxSizeLength" }),
	std::pair<uint64_t, ebml_entry>(0x4282, ebml_entry { ebml_type::ebml_string, "DocType" }),
	std::pair<uint64_t, ebml_entry>(0x4287, ebml_entry { ebml_type::ebml_uint, "DocTypeVersion" }),
	std::pair<uint64_t, ebml_entry>(0x4285, ebml_entry { ebml_type::ebml_uint, "DocTypeReadVersion" }),

	std::pair<uint64_t, ebml_entry>(0x18538067, ebml_entry { ebml_type::ebml_master, "Segment" }),
	std::pair<uint64_t, ebml_entry>(0x114D9B74, ebml_entry { ebml_type::ebml_master, "SeekHead" }),
	std::pair<uint64_t, ebml_entry>(0x4DBB, ebml_entry { ebml_type::ebml_master, "Seek" }),
	std::pair<uint64_t, ebml_entry>(0x53AB, ebml_entry { ebml_type::ebml_binary, "SeekID" }),
	std::pair<uint64_t, ebml_entry>(0x53AC, ebml_entry { ebml_type::ebml_uint, "SeekPosition" }),
	std::pair<uint64_t, ebml_entry>(0x1549A966, ebml_entry { ebml_type::ebml_master, "Info" }),
	std::pair<uint64_t, ebml_entry>(0x73A4, ebml_entry { ebml_type::ebml_binary, "SegmentUUID" }),

	std::pair<uint64_t, ebml_entry>(0x1F43B675, ebml_entry { ebml_type::ebml_master, "Cluster" }),
	std::pair<uint64_t, ebml_entry>(0x1654AE6B, ebml_entry { ebml_type::ebml_master, "Tracks" }),
	std::pair<uint64_t, ebml_entry>(0x1C53BB6B, ebml_entry { ebml_type::ebml_master, "Cues" }),
	std::pair<uint64_t, ebml_entry>(0x1941A469, ebml_entry { ebml_type::ebml_master, "Attachments" }),
	std::pair<uint64_t, ebml_entry>(0x1043A770, ebml_entry { ebml_type::ebml_master, "Chapters" }),
	std::pair<uint64_t, ebml_entry>(0x1254C367, ebml_entry { ebml_type::ebml_master, "Tags" }),
};

template<typename T>
T raw_read(std::istream& input) {
	T buffer = 0;
	input.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));
	return buffer;
}

static std::pair<uint64_t, uint64_t> read_varint(std::ifstream& input, bool keep_all_bits) {

	uint8_t byte1 = raw_read<uint8_t>(input);
	uint64_t extra_bytes_to_read = std::countl_zero(byte1);
	uint64_t value = byte1;

	for (uint64_t i = 0; i < extra_bytes_to_read; ++i) {
		value <<= 8;
		value |= raw_read<uint8_t>(input);
	}

	if (!keep_all_bits) {
		auto max_size = 7 * extra_bytes_to_read + 7;
		auto mask = (static_cast<uint64_t>(std::pow(2, max_size)) - 1);
		value &= mask;
	}

	return std::pair<uint64_t, uint64_t>(value, extra_bytes_to_read + 1);
}

static void print_tabs(uint64_t number_of_tabs) {
	for (uint64_t i = 0; i < number_of_tabs; ++i) {
		std::cout << '\t';
	}
}

static std::pair<uint64_t, bool> read_element(std::ifstream& input, uint64_t level) {
	auto id = read_varint(input, true);
	auto size = read_varint(input, false);
	ebml_entry& info = ebml_table[id.first];

	auto total_size = id.second + size.second + size.first;

	if (info.type == ebml_type::ebml_master) {
		print_tabs(level);
		std::cout << "[" << info.name << "]" << std::endl;

		// Skip clusters
		if (info.name == "Cluster") {
			return std::pair<uint64_t, bool>(total_size, false);
		}

		uint64_t size_to_read = size.first;

		while (size_to_read > 0) {
			auto read_element_size = read_element(input, level + 1);

			if (!read_element_size.second) {
				return std::pair<uint64_t, bool>(total_size, false);
			}

			size_to_read -= read_element_size.first;
		}
	}
	else if (info.type == ebml_type::ebml_string) {
		std::string data(size.first, 0);
		input.read(reinterpret_cast<char*>(data.data()), size.first);

		print_tabs(level);
		std::cout << info.name << " (string): " << data << std::endl;
	}
	else if (info.type == ebml_type::ebml_utf8) {
		std::vector<uint8_t> data(size.first);
		input.read(reinterpret_cast<char*>(data.data()), size.first);

		print_tabs(level);
		std::cout << info.name << " (utf8)" << std::endl;
	}
	else if (info.type == ebml_type::ebml_date) {
		int64_t data = 0;

		print_tabs(level);
		std::cout << info.name << " (date)" << std::endl;
	}
	else if (info.type == ebml_type::ebml_float) {
		uint64_t data = 0;

		for (uint64_t i = 0; i < size.first; ++i) {
			data <<= 8;
			data |= raw_read<uint8_t>(input);
		}

		print_tabs(level);
		std::cout << info.name << " (float): ";

		if (size.first == 4) {
			std::cout << static_cast<float>(data);
		}
		else if(size.first == 8) {
			std::cout << static_cast<double>(data);
		}

		std::cout << std::endl;
	}
	else if (info.type == ebml_type::ebml_uint || info.type == ebml_type::ebml_int) {
		uint64_t data = 0;

		for (uint64_t i = 0; i < size.first; ++i) {
			data <<= 8;
			data |= raw_read<uint8_t>(input);
		}

		print_tabs(level);
		std::cout << info.name;

		if (info.type == ebml_type::ebml_int) {
			std::cout << " (int): " << static_cast<int64_t>(data);
		}
		else {
			std::cout << " (uint): " << data;
		}

		std::cout << std::endl;
	}
	else if(info.type == ebml_type::ebml_binary){
		std::vector<uint8_t> data(size.first);
		input.read(reinterpret_cast<char*>(data.data()), size.first);

		print_tabs(level);
		std::cout << info.name << " (binary)" << std::endl;
	}
	else {
		std::vector<uint8_t> data(size.first);
		input.read(reinterpret_cast<char*>(data.data()), size.first);

		print_tabs(level);
		std::cout << "(unprocessed)" << std::endl;
	}

	return std::pair<uint64_t, bool>(total_size, true);
}

int main(int argc, char* argv[]) {

	if (argc != 2) {
		return EXIT_FAILURE;
	}

	std::ifstream input(argv[1], std::ios::binary);

	if (!input) {
		return EXIT_FAILURE;
	}

	// header
	read_element(input, 0);

	// segment
	read_element(input, 0);

	return EXIT_SUCCESS;
}