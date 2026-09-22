/*
** Mode.cpp — Handler do comando MODE.
**
** MODE <channel> [+/-modes [params...]]
** Consulta ou altera os modos de um canal. Apenas operadores podem alterar.
**
** Modos obrigatórios pelo subject:
**   +i / -i  invite-only      (sem parâmetro)
**   +t / -t  topic restricted (sem parâmetro)
**   +k <key> / -k  channel key (+k precisa de key)
**   +o <nick> / -o <nick>   give/take operator
**   +l <n> / -l  user limit  (+l precisa de número)
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
** Processa a string de modos e aplica cada um, consumindo parâmetros
** conforme necessário. Difunde os modos realmente aplicados.
**
** Recebe: client — quem enviou MODE (deve ser operador para alterar).
**         msg    — params[0]=canal, params[1]=modestring (opcional),
**                  params[2..n]=parâmetros dos modos.
**
** Respostas possíveis:
**   461 ERR_NEEDMOREPARAMS    — sem canal
**   403 ERR_NOSUCHCHANNEL    — canal não existe
**   482 ERR_CHANOPRIVSNEEDED — não é operador
**   472 ERR_UNKNOWNMODE      — carácter de modo desconhecido
**   324 RPL_CHANNELMODEIS    — consulta de modos (sem modestring)
**   MODE broadcast           — modos aplicados com sucesso
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

	/* ── Consulta: MODE #chan (sem modestring) ───────────────────────── */
	if (msg.params.size() == 1)
	{
		_server.sendToClient(client.getFd(),
		                     RPL_CHANNELMODEIS(nick, chanName, ch->getModeString()));
		return;
	}

	/* ── Alteração: requer privilégio de operador ────────────────────── */
	if (!ch->isOperator(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_CHANOPRIVSNEEDED(nick, chanName));
		return;
	}

	const std::string& modeStr = msg.params[1];
	bool        adding     = true;      /* '+' → true, '-' → false */
	std::size_t paramIdx   = 2;         /* índice do próximo parâmetro a consumir */

	/* Acumular modos e parâmetros aplicados para o broadcast */
	std::string appliedModes;
	std::string appliedParams;
	char        lastSign = 0;           /* último sinal emitido na string resultante */

	for (std::size_t i = 0; i < modeStr.size(); ++i)
	{
		char c = modeStr[i];

		if (c == '+') { adding = true;  continue; }
		if (c == '-') { adding = false; continue; }

		bool        applied = false;
		std::string param;

		switch (c)
		{
			/* ── +i / -i : invite-only ─────────────────────────────── */
			case 'i':
				ch->setInviteOnly(adding);
				applied = true;
				break;

			/* ── +t / -t : topic restricted ────────────────────────── */
			case 't':
				ch->setTopicRestricted(adding);
				applied = true;
				break;

			/* ── +k <key> / -k : channel key ───────────────────────── */
			case 'k':
				if (adding)
				{
					if (paramIdx >= msg.params.size())
						break;          /* sem key — ignorar */
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

			/* ── +o <nick> / -o <nick> : operator ──────────────────── */
			case 'o':
			{
				if (paramIdx >= msg.params.size())
					break;
				const std::string& targetNick = msg.params[paramIdx];
				++paramIdx;
				Client* targetClient = _server.getClientByNick(targetNick);
				if (!targetClient || !ch->isMember(targetClient->getFd()))
					break;              /* nick não existe ou não está no canal */
				if (adding)
					ch->addOperator(targetClient->getFd());
				else
					ch->removeOperator(targetClient->getFd());
				param   = targetNick;
				applied = true;
				break;
			}

			/* ── +l <n> / -l : user limit ──────────────────────────── */
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
				/* Modo desconhecido */
				_server.sendToClient(client.getFd(),
				                     ERR_UNKNOWNMODE(nick, std::string(1, c)));
				break;
		}

		/* Acrescentar à string de modos aplicados */
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

	/* Só difundir se algo foi realmente alterado */
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
