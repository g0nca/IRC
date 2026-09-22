/*
** Utils.cpp — Utilitários partilhados pelas camadas de rede e protocolo.
**
** Funções puras (sem estado global). Implementam operações básicas que
** o C++98 não fornece nativamente (to_string, trim, etc.).
*/

#include "Utils.hpp"
#include <sstream>
#include <cctype>

/*
** Utils::split
** Divide a string 's' cada vez que encontra 'delimiter'.
** Tokens vazios (delimitadores consecutivos) são descartados.
** Recebe: string de entrada, carácter delimitador.
** Devolve: vector com os tokens não-vazios.
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
** Divide a string em tokens separados por espaços e tabs (runs de whitespace).
** Recebe: string de entrada.
** Devolve: vector de tokens, sem strings vazias.
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
** Converte um int para a sua representação decimal em string.
** Substitui std::to_string (indisponível em C++98).
** Recebe: valor inteiro.
** Devolve: string decimal do valor.
*/
std::string Utils::toString(int value)
{
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

/*
** Utils::trim
** Remove espaços, tabs, '\r' e '\n' do início e fim da string.
** Recebe: string de entrada.
** Devolve: cópia sem whitespace nas extremidades.
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
** Devolve uma cópia ASCII-maiúscula da string.
** Recebe: string de entrada.
** Devolve: string com todos os caracteres em maiúsculas.
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
** Valida o nome de um canal IRC. Deve começar por '#' e não pode conter
** espaços, vírgulas, o carácter nulo nem BEL (\a).
** Recebe: nome do canal (com '#').
** Devolve: true se o nome é válido, false caso contrário.
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
** Valida um nickname IRC (RFC 1459): 1-9 caracteres; o primeiro deve ser
** letra ou caracter especial (_-[]\\^{}|); os restantes podem incluir dígitos.
** Recebe: nickname proposto.
** Devolve: true se válido, false caso contrário.
*/
bool Utils::isValidNickname(const std::string& nick)
{
	static const std::string special = "_-[]\\^{}|";

	if (nick.empty() || nick.size() > 9)
		return false;

	/* primeiro carácter: letra ou special */
	char first = nick[0];
	if (!std::isalpha(static_cast<unsigned char>(first)) &&
	    special.find(first) == std::string::npos)
		return false;

	/* restantes: alfanumérico ou special */
	for (std::size_t i = 1; i < nick.size(); ++i)
	{
		char c = nick[i];
		if (!std::isalnum(static_cast<unsigned char>(c)) &&
		    special.find(c) == std::string::npos)
			return false;
	}
	return true;
}
