/*
** Topic.cpp — Handler do comando TOPIC.
**
** TOPIC <channel> [:<topic>]
** Sem trailing: consulta o tópico actual (331 ou 332).
** Com trailing:  define um novo tópico (ou limpa se vazio), respeitando +t.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handleTopic
** Consulta ou altera o tópico do canal.
**
** Recebe: client — quem enviou TOPIC.
**         msg    — params[0]=canal, trailing=novo tópico (opcional).
**
** Respostas possíveis:
**   461 ERR_NEEDMOREPARAMS  — sem canal
**   403 ERR_NOSUCHCHANNEL   — canal não existe
**   442 ERR_NOTONCHANNEL    — não está no canal
**   482 ERR_CHANOPRIVSNEEDED — +t activo e não é operador
**   331 RPL_NOTOPIC         — consulta sem tópico definido
**   332 RPL_TOPIC           — consulta com tópico definido
**   TOPIC broadcast         — definição de tópico bem-sucedida
*/
void CommandHandler::handleTopic(Client& client, const Message& msg)
{
	if (!requireRegistered(client))
		return;

	if (msg.params.empty())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_NEEDMOREPARAMS(client.getNickname(), "TOPIC"));
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

	if (!ch->isMember(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_NOTONCHANNEL(nick, chanName));
		return;
	}

	/* ── Consulta: sem trailing ──────────────────────────────────────── */
	if (!msg.hasTrailing)
	{
		if (ch->hasTopic())
			_server.sendToClient(client.getFd(), RPL_TOPIC(nick, chanName, ch->getTopic()));
		else
			_server.sendToClient(client.getFd(), RPL_NOTOPIC(nick, chanName));
		return;
	}

	/* ── Definição: com trailing ─────────────────────────────────────── */
	/* Se +t activo, só operadores podem alterar */
	if (ch->isTopicRestricted() && !ch->isOperator(client.getFd()))
	{
		_server.sendToClient(client.getFd(), ERR_CHANOPRIVSNEEDED(nick, chanName));
		return;
	}

	ch->setTopic(msg.trailing);

	/* Broadcast da mudança de tópico a todos os membros */
	std::string topicMsg = ":" + client.getPrefix()
	                     + " TOPIC " + chanName + " :" + msg.trailing + "\r\n";
	_server.broadcastToChannel(chanName, topicMsg, -1);
}
