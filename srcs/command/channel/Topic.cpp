/*
** Topic.cpp — Handler for the TOPIC command.
**
** TOPIC <channel> [:<topic>]
** Without trailing: queries the current topic (331 or 332).
** With trailing:    sets a new topic (or clears it if empty), honouring +t.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handleTopic
** Queries or changes the channel topic.
**
** Receives: client — who sent TOPIC.
**           msg    — params[0]=channel, trailing=new topic (optional).
**
** Possible replies:
**   461 ERR_NEEDMOREPARAMS   — no channel supplied
**   403 ERR_NOSUCHCHANNEL    — channel does not exist
**   442 ERR_NOTONCHANNEL     — not in the channel
**   482 ERR_CHANOPRIVSNEEDED — +t is active and client is not an operator
**   331 RPL_NOTOPIC          — query with no topic set
**   332 RPL_TOPIC            — query with a topic set
**   TOPIC broadcast          — topic change succeeded
*/
void CommandHandler::handleTopic(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	if (msg.params.empty())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NEEDMOREPARAMS(client.getNickname(), "TOPIC"));
		return;
	}

	const std::string& chanName = msg.params[0];
	const std::string& nick     = client.getNickname();

	Channel* ch = _server.getChannel(chanName);
	if (!ch)
	{
		_server.sendToClient(client.getFd(), ERR_NOSUCHCHANNEL(nick, chanName));
		return;
	}

	if (!ch->isMember(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_NOTONCHANNEL(nick, chanName));
		return;
	}

	/* ── Query: no trailing ──────────────────────────────────────────────── */
	if (!msg.hasTrailing)
	{
		if (ch->hasTopic())
			_server.sendToClient(client.getFd(), RPL_TOPIC(nick, chanName, ch->getTopic()));
		else
			_server.sendToClient(client.getFd(), RPL_NOTOPIC(nick, chanName));
		return;
	}

	/* ── Change: with trailing ───────────────────────────────────────────── */
	/* If +t is active, only operators may change the topic */
	if (ch->isTopicRestricted() && !ch->isOperator(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_CHANOPRIVSNEEDED(nick, chanName));
		return;
	}

	ch->setTopic(msg.trailing);

	/* Broadcast the topic change to all members */
	std::string topicMsg = ":" + client.getPrefix()
	                     + " TOPIC " + chanName + " :" + msg.trailing + "\r\n";
	_server.broadcastToChannel(chanName, topicMsg, -1);
}
