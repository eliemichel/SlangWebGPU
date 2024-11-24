#pragma once

#include <slang-webgpu/common/result.h>

#include <filesystem>

Result<std::string, Error> loadTextFile(
	const std::filesystem::path& path
);

Result<Void, Error> saveTextFile(
	const std::filesystem::path& path,
	const std::string& contents
);
