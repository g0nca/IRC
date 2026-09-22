/*
** Join.cpp — Handler for the JOIN command.
**
** JOIN <channel>[,<channel>...] [<key>[,<key>...]]
** Makes the client join one or more channels. The first member becomes an
** operator. Checks modes +i (invite-only), +k (key) and +l (limit).
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Replies.hpp"
#include "Utils.hpp"
#include <string>
#include <vector>

/*
** buildNamesList
** Builds the names string for the 353 reply (NAMREPLY).
** Operators are prefixed with '@'.
** Receives: channel, server (to look up nicks).
** Returns: string "[@]nick [[@]nick ...]".
*/
static std::string buildNamesList(Channel* ch, Server& server)
{
	std::string names;
	const std::set<int>& members = ch->getMembers();
	for (std::set<int>::const_iterator it = members.begin();
	     it != members.end(); ++it)
	{
		Client* m = server.getClientByFd(*it);
		if (!m)
			continue;
		if (!names.empty())
			names += ' ';
		if (ch->isOperator(*it))
			names += '@';
		names += m->getNickname();
	}
	return names;
}

/*
** CommandHandler::handleJoin
** Processes JOIN for each channel in the comma-separated list.
**
** Receives: client — who sent JOIN.
**           msg    — params[0]=channel list, params[1]=key list (optional).
**
** Possible replies:
**   461 ERR_NEEDMOREPARAMS — no argument
**   403 ERR_NOSUCHCHANNEL  — invalid channel name
**   473 ERR_INVITEONLYCHAN — channel is +i and client was not invited
**   475 ERR_BADCHANNELKEY  — channel is +k and key is wrong or missing
**   471 ERR_CHANNELISFULL  — channel is +l and full
**   JOIN broadcast + 353 + 366 on success
*/
void CommandHandler::handleJoin(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	if (msg.params.empty())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NEEDMOREPARAMS(client.getNickname(), "JOIN"));
		return;
	}

	/* Split channel list and key list by ',' */
	std::vector<std::string> channels = Utils::split(msg.params[0], ',');
	std::vector<std::string> keys;
	if (msg.params.size() >= 2)
		keys = Utils::split(msg.params[1], ',');

	for (std::size_t i = 0; i < channels.size(); ++i)
	{
		const std::string& chanName = channels[i];
		std::string        key      = (i < keys.size()) ? keys[i] : "";

		/* Validate channel name */
		if (!Utils::isValidChannelName(chanName))
		{
			_server.sendToClient(client.getFd(),
			                     ERR_NOSUCHCHANNEL(client.getNickname(), chanName));
			continue;
		}

		/* Already a member: ignore */
		Channel* ch = _server.getChannel(chanName);
		if (ch && ch->isMember(client.getFd()))
			continue;

		/* Channel may not exist yet — check modes before creating it */
		if (ch)
		{
			/* Mode +i: invite-only */
			if (ch->isInviteOnly() && !ch->isInvited(client.getFd()))
			{
				_server.sendToClient(client.getFd(),
				                     ERR_INVITEONLYCHAN(client.getNickname(), chanName));
				continue;
			}

			/* Mode +k: key required */
			if (ch->hasKey() && ch->getKey() != key)
			{
				_server.sendToClient(client.getFd(),
				                     ERR_BADCHANNELKEY(client.getNickname(), chanName));
				continue;
			}

			/* Mode +l: member limit */
			if (ch->hasUserLimit() && ch->memberCount() >= ch->getUserLimit())
			{
				_server.sendToClient(client.getFd(),
				                     ERR_CHANNELISFULL(client.getNickname(), chanName));
				continue;
			}
		}

		/* Create the channel if it did not exist */
		ch = _server.getOrCreateChannel(chanName);
		bool firstMember = ch->isEmpty();

		ch->addMember(client.getFd());

		/* First member becomes operator */
		if (firstMember)
			ch->addOperator(client.getFd());

		/* Remove from invite list after joining */
		ch->removeInvite(client.getFd());

		/* Broadcast JOIN to all members (including the new one) */
		std::string joinMsg = ":" + client.getPrefix() + " JOIN " + chanName + "\r\n";
		_server.broadcastToChannel(chanName, joinMsg, -1);

		/* Send the topic to the new member */
		if (ch->hasTopic())
			_server.sendToClient(client.getFd(),
			                     RPL_TOPIC(client.getNickname(), chanName, ch->getTopic()));
		else
			_server.sendToClient(client.getFd(),
			                     RPL_NOTOPIC(client.getNickname(), chanName));

		/* Send the names list (353 + 366) */
		std::string names = buildNamesList(ch, _server);
		_server.sendToClient(client.getFd(),
		                     RPL_NAMREPLY(client.getNickname(), chanName, names));
		_server.sendToClient(client.getFd(),
		                     RPL_ENDOFNAMES(client.getNickname(), chanName));
	}
}
