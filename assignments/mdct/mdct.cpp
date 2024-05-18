#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <vector>
#include <optional>
#include <unordered_map>
#include <numbers>
#include <algorithm>
#include <sstream>
#include <cmath>

template<typename T>
std::vector<double> transform(const std::vector<T>& values, uint64_t window_size) {

	// Minimum number of windows to fit the samples
	uint64_t number_of_windows = static_cast<uint64_t>(std::ceil(
		static_cast<double>(values.size()) / static_cast<double>(window_size)));

	// Vector with padding where sample data is copied after the initial padding
	std::vector<T> samples_with_padding((number_of_windows + 2) * window_size);
	std::copy(values.begin(), values.end(), samples_with_padding.begin() + window_size);

	std::vector<double> coefficients((number_of_windows - 1) * window_size);

	std::unordered_map <uint64_t, std::unordered_map<uint64_t, double>> cos_cache;
	std::unordered_map<uint64_t, double> w_cache;

	// Calculate cache
	for (uint64_t n = 0; n < window_size * 2; ++n) {
		double wn_body = std::numbers::pi / (2 * window_size) * (n + 0.5);
		w_cache[n] = std::sin(wn_body);
	}

	for (uint64_t k = 0; k < window_size; ++k) {
		for (uint64_t n = 0; n < window_size * 2; ++n) {
			double cos_body = (std::numbers::pi / window_size) * (n + 0.5 + window_size / 2.0) * (k + 0.5);
			cos_cache[k][n] = std::cos(cos_body);
		}
	}

	std::cout << "Transform begin" << std::endl;

	// For each window pair
	for (uint64_t i = 0; i < number_of_windows - 1; ++i) {

		uint64_t from = i * window_size;

		// Calculate the window
		for (uint64_t k = 0; k < window_size; ++k) {
			double Xk = 0;
			for (uint64_t n = 0; n < window_size * 2; ++n) {
				Xk += (samples_with_padding[from + n] * w_cache[n] * cos_cache[k][n]);
			}
			coefficients[from + k] = Xk;
		}

		std::cout << "Window pair: " << i << " calculated" << std::endl;
	}

	std::cout << "Transform completed, total coefficients: " << coefficients .size() << std::endl;

	return coefficients;
}

template<typename T>
std::vector<T> anti_transform(const std::vector<double>& coefficients, uint64_t window_size) {

	std::vector<T> samples(coefficients.size() - window_size);

	std::unordered_map <uint64_t, std::unordered_map<uint64_t, double>> cos_cache;
	std::unordered_map<uint64_t, double> w_cache;

	// Calculate cache
	for (uint64_t n = 0; n < window_size * 2; ++n) {
		double wn_body = std::numbers::pi / (2 * window_size) * (n + 0.5);
		w_cache[n] = std::sin(wn_body);
	}

	for (uint64_t k = 0; k < window_size; ++k) {
		for (uint64_t n = 0; n < window_size * 2; ++n) {
			double cos_body = (std::numbers::pi / window_size) * (n + 0.5 + window_size / 2.0) * (k + 0.5);
			cos_cache[k][n] = std::cos(cos_body);
		}
	}

	std::cout << "Anti transform begin" << std::endl;

	// For every coefficient
	for (uint64_t i = 0; i < coefficients.size(); ++i) {

		std::vector<double> window;

		// Calculate the window
		for (uint64_t n = 0; n < window_size * 2; ++n) {

			double yn = 0;

			for (uint64_t k = 0; k < window_size; ++k) {
				double Xk = coefficients[k];
				yn += (Xk * cos_cache[k][n]);
			}

			yn = yn * w_cache[n] * 2 / window_size;

			window.push_back(yn);
		}

		// Reconstruct the samples

		if (i != 0) {
			// first half -> sum to previous window
			uint64_t sample_offset = (i - 1) * window_size;

			for (uint64_t window_offset = 0; window_offset < window_size; ++window_offset) {
				samples[sample_offset + window_offset] = window[window_offset];
			}
		}

		if (i != coefficients.size() - 1) {
			// second half -> sum to current window
			uint64_t sample_offset = i * window_size;

			for (uint64_t window_offset = 0; window_offset < window_size; ++window_offset) {
				samples[sample_offset + window_offset] = window[window_size + window_offset];
			}
		}

		std::cout << "Coefficient: " << i << " converted to window" << std::endl;
	}

	std::cout << "Transform completed, total samples: " << samples.size() << std::endl;

	return samples;
}

template<typename IN, typename OUT>
std::vector<OUT> quantize(const std::vector<IN>& values, int32_t quantization) {

	std::vector<OUT> quantized(values.size());

	std::transform(values.begin(), values.end(),
		quantized.begin(), [&quantization](const IN& value) {
			return value / quantization;
		});

	return quantized;
}

template<typename IN, typename OUT>
std::vector<OUT> dequantize(const std::vector<IN>& values, int32_t quantization) {

	std::vector<OUT> dequantized(values.size());

	std::transform(values.begin(), values.end(),
		dequantized.begin(), [&quantization](const IN& value) {
			return static_cast<OUT>(value * quantization);
		});

	return dequantized;
}

