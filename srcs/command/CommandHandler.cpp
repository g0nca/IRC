/*
** CommandHandler.cpp — Central IRC command dispatcher.
**
** dispatch() receives a Client and an already-parsed Message and invokes
** the correct handler. Also contains handleQuit (too simple for its own file)
** and requireRegistered (helper shared by all handlers).
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Replies.hpp"
#include <iostream>

/* ─── Constructor / Destructor ────────────────────────────────────────────── */

/*
** CommandHandler(Server& server)
** Receives the server reference (the contract). Never uses it during its own
** construction — safe even if Server is not fully built yet.
** Receives: reference to the Server that owns this handler.
*/
CommandHandler::CommandHandler(Server& server)
	: _server(server)
{}

/*
** ~CommandHandler()
** Trivial destructor — the handler owns no resources.
*/
CommandHandler::~CommandHandler() {}

/* ─── dispatch ─────────────────────────────────────────────────────────────── */

/*
** dispatch
** Entry point called by the network layer for each complete line.
** Selects the correct handler by command name (already upper-cased).
** Unknown commands receive ERR_UNKNOWNCOMMAND (421).
** PING is handled inline (too simple for its own handler).
**
** Receives: client — the client that sent the line.
**           msg    — the line already parsed into a Message struct.
*/
void CommandHandler::dispatch(Client& client, const Message& msg)
{
	const std::string& cmd = msg.command;

	/* CAP: IRCv3 capability negotiation. The server supports none;
	   reply with an empty list to LS and reject REQ so the client proceeds. */
	if (cmd == "CAP")
	{
		std::string sub = msg.params.empty() ? "" : msg.params[0];
		std::string nick = client.getNickname().empty() ? "*" : client.getNickname();
		if (sub == "LS")
			_server.sendToClient(client.getFd(),
				":" SERVER_NAME " CAP " + nick + " LS :\r\n");
		else if (sub == "REQ")
		{
			std::string caps = msg.hasTrailing ? msg.trailing : "";
			_server.sendToClient(client.getFd(),
				":" SERVER_NAME " CAP " + nick + " NAK :" + caps + "\r\n");
		}
		/* END and other sub-commands: ignore silently */
		return;
	}

	/* Registration commands (do not require the client to be registered) */
	if      (cmd == "PASS")    handlePass(client, msg);
	else if (cmd == "NICK")    handleNick(client, msg);
	else if (cmd == "USER")    handleUser(client, msg);
	else if (cmd == "QUIT")    handleQuit(client, msg);

	/* PING/PONG: immediate reply, no authentication required */
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
		/* Reply to our PING — ignored (the server does not send PING) */
	}

	/* Messaging commands */
	else if (cmd == "PRIVMSG") handlePrivmsg(client, msg);
	else if (cmd == "NOTICE")  handleNotice(client, msg);

	/* Channel commands */
	else if (cmd == "JOIN")    handleJoin(client, msg);
	else if (cmd == "PART")    handlePart(client, msg);
	else if (cmd == "TOPIC")   handleTopic(client, msg);

	/* Operator commands */
	else if (cmd == "KICK")    handleKick(client, msg);
	else if (cmd == "INVITE")  handleInvite(client, msg);
	else if (cmd == "MODE")    handleMode(client, msg);

	/* Unknown command */
	else
	{
		/* Only send ERR_UNKNOWNCOMMAND if the client is already registered;
		   before registration many clients probe non-standard commands and
		   the error reply could confuse them. */
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
** Processes the QUIT [:<reason>] command.
** Notifies all shared channels with the quit message, then disconnects.
** The client does NOT receive a reply (already disconnected when quitClient returns).
**
** Receives: client — who sent QUIT.
**           msg    — parsed; the reason is in msg.trailing (optional).
*/
void CommandHandler::handleQuit(Client& client, const Message& msg)
{
	std::string reason = msg.hasTrailing ? msg.trailing : "Client quit";
	_server.quitClient(client.getFd(), reason);
}

/* ─── requireRegistered ─────────────────────────────────────────────────────── */

/*
** requireRegistered
** Helper used by all handlers that require full registration (PASS+NICK+USER).
** If the client is not registered, sends ERR_NOTREGISTERED and returns false
** so the handler aborts.
**
** Receives: client — the client to check.
** Returns: true if registered and the handler may continue;
**          false if not registered (451 reply already sent).
*/
bool CommandHandler::requireRegistered(Client& client)
{
	if (client.isRegistered())
		return true;

	/* Use '*' as a nick placeholder before full registration */
	std::string nick = client.getNickname().empty() ? "*" : client.getNickname();
	_server.sendToClient(client.getFd(), ERR_NOTREGISTERED(nick));
	return false;
}
