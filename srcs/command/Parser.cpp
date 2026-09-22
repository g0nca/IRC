/*
** Parser.cpp — Converts a raw IRC line into a Message struct.
**
** TCP delivers bytes as a stream; the network layer (Person A) reconstructs
** complete lines (terminated by "\r\n") and passes them here. The result
** (Message) is delivered to CommandHandler::dispatch().
**
** IRC protocol wire format (RFC 1459):
**   [":" prefix SP] command [SP params] [SP ":" trailing]
**
** Examples:
**   NICK alice                   → command=NICK, params=[alice]
**   JOIN #42                     → command=JOIN, params=[#42]
**   PRIVMSG #42 :Hello!          → command=PRIVMSG, params=[#42], trailing=Hello!
**   :alice!u@h PRIVMSG bob :hi   → prefix=alice!u@h, command=PRIVMSG, params=[bob], trailing=hi
*/

#include "Message.hpp"
#include <cctype>

/*
** parseMessage
** Parses one IRC line already stripped of its "\r\n" terminator.
** Converts the command to upper case (IRC commands are case-insensitive).
**
** Receives: 'line' — IRC line without "\r\n".
** Returns: populated Message struct; returns an empty Message if the line is empty.
*/
Message parseMessage(const std::string& line)
{
	Message     msg;
	std::size_t pos = 0;

	if (line.empty())
		return msg;

	/* ── 1. Optional prefix ──────────────────────────────────────────────── */
	if (line[pos] == ':')
	{
		std::size_t spacePos = line.find(' ', pos);
		if (spacePos == std::string::npos)
			return msg;                     /* malformed line */
		msg.prefix = line.substr(1, spacePos - 1);
		pos = spacePos + 1;
		while (pos < line.size() && line[pos] == ' ')
			++pos;
	}

	if (pos >= line.size())
		return msg;

	/* ── 2. Command ──────────────────────────────────────────────────────── */
	std::size_t cmdEnd = line.find(' ', pos);
	if (cmdEnd == std::string::npos)
	{
		/* Line contains only the command, no parameters */
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

	/* ── 3. Parameters and trailing ──────────────────────────────────────── */
	while (pos < line.size())
	{
		/* Skip extra spaces */
		while (pos < line.size() && line[pos] == ' ')
			++pos;
		if (pos >= line.size())
			break;

		/* A ':' signals the trailing parameter (may contain spaces) */
		if (line[pos] == ':')
		{
			msg.trailing    = line.substr(pos + 1);
			msg.hasTrailing = true;
			break;
		}

		/* Normal parameter: runs until the next space */
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
