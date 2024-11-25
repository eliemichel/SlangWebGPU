#include <slang-webgpu/common/io.h>

#include <fstream>
#include <sstream>

Result<std::string, Error> loadTextFile(
	const std::filesystem::path& path
) {
	std::ifstream file;
	file.open(path);
	if (!file.is_open()) {
		return Error{ "Could not open input file '" + path.string() + "'" };
	}
	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

Result<Void, Error> saveTextFile(
	const std::filesystem::path& path,
	const std::string& contents
) {
	// Ensure parent directory
	auto parent = path.parent_path();
	if (!std::filesystem::exists(parent)) {
		std::error_code err;
		if (!std::filesystem::create_directories(parent, err)) {
			return Error{ "Could not create parent directory for output file '" + path.string() + "': " + err.message() };
		}
	}

	// Create file
	std::ofstream file;
	file.open(path);
	if (!file.is_open()) {
		return Error{ "Could not open output file '" + path.string() + "'" };
	}
	file << contents;
	return {};
}
