/*
** Kick.cpp — Handler do comando KICK.
**
** KICK <channel> <nick> [:<reason>]
** Expulsa 'nick' do canal. Apenas operadores podem usar este comando.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handleKick
** Expulsa um utilizador do canal.
**
** Recebe: client — quem enviou KICK (deve ser operador).
**         msg    — params[0]=canal, params[1]=nick a expulsar,
**                  trailing=razão (opcional).
**
** Respostas possíveis:
**   461 ERR_NEEDMOREPARAMS    — faltam parâmetros
**   403 ERR_NOSUCHCHANNEL    — canal não existe
**   442 ERR_NOTONCHANNEL     — o executor não está no canal
**   482 ERR_CHANOPRIVSNEEDED — não é operador
**   401 ERR_NOSUCHNICK       — nick não existe no servidor
**   441 ERR_USERNOTINCHANNEL — nick não está no canal
**   KICK broadcast em caso de sucesso
*/
void CommandHandler::handleKick(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	if (msg.params.size() < 2)
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NEEDMOREPARAMS(client.getNickname(), "KICK"));
		return;
	}

	const std::string& chanName   = msg.params[0];
	const std::string& targetNick = msg.params[1];
	const std::string& nick       = client.getNickname();
	std::string        reason     = msg.hasTrailing ? msg.trailing : nick;

	Channel* ch = _server.getChannel(chanName);
	if (!ch)
	{
		_server.sendToClient(client.getFd(), ERR_NOSUCHCHANNEL(nick, chanName));
		return;
	}

	if (!ch->isMember(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_NOTONCHANNEL(nick, chanName));
		return;
	}

	if (!ch->isOperator(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_CHANOPRIVSNEEDED(nick, chanName));
		return;
	}

	Client* target = _server.getClientByNick(targetNick);
	if (!target)
	{
		_server.sendToClient(client.getFd(), ERR_NOSUCHNICK(nick, targetNick));
		return;
	}

	if (!ch->isMember(target->getFd()))
	{
		_server.sendToClient(client.getFd(),
		                     ERR_USERNOTINCHANNEL(nick, targetNick, chanName));
		return;
	}

	/* Broadcast KICK antes de remover */
	std::string kickMsg = ":" + client.getPrefix()
	                    + " KICK " + chanName + " " + targetNick + " :" + reason + "\r\n";
	_server.broadcastToChannel(chanName, kickMsg, -1);

	ch->removeMember(target->getFd());
	_server.removeChannelIfEmpty(chanName);
}
