#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>
#include <vector>

/**
 * @struct Message
 * @brief  Parsed form of ONE raw IRC line (without the trailing "\r\n").
 *
 * This struct is the data contract between the two layers:
 *   - Person A (network) reads raw bytes, splits them on "\r\n" and hands a
 *     single complete line to the parser.
 *   - The parser turns that line into a Message.
 *   - Person B (protocol) receives this Message in CommandHandler::dispatch().
 *
 * Wire format of a raw IRC line:
 *
 *     [":" prefix SPACE] command [SPACE params] [SPACE ":" trailing]
 *
 * Example line:  "PRIVMSG #42 :Hello team!"
 *   command  = "PRIVMSG"
 *   params   = { "#42" }
 *   trailing = "Hello team!"   (everything after the first " :", spaces kept)
 */
struct Message
{
	std::string                 prefix;      // Optional source ("nick!user@host").
	                                         // Clients almost never send it; the
	                                         // SERVER adds it on outgoing messages.
	std::string                 command;     // The verb: "PASS", "NICK", "JOIN"...
	                                         // (could also be a 3-digit numeric).
	std::vector<std::string>    params;      // Middle parameters, in order, WITHOUT
	                                         // the trailing part. e.g. { "#chan" }.
	std::string                 trailing;    // Text after the first " :". It is the
	                                         // only field allowed to contain spaces.
	bool                        hasTrailing; // true if a " :" trailing was present
	                                         // (distinguishes "" from "no trailing").

	/** @brief Builds an empty message (hasTrailing = false). */
	Message() : prefix(), command(), params(), trailing(), hasTrailing(false) {}
};

/**
 * @brief Parse one raw IRC line (without its terminating "\r\n") into a Message.
 *
 * Wire format:  [":" prefix SP] command [SP params] [SP ":" trailing]
 *
 * @param line the raw input line, already stripped of "\r\n".
 * @return the populated Message struct; command is upper-cased.
 */
Message parseMessage(const std::string& line);

#endif // MESSAGE_HPP
