#pragma once

#include <slang-webgpu/common/result.h>

#include <filesystem>

Result<std::string, Error> loadTextFile(const std::filesystem::path& path);
