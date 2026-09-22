/*
** Kick.cpp — Handler for the KICK command.
**
** KICK <channel> <nick> [:<reason>]
** Removes 'nick' from the channel. Only operators may use this command.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handleKick
** Kicks a user from the channel.
**
** Receives: client — who sent KICK (must be an operator).
**           msg    — params[0]=channel, params[1]=nick to kick,
**                    trailing=reason (optional).
**
** Possible replies:
**   461 ERR_NEEDMOREPARAMS    — missing parameters
**   403 ERR_NOSUCHCHANNEL    — channel does not exist
**   442 ERR_NOTONCHANNEL     — the kicker is not in the channel
**   482 ERR_CHANOPRIVSNEEDED — not an operator
**   401 ERR_NOSUCHNICK       — nick does not exist on the server
**   441 ERR_USERNOTINCHANNEL — nick is not in the channel
**   KICK broadcast on success
*/
void CommandHandler::handleKick(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	if (msg.params.size() < 2)
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NEEDMOREPARAMS(client.getNickname(), "KICK"));
		return;
	}

	const std::string& chanName   = msg.params[0];
	const std::string& targetNick = msg.params[1];
	const std::string& nick       = client.getNickname();
	std::string        reason     = msg.hasTrailing ? msg.trailing : nick;

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

	if (!ch->isOperator(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_CHANOPRIVSNEEDED(nick, chanName));
		return;
	}

	Client* target = _server.getClientByNick(targetNick);
	if (!target)
	{
		_server.sendToClient(client.getFd(), ERR_NOSUCHNICK(nick, targetNick));
		return;
	}

	if (!ch->isMember(target->getFd()))
	{
		_server.sendToClient(client.getFd(),
		                     ERR_USERNOTINCHANNEL(nick, targetNick, chanName));
		return;
	}

	/* Broadcast KICK before removing the member */
	std::string kickMsg = ":" + client.getPrefix()
	                    + " KICK " + chanName + " " + targetNick + " :" + reason + "\r\n";
	_server.broadcastToChannel(chanName, kickMsg, -1);

	ch->removeMember(target->getFd());
	_server.removeChannelIfEmpty(chanName);
}
