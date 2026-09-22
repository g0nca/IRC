/*
** Client.cpp — Represents a connected TCP user.
**
** The network layer (Person A) manages the lifecycle (created on accept,
** destroyed on quit/disconnect) and fills the I/O buffers.
** The protocol layer (Person B) reads the identity fields and flips the
** registration flags.
*/

#include "Client.hpp"
#include <cstddef>

/* ─── Orthodox Canonical Form ─────────────────────────────────────────────── */

/*
** Client()
** Default constructor. Creates a Client with no associated socket.
** _fd = -1 signals "no socket". All flags initialised to false.
*/
Client::Client()
	: _fd(-1),
	  _nickname(),
	  _username(),
	  _realname(),
	  _hostname("unknown"),
	  _inBuffer(),
	  _outBuffer(),
	  _passReceived(false),
	  _registered(false)
{}

/*
** Client(int fd)
** Main constructor. Binds the Client to the file descriptor of an accepted socket.
** Receives: fd — the TCP socket file descriptor.
*/
Client::Client(int fd)
	: _fd(fd),
	  _nickname(),
	  _username(),
	  _realname(),
	  _hostname("unknown"),
	  _inBuffer(),
	  _outBuffer(),
	  _passReceived(false),
	  _registered(false)
{}

/*
** Client(const Client& other)
** Copy constructor. Copies all fields including buffers.
** Receives: const reference to the Client to copy.
*/
Client::Client(const Client& other)
	: _fd(other._fd),
	  _nickname(other._nickname),
	  _username(other._username),
	  _realname(other._realname),
	  _hostname(other._hostname),
	  _inBuffer(other._inBuffer),
	  _outBuffer(other._outBuffer),
	  _passReceived(other._passReceived),
	  _registered(other._registered)
{}

/*
** operator=
** Copy assignment. Guards against self-assignment.
** Receives: const reference to the source Client.
** Returns: reference to this.
*/
Client& Client::operator=(const Client& other)
{
	if (this != &other)
	{
		_fd           = other._fd;
		_nickname     = other._nickname;
		_username     = other._username;
		_realname     = other._realname;
		_hostname     = other._hostname;
		_inBuffer     = other._inBuffer;
		_outBuffer    = other._outBuffer;
		_passReceived = other._passReceived;
		_registered   = other._registered;
	}
	return *this;
}

/*
** ~Client()
** Destructor. Does NOT close the fd — the Server is responsible for closing
** the socket before deleting the Client.
*/
Client::~Client() {}

/* ─── Identity getters ─────────────────────────────────────────────────────── */

/*
** getFd / getNickname / getUsername / getRealname / getHostname
** Simple accessors for the client identity fields.
** Return: const reference or integer value of the requested field.
*/
int                Client::getFd()       const { return _fd; }
const std::string& Client::getNickname() const { return _nickname; }
const std::string& Client::getUsername() const { return _username; }
const std::string& Client::getRealname() const { return _realname; }
const std::string& Client::getHostname() const { return _hostname; }

/* ─── Identity setters ─────────────────────────────────────────────────────── */

/*
** setNickname / setUsername / setRealname / setHostname
** Simple mutators. Called by the protocol layer during registration
** and during runtime nick changes.
** Receive: string with the new value for the field.
*/
void Client::setNickname(const std::string& nickname) { _nickname = nickname; }
void Client::setUsername(const std::string& username) { _username = username; }
void Client::setRealname(const std::string& realname) { _realname = realname; }
void Client::setHostname(const std::string& hostname) { _hostname = hostname; }

/* ─── Registration state ───────────────────────────────────────────────────── */

/*
** hasReceivedPass / setPassReceived
** Flag that marks whether the client has already sent a valid PASS.
** Returns/receives: bool.
*/
bool Client::hasReceivedPass() const        { return _passReceived; }
void Client::setPassReceived(bool value)    { _passReceived = value; }

/*
** isRegistered / setRegistered
** Flag that marks whether the PASS+NICK+USER handshake completed successfully.
** Only after setRegistered(true) are normal commands accepted.
*/
bool Client::isRegistered() const           { return _registered; }
void Client::setRegistered(bool value)      { _registered = value; }

/* ─── Prefix ───────────────────────────────────────────────────────────────── */

/*
** getPrefix
** Builds the "nick!user@host" prefix prepended to messages the server sends
** on behalf of this client.
** Returns: string in the format "nick!user@host".
*/
std::string Client::getPrefix() const
{
	std::string user = _username.empty() ? "unknown" : _username;
	return _nickname + "!" + user + "@" + _hostname;
}

/* ─── Input framing ────────────────────────────────────────────────────────── */

/*
** appendToInBuffer
** Appends bytes received by recv() to the input buffer.
** TCP is a stream, so data may arrive in fragments; this buffer accumulates
** them until a complete line is available.
** Receives: string containing the bytes read from the socket.
*/
void Client::appendToInBuffer(const std::string& data)
{
	_inBuffer += data;
}

/*
** extractMessage
** Extracts ONE complete line (terminated by "\r\n" or "\n") from the input
** buffer. Strips the terminator from the result.
** Receives: reference to a string that will receive the extracted line.
** Returns: true if a line was extracted; false if the buffer has no complete
**          terminator yet.
*/
bool Client::extractMessage(std::string& lineOut)
{
	std::size_t crlf = _inBuffer.find("\r\n");
	std::size_t lf   = _inBuffer.find('\n');

	std::size_t pos;
	std::size_t skip;

	if (crlf != std::string::npos && (lf == std::string::npos || crlf <= lf))
	{
		pos  = crlf;
		skip = 2;
	}
	else if (lf != std::string::npos)
	{
		pos  = lf;
		skip = 1;
	}
	else
		return false;

	lineOut = _inBuffer.substr(0, pos);
	if (!lineOut.empty() && lineOut[lineOut.size() - 1] == '\r')
		lineOut.erase(lineOut.size() - 1);
	_inBuffer.erase(0, pos + skip);
	return true;
}

/* ─── Output queue ─────────────────────────────────────────────────────────── */

/*
** appendToOutBuffer
** Appends data to the output buffer. The server enables POLLOUT for this fd
** immediately afterwards. The actual send happens in flushClientOutput().
** Receives: string containing the bytes to enqueue.
*/
void Client::appendToOutBuffer(const std::string& data)
{
	_outBuffer += data;
}

/*
** getOutBuffer
** Returns a const reference to the output buffer (used by the Server to
** call send() with the pending data).
** Returns: const reference to _outBuffer.
*/
const std::string& Client::getOutBuffer() const
{
	return _outBuffer;
}

/*
** consumeOutBuffer
** Removes 'count' bytes from the front of the output buffer after a partial
** or full send() succeeded.
** Receives: number of bytes confirmed as sent.
*/
void Client::consumeOutBuffer(std::size_t count)
{
	if (count >= _outBuffer.size())
		_outBuffer.clear();
	else
		_outBuffer.erase(0, count);
}

/*
** hasPendingOutput
** Indicates whether there are bytes waiting to be sent in the output buffer.
** Returns: true if the buffer has content, false if empty.
*/
bool Client::hasPendingOutput() const
{
	return !_outBuffer.empty();
}
