#include <fstream>
#include <iostream>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <numeric>
#include <algorithm>
#include <iomanip>

static void calculate_crc(uint16_t& crc, uint8_t byte)
{
	static const uint16_t crc_table[16] =
	{
		0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
		0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400
	};

	uint16_t tmp;

	// compute checksum of lower four bits of byte
	tmp = crc_table[crc & 0xF];
	crc = (crc >> 4) & 0x0FFF;
	crc = crc ^ tmp ^ crc_table[byte & 0xF];

	// now compute checksum of upper four bits of byte
	tmp = crc_table[crc & 0xF];
	crc = (crc >> 4) & 0x0FFF;
	crc = crc ^ tmp ^ crc_table[(byte >> 4) & 0xF];
}

class fit_reader {

private:

#pragma pack(1)
	struct fit_header {
		uint8_t header_size = 0;
		uint8_t protocol_version = 0;
		uint16_t profile_version = 0;
		uint32_t data_size = 0;
		uint8_t data_type[4] = { 0 };
		uint16_t crc = 0;
	};

#pragma pack(1)
	struct field_definition {
		uint8_t number = 0;
		uint8_t size = 0;
		uint8_t base_type = 0;
	};

#pragma pack(1)
	struct definition_message {
		uint8_t reserved = 0;
		uint8_t architecture = 0;
		uint16_t global_message_identifier = 0;
		uint8_t num_fields = 0;
		std::vector<field_definition> field_definitions;
		uint8_t local_message_type = 0;

		size_t data_size() const {
			return std::accumulate(
				field_definitions.begin(),
				field_definitions.end(), static_cast<size_t>(0), [](const size_t& a, const field_definition& b) {
					return a + b.size;
				});
		}

		size_t definition_size() const {
			return static_cast<size_t>(num_fields) * 3 + 5;
		}

		void crc(uint16_t& last_crc) const {
			calculate_crc(last_crc, reserved);
			calculate_crc(last_crc, architecture);
			calculate_crc(last_crc, global_message_identifier & 0b11111111);
			calculate_crc(last_crc, (global_message_identifier >> 8) & 0b11111111);		
			calculate_crc(last_crc, num_fields);

			for (const auto& item : field_definitions) {
				calculate_crc(last_crc, item.number);
				calculate_crc(last_crc, item.size);
				calculate_crc(last_crc, item.base_type);
			}
		}
	};

	struct message_header {
		uint8_t message_type = 0;
		uint8_t local_message_type = 0;
	};

	std::ifstream& input_;
	fit_header header_;
	std::unordered_map<uint8_t, definition_message> definitions_;
	uint16_t last_data_crc_ = 0;

	bool check_crc(char* raw_data, size_t raw_data_size, uint16_t good_crc) {

		uint16_t crc = 0;

		for (int i = 0; i < raw_data_size; ++i) {
			calculate_crc(crc, raw_data[i]);
		}

		return crc == good_crc;
	}

	size_t read_message() {

		uint8_t message_header_byte = 0;
		input_.read(reinterpret_cast<char*>(&message_header_byte), sizeof(message_header_byte));
		calculate_crc(last_data_crc_, message_header_byte);

		message_header message_header;
		message_header.message_type = (message_header_byte & 0b11110000) >> 4;
		message_header.local_message_type = message_header_byte & 0b00001111;

		if (message_header.message_type == 4) {
			const auto& definition = read_definition(message_header);
			definition.crc(last_data_crc_);
			definitions_[definition.local_message_type] = definition;
			return definition.definition_size() + 1;
		}
		else if (message_header.message_type == 0) {
			const auto& definition = definitions_[message_header.local_message_type];

			for (uint32_t i = 0; i < definition.num_fields; ++i) {
				std::vector<uint8_t> local_data(definition.field_definitions[i].size);
				input_.read(reinterpret_cast<char*>(local_data.data()), local_data.size());

				if (definition.global_message_identifier == 0 && definition.field_definitions[i].number == 4) {

					uint32_t time_created = (local_data[0] << 0) | (local_data[1] << 8) | (local_data[2] << 16) | (local_data[3] << 24);
					std::cout << "time_created = " << time_created << std::endl;

				}
				else if (definition.global_message_identifier == 19 && definition.field_definitions[i].number == 13) {

					uint16_t avg_speed = (local_data[0] << 0) | (local_data[1] << 8);

					double converted = static_cast<double>(avg_speed) / 1000 / 1000 * 3600;
					std::cout << "avg_speed = " << std::setprecision(4) << converted << " km/h" << std::endl;
				}

				for (const auto& item : local_data) {
					calculate_crc(last_data_crc_, item);
				}
			}

			return definition.data_size() + 1;
		}
		else {
			return 0;
		}
	}

	definition_message read_definition(const message_header& header) {
		definition_message definition;
		input_.read(reinterpret_cast<char*>(&definition), 5);
		definition.field_definitions.resize(definition.num_fields);
		input_.read(reinterpret_cast<char*>(definition.field_definitions.data()), definition.num_fields * sizeof(field_definition));
		definition.local_message_type = header.local_message_type;
		return definition;
	}

public:
	fit_reader(std::ifstream& input) : input_(input) { }

	bool parse() {

		input_.read(reinterpret_cast<char*>(&header_), sizeof(header_));

		if (check_crc(reinterpret_cast<char*>(&header_), sizeof(header_) - 2, header_.crc)) {
			std::cout << "Header CRC ok" << std::endl;
		}
		else {
			return false;
		}

		size_t read_bytes = 0;

		while (read_bytes < header_.data_size) {
			read_bytes += read_message();
		}

		uint16_t data_crc = 0;
		input_.read(reinterpret_cast<char*>(&data_crc), sizeof(data_crc));

		if (last_data_crc_ != data_crc) {
			return false;
		}

		std::cout << "File CRC ok" << std::endl;

		return true;
	}
};

int main(int argc, char* argv[])
{
	if (argc != 2) {
		return EXIT_FAILURE;
	}

	std::ifstream input(argv[1], std::ios::binary);

	if (!input) {
		return EXIT_FAILURE;
	}

	fit_reader reader(input);

	if (!reader.parse()) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}