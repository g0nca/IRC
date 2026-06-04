/* ************************************************************************** */
/*                                                                            */
/*   ft_irc - main.cpp                                                        */
/*                                                                            */
/*   Entry point of the IRC server.                                           */
/*                                                                            */
/*   Usage:   ./ircserv <port> <password>                                     */
/*       port      : TCP port the server listens on (1..65535).               */
/*       password  : connection password every client must send via PASS.     */
/*                                                                            */
/*   --------------------------------------------------------------------     */
/*   TEAM CONTRACT (the "meeting point" between the two developers)           */
/*   --------------------------------------------------------------------     */
/*   Person A (you)        -> NETWORK layer:  Server + Client classes.        */
/*                            Owns the socket, the single poll() loop,        */
/*                            recv()/send(), buffering and partial packets.   */
/*   Person B (teammate)   -> PROTOCOL layer: CommandHandler + Channel.       */
/*                            Owns parsing, auth, channels and commands.      */
/*                                                                            */
/*   The handoff between the two layers is a SINGLE function call:            */
/*                                                                            */
/*       CommandHandler::dispatch(Client& client, const Message& msg);        */
/*                                                                            */
/*   The demoContract() function below shows, HARD-CODED, the three pieces    */
/*   of data that cross that boundary, so both developers can build and test  */
/*   their side independently (A with a stub dispatcher, B with a mock        */
/*   server). Delete demoContract() once the real Server is wired in.         */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <cstdlib>      // std::strtol
#include <cstddef>      // std::size_t
#include <string>

#include "include/Server.hpp"   // pulls in Client.hpp, Channel.hpp, CommandHandler.hpp, Message.hpp
#include "include/Replies.hpp"  // numeric reply builders (used by Person B)

/**
 * @brief Make a raw IRC string printable by showing \r and \n as visible text.
 *        (Used only by the demo so the CRLF terminators are easy to see.)
 */
static std::string visible(const std::string& s)
{
	std::string out;
	for (std::size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == '\r')
			out += "\\r";
		else if (s[i] == '\n')
			out += "\\n";
		else
			out += s[i];
	}
	return out;
}

/**
 * @brief Validate and convert the <port> argument.
 * @param arg  the raw argv string.
 * @param port out: the parsed port on success.
 * @return true if arg is an integer in [1, 65535].
 */
static bool parsePort(const std::string& arg, int& port)
{
	if (arg.empty())
		return false;

	char* end = 0;
	long  value = std::strtol(arg.c_str(), &end, 10);

	if (*end != '\0')               // there were non-digit characters
		return false;
	if (value < 1 || value > 65535) // outside the valid TCP port range
		return false;

	port = static_cast<int>(value);
	return true;
}

/**
 * @brief HARD-CODED demonstration of the A <-> B contract.
 *
 * Shows the exact data that crosses the boundary for the command
 * "PRIVMSG #42 :Hello team!":
 *   [1] the raw bytes that arrive on the socket   (Person A receives)
 *   [2] the parsed Message handed to the handler   (Person B receives)
 *   [3] the reply bytes to send back to members    (Person A sends)
 */
static void demoContract(void)
{
	std::cout << "\n========================================================\n";
	std::cout << " CONTRACT DEMO - what crosses the A <-> B boundary\n";
	std::cout << "========================================================\n";

	// [1] RAW INPUT - Person A reads this from recv(). In real life it may
	//     arrive in several pieces; A must rebuild it and split on "\r\n".
	std::string rawInput = "PRIVMSG #42 :Hello team!\r\n";
	std::cout << "\n[1] RAW INPUT  (Person A reads from the socket):\n";
	std::cout << "    \"" << visible(rawInput) << "\"\n";

	// [2] PARSED MESSAGE - what Person B receives via dispatch().
	//     Built by hand here to document the exact shape of Message.
	Message msg;
	msg.prefix      = "";              // clients send no prefix
	msg.command     = "PRIVMSG";
	msg.params.push_back("#42");       // the target channel
	msg.trailing    = "Hello team!";   // text after the first " :"
	msg.hasTrailing = true;

	std::cout << "\n[2] PARSED Message  (Person B receives this struct):\n";
	std::cout << "    command  = \"" << msg.command << "\"\n";
	std::cout << "    params   = [";
	for (std::size_t i = 0; i < msg.params.size(); ++i)
		std::cout << "\"" << msg.params[i] << "\""
		          << (i + 1 < msg.params.size() ? ", " : "");
	std::cout << "]\n";
	std::cout << "    trailing = \"" << msg.trailing << "\"  (present="
	          << (msg.hasTrailing ? "true" : "false") << ")\n";

	// [3] REPLY - what Person B builds and Person A must send to every member
	//     of #42. The server prepends the sender's "nick!user@host" prefix.
	std::string reply = ":alice!alice@localhost PRIVMSG #42 :Hello team!\r\n";
	std::cout << "\n[3] REPLY  (Person B builds it, Person A sends via send()):\n";
	std::cout << "    \"" << visible(reply) << "\"\n";
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << (argc > 0 ? argv[0] : "./ircserv")
		          << " <port> <password>\n";
		return 1;
	}

	int port = 0;
	if (!parsePort(argv[1], port))
	{
		std::cerr << "Error: <port> must be an integer in [1, 65535].\n";
		return 1;
	}

	std::string password = argv[2];
	if (password.empty())
	{
		std::cerr << "Error: <password> must not be empty.\n";
		return 1;
	}

	std::cout << "ircserv ready to start on port " << port
	          << " (password defined, " << password.size() << " chars).\n";

	// HARD-CODED contract demonstration. Remove this call once the real
	// Server is implemented and wired in below.
	demoContract();

	// ===== REAL ENTRY POINT (uncomment when Person A's Server is ready) =====
	// Server server(port, password);
	// server.run();
	// ========================================================================

	return 0;
}
