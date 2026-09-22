/*
** User.cpp — Handler do comando USER.
**
** USER <username> <hostname> <servername> :<realname>
** Último passo do handshake (PASS → NICK → USER). Define o username e o
** realname do cliente. Se PASS e NICK já estiverem feitos, completa o
** registo e envia as boas-vindas (001–004).
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handleUser
** Processa o comando USER e, se o handshake estiver completo, regista o cliente.
**
** Recebe: client — quem enviou USER.
**         msg    — params[0]=username, trailing=realname (obrigatório).
**
** Respostas possíveis:
**   462 ERR_ALREADYREGISTERED — já registado
**   461 ERR_NEEDMOREPARAMS    — faltam parâmetros
**   001–004 RPL_WELCOME…      — registo completo com sucesso
*/
void CommandHandler::handleUser(Client& client, const Message& msg)
{
	/* Já registado: USER não pode repetir-se */
	if (client.isRegistered())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_ALREADYREGISTERED(client.getNickname()));
		return;
	}

	/* Precisa de: username (params[0]) e realname (trailing) */
	if (msg.params.empty() || !msg.hasTrailing)
	{
		std::string nick = client.getNickname().empty() ? "*" : client.getNickname();
		_server.sendToClient(client.getFd(), ERR_NEEDMOREPARAMS(nick, "USER"));
		return;
	}

	client.setUsername(msg.params[0]);
	client.setRealname(msg.trailing);

	/* Verificar se o registo está completo (PASS + NICK + USER) */
	if (client.hasReceivedPass() && !client.getNickname().empty())
	{
		client.setRegistered(true);
		const std::string& nick = client.getNickname();
		_server.sendToClient(client.getFd(), RPL_WELCOME(nick));
		_server.sendToClient(client.getFd(), RPL_YOURHOST(nick));
		_server.sendToClient(client.getFd(), RPL_CREATED(nick));
		_server.sendToClient(client.getFd(), RPL_MYINFO(nick));
	}
}
