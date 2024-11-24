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
	std::ofstream file;
	file.open(path);
	if (!file.is_open()) {
		return Error{ "Could not open output file '" + path.string() + "'" };
	}
	file << contents;
	return {};
}
