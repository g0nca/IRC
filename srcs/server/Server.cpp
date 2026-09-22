/*
** Server.cpp — Constructor, destructor and public contract methods of Server.
**
** The contract is the surface that CommandHandler (Person B) uses to
** interact with the network layer without touching sockets directly.
** Socket/poll logic lives in ServerSocket.cpp and ServerLoop.cpp.
*/

#include "Server.hpp"
#include <unistd.h>     /* close() */
#include <iostream>
#include <vector>

/* Static member definition (SIGINT flag) */
bool Server::_running = true;

/* ─── Constructor / Destructor ────────────────────────────────────────────── */

/*
** Server(int port, const std::string& password)
** Initialises the server with the given port and password.
** The socket is created in setupSocket(), called by run().
** _commands is initialised with *this (reference — safe because CommandHandler
** only stores the reference and does not use it during its own construction).
** Receives: port (1–65535), password (non-empty).
*/
Server::Server(int port, const std::string& password)
	: _listenFd(-1),
	  _port(port),
	  _password(password),
	  _pollFds(),
	  _clients(),
	  _channels(),
	  _commands(*this)
{}

/*
** ~Server()
** Destroys the server: closes all client fds, deletes Client* and Channel*
** objects, and closes the listening socket.
** Does not send any messages — cleanup is silent.
*/
Server::~Server()
{
	/* Close and delete all clients */
	for (std::map<int, Client*>::iterator it = _clients.begin();
	     it != _clients.end(); ++it)
	{
		close(it->first);
		delete it->second;
	}
	_clients.clear();

	/* Delete all channels */
	for (std::map<std::string, Channel*>::iterator it = _channels.begin();
	     it != _channels.end(); ++it)
		delete it->second;
	_channels.clear();

	/* Close the listening socket */
	if (_listenFd != -1)
	{
		close(_listenFd);
		_listenFd = -1;
	}
}

/* ─── Static signal helpers ────────────────────────────────────────────────── */

/*
** requestStop
** Called by the SIGINT handler. Clears the _running flag so the main loop
** exits cleanly on the next iteration.
*/
void Server::requestStop() { _running = false; }

/*
** isRunning
** Returns: true while the server has not received a stop signal.
*/
bool Server::isRunning()   { return _running; }

/* ─── Contract: configuration getters ───────────────────────────────────────── */

/*
** getPassword
** Returns: const reference to the password clients must send via PASS.
*/
const std::string& Server::getPassword() const { return _password; }

/* ─── Contract: client lookup ───────────────────────────────────────────────── */

/*
** getClientByFd
** Looks up a client by its file descriptor.
** Receives: the client socket fd.
** Returns: Client* if found, NULL otherwise.
*/
Client* Server::getClientByFd(int fd)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	return (it != _clients.end()) ? it->second : NULL;
}

/*
** getClientByNick
** Looks up a client by nickname (case-sensitive comparison as per the RFC —
** the server performs no case folding).
** Receives: nickname to search for.
** Returns: Client* if found, NULL otherwise.
*/
Client* Server::getClientByNick(const std::string& nick)
{
	for (std::map<int, Client*>::iterator it = _clients.begin();
	     it != _clients.end(); ++it)
	{
		if (it->second->getNickname() == nick)
			return it->second;
	}
	return NULL;
}

/* ─── Contract: channel lookup ──────────────────────────────────────────────── */

/*
** getChannel
** Looks up a channel by name (case-sensitive).
** Receives: channel name (including '#').
** Returns: Channel* if it exists, NULL otherwise.
*/
Channel* Server::getChannel(const std::string& name)
{
	std::map<std::string, Channel*>::iterator it = _channels.find(name);
	return (it != _channels.end()) ? it->second : NULL;
}

/*
** getOrCreateChannel
** Returns the channel with the given name. Creates and registers it if it
** does not exist yet.
** Receives: channel name (must be valid).
** Returns: pointer to the Channel (never NULL).
*/
Channel* Server::getOrCreateChannel(const std::string& name)
{
	Channel* ch = getChannel(name);
	if (!ch)
	{
		ch = new Channel(name);
		_channels[name] = ch;
	}
	return ch;
}

