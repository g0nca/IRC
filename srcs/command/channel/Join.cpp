/*
** Join.cpp — Handler do comando JOIN.
**
** JOIN <channel>[,<channel>...] [<key>[,<key>...]]
** Faz o cliente entrar num ou mais canais. O primeiro a entrar torna-se
** operador. Verifica os modos +i (invite-only), +k (key) e +l (limit).
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Replies.hpp"
#include "Utils.hpp"
#include <string>
#include <vector>

/*
** buildNamesList
** Constrói a string de nomes para o reply 353 (NAMREPLY).
** Operadores são prefixados com '@'.
** Recebe: canal, servidor (para obter nicks).
** Devolve: string "[@]nick [[@]nick ...]".
*/
static std::string buildNamesList(Channel* ch, Server& server)
{
	std::string names;
	const std::set<int>& members = ch->getMembers();
	for (std::set<int>::const_iterator it = members.begin();
	     it != members.end(); ++it)
	{
		Client* m = server.getClientByFd(*it);
		if (!m)
			continue;
		if (!names.empty())
			names += ' ';
		if (ch->isOperator(*it))
			names += '@';
		names += m->getNickname();
	}
	return names;
}

/*
** CommandHandler::handleJoin
** Processa JOIN para cada canal da lista separada por vírgulas.
**
** Recebe: client — quem enviou JOIN.
**         msg    — params[0]=lista de canais, params[1]=lista de keys (opcional).
**
** Respostas possíveis:
**   461 ERR_NEEDMOREPARAMS — sem argumento
**   403 ERR_NOSUCHCHANNEL  — nome de canal inválido
**   473 ERR_INVITEONLYCHAN — canal +i e sem convite
**   475 ERR_BADCHANNELKEY  — canal +k e key errada/ausente
**   471 ERR_CHANNELISFULL  — canal +l e cheio
**   JOIN broadcast + 353 + 366 em caso de sucesso
*/
void CommandHandler::handleJoin(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	if (msg.params.empty())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NEEDMOREPARAMS(client.getNickname(), "JOIN"));
		return;
	}

	/* Dividir lista de canais e lista de keys por ',' */
	std::vector<std::string> channels = Utils::split(msg.params[0], ',');
	std::vector<std::string> keys;
	if (msg.params.size() >= 2)
		keys = Utils::split(msg.params[1], ',');

	for (std::size_t i = 0; i < channels.size(); ++i)
	{
		const std::string& chanName = channels[i];
		std::string        key      = (i < keys.size()) ? keys[i] : "";

		/* Validar nome do canal */
		if (!Utils::isValidChannelName(chanName))
		{
			_server.sendToClient(client.getFd(),
			                     ERR_NOSUCHCHANNEL(client.getNickname(), chanName));
			continue;
		}

		/* Se já está no canal, ignorar */
		Channel* ch = _server.getChannel(chanName);
		if (ch && ch->isMember(client.getFd()))
			continue;

		/* O canal pode não existir ainda — verificar modos antes de criar */
		if (ch)
		{
			/* Modo +i: invite-only */
			if (ch->isInviteOnly() && !ch->isInvited(client.getFd()))
			{
				_server.sendToClient(client.getFd(),
				                     ERR_INVITEONLYCHAN(client.getNickname(), chanName));
				continue;
			}

			/* Modo +k: key obrigatória */
			if (ch->hasKey() && ch->getKey() != key)
			{
				_server.sendToClient(client.getFd(),
				                     ERR_BADCHANNELKEY(client.getNickname(), chanName));
				continue;
			}

			/* Modo +l: limite de membros */
			if (ch->hasUserLimit() && ch->memberCount() >= ch->getUserLimit())
			{
				_server.sendToClient(client.getFd(),
				                     ERR_CHANNELISFULL(client.getNickname(), chanName));
				continue;
			}
		}

		/* Criar o canal se não existia */
		ch = _server.getOrCreateChannel(chanName);
		bool firstMember = ch->isEmpty();

		ch->addMember(client.getFd());

		/* Primeiro membro torna-se operador */
		if (firstMember)
			ch->addOperator(client.getFd());

		/* Remover da lista de convidados após entrar */
		ch->removeInvite(client.getFd());

		/* Broadcast JOIN a todos os membros (incluindo o novo) */
		std::string joinMsg = ":" + client.getPrefix() + " JOIN " + chanName + "\r\n";
		_server.broadcastToChannel(chanName, joinMsg, -1);

		/* Enviar tópico ao novo membro */
		if (ch->hasTopic())
			_server.sendToClient(client.getFd(),
			                     RPL_TOPIC(client.getNickname(), chanName, ch->getTopic()));
		else
			_server.sendToClient(client.getFd(),
			                     RPL_NOTOPIC(client.getNickname(), chanName));

		/* Enviar lista de nomes (353 + 366) */
		std::string names = buildNamesList(ch, _server);
		_server.sendToClient(client.getFd(),
		                     RPL_NAMREPLY(client.getNickname(), chanName, names));
		_server.sendToClient(client.getFd(),
		                     RPL_ENDOFNAMES(client.getNickname(), chanName));
	}
}
