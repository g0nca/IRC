/*
** Invite.cpp — Handler for the INVITE command.
**
** INVITE <nick> <channel>
** Invites a user to a channel. Relevant with mode +i (invite-only).
** Only operators may invite when the channel is +i.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handleInvite
** Invites 'nick' to 'channel'.
**
** Receives: client — who sent INVITE (must be in the channel).
**           msg    — params[0]=nick to invite, params[1]=channel.
**
** Possible replies:
**   461 ERR_NEEDMOREPARAMS    — missing parameters
**   401 ERR_NOSUCHNICK        — nick does not exist
**   403 ERR_NOSUCHCHANNEL    — channel does not exist
**   442 ERR_NOTONCHANNEL     — the inviter is not in the channel
**   482 ERR_CHANOPRIVSNEEDED — channel is +i and inviter is not an operator
**   443 ERR_USERONCHANNEL    — the invitee is already in the channel
**   341 RPL_INVITING         — sent to the inviter (confirmation)
**   INVITE private message    — sent to the invitee
*/
void CommandHandler::handleInvite(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	if (msg.params.size() < 2)
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NEEDMOREPARAMS(client.getNickname(), "INVITE"));
		return;
	}

	const std::string& targetNick = msg.params[0];
	const std::string& chanName   = msg.params[1];
	const std::string& nick       = client.getNickname();

	/* Check that the target exists */
	Client* target = _server.getClientByNick(targetNick);
	if (!target)
	{
		_server.sendToClient(client.getFd(), ERR_NOSUCHNICK(nick, targetNick));
		return;
	}

	Channel* ch = _server.getChannel(chanName);
	if (!ch)
	{
		_server.sendToClient(client.getFd(), ERR_NOSUCHCHANNEL(nick, chanName));
		return;
	}

	/* Inviter must be in the channel */
	if (!ch->isMember(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_NOTONCHANNEL(nick, chanName));
		return;
	}

	/* If +i, only operators may invite */
	if (ch->isInviteOnly() && !ch->isOperator(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_CHANOPRIVSNEEDED(nick, chanName));
		return;
	}

	/* Already in the channel */
	if (ch->isMember(target->getFd()))
	{
		_server.sendToClient(client.getFd(),
		                     ERR_USERONCHANNEL(nick, targetNick, chanName));
		return;
	}

	/* Record the invite */
	ch->addInvite(target->getFd());

	/* Confirmation to the inviter */
	_server.sendToClient(client.getFd(), RPL_INVITING(nick, targetNick, chanName));

	/* Notification to the invitee */
	std::string inviteMsg = ":" + client.getPrefix()
	                      + " INVITE " + targetNick + " " + chanName + "\r\n";
	_server.sendToClient(target->getFd(), inviteMsg);
}
