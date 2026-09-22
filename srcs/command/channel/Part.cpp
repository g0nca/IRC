/*
** Part.cpp — Handler do comando PART.
**
** PART <channel>[,<channel>...] [:<reason>]
** Faz o cliente sair de um ou mais canais. Difunde a saída a todos os
** membros e destrói o canal se ficar vazio.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Replies.hpp"
#include "Utils.hpp"
#include <vector>
#include <string>

/*
** CommandHandler::handlePart
** Processa PART para cada canal da lista separada por vírgulas.
**
** Recebe: client — quem enviou PART.
**         msg    — params[0]=lista de canais, trailing=razão (opcional).
**
** Respostas possíveis:
**   461 ERR_NEEDMOREPARAMS — sem argumento
**   403 ERR_NOSUCHCHANNEL  — canal não existe
**   442 ERR_NOTONCHANNEL   — cliente não está no canal
**   PART broadcast em caso de sucesso
*/
void CommandHandler::handlePart(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	if (msg.params.empty())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NEEDMOREPARAMS(client.getNickname(), "PART"));
		return;
	}

	std::string reason = msg.hasTrailing ? msg.trailing : client.getNickname();
	std::vector<std::string> channels = Utils::split(msg.params[0], ',');

	for (std::size_t i = 0; i < channels.size(); ++i)
	{
		const std::string& chanName = channels[i];

		Channel* ch = _server.getChannel(chanName);
		if (!ch)
		{
			_server.sendToClient(client.getFd(),
			                     ERR_NOSUCHCHANNEL(client.getNickname(), chanName));
			continue;
		}

		if (!ch->isMember(client.getFd()))
		{
			_server.sendToClient(client.getFd(),
			                     ERR_NOTONCHANNEL(client.getNickname(), chanName));
			continue;
		}

		/* Broadcast PART a todos (incluindo quem sai, antes de remover) */
		std::string partMsg = ":" + client.getPrefix()
		                    + " PART " + chanName + " :" + reason + "\r\n";
		_server.broadcastToChannel(chanName, partMsg, -1);

		/* Remover do canal e destruir se vazio */
		ch->removeMember(client.getFd());
		_server.removeChannelIfEmpty(chanName);
	}
}
