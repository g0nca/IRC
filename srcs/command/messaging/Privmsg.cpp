/*
** Privmsg.cpp — Handler for the PRIVMSG command.
**
** PRIVMSG <target> :<text>
** Sends a message to a nick or a channel. Produces errors if the target
** does not exist or if the message or target is missing.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handlePrivmsg
** Forwards 'text' to 'target' (channel or nick).
**
** Receives: client — who sent PRIVMSG.
**           msg    — params[0]=target, trailing=text.
**
** Possible replies:
**   411 ERR_NORECIPIENT   — no target
**   412 ERR_NOTEXTTOSEND  — no text
**   401 ERR_NOSUCHNICK    — nick not found
**   403 ERR_NOSUCHCHANNEL — channel not found
*/
void CommandHandler::handlePrivmsg(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	const std::string& nick = client.getNickname();

	/* No target */
	if (msg.params.empty())
	{
		_server.sendToClient(client.getFd(), ERR_NORECIPIENT(nick, "PRIVMSG"));
		return;
	}

	/* No text */
	if (!msg.hasTrailing || msg.trailing.empty())
	{
		_server.sendToClient(client.getFd(), ERR_NOTEXTTOSEND(nick));
		return;
	}

	const std::string& target = msg.params[0];
	const std::string& text   = msg.trailing;

	std::string fullMsg = ":" + client.getPrefix()
	                    + " PRIVMSG " + target + " :" + text + "\r\n";

	/* ── Channel ─────────────────────────────────────────────────────────── */
	if (!target.empty() && target[0] == '#')
	{
		Channel* ch = _server.getChannel(target);
		if (!ch)
		{
			_server.sendToClient(client.getFd(), ERR_NOSUCHCHANNEL(nick, target));
			return;
		}
		/* Send to all members except the sender */
		_server.broadcastToChannel(target, fullMsg, client.getFd());
		return;
	}

	/* ── Nick ────────────────────────────────────────────────────────────── */
	Client* dest = _server.getClientByNick(target);
	if (!dest)
	{
		_server.sendToClient(client.getFd(), ERR_NOSUCHNICK(nick, target));
		return;
	}
	_server.sendToClient(dest->getFd(), fullMsg);
}
