#ifndef COMMANDHANDLER_HPP
#define COMMANDHANDLER_HPP

#include "Message.hpp"

// Forward declarations: we only need references/pointers here, so we avoid
// including the full headers (and avoid an include cycle with Server.hpp).
class Server;
class Client;

/**
 * @class CommandHandler
 * @brief The protocol brain (Person B).
 *
 * For every full command, the network layer calls dispatch(client, msg).
 * The handler then uses the Server's public "contract" methods to look up
 * clients/channels and to queue replies. It NEVER touches sockets directly.
 *
 * Copying is forbidden: a handler is bound to exactly one Server (held by
 * reference), so a copy would be meaningless.
 */
class CommandHandler
{
private:
	Server& _server;   // The owner server. This reference IS the contract surface.

	// Copying disabled (reference member + single-owner semantics).
	CommandHandler(const CommandHandler& other);
	CommandHandler& operator=(const CommandHandler& other);

public:
	explicit CommandHandler(Server& server);
	~CommandHandler();

	/**
	 * @brief Entry point invoked by the network layer for each full command.
	 * @param client the client that sent the command.
	 * @param msg    the already-parsed message.
	 */
	void dispatch(Client& client, const Message& msg);

private:
	// --- registration / connection ---
	void handlePass(Client& client, const Message& msg);    // PASS <password>
	void handleNick(Client& client, const Message& msg);    // NICK <nickname>
	void handleUser(Client& client, const Message& msg);    // USER <u> 0 * :<real>
	void handleQuit(Client& client, const Message& msg);    // QUIT [:reason]

	// --- messaging ---
	void handlePrivmsg(Client& client, const Message& msg); // PRIVMSG <tgt> :<text>
	void handleNotice(Client& client, const Message& msg);  // NOTICE  <tgt> :<text>

	// --- channels ---
	void handleJoin(Client& client, const Message& msg);    // JOIN <chan> [key]
	void handlePart(Client& client, const Message& msg);    // PART <chan> [:reason]
	void handleTopic(Client& client, const Message& msg);   // TOPIC <chan> [:topic]

	// --- channel-operator commands ---
	void handleKick(Client& client, const Message& msg);    // KICK <chan> <user>
	void handleInvite(Client& client, const Message& msg);  // INVITE <user> <chan>
	void handleMode(Client& client, const Message& msg);    // MODE <chan> <+/-modes>

	/**
	 * @brief Helper: a client must be fully registered before most commands.
	 * @return true if registered; otherwise queues ERR_NOTREGISTERED (451).
	 */
	bool requireRegistered(Client& client);
};

#endif // COMMANDHANDLER_HPP
