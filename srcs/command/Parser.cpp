/*
** Parser.cpp — Converte uma linha IRC crua num struct Message.
**
** O TCP entrega bytes em stream; a camada de rede (Person A) reconstrói
** linhas completas (terminadas em "\r\n") e passa-as aqui. O resultado
** (Message) é entregue ao CommandHandler::dispatch().
**
** Formato do protocolo IRC (RFC 1459):
**   [":" prefix SP] command [SP params] [SP ":" trailing]
**
** Exemplos:
**   NICK alice                   → command=NICK, params=[alice]
**   JOIN #42                     → command=JOIN, params=[#42]
**   PRIVMSG #42 :Olá!            → command=PRIVMSG, params=[#42], trailing=Olá!
**   :alice!u@h PRIVMSG bob :oi   → prefix=alice!u@h, command=PRIVMSG, params=[bob], trailing=oi
*/

#include "Message.hpp"
#include <cctype>

/*
** parseMessage
** Analisa sintaticamente uma linha IRC já sem o terminador "\r\n".
** Converte o comando para maiúsculas (protocolo IRC é case-insensitive no comando).
**
** Recebe: 'line' — linha IRC sem "\r\n".
** Devolve: struct Message populado; se a linha estiver vazia, devolve Message vazio.
*/
Message parseMessage(const std::string& line)
{
	Message     msg;
	std::size_t pos = 0;

	if (line.empty())
		return msg;

	/* ── 1. Prefixo opcional ─────────────────────────────────────────────── */
	if (line[pos] == ':')
	{
		std::size_t spacePos = line.find(' ', pos);
		if (spacePos == std::string::npos)
			return msg;                     /* linha malformada */
		msg.prefix = line.substr(1, spacePos - 1);
		pos = spacePos + 1;
		while (pos < line.size() && line[pos] == ' ')
			++pos;
	}

	if (pos >= line.size())
		return msg;

	/* ── 2. Comando ──────────────────────────────────────────────────────── */
	std::size_t cmdEnd = line.find(' ', pos);
	if (cmdEnd == std::string::npos)
	{
		/* Linha só tem o comando, sem parâmetros */
		msg.command = line.substr(pos);
		for (std::size_t i = 0; i < msg.command.size(); ++i)
			msg.command[i] = static_cast<char>(
				std::toupper(static_cast<unsigned char>(msg.command[i])));
		return msg;
	}
	msg.command = line.substr(pos, cmdEnd - pos);
	for (std::size_t i = 0; i < msg.command.size(); ++i)
		msg.command[i] = static_cast<char>(
			std::toupper(static_cast<unsigned char>(msg.command[i])));
	pos = cmdEnd + 1;

	/* ── 3. Parâmetros e trailing ────────────────────────────────────────── */
	while (pos < line.size())
	{
		/* Saltar espaços extra */
		while (pos < line.size() && line[pos] == ' ')
			++pos;
		if (pos >= line.size())
			break;

		/* Se começa com ':' é o trailing (pode conter espaços) */
		if (line[pos] == ':')
		{
			msg.trailing    = line.substr(pos + 1);
			msg.hasTrailing = true;
			break;
		}

		/* Parâmetro normal: vai até ao próximo espaço */
		std::size_t spacePos = line.find(' ', pos);
		if (spacePos == std::string::npos)
		{
			msg.params.push_back(line.substr(pos));
			break;
		}
		msg.params.push_back(line.substr(pos, spacePos - pos));
		pos = spacePos + 1;
	}

	return msg;
}
