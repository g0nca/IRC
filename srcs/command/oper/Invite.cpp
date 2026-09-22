/*
** Invite.cpp — Handler do comando INVITE.
**
** INVITE <nick> <channel>
** Convida um utilizador para um canal. Relevante com modo +i (invite-only).
** Apenas operadores podem convidar quando o canal é +i.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handleInvite
** Convida 'nick' para 'channel'.
**
** Recebe: client — quem enviou INVITE (deve estar no canal).
**         msg    — params[0]=nick a convidar, params[1]=canal.
**
** Respostas possíveis:
**   461 ERR_NEEDMOREPARAMS    — faltam parâmetros
**   401 ERR_NOSUCHNICK        — nick não existe
**   403 ERR_NOSUCHCHANNEL    — canal não existe
**   442 ERR_NOTONCHANNEL     — o executor não está no canal
**   482 ERR_CHANOPRIVSNEEDED — canal +i e não é operador
**   443 ERR_USERONCHANNEL    — o convidado já está no canal
**   341 RPL_INVITING         — ao executor (confirmação)
**   INVITE privado            — ao convidado
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

	/* Verificar que o destino existe */
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

	/* Executor deve estar no canal */
	if (!ch->isMember(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_NOTONCHANNEL(nick, chanName));
		return;
	}

	/* Se +i, apenas operadores podem convidar */
	if (ch->isInviteOnly() && !ch->isOperator(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_CHANOPRIVSNEEDED(nick, chanName));
		return;
	}

	/* Já está no canal */
	if (ch->isMember(target->getFd()))
	{
		_server.sendToClient(client.getFd(),
		                     ERR_USERONCHANNEL(nick, targetNick, chanName));
		return;
	}

	/* Registar o convite */
	ch->addInvite(target->getFd());

	/* Confirmação ao executor */
	_server.sendToClient(client.getFd(), RPL_INVITING(nick, targetNick, chanName));

	/* Notificação ao convidado */
	std::string inviteMsg = ":" + client.getPrefix()
	                      + " INVITE " + targetNick + " " + chanName + "\r\n";
	_server.sendToClient(target->getFd(), inviteMsg);
}