/*
** removeChannelIfEmpty
** Deletes the channel if it has no members. Called after every PART/KICK/QUIT.
** Receives: channel name.
*/
void Server::removeChannelIfEmpty(const std::string& name)
{
	std::map<std::string, Channel*>::iterator it = _channels.find(name);
	if (it != _channels.end() && it->second->isEmpty())
	{
		delete it->second;
		_channels.erase(it);
	}
}

/* ─── Contract: message sending ─────────────────────────────────────────────── */

/*
** sendToClient
** Queues a message in the client's output buffer and enables POLLOUT so
** the server sends it as soon as the socket is ready.
** Receives: recipient fd, message string (must end with "\r\n").
*/
void Server::sendToClient(int fd, const std::string& message)
{
	Client* c = getClientByFd(fd);
	if (!c)
		return;

	c->appendToOutBuffer(message);

	/* Enable POLLOUT for this fd */
	for (std::size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd == fd)
		{
			_pollFds[i].events |= POLLOUT;
			break;
		}
	}
}

/*
** broadcastToChannel
** Sends 'message' to every member of the channel except the fd given in
** 'exceptFd' (pass -1 to send to everyone).
** Receives: channel name, message, fd to exclude (-1 = exclude nobody).
*/
void Server::broadcastToChannel(const std::string& channel,
                                const std::string& message,
                                int                exceptFd)
{
	Channel* ch = getChannel(channel);
	if (!ch)
		return;

	const std::set<int>& members = ch->getMembers();
	for (std::set<int>::const_iterator it = members.begin();
	     it != members.end(); ++it)
	{
		if (*it != exceptFd)
			sendToClient(*it, message);
	}
}

/* ─── Contract: disconnection ───────────────────────────────────────────────── */

/*
** disconnectClient
** Removes the client from all channels, closes the fd, removes it from poll(),
** unregisters it and frees its memory.
** Does NOT send notifications — use quitClient() for that.
** Receives: fd of the client to disconnect.
*/
void Server::disconnectClient(int fd)
{
	Client* c = getClientByFd(fd);
	if (!c)
		return;

	/* Remove from channels; destroy empty channels */
	for (std::map<std::string, Channel*>::iterator it = _channels.begin();
	     it != _channels.end(); )
	{
		if (it->second->isMember(fd))
		{
			it->second->removeMember(fd);
			if (it->second->isEmpty())
			{
				delete it->second;
				std::map<std::string, Channel*>::iterator toErase = it;
				++it;
				_channels.erase(toErase);
				continue;
			}
		}
		++it;
	}

	close(fd);
	removeFromPoll(fd);
	_clients.erase(fd);
	delete c;

	std::cout << "[-] client disconnected: fd=" << fd
	          << " (total: " << _clients.size() << ")\n";
}

/*
** quitClient
** Notifies shared channels with a QUIT message, then calls disconnectClient().
** Used by the QUIT handler and by handleClientData when recv() returns 0.
** Receives: client fd, quit reason.
*/
void Server::quitClient(int fd, const std::string& reason)
{
	Client* c = getClientByFd(fd);
	if (!c)
		return;

	/* Only broadcast QUIT if the client was already registered (has a nick) */
	if (c->isRegistered())
	{
		std::string quitMsg = ":" + c->getPrefix() + " QUIT :" + reason + "\r\n";

		for (std::map<std::string, Channel*>::iterator it = _channels.begin();
		     it != _channels.end(); ++it)
		{
			if (it->second->isMember(fd))
				broadcastToChannel(it->first, quitMsg, fd);
		}
	}

	disconnectClient(fd);
}

/*
** getClientChannels
** Returns the names of every channel where the given fd is currently a member.
** Used by the NICK handler (for nick-change broadcasts) and by QUIT.
** Receives: client fd.
** Returns: vector of channel names.
*/
std::vector<std::string> Server::getClientChannels(int fd) const
{
	std::vector<std::string> result;
	for (std::map<std::string, Channel*>::const_iterator it = _channels.begin();
	     it != _channels.end(); ++it)
	{
		if (it->second->isMember(fd))
			result.push_back(it->first);
	}
	return result;
}