template<typename T>
class entropy_calculator {

private:
	std::unordered_map<T, uint64_t> frequencies_;
	double entropy_ = 0;

public:

	entropy_calculator(const std::vector<T>& data) {

		for (const auto& item : data) {
			frequencies_[item]++;
		}

		for (const auto& symbol_frequency : frequencies_) {

			double p = static_cast<double>(symbol_frequency.second) / data.size();
			double inv_p = 1.0 / p;
			double log_inv_p(std::log2(inv_p));
			entropy_ += (p * log_inv_p);
		}
	}

	double entropy() const {
		return entropy_;
	}
};

template<typename T>
std::optional<std::vector<T>> read_samples(const std::string& input_file) {

	std::ifstream input(input_file, std::ios::binary);

	if (!input) {
		return std::nullopt;
	}

	std::vector<T> samples;

	while (true) {

		T sample = 0;

		input.read(reinterpret_cast<char*>(&sample), sizeof(sample));

		if (input.eof()) {
			break;
		}

		samples.push_back(sample);
	}

	return std::optional(samples);
}

template<typename T>
bool write_samples(const std::string& output_file, const std::vector<T>& data) {

	std::ofstream output(output_file, std::ios::binary);

	if (!output) {
		return false;
	}

	output.write(reinterpret_cast<const char*>(data.data()), sizeof(T) * data.size());

	return true;
}

static bool reconstruct_with_quantization(const std::string& input_file, const std::string& output_path) {

	uint32_t quantization = 2600;

	auto samples = read_samples<int16_t>(input_file);

	if (!samples.has_value()) {
		return false;
	}

	entropy_calculator<int16_t> samples_entropy(samples.value());
	std::cout << "Samples entropy: " << samples_entropy.entropy() << std::endl;

	auto quantized_samples = quantize<int16_t, int32_t>(samples.value(), quantization);

	entropy_calculator<int32_t> quantized_samples_entropy(quantized_samples);
	std::cout << "Quantized samples entropy: " << quantized_samples_entropy.entropy() << std::endl;

	auto dequantized_samples = dequantize<int32_t, int16_t>(quantized_samples, quantization);

	entropy_calculator<int16_t> dequantized_samples_entropy(dequantized_samples);
	std::cout << "Dequantized samples entropy: " << dequantized_samples_entropy.entropy() << std::endl;

	std::string output_file_name = "output_qt.raw";
	std::string quantized_output_path = output_path.empty() ? output_file_name : output_path + "/" + output_file_name;

	if (!write_samples<int16_t>(quantized_output_path, dequantized_samples)) {
		return false;
	}

	std::vector<int16_t> error(samples.value().size());

	for (uint64_t i = 0; i < error.size(); ++i) {
		error[i] = (samples.value()[i] - dequantized_samples[i]);
	}

	std::string error_file_name = "error_qt.raw";
	std::string error_output_path = output_path.empty() ? error_file_name : output_path + "/" + error_file_name;
	
	if (!write_samples<int16_t>(error_output_path, error)) {
		return false;
	}

	return true;
}

static bool reconstruct_with_mdc(const std::string& input_file, const std::string& output_path) {

	uint32_t window_size = 1024;
	uint32_t quantization = 10000;

	auto samples = read_samples<int16_t>(input_file);

	if (!samples.has_value()) {
		return false;
	}

	entropy_calculator<int16_t> samples_entropy(samples.value());
	std::cout << "Samples entropy: " << samples_entropy.entropy() << std::endl;

	auto coefficients = transform<int16_t>(samples.value(), window_size);
	auto quantized_coefficients = quantize<double, int32_t>(coefficients, quantization);
	entropy_calculator<int32_t> quantized_coefficients_entropy(quantized_coefficients);
	std::cout << "Quantized coefficients entropy: " << quantized_coefficients_entropy.entropy() << std::endl;

	auto dequantized_samples = dequantize<int32_t, double>(quantized_coefficients, quantization);

	auto reconstructed_samples = anti_transform<int16_t>(dequantized_samples, window_size);

	std::string output_file_name = "output.raw";
	std::string reconstructed_output_path = output_path.empty() ? output_file_name : output_path + "/" + output_file_name;

	if (!write_samples<int16_t>(reconstructed_output_path, reconstructed_samples)) {
		return false;
	}

	std::vector<int16_t> error(samples.value().size());

	for (uint64_t i = 0; i < error.size(); ++i) {
		error[i] = (samples.value()[i] - reconstructed_samples[i]);
	}

	std::string error_file_name = "error.raw";
	std::string error_output_path = output_path.empty() ? error_file_name : output_path + "/" + error_file_name;

	if (!write_samples<int16_t>(error_output_path, error)) {
		return false;
	}

	return true;
}

int main(int argc, char* argv[]) {

	std::string input_file = "test.raw";
	std::string output_folder;

	if (argc == 3) {
		input_file = argv[1];
		output_folder = argv[2];
	}

	if (!reconstruct_with_quantization(input_file, output_folder)) {
		return EXIT_FAILURE;
	}

	if (!reconstruct_with_mdc(input_file, output_folder)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}