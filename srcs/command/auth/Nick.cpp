/*
** Nick.cpp — Handler do comando NICK.
**
** NICK <nickname>
** Define ou altera o nickname do cliente. Antes do registo é o segundo
** passo do handshake (PASS → NICK → USER). Após o registo, muda o nick
** e notifica todos os canais comuns.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Replies.hpp"
#include "Utils.hpp"
#include <vector>
#include <string>

/*
** CommandHandler::handleNick
** Define ou altera o nickname.
**
** Recebe: client — quem enviou NICK.
**         msg    — msg.params[0] deve conter o novo nickname.
**
** Respostas possíveis:
**   431 ERR_NONICKNAMEGIVEN  — sem argumento
**   432 ERR_ERRONEUSNICKNAME — nickname inválido (caracteres ilegais)
**   433 ERR_NICKNAMEINUSE    — outro cliente já usa este nick
**   broadcast NICK           — se já registado e nick aceite
**   (nenhuma se pré-registo e tudo OK)
*/
void CommandHandler::handleNick(Client& client, const Message& msg)
{
	/* Nick placeholder enquanto não registado */
	std::string oldNick = client.getNickname().empty() ? "*" : client.getNickname();

	/* Sem argumento */
	if (msg.params.empty())
	{
		_server.sendToClient(client.getFd(), ERR_NONICKNAMEGIVEN(oldNick));
		return;
	}

	const std::string& newNick = msg.params[0];

	/* Validar formato */
	if (!Utils::isValidNickname(newNick))
	{
		_server.sendToClient(client.getFd(),
		                     ERR_ERRONEUSNICKNAME(oldNick, newNick));
		return;
	}

	/* Verificar unicidade (case-sensitive conforme RFC 1459 client mode) */
	Client* existing = _server.getClientByNick(newNick);
	if (existing && existing->getFd() != client.getFd())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NICKNAMEINUSE(oldNick, newNick));
		return;
	}

	/* ── Já registado: mudança de nick em runtime ─────────────────────── */
	if (client.isRegistered())
	{
		std::string oldPrefix = client.getPrefix();
		client.setNickname(newNick);

		/* Notificar o próprio cliente */
		std::string nickMsg = ":" + oldPrefix + " NICK :" + newNick + "\r\n";
		_server.sendToClient(client.getFd(), nickMsg);

		/* Notificar todos os canais onde o cliente está (sem duplicados) */
		std::vector<std::string> chans = _server.getClientChannels(client.getFd());
		for (std::size_t i = 0; i < chans.size(); ++i)
			_server.broadcastToChannel(chans[i], nickMsg, client.getFd());

		return;
	}

	/* ── Pré-registo: só guardar o nick ──────────────────────────────── */
	client.setNickname(newNick);

	/* Verificar se o registo está completo agora */
	if (client.hasReceivedPass() && !client.getUsername().empty())
	{
		client.setRegistered(true);
		_server.sendToClient(client.getFd(), RPL_WELCOME(newNick));
		_server.sendToClient(client.getFd(), RPL_YOURHOST(newNick));
		_server.sendToClient(client.getFd(), RPL_CREATED(newNick));
		_server.sendToClient(client.getFd(), RPL_MYINFO(newNick));
	}
}
