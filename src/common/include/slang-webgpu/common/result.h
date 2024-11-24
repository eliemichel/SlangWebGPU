#pragma once

#include <variant>

/**
 * The Result type contains either a correct value or an error.
 * This is a common design pattern to propagate errors without using
 * exceptions. It is close in spirit to rust's Result type.
 */
template <typename Value, typename Error>
using Result = std::variant<Value, Error>;

template <typename Value, typename Error>
bool isError(const Result<Value, Error>& result) {
	return result.index() == 1;
}
