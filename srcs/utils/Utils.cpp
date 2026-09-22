/*
** Utils.cpp — Shared utilities for the network and protocol layers.
**
** Pure functions (no global state). Implement basic operations that
** C++98 does not provide natively (to_string, trim, etc.).
*/

#include "Utils.hpp"
#include <sstream>
#include <cctype>

/*
** Utils::split
** Splits string 's' on every occurrence of 'delimiter'.
** Empty tokens (consecutive delimiters) are discarded.
** Receives: input string, delimiter character.
** Returns: vector of non-empty tokens.
*/
std::vector<std::string> Utils::split(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::string               token;

	for (std::size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == delimiter)
		{
			if (!token.empty())
			{
				tokens.push_back(token);
				token.clear();
			}
		}
		else
			token += s[i];
	}
	if (!token.empty())
		tokens.push_back(token);
	return tokens;
}

/*
** Utils::splitWhitespace
** Splits the string on runs of spaces and tabs.
** Receives: input string.
** Returns: vector of tokens, no empty strings.
*/
std::vector<std::string> Utils::splitWhitespace(const std::string& s)
{
	std::vector<std::string> tokens;
	std::string               token;

	for (std::size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == ' ' || s[i] == '\t')
		{
			if (!token.empty())
			{
				tokens.push_back(token);
				token.clear();
			}
		}
		else
			token += s[i];
	}
	if (!token.empty())
		tokens.push_back(token);
	return tokens;
}

/*
** Utils::toString
** Converts an int to its decimal string representation.
** Replaces std::to_string (unavailable in C++98).
** Receives: integer value.
** Returns: decimal string of the value.
*/
std::string Utils::toString(int value)
{
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

/*
** Utils::trim
** Removes spaces, tabs, '\r' and '\n' from both ends of the string.
** Receives: input string.
** Returns: copy without leading/trailing whitespace.
*/
std::string Utils::trim(const std::string& s)
{
	std::size_t start = 0;
	while (start < s.size() &&
	       (s[start] == ' ' || s[start] == '\t' ||
	        s[start] == '\r' || s[start] == '\n'))
		++start;

	std::size_t end = s.size();
	while (end > start &&
	       (s[end - 1] == ' ' || s[end - 1] == '\t' ||
	        s[end - 1] == '\r' || s[end - 1] == '\n'))
		--end;

	return s.substr(start, end - start);
}

/*
** Utils::toUpper
** Returns an ASCII-uppercased copy of the string.
** Receives: input string.
** Returns: string with all characters in upper case.
*/
std::string Utils::toUpper(const std::string& s)
{
	std::string result = s;
	for (std::size_t i = 0; i < result.size(); ++i)
		result[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[i])));
	return result;
}

/*
** Utils::isValidChannelName
** Validates an IRC channel name. Must start with '#' and must not contain
** spaces, commas, the null character or BEL (\a).
** Receives: channel name (including '#').
** Returns: true if the name is valid, false otherwise.
*/
bool Utils::isValidChannelName(const std::string& name)
{
	if (name.empty() || name[0] != '#' || name.size() < 2)
		return false;
	for (std::size_t i = 1; i < name.size(); ++i)
	{
		char c = name[i];
		if (c == ' ' || c == '\0' || c == '\a' || c == ',')
			return false;
	}
	return true;
}

/*
** Utils::isValidNickname
** Validates an IRC nickname (RFC 1459): 1-9 characters; the first must be
** a letter or special character (_-[]\\^{}|); the rest may include digits.
** Receives: proposed nickname.
** Returns: true if valid, false otherwise.
*/
bool Utils::isValidNickname(const std::string& nick)
{
	static const std::string special = "_-[]\\^{}|";

	if (nick.empty() || nick.size() > 9)
		return false;

	/* first character: letter or special */
	char first = nick[0];
	if (!std::isalpha(static_cast<unsigned char>(first)) &&
	    special.find(first) == std::string::npos)
		return false;

	/* remaining characters: alphanumeric or special */
	for (std::size_t i = 1; i < nick.size(); ++i)
	{
		char c = nick[i];
		if (!std::isalnum(static_cast<unsigned char>(c)) &&
		    special.find(c) == std::string::npos)
			return false;
	}
	return true;
}
