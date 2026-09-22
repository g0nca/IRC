/*
** CommandHandler.cpp — Dispatcher central de comandos IRC.
**
** dispatch() recebe um Client e um Message já parseado e invoca o handler
** correcto. Inclui também handleQuit (simples demais para ficheiro próprio)
** e requireRegistered (helper partilhado por todos os handlers).
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Replies.hpp"
#include <iostream>

/* ─── Constructor / Destructor ────────────────────────────────────────────── */

/*
** CommandHandler(Server& server)
** Recebe a referência ao servidor (o contrato). Nunca a usa durante
** a própria construção — é seguro mesmo que Server não esteja completo ainda.
** Recebe: referência ao Server dono deste handler.
*/
CommandHandler::CommandHandler(Server& server)
	: _server(server)
{}

/*
** ~CommandHandler()
** Destrutor trivial — o handler não possui recursos.
*/
CommandHandler::~CommandHandler() {}

/* ─── dispatch ─────────────────────────────────────────────────────────────── */

/*
** dispatch
** Ponto de entrada chamado pela camada de rede para cada linha completa.
** Selecciona o handler correcto pelo nome do comando (já em maiúsculas).
** Comandos desconhecidos recebem ERR_UNKNOWNCOMMAND (421).
** PING é tratado inline (muito simples para handler próprio).
**
** Recebe: client — o cliente que enviou a linha.
**         msg    — a linha já parseada em struct Message.
*/
void CommandHandler::dispatch(Client& client, const Message& msg)
{
	const std::string& cmd = msg.command;

	/* Comandos de registo (não requerem estar registado) */
	if      (cmd == "PASS")    handlePass(client, msg);
	else if (cmd == "NICK")    handleNick(client, msg);
	else if (cmd == "USER")    handleUser(client, msg);
	else if (cmd == "QUIT")    handleQuit(client, msg);

	/* PING/PONG: resposta imediata, sem autenticação necessária */
	else if (cmd == "PING")
	{
		std::string token = msg.params.empty()
		                    ? std::string(SERVER_NAME)
		                    : msg.params[0];
		_server.sendToClient(client.getFd(),
		                     ":" SERVER_NAME " PONG " SERVER_NAME " :" + token + "\r\n");
	}
	else if (cmd == "PONG")
	{
		/* Resposta ao nosso PING — ignoramos (servidor não envia PING) */
	}

	/* Comandos de mensagem */
	else if (cmd == "PRIVMSG") handlePrivmsg(client, msg);
	else if (cmd == "NOTICE")  handleNotice(client, msg);

	/* Comandos de canal */
	else if (cmd == "JOIN")    handleJoin(client, msg);
	else if (cmd == "PART")    handlePart(client, msg);
	else if (cmd == "TOPIC")   handleTopic(client, msg);

	/* Comandos de operador */
	else if (cmd == "KICK")    handleKick(client, msg);
	else if (cmd == "INVITE")  handleInvite(client, msg);
	else if (cmd == "MODE")    handleMode(client, msg);

	/* Comando desconhecido */
	else
	{
		/* Só envia ERR_UNKNOWNCOMMAND se o cliente já está registado;
		   antes do registo há demasiados clientes que testam comandos
		   não-standard e a resposta poderia confundi-los. */
		if (client.isRegistered())
		{
			_server.sendToClient(client.getFd(),
			                     ERR_UNKNOWNCOMMAND(client.getNickname(), cmd));
		}
	}
}

/* ─── handleQuit ────────────────────────────────────────────────────────────── */

/*
** handleQuit
** Processa o comando QUIT [:<reason>].
** Notifica todos os canais com a mensagem de saída e depois desliga.
** O cliente NÃO recebe resposta (já foi desligado quando quitClient retorna).
**
** Recebe: client — quem enviou QUIT.
**         msg    — parseado; o reason está em msg.trailing (opcional).
*/
void CommandHandler::handleQuit(Client& client, const Message& msg)
{
	std::string reason = msg.hasTrailing ? msg.trailing : "Client quit";
	_server.quitClient(client.getFd(), reason);
}

/* ─── requireRegistered ─────────────────────────────────────────────────────── */

/*
** requireRegistered
** Helper usado por todos os handlers que exigem o registo completo
** (PASS+NICK+USER). Se o cliente não estiver registado, envia ERR_NOTREGISTERED
** e devolve false para que o handler aborte.
**
** Recebe: client — o cliente a verificar.
** Devolve: true se registado e o handler pode continuar;
**          false se não registado (resposta 451 já enviada).
*/
bool CommandHandler::requireRegistered(Client& client)
{
	if (client.isRegistered())
		return true;

	/* Usa '*' como nick placeholder antes do registo completo */
	std::string nick = client.getNickname().empty() ? "*" : client.getNickname();
	_server.sendToClient(client.getFd(), ERR_NOTREGISTERED(nick));
	return false;
}
