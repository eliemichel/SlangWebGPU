#pragma once

#include <variant>
#include <string>
#include <sstream>

/////////////////////////////////////////////////
// GENERIC RESULT TYPE

/**
 * The Result type contains either a correct value or an error.
 * This is a common design pattern to propagate errors without using
 * exceptions. It is close in spirit to rust's Result type.
 */
template <typename Value, typename Error>
using Result = std::variant<Value, Error>;

/**
 * Test if a Result contains an error rather than a result value
 */
template <typename Value, typename Error>
bool isError(const Result<Value, Error>& result) {
	return result.index() == 1;
}

/**
 * A unit type, used as first argument of Result for function that return void or an error.
 */
struct Void {};

/**
 * A generic error type, to be used when there is no need for a specific type.
 */
struct Error {
	std::string message;
};

/**
 * Macro that tries to assign the result of 'expression' to 'variable'.
 * The expression must evaluate into a Result<Value,Error> type, such that
 * Value is the type of 'variable'. In case of error, the Error object is
 * returned, which implies that this macro may only be called from within
 * a function whose return type is Result<_,Error>.
 */
#define TRY_ASSIGN(variable, expression) \
	{ \
		auto result = (expression); \
		if (isError(result)) { \
			return std::get<1>(result); \
		} else { \
			variable = std::move(std::get<0>(result)); \
		} \
	}

/**
 * Macro that tries to execute a statement that evaluates into
 * Result<Void,Error>. In case of error, the Error object is returned, which
 * implies that this macro may only be called from within a function whose
 * return type is Result<_,Error>.
 */
#define TRY(statement) \
	{ \
		auto result = (statement); \
		if (isError(result)) { \
			return std::get<1>(result); \
		} \
	}

/**
 * Sort of assertion, except it does not throw but rather return an error,
 * which requires that this macro may only be called from within a function
 * whose return type is Result<_,Error>.
 */
#define TRY_ASSERT(test, message) \
	if (!(test)) { \
		std::ostringstream _out; \
		_out << "Assertion failed: " << message; \
		return Error{ _out.str() }; \
	}
