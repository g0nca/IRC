/*
** Notice.cpp — Handler do comando NOTICE.
**
** NOTICE <target> :<text>
** Idêntico ao PRIVMSG mas NUNCA gera respostas de erro automáticas
** (regra fundamental do protocolo IRC — evita loops entre bots).
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"

/*
** CommandHandler::handleNotice
** Encaminha 'text' para 'target' (canal ou nick) sem gerar erros.
** Se o destino não existir, a mensagem é simplesmente descartada.
**
** Recebe: client — quem enviou NOTICE.
**         msg    — params[0]=target, trailing=texto.
*/
void CommandHandler::handleNotice(Client& client, const Message& msg)
{
	if (!client.isRegistered())
		return;     /* sem resposta de erro antes do registo */

	/* Sem destino ou sem texto: descarta silenciosamente */
	if (msg.params.empty() || !msg.hasTrailing || msg.trailing.empty())
		return;

	const std::string& target = msg.params[0];
	const std::string& text   = msg.trailing;

	std::string fullMsg = ":" + client.getPrefix()
	                    + " NOTICE " + target + " :" + text + "\r\n";

	/* ── Canal ───────────────────────────────────────────────────────── */
	if (!target.empty() && target[0] == '#')
	{
		Channel* ch = _server.getChannel(target);
		if (!ch)
			return;                         /* sem erro de protocolo */
		_server.broadcastToChannel(target, fullMsg, client.getFd());
		return;
	}

	/* ── Nick ────────────────────────────────────────────────────────── */
	Client* dest = _server.getClientByNick(target);
	if (!dest)
		return;                             /* sem erro de protocolo */
	_server.sendToClient(dest->getFd(), fullMsg);
}
