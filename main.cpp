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

#include "Server.hpp"

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

	try
	{
		Server server(port, password);
		server.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}

	return 0;
}
