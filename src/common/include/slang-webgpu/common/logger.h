#pragma once

#include <sstream>
#include <iostream>

/**
 * A very simple logging class.
 * Do not use this directly, prefer using the LOG macro below.
 */
class Logger {
public:
	enum class Level {
		ERROR,
		WARNING,
		INFO,
		DEBUG,
	};

public:
	Logger(Level level, const char* file, int line)
		: m_level(level)
		, m_file(file)
		, m_line(line)
	{}
	~Logger() {
		switch (m_level) {
		case Level::ERROR:
			std::cout << "ERROR: " << m_stream.str() << std::endl;
			break;
		case Level::WARNING:
			std::cout << "WARNING: " << m_stream.str() << std::endl;
			break;
		case Level::INFO:
			std::cout << "INFO: " << m_stream.str() << std::endl;
			break;
		case Level::DEBUG:
			std::cout << "DEBUG(" << m_file << ", line" << m_line << "): " << m_stream.str() << std::endl;
			break;
		}
	}
	Logger(Logger&) = delete;
	Logger& operator=(const Logger&) = delete;
	std::ostringstream& stream() { return m_stream; }

private:
	Level m_level;
	const char* m_file;
	int m_line;
	std::ostringstream m_stream;
};

// General macro for logging
#define LOG(LEVEL) Logger(Logger::Level::LEVEL, __FILE__ , __LINE__).stream()
