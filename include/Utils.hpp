#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <cstddef>

/**
 * @file Utils.hpp
 * @brief Small free helpers shared by both layers. Implement in Utils.cpp.
 *
 * C++98 has no std::to_string and no <sstream> niceties baked in, so these
 * little helpers avoid copy-pasted code across the project.
 */
namespace Utils
{
	/** @brief Split a string on a single-character delimiter. */
	std::vector<std::string> split(const std::string& s, char delimiter);

	/** @brief Split on runs of whitespace (used by the IRC line parser). */
	std::vector<std::string> splitWhitespace(const std::string& s);

	/** @brief Convert an int to its decimal string (replacement for to_string). */
	std::string toString(int value);

	/** @brief Remove leading/trailing spaces, "\r" and "\n". */
	std::string trim(const std::string& s);

	/** @brief ASCII upper-case copy (IRC nick/channel comparisons are case-insensitive). */
	std::string toUpper(const std::string& s);

	/** @brief True if name is a syntactically valid channel name (starts with '#'). */
	bool isValidChannelName(const std::string& name);

	/** @brief True if nick is a syntactically valid nickname. */
	bool isValidNickname(const std::string& nick);
}

#endif // UTILS_HPP
