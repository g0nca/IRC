/*
** Mode.cpp — Handler for the MODE command.
**
** MODE <channel> [+/-modes [params...]]
** Queries or changes the modes of a channel. Only operators may change modes.
**
** Modes required by the subject:
**   +i / -i  invite-only      (no parameter)
**   +t / -t  topic restricted (no parameter)
**   +k <key> / -k  channel key (+k requires a key)
**   +o <nick> / -o <nick>   give/take operator
**   +l <n> / -l  user limit  (+l requires a number)
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Replies.hpp"
#include "Utils.hpp"
#include <cstdlib>      /* atoi */
#include <string>

/*
** CommandHandler::handleMode
** Parses the mode string and applies each mode, consuming parameters as needed.
** Broadcasts the modes that were actually applied.
**
** Receives: client — who sent MODE (must be an operator to change modes).
**           msg    — params[0]=channel, params[1]=mode string (optional),
**                    params[2..n]=mode parameters.
**
** Possible replies:
**   461 ERR_NEEDMOREPARAMS    — no channel supplied
**   403 ERR_NOSUCHCHANNEL    — channel does not exist
**   482 ERR_CHANOPRIVSNEEDED — not an operator
**   472 ERR_UNKNOWNMODE      — unknown mode character
**   324 RPL_CHANNELMODEIS    — mode query (no mode string supplied)
**   MODE broadcast           — modes applied successfully
*/
void CommandHandler::handleMode(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	if (msg.params.empty())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NEEDMOREPARAMS(client.getNickname(), "MODE"));
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

	/* ── Query: MODE #chan (no mode string) ──────────────────────────────── */
	if (msg.params.size() == 1)
	{
		_server.sendToClient(client.getFd(),
		                     RPL_CHANNELMODEIS(nick, chanName, ch->getModeString()));
		return;
	}

	/* ── Change: requires operator privilege ─────────────────────────────── */
	if (!ch->isOperator(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_CHANOPRIVSNEEDED(nick, chanName));
		return;
	}

	const std::string& modeStr = msg.params[1];
	bool        adding     = true;      /* '+' → true, '-' → false */
	std::size_t paramIdx   = 2;         /* index of the next parameter to consume */

	/* Accumulate applied modes and parameters for the broadcast */
	std::string appliedModes;
	std::string appliedParams;
	char        lastSign = 0;           /* last sign emitted in the result string */

	for (std::size_t i = 0; i < modeStr.size(); ++i)
	{
		char c = modeStr[i];

		if (c == '+') { adding = true;  continue; }
		if (c == '-') { adding = false; continue; }

		bool        applied = false;
		std::string param;

		switch (c)
		{
			/* ── +i / -i : invite-only ──────────────────────────────────── */
			case 'i':
				ch->setInviteOnly(adding);
				applied = true;
				break;

			/* ── +t / -t : topic restricted ─────────────────────────────── */
			case 't':
				ch->setTopicRestricted(adding);
				applied = true;
				break;

			/* ── +k <key> / -k : channel key ────────────────────────────── */
			case 'k':
				if (adding)
				{
					if (paramIdx >= msg.params.size())
						break;          /* no key supplied — skip */
					ch->setKey(msg.params[paramIdx]);
					param = msg.params[paramIdx];
					++paramIdx;
					applied = true;
				}
				else
				{
					ch->removeKey();
					applied = true;
				}
				break;

			/* ── +o <nick> / -o <nick> : operator ───────────────────────── */
			case 'o':
			{
				if (paramIdx >= msg.params.size())
					break;
				const std::string& targetNick = msg.params[paramIdx];
				++paramIdx;
				Client* targetClient = _server.getClientByNick(targetNick);
				if (!targetClient || !ch->isMember(targetClient->getFd()))
					break;              /* nick does not exist or is not in the channel */
				if (adding)
					ch->addOperator(targetClient->getFd());
				else
					ch->removeOperator(targetClient->getFd());
				param   = targetNick;
				applied = true;
				break;
			}

			/* ── +l <n> / -l : user limit ───────────────────────────────── */
			case 'l':
				if (adding)
				{
					if (paramIdx >= msg.params.size())
						break;
					int lim = std::atoi(msg.params[paramIdx].c_str());
					if (lim > 0)
					{
						ch->setUserLimit(static_cast<std::size_t>(lim));
						param = msg.params[paramIdx];
						applied = true;
					}
					++paramIdx;
				}
				else
				{
					ch->removeUserLimit();
					applied = true;
				}
				break;

			default:
				/* Unknown mode */
				_server.sendToClient(client.getFd(),
				                     ERR_UNKNOWNMODE(nick, std::string(1, c)));
				break;
		}

		/* Append to the applied mode string */
		if (applied)
		{
			char sign = adding ? '+' : '-';
			if (sign != lastSign)
			{
				appliedModes += sign;
				lastSign = sign;
			}
			appliedModes += c;
			if (!param.empty())
			{
				if (!appliedParams.empty())
					appliedParams += ' ';
				appliedParams += param;
			}
		}
	}

	/* Broadcast only if something was actually changed */
	if (!appliedModes.empty())
	{
		std::string modeMsg = ":" + client.getPrefix()
		                    + " MODE " + chanName + " " + appliedModes;
		if (!appliedParams.empty())
			modeMsg += " " + appliedParams;
		modeMsg += "\r\n";
		_server.broadcastToChannel(chanName, modeMsg, -1);
	}
}
