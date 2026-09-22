/*
** Privmsg.cpp — Handler do comando PRIVMSG.
**
** PRIVMSG <target> :<text>
** Envia uma mensagem a um nick ou a um canal. Gera erros se o destino
** não existir ou se a mensagem/destino estiverem ausentes.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handlePrivmsg
** Encaminha 'text' para 'target' (canal ou nick).
**
** Recebe: client — quem enviou PRIVMSG.
**         msg    — params[0]=target, trailing=texto.
**
** Respostas possíveis:
**   411 ERR_NORECIPIENT   — sem destino
**   412 ERR_NOTEXTTOSEND  — sem texto
**   401 ERR_NOSUCHNICK    — nick não encontrado
**   403 ERR_NOSUCHCHANNEL — canal não encontrado
*/
void CommandHandler::handlePrivmsg(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	const std::string& nick = client.getNickname();

	/* Sem destino */
	if (msg.params.empty())
	{
		_server.sendToClient(client.getFd(), ERR_NORECIPIENT(nick, "PRIVMSG"));
		return;
	}

	/* Sem texto */
	if (!msg.hasTrailing || msg.trailing.empty())
	{
		_server.sendToClient(client.getFd(), ERR_NOTEXTTOSEND(nick));
		return;
	}

	const std::string& target = msg.params[0];
	const std::string& text   = msg.trailing;

	std::string fullMsg = ":" + client.getPrefix()
	                    + " PRIVMSG " + target + " :" + text + "\r\n";

	/* ── Canal ───────────────────────────────────────────────────────── */
	if (!target.empty() && target[0] == '#')
	{
		Channel* ch = _server.getChannel(target);
		if (!ch)
		{
			_server.sendToClient(client.getFd(), ERR_NOSUCHCHANNEL(nick, target));
			return;
		}
		/* Enviar a todos os membros menos ao remetente */
		_server.broadcastToChannel(target, fullMsg, client.getFd());
		return;
	}

	/* ── Nick ────────────────────────────────────────────────────────── */
	Client* dest = _server.getClientByNick(target);
	if (!dest)
	{
		_server.sendToClient(client.getFd(), ERR_NOSUCHNICK(nick, target));
		return;
	}
	_server.sendToClient(dest->getFd(), fullMsg);
}
