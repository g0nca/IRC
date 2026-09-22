/*
** User.cpp — Handler for the USER command.
**
** USER <username> <hostname> <servername> :<realname>
** Last step of the handshake (PASS → NICK → USER). Sets the username and
** realname of the client. If PASS and NICK are already done, completes the
** registration and sends the welcome replies (001–004).
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handleUser
** Processes the USER command and, if the handshake is complete, registers the client.
**
** Receives: client — who sent USER.
**           msg    — params[0]=username, trailing=realname (required).
**
** Possible replies:
**   462 ERR_ALREADYREGISTERED — already registered
**   461 ERR_NEEDMOREPARAMS    — missing parameters
**   001–004 RPL_WELCOME…      — registration completed successfully
*/
void CommandHandler::handleUser(Client& client, const Message& msg)
{
	/* Already registered: USER cannot be repeated */
	if (client.isRegistered())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_ALREADYREGISTERED(client.getNickname()));
		return;
	}

	/* Requires: username (params[0]) and realname (trailing) */
	if (msg.params.empty() || !msg.hasTrailing)
	{
		std::string nick = client.getNickname().empty() ? "*" : client.getNickname();
		_server.sendToClient(client.getFd(), ERR_NEEDMOREPARAMS(nick, "USER"));
		return;
	}

	client.setUsername(msg.params[0]);
	client.setRealname(msg.trailing);

	/* Check if registration is complete (PASS + NICK + USER) */
	if (client.hasReceivedPass() && !client.getNickname().empty())
	{
		client.setRegistered(true);
		const std::string& nick = client.getNickname();
		_server.sendToClient(client.getFd(), RPL_WELCOME(nick));
		_server.sendToClient(client.getFd(), RPL_YOURHOST(nick));
		_server.sendToClient(client.getFd(), RPL_CREATED(nick));
		_server.sendToClient(client.getFd(), RPL_MYINFO(nick));
	}
}
