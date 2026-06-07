#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <cstddef>

/**
 * @class Client
 * @brief Represents ONE connected user, i.e. one TCP socket.
 *
 * Ownership: the Server keeps clients in std::map<int, Client*> keyed by fd.
 *
 * Responsibilities split:
 *   - Person A (network) fills _inBuffer from recv(), drains _outBuffer with
 *     send(), and manages the lifecycle (create on accept, destroy on quit).
 *   - Person B (protocol) reads the identity fields, flips the registration
 *     flags, and pushes replies through appendToOutBuffer().
 */
class Client
{
	private:
		int         _fd;            // Socket file descriptor. -1 means "no socket".
		std::string _nickname;      // NICK. Unique per server. Empty until set.
		std::string _username;      // USER <username>. The login / ident name.
		std::string _realname;      // USER ... :<realname>. Free text, can have spaces.
		std::string _hostname;      // Client host or IP. Used to build reply prefixes.
		std::string _inBuffer;      // Raw bytes received but not yet a full command
									// (TCP is a stream: a command may arrive in pieces).
		std::string _outBuffer;     // Bytes queued to send when the socket is writable.
		bool        _passReceived;  // true once a CORRECT password (PASS) was given.
		bool        _registered;    // true once PASS + NICK + USER are all complete.

	public:
		// --- Orthodox Canonical Form ---
		Client();                                   // Empty client (_fd = -1).
		explicit Client(int fd);                    // Client bound to an accepted socket.
		Client(const Client& other);
		Client& operator=(const Client& other);
		~Client();

		// --- identity getters ---
		int                getFd() const;
		const std::string& getNickname() const;
		const std::string& getUsername() const;
		const std::string& getRealname() const;
		const std::string& getHostname() const;

		// --- identity setters ---
		void setNickname(const std::string& nickname);
		void setUsername(const std::string& username);
		void setRealname(const std::string& realname);
		void setHostname(const std::string& hostname);

		// --- registration state ---
		bool hasReceivedPass() const;
		void setPassReceived(bool value);
		bool isRegistered() const;
		void setRegistered(bool value);

		/**
		 * @brief Builds the "nick!user@host" prefix prepended to outgoing messages.
		 */
		std::string getPrefix() const;

		// --- input framing (owned by Person A) ---
		/** @brief Append freshly received bytes to the input buffer. */
		void appendToInBuffer(const std::string& data);
		/**
		 * @brief Pop ONE complete command (terminated by "\r\n") from _inBuffer.
		 * @param lineOut receives the command text WITHOUT the trailing "\r\n".
		 * @return true if a full command was extracted; false if still partial.
		 */
		bool extractMessage(std::string& lineOut);

		// --- output queue (Person B writes, Person A reads) ---
		void               appendToOutBuffer(const std::string& data);
		const std::string& getOutBuffer() const;
		void               consumeOutBuffer(std::size_t count); // erase already-sent bytes
		bool               hasPendingOutput() const;
};

#endif // CLIENT_HPP
