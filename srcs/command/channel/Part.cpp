/*
** Part.cpp — Handler for the PART command.
**
** PART <channel>[,<channel>...] [:<reason>]
** Makes the client leave one or more channels. Broadcasts the departure to
** all members and destroys the channel if it becomes empty.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Replies.hpp"
#include "Utils.hpp"
#include <vector>
#include <string>

/*
** CommandHandler::handlePart
** Processes PART for each channel in the comma-separated list.
**
** Receives: client — who sent PART.
**           msg    — params[0]=channel list, trailing=reason (optional).
**
** Possible replies:
**   461 ERR_NEEDMOREPARAMS — no argument
**   403 ERR_NOSUCHCHANNEL  — channel does not exist
**   442 ERR_NOTONCHANNEL   — client is not in the channel
**   PART broadcast on success
*/
void CommandHandler::handlePart(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	if (msg.params.empty())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NEEDMOREPARAMS(client.getNickname(), "PART"));
		return;
	}

	std::string reason = msg.hasTrailing ? msg.trailing : client.getNickname();
	std::vector<std::string> channels = Utils::split(msg.params[0], ',');

	for (std::size_t i = 0; i < channels.size(); ++i)
	{
		const std::string& chanName = channels[i];

		Channel* ch = _server.getChannel(chanName);
		if (!ch)
		{
			_server.sendToClient(client.getFd(),
			                     ERR_NOSUCHCHANNEL(client.getNickname(), chanName));
			continue;
		}

		if (!ch->isMember(client.getFd()))
		{
			_server.sendToClient(client.getFd(),
			                     ERR_NOTONCHANNEL(client.getNickname(), chanName));
			continue;
		}

		/* Broadcast PART to everyone (including the leaving member, before removing) */
		std::string partMsg = ":" + client.getPrefix()
		                    + " PART " + chanName + " :" + reason + "\r\n";
		_server.broadcastToChannel(chanName, partMsg, -1);

		/* Remove from channel and destroy it if now empty */
		ch->removeMember(client.getFd());
		_server.removeChannelIfEmpty(chanName);
	}
}
