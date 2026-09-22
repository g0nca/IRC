/*
** Notice.cpp — Handler for the NOTICE command.
**
** NOTICE <target> :<text>
** Identical to PRIVMSG but NEVER generates automatic error replies
** (a fundamental IRC protocol rule — prevents loops between bots).
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"

/*
** CommandHandler::handleNotice
** Forwards 'text' to 'target' (channel or nick) without generating errors.
** If the target does not exist, the message is silently discarded.
**
** Receives: client — who sent NOTICE.
**           msg    — params[0]=target, trailing=text.
*/
void CommandHandler::handleNotice(Client& client, const Message& msg)
{
	if (!client.isRegistered())
		return;     /* no error reply before registration */

	/* No target or no text: discard silently */
	if (msg.params.empty() || !msg.hasTrailing || msg.trailing.empty())
		return;

	const std::string& target = msg.params[0];
	const std::string& text   = msg.trailing;

	std::string fullMsg = ":" + client.getPrefix()
	                    + " NOTICE " + target + " :" + text + "\r\n";

	/* ── Channel ─────────────────────────────────────────────────────────── */
	if (!target.empty() && target[0] == '#')
	{
		Channel* ch = _server.getChannel(target);
		if (!ch)
			return;                         /* no protocol error */
		_server.broadcastToChannel(target, fullMsg, client.getFd());
		return;
	}

	/* ── Nick ────────────────────────────────────────────────────────────── */
	Client* dest = _server.getClientByNick(target);
	if (!dest)
		return;                             /* no protocol error */
	_server.sendToClient(dest->getFd(), fullMsg);
}
