#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <vector>
#include <poll.h>
#include "Client.hpp"
#include "Channel.hpp"
#include "CommandHandler.hpp"
#include "Errors.hpp"

/**
 * @class Server
 * @brief Owns the listening socket and the SINGLE poll() loop (Person A),
 *        plus the registries of clients and channels.
 *
 * The public methods under the "CONTRACT" banner are EXACTLY what
 * CommandHandler (Person B) is allowed to call. Freeze that section together
 * first: it is the meeting point that lets both people work in parallel
 * (A behind a stub dispatcher, B behind a mock server).
 *
 * Copying is forbidden: a server owns sockets and heap objects.
 */
class Server
{
	private:
		int                             _listenFd;  // Listening socket fd.
		int                             _port;      // Bound port (1..65535).
		std::string                     _password;  // Value every client must PASS.
		std::vector<struct pollfd>      _pollFds;   // One entry per watched fd (the
													// listen socket + every client).
		std::map<int, Client*>          _clients;   // fd -> connected Client.
		std::map<std::string, Channel*> _channels;  // channel name -> Channel.
		CommandHandler                  _commands;  // The protocol dispatcher (Person B).
		static bool                     _running;   // Set to false by the SIGINT handler
													// to break the poll() loop cleanly.

		// Copying disabled.
		Server(const Server& other);
		Server& operator=(const Server& other);

	public:
		Server(int port, const std::string& password);
		~Server();   // closes all fds and frees every Client/Channel.

		/**
		 * @brief Set up the socket and run the single poll() event loop.
		 *        Implemented by A Person.
		 */
		void run();

		static void requestStop();   // called from the signal handler
		static bool isRunning();

		// ========================= CONTRACT (used by B Person) =========================
		// Everything below is the agreed surface CommandHandler relies on.

		const std::string& getPassword() const;

		/** @return the Client* for an fd, or NULL if unknown. */
		Client*  getClientByFd(int fd);
		/** @return the Client* whose nickname matches, or NULL if none. */
		Client*  getClientByNick(const std::string& nick);

		/** @return the Channel* for a name, or NULL if it does not exist. */
		Channel* getChannel(const std::string& name);
		/** @return the existing channel, creating an empty one if needed. */
		Channel* getOrCreateChannel(const std::string& name);
		/** @brief Delete the channel if it has no members left. */
		void     removeChannelIfEmpty(const std::string& name);

		/**
		 * @brief Queue a message to a single client: appends to its out buffer and
		 *        makes sure poll() watches POLLOUT for that fd.
		 */
		void sendToClient(int fd, const std::string& message);

		/**
		 * @brief Send a message to every member of a channel.
		 * @param exceptFd fd to skip (e.g. the sender). Pass -1 to send to everyone.
		 */
		void broadcastToChannel(const std::string& channel,
								const std::string& message, int exceptFd);

		/**
		 * @brief Fully disconnect a client: remove it from all channels, close its
		 *        fd, drop it from poll() and free the object.
		 */
		void disconnectClient(int fd);

		/**
		 * @brief Broadcast a QUIT message to all channels the client is in,
		 *        then call disconnectClient(). Used by QUIT handler and by
		 *        handleClientData when recv() returns 0.
		 * @param reason the quit reason shown to other users.
		 */
		void quitClient(int fd, const std::string& reason);

		/**
		 * @brief Returns the names of every channel the given fd is currently in.
		 *        Used by NICK change and QUIT broadcasts.
		 */
		std::vector<std::string> getClientChannels(int fd) const;
		// ===============================================================================

	private:
		// --- network internals (Person A) ---
		void setupSocket();              // socket + setsockopt + bind + listen
		void acceptNewClient();          // accept + O_NONBLOCK + register in poll
		void handleClientData(int fd);   // recv -> buffer -> parse -> _commands.dispatch
		void flushClientOutput(int fd);  // send the queued out buffer (on POLLOUT)
		void addToPoll(int fd, short events);
		void removeFromPoll(int fd);
};

#endif // SERVER_HPP
