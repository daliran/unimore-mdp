#include <fstream>
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <optional>
#include <iostream>
#include <memory>
#include <algorithm>

using vec3b = std::array<uint8_t, 3>;

template<typename T>
T raw_read(std::istream& input, size_t size = sizeof(T)) {
	T value = 0;
	input.read(reinterpret_cast<char*>(&value), size);
	return value;
}

template<typename T>
void raw_write(std::ostream& output, T value, size_t size = sizeof(T)) {
	output.write(reinterpret_cast<char*>(&value), size);
}

template<typename T>
class matrix {
private:
	uint64_t rows_;
	uint64_t cols_;
	std::vector<T> data_;

public:
	matrix() {}

	matrix(uint64_t rows, uint64_t cols) {
		resize(rows, cols);
	}

	void resize(uint64_t rows, uint64_t cols) {
		rows_ = rows;
		cols_ = cols;
		data_.resize(rows_ * cols_);
	}

	T& operator() (uint64_t row, uint64_t col) {
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

static bool write_ppm(std::string filename, matrix<vec3b>& raster) {

	std::ofstream output(filename, std::ios::binary);

	if (!output) {
		return false;
	}

	output << "P6" << std::endl;
	output << "#PPM creato nel corso di SdEM" << std::endl; // Required by OLJ
	output << raster.cols() << " " << raster.rows() << std::endl;
	output << 255 << std::endl;

	for (uint64_t row = 0; row < raster.rows(); ++row) {
		for (uint64_t col = 0; col < raster.cols(); ++col) {
			const vec3b& pixel = raster(row, col);
			raw_write(output, pixel[0]);
			raw_write(output, pixel[1]);
			raw_write(output, pixel[2]);
		}
	}

	return true;
}

class ubjson_element {
public:
	virtual ~ubjson_element() = default;
};

static std::unique_ptr<ubjson_element> create_ubjson_element(std::ifstream& input, uint8_t type);

class ubjson_numeric : public ubjson_element {
private:
	int64_t value_ = 0;
public:
	ubjson_numeric(std::ifstream& input, uint8_t type) {

		if (type == 'i') {
			// 8 bit
			value_ = raw_read<int8_t>(input);
		}
		else if (type == 'U') {
			// 8 bit usigned
			value_ = raw_read<uint8_t>(input);
		}
		else if (type == 'I') {
			// 16 bit
			uint64_t byte1 = raw_read<uint8_t>(input);
			uint64_t byte2 = raw_read<uint8_t>(input);
			value_ = (byte1 << 8) | byte2;
		}
		else if (type == 'l') {
			// 32 bit
			uint64_t byte1 = raw_read<uint8_t>(input);
			uint64_t byte2 = raw_read<uint8_t>(input);
			uint64_t byte3 = raw_read<uint8_t>(input);
			uint64_t byte4 = raw_read<uint8_t>(input);
			value_ = (byte1 << 24) | (byte2 << 16) | (byte3 << 8) | byte4;
		}
		else {
			std::cerr << "Unsupported numeric type" << std::endl;
		}
	}

	int64_t value() const {
		return value_;
	}
};

class ubjson_string : public ubjson_element {
private:
	std::string value_;

public:
	ubjson_string(std::ifstream& input) {
		uint8_t numeric_type = raw_read<uint8_t>(input);
		ubjson_numeric string_length(input, numeric_type);

		value_.resize(string_length.value());
		input.read(reinterpret_cast<char*>(value_.data()), value_.size());
	}

	std::string value() const {
		return value_;
	}
};

class ubjson_object : public ubjson_element {
private:
	// There can be elements with the same key and the insertion order is important
	std::vector<std::pair<std::string, std::unique_ptr<ubjson_element>>> data_;

public:
	ubjson_object(std::ifstream& input) {

		while (true) {

			uint8_t current_tag = input.peek();

			if (current_tag == '}') {
				// End of object
				input.ignore(1);
				break;
			}

			ubjson_string element_name = ubjson_string(input);

			uint8_t element_type = raw_read<uint8_t>(input);
			data_.emplace_back(std::make_pair(element_name.value(), create_ubjson_element(input, element_type)));
		}
	}

	template<typename T>
	std::vector<std::pair<std::string, std::reference_wrapper<T>>> get_elements_by_key(std::optional<std::string> key) const {

		std::vector<std::pair<std::string, std::reference_wrapper<T>>> matches;

		for (const auto& item : data_) {
			if (!key.has_value() || (key.has_value() && item.first == key)) {
				matches.emplace_back(
					std::make_pair(
						item.first,
						std::reference_wrapper<T>(dynamic_cast<T&>(*item.second))
					)
				);
			}
		}

		return matches;
	}

	template<typename T>
	T& get_element_by_key(const std::string& key) const {

		for (const auto& item : data_) {
			if (item.first == key) {
				return dynamic_cast<T&>(*item.second);
			}
		}

		throw std::exception();
	}
};

class ubjson_array : public ubjson_element {
private:
	std::vector<std::unique_ptr<ubjson_element>> data_;

public:
	ubjson_array(std::ifstream& input) {

		std::optional<uint8_t> array_type;
		std::optional<int64_t> array_count;

		if (input.peek() == '$') {
			// type
			input.ignore(1);
			array_type = raw_read<uint8_t>(input);
		}

		if (input.peek() == '#') {
			// count
			input.ignore(1);
			uint8_t numeric_type = raw_read<uint8_t>(input);
			ubjson_numeric count(input, numeric_type);
			array_count = count.value();
		}

		while (!array_count.has_value() || (array_count.has_value() && array_count.value() > 0)) {

			if (!array_count.has_value() && input.peek() == ']') {
				// End of array
				input.ignore(1);
				break;
			}

			if (array_type.has_value()) {
				data_.emplace_back(create_ubjson_element(input, array_type.value()));
			}
			else {
				uint8_t element_type = raw_read<uint8_t>(input);
				data_.emplace_back(create_ubjson_element(input, element_type));
			}

			if (array_count.has_value()) {
				array_count.value()--;
			}
		}
	}

	size_t size() const {
		return data_.size();
	}

	template<typename T>
	T& get_value(uint64_t index) const {
		return dynamic_cast<T&>(*data_[index]);
	}

};

static std::unique_ptr<ubjson_element> create_ubjson_element(std::ifstream& input, uint8_t type) {

	switch (type) {
	case '{':
		return std::make_unique<ubjson_object>(input);
	case '[':
		return std::make_unique<ubjson_array>(input);
	case 'S':
		return std::make_unique<ubjson_string>(input);
	case 'i':
	case 'U':
	case 'I':
	case 'l':
		return std::make_unique<ubjson_numeric>(input, type);
	case 'd': // float 32
	case 'D': // float 64
	case 'H': // High precision
	case 'L': // int 64
	case 'Z': // null
	case 'N': // no-op
	case 'T': // true
	case 'F': // false
	case 'C': // char
	default:
		std::cerr << "Read an unsupported type" << std::endl;
		return std::make_unique<ubjson_element>();
	}
}

static std::optional<std::unique_ptr<ubjson_element>> read_ubjson(std::string filename) {

	std::ifstream input(filename, std::ios::binary);

	if (!input) {
		return std::nullopt;
	}

	uint8_t element_type = raw_read<uint8_t>(input);
	return create_ubjson_element(input, element_type);
}

struct ubjson_canvas {
	uint64_t width = 0;
	uint64_t height = 0;
	vec3b background = { 0 };
};

struct ubjson_image {
	uint64_t x = 0;
	uint64_t y = 0;
	uint64_t width = 0;
	uint64_t height = 0;
	std::vector<vec3b> data;
};

static ubjson_canvas get_canvas(const ubjson_object& root_element) {

	ubjson_canvas canvas;

	auto& canvas_object = root_element.get_element_by_key<ubjson_object>("canvas");
	canvas.width = canvas_object.get_element_by_key<ubjson_numeric>("width").value();
	canvas.height = canvas_object.get_element_by_key<ubjson_numeric>("height").value();

	auto& background_array = canvas_object.get_element_by_key<ubjson_array>("background");
	canvas.background[0] = static_cast<uint8_t>(background_array.get_value<ubjson_numeric>(0).value());
	canvas.background[1] = static_cast<uint8_t>(background_array.get_value<ubjson_numeric>(1).value());
	canvas.background[2] = static_cast<uint8_t>(background_array.get_value<ubjson_numeric>(2).value());

	return canvas;
}

static ubjson_image get_image(const ubjson_object& image_element) {

	ubjson_image image;

	image.x = image_element.get_element_by_key<ubjson_numeric>("x").value();
	image.y = image_element.get_element_by_key<ubjson_numeric>("y").value();
	image.width = image_element.get_element_by_key<ubjson_numeric>("width").value();
	image.height = image_element.get_element_by_key<ubjson_numeric>("height").value();

	auto& data_array = image_element.get_element_by_key<ubjson_array>("data");

	for (size_t i = 0; i < data_array.size(); i += 3) {
		uint8_t r = static_cast<uint8_t>(data_array.get_value<ubjson_numeric>(i).value());
		uint8_t g = static_cast<uint8_t>(data_array.get_value<ubjson_numeric>(i + 1).value());
		uint8_t b = static_cast<uint8_t>(data_array.get_value<ubjson_numeric>(i + 2).value());
		vec3b pixel = { r, g, b };
		image.data.push_back(pixel);
	}

	return image;
}

static std::vector<ubjson_image> get_images(const ubjson_object& root_element) {
	std::vector<ubjson_image> images;

	auto& elements_object = root_element.get_element_by_key<ubjson_object>("elements");
	auto image_elements = elements_object.get_elements_by_key<ubjson_object>("image");

	for (auto& item : image_elements) {
		images.emplace_back(get_image(item.second));
	}

	return images;
}

static void list_elements(const ubjson_object& root_element) {

	auto& elements_object = root_element.get_element_by_key<ubjson_object>("elements");

	for (const auto& item : elements_object.get_elements_by_key<ubjson_object>(std::nullopt)) {
		std::cout << item.first << " : ";

		for (const auto& item2 : item.second.get().get_elements_by_key<ubjson_element>(std::nullopt)) {
			std::cout << item2.first << ",";
		}

		std::cout << std::endl;
	}
}

int main(int argc, char* argv[]) {

	if (argc != 3) {
		std::cerr << "Wrong number of arguments" << std::endl;
		return EXIT_FAILURE;
	}

	std::optional<std::unique_ptr<ubjson_element>> root_element = read_ubjson(argv[1]);

	if (!root_element.has_value()) {
		std::cerr << "Failed to read the ubjson" << std::endl;
		return EXIT_FAILURE;
	}

	ubjson_object& casted_root_element = dynamic_cast<ubjson_object&>(*root_element.value());

	ubjson_canvas canvas = get_canvas(casted_root_element);

	std::vector<ubjson_image> images = get_images(casted_root_element);

	matrix<vec3b> canvas_raster(canvas.height, canvas.width);

	for (uint64_t row = 0; row < canvas_raster.rows(); ++row) {
		for (uint64_t col = 0; col < canvas_raster.cols(); ++col) {
			canvas_raster(row, col) = canvas.background;
		}
	}

	write_ppm("canvas.ppm", canvas_raster);

	for (uint64_t i = 0; i < images.size(); ++i) {
		matrix<vec3b> image_raster(images[i].height, images[i].width);
		std::copy(images[i].data.begin(), images[i].data.end(), image_raster.begin());
		write_ppm("image" + std::to_string(i + 1) + ".ppm", image_raster);

		for (uint64_t row = 0; row < canvas_raster.rows(); ++row) {
			for (uint64_t col = 0; col < canvas_raster.cols(); ++col) {
				if (row >= images[i].y && row < images[i].height + images[i].y &&
					col >= images[i].x && col < images[i].width + images[i].x) {
					canvas_raster(row, col) = image_raster(row - images[i].y, col - images[i].x);
				}
			}
		}
	}

	write_ppm(argv[2], canvas_raster);

	list_elements(casted_root_element);

	return EXIT_SUCCESS;
}