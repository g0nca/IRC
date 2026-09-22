/*
** Pass.cpp — Handler do comando PASS.
**
** PASS <password>
** Deve ser o primeiro comando enviado pelo cliente. Define se a password
** está correcta; sem PASS válido o registo não pode completar-se.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handlePass
** Valida a password fornecida contra a do servidor.
** Só pode ser chamado antes do registo estar completo.
**
** Recebe: client — quem enviou PASS.
**         msg    — msg.params[0] deve conter a password.
**
** Respostas possíveis:
**   462 ERR_ALREADYREGISTERED — já está registado
**   461 ERR_NEEDMOREPARAMS    — sem argumento
**   464 ERR_PASSWDMISMATCH    — password errada
**   (nenhuma, em caso de sucesso — o flag é guardado internamente)
*/
void CommandHandler::handlePass(Client& client, const Message& msg)
{
	/* Já registado: não pode re-registar */
	if (client.isRegistered())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_ALREADYREGISTERED(client.getNickname()));
		return;
	}

	/* Sem argumento */
	if (msg.params.empty())
	{
		std::string nick = client.getNickname().empty() ? "*" : client.getNickname();
		_server.sendToClient(client.getFd(), ERR_NEEDMOREPARAMS(nick, "PASS"));
		return;
	}

	/* Verificar a password */
	if (msg.params[0] != _server.getPassword())
	{
		std::string nick = client.getNickname().empty() ? "*" : client.getNickname();
		_server.sendToClient(client.getFd(), ERR_PASSWDMISMATCH(nick));
		return;
	}

	/* Password correcta: marcar o flag */
	client.setPassReceived(true);
}
