/*
** ServerSocket.cpp — Listening socket creation and poll() array management.
**
** All system-level socket interaction (socket, bind, listen, accept, fcntl)
** lives here. Event-loop logic is in ServerLoop.cpp.
*/

#include "Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstring>      /* memset */

/*
** Server::setupSocket
** Creates the TCP listening socket and prepares it to accept connections:
**   1. socket()       — create the fd
**   2. setsockopt()   — SO_REUSEADDR (avoids "Address already in use" on restart)
**   3. bind()         — bind to the requested port on all interfaces
**   4. listen()       — put the socket in listening mode
**   5. fcntl(O_NONBLOCK) — make the fd non-blocking
**   6. addToPoll()    — register in the poll() array for POLLIN
** Throws std::runtime_error on any system call failure.
*/
void Server::setupSocket()
{
	/* 1. Create TCP socket */
	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd < 0)
		throw std::runtime_error("socket() failed");

	/* 2. Allow immediate address reuse (for instant restart) */
	int opt = 1;
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		close(_listenFd);
		throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
	}

	/* 3. Bind to the requested port on all interfaces */
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port        = htons(static_cast<uint16_t>(_port));

	if (bind(_listenFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
	{
		close(_listenFd);
		throw std::runtime_error("bind() failed");
	}

	/* 4. Start listening (backlog = 128) */
	if (listen(_listenFd, 128) < 0)
	{
		close(_listenFd);
		throw std::runtime_error("listen() failed");
	}

	/* 5. Non-blocking mode */
	if (fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(_listenFd);
		throw std::runtime_error("fcntl(O_NONBLOCK) failed on listen socket");
	}

	/* 6. Register in poll() for new connections */
	addToPoll(_listenFd, POLLIN);

	std::cout << "[*] listening on port " << _port << " (fd=" << _listenFd << ")\n";
}

/*
** Server::acceptNewClient
** Accepts a new TCP connection on the listening socket:
**   1. accept()       — create the client fd
**   2. fcntl(O_NONBLOCK) — make it non-blocking
**   3. Create Client* — record the peer hostname from the address
**   4. addToPoll()    — start watching POLLIN for client data
** Called when _pollFds[0] (the listen fd) has POLLIN.
*/
void Server::acceptNewClient()
{
	struct sockaddr_in clientAddr;
	socklen_t          addrLen = sizeof(clientAddr);

	int clientFd = accept(_listenFd,
	                      reinterpret_cast<struct sockaddr*>(&clientAddr),
	                      &addrLen);
	if (clientFd < 0)
		return;                             /* EAGAIN / EWOULDBLOCK — OK */

	/* Non-blocking mode for the client fd */
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(clientFd);
		return;
	}

	/* Create the Client object and register it */
	Client* newClient = new Client(clientFd);
	newClient->setHostname(inet_ntoa(clientAddr.sin_addr));
	_clients[clientFd] = newClient;
	addToPoll(clientFd, POLLIN);

	std::cout << "[+] client connected: fd=" << clientFd
	          << " (total: " << _clients.size() << ")\n";
}

/*
** Server::addToPoll
** Adds an fd to the _pollFds array watched by poll().
** Receives: fd to add, event mask to watch (e.g. POLLIN, POLLIN|POLLOUT).
*/
void Server::addToPoll(int fd, short events)
{
	struct pollfd pfd;
	pfd.fd      = fd;
	pfd.events  = events;
	pfd.revents = 0;
	_pollFds.push_back(pfd);
}

/*
** Server::removeFromPoll
** Removes an fd from the _pollFds array. Uses swap-and-pop for O(n)
** without reallocation (order does not matter for poll()).
** Receives: fd to remove.
*/
void Server::removeFromPoll(int fd)
{
	for (std::size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd == fd)
		{
			/* Replace with the last element and shrink the vector */
			_pollFds[i] = _pollFds.back();
			_pollFds.pop_back();
			return;
		}
	}
}
