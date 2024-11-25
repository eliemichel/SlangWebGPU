#pragma once

#include <slang-webgpu/common/result.h>

/////////////////////////////////////////////////
// SLANG-SPECIFIC RESULT UTILS

/**
 * Bridges Slang's error management with our own Result<_, Error> management.
 * This may only be used in a function whose result type is Result<_, Error>.
 */
#define TRY_SLANG(statement) \
	{ \
		SlangResult res = (statement); \
		if (SLANG_FAILED(res)) { \
			return Error{ "Slang Error, status = " + std::to_string(res) }; \
		} \
	}

