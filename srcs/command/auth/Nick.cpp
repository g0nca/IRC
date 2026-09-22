/*
** Nick.cpp — Handler for the NICK command.
**
** NICK <nickname>
** Sets or changes the client's nickname. Before registration it is the second
** step of the handshake (PASS → NICK → USER). After registration, it changes
** the nick and notifies all shared channels.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Replies.hpp"
#include "Utils.hpp"
#include <vector>
#include <string>

/*
** CommandHandler::handleNick
** Sets or changes the nickname.
**
** Receives: client — who sent NICK.
**           msg    — msg.params[0] must contain the new nickname.
**
** Possible replies:
**   431 ERR_NONICKNAMEGIVEN  — no argument
**   432 ERR_ERRONEUSNICKNAME — invalid nickname (illegal characters)
**   433 ERR_NICKNAMEINUSE    — another client already uses this nick
**   broadcast NICK           — if already registered and nick accepted
**   (none if pre-registration and everything is OK)
*/
void CommandHandler::handleNick(Client& client, const Message& msg)
{
	/* Nick placeholder while not yet registered */
	std::string oldNick = client.getNickname().empty() ? "*" : client.getNickname();

	/* No argument */
	if (msg.params.empty())
	{
		_server.sendToClient(client.getFd(), ERR_NONICKNAMEGIVEN(oldNick));
		return;
	}

	const std::string& newNick = msg.params[0];

	/* Validate format */
	if (!Utils::isValidNickname(newNick))
	{
		_server.sendToClient(client.getFd(),
		                     ERR_ERRONEUSNICKNAME(oldNick, newNick));
		return;
	}

	/* Check uniqueness (case-sensitive as per RFC 1459 client mode) */
	Client* existing = _server.getClientByNick(newNick);
	if (existing && existing->getFd() != client.getFd())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NICKNAMEINUSE(oldNick, newNick));
		return;
	}

	/* ── Already registered: runtime nick change ──────────────────────── */
	if (client.isRegistered())
	{
		std::string oldPrefix = client.getPrefix();
		client.setNickname(newNick);

		/* Notify the client itself */
		std::string nickMsg = ":" + oldPrefix + " NICK :" + newNick + "\r\n";
		_server.sendToClient(client.getFd(), nickMsg);

		/* Notify all channels the client is in (no duplicates) */
		std::vector<std::string> chans = _server.getClientChannels(client.getFd());
		for (std::size_t i = 0; i < chans.size(); ++i)
			_server.broadcastToChannel(chans[i], nickMsg, client.getFd());

		return;
	}

	/* ── Pre-registration: just store the nick ──────────────────────────── */
	client.setNickname(newNick);

	/* Check if registration is now complete */
	if (client.hasReceivedPass() && !client.getUsername().empty())
	{
		client.setRegistered(true);
		_server.sendToClient(client.getFd(), RPL_WELCOME(newNick));
		_server.sendToClient(client.getFd(), RPL_YOURHOST(newNick));
		_server.sendToClient(client.getFd(), RPL_CREATED(newNick));
		_server.sendToClient(client.getFd(), RPL_MYINFO(newNick));
	}
}
