/*
** ServerLoop.cpp — Main event loop (single poll()) and read/write handling.
**
** Subject rules (mandatory):
**   - A single poll() for all I/O; all fds non-blocking.
**   - No fork(). No recv()/send() outside the poll() flow.
**   - SIGPIPE ignored (client closes socket during a send).
**   - SIGINT (Ctrl+C) clears _running for a clean exit.
*/

#include "Server.hpp"
#include "Message.hpp"
#include <poll.h>
#include <csignal>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <vector>

/*
** Server::run
** Entry point after construction. Sequence:
**   1. Configure signals (SIGPIPE ignored, SIGINT → requestStop)
**   2. Create the listening socket via setupSocket()
**   3. Enter the single poll() loop while _running is true
**   4. On exit, print shutdown message (destructor cleans up resources)
**
** The loop:
**   - poll() with 1 s timeout (to react to SIGINT even with no activity)
**   - fd[0] = listen socket: POLLIN → acceptNewClient()
**   - fd[1..n] = clients:   POLLIN → handleClientData(); POLLOUT → flushClientOutput()
**   - POLLERR/POLLHUP/POLLNVAL → disconnectClient()
*/
void Server::run()
{
	/* Ignore SIGPIPE: send() to a closed socket returns -1 instead of killing the process */
	signal(SIGPIPE, SIG_IGN);
	/* SIGINT (Ctrl+C): clear the flag safely */
	signal(SIGINT, reinterpret_cast<void (*)(int)>(&Server::requestStop));

	setupSocket();

	while (_running)
	{
		/* poll() with 1000 ms timeout to avoid blocking indefinitely */
		int ready = poll(_pollFds.data(),
		                 static_cast<nfds_t>(_pollFds.size()),
		                 1000);

		if (ready < 0)
		{
			if (errno == EINTR)     /* interrupted by a signal — check _running */
				continue;
			std::cerr << "[!] poll() error\n";
			break;
		}
		if (ready == 0)             /* timeout with no activity */
			continue;

		/* ── 1. Listening socket: new connection ────────────────────────── */
		if (_pollFds[0].revents & POLLIN)
			acceptNewClient();

		/* ── 2. Client sockets ──────────────────────────────────────────── */
		/* Note: _pollFds may grow (acceptNewClient) or shrink
		   (disconnectClient). Use an index instead of an iterator
		   and do not increment i when an element was removed.          */
		for (std::size_t i = 1; i < _pollFds.size(); )
		{
			int   fd      = _pollFds[i].fd;
			short revents = _pollFds[i].revents;

			/* Error or hang-up: disconnect immediately */
			if (revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				quitClient(fd, "connection error");
				/* element removed — do not increment i */
				continue;
			}

			/* Data available to read */
			if (revents & POLLIN)
			{
				handleClientData(fd);
				/* handleClientData may have called quitClient/disconnectClient */
				if (!getClientByFd(fd))
					continue;           /* element removed */
			}

			/* Socket ready to write (pending output buffer) */
			if ((revents & POLLOUT) && getClientByFd(fd))
			{
				flushClientOutput(fd);
				if (!getClientByFd(fd))
					continue;
			}

			++i;
		}
	}

	std::cout << "\n[*] shutting down, closing all sockets...\n";
}

/*
** Server::handleClientData
** Reads bytes from the client socket with recv(), appends them to the input
** buffer and processes every complete line ("\r\n") that becomes available.
** Each line is parsed into a Message and delivered to CommandHandler::dispatch().
**
** If recv() returns 0 → client closed the connection → quitClient().
** If recv() returns <0 and errno != EAGAIN → error → quitClient().
**
** Receives: the client socket fd.
*/
void Server::handleClientData(int fd)
{
	Client* client = getClientByFd(fd);
	if (!client)
		return;

	char    buf[4096];
	ssize_t n = recv(fd, buf, sizeof(buf), 0);

	if (n == 0)
	{
		/* Client closed the connection cleanly */
		quitClient(fd, "connection closed");
		return;
	}
	if (n < 0)
	{
		/* EAGAIN/EWOULDBLOCK: no data right now, not a fatal error */
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		quitClient(fd, "read error");
		return;
	}

	client->appendToInBuffer(std::string(buf, static_cast<std::size_t>(n)));

	/* Process all complete lines accumulated in the buffer */
	std::string line;
	while (client->extractMessage(line))
	{
		Message msg = parseMessage(line);
		if (msg.command.empty())
			continue;

		_commands.dispatch(*client, msg);

		/* dispatch() may have disconnected this client (e.g. QUIT) */
		if (!getClientByFd(fd))
			return;
	}
}

/*
** Server::flushClientOutput
** Sends as much of the client's output buffer as possible using send().
** Non-blocking send() may send only part of the data; the rest stays in the
** buffer for the next POLLOUT activation.
** When the buffer empties, POLLOUT is disabled for this fd.
**
** Receives: the client socket fd.
*/
void Server::flushClientOutput(int fd)
{
	Client* client = getClientByFd(fd);
	if (!client || !client->hasPendingOutput())
	{
		/* Nothing to send: disable POLLOUT */
		for (std::size_t i = 0; i < _pollFds.size(); ++i)
		{
			if (_pollFds[i].fd == fd)
			{
				_pollFds[i].events &= ~POLLOUT;
				break;
			}
		}
		return;
	}

	const std::string& out = client->getOutBuffer();
	ssize_t sent = send(fd, out.c_str(), out.size(), 0);

	if (sent < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;                     /* retry on next iteration */
		quitClient(fd, "write error");
		return;
	}

	client->consumeOutBuffer(static_cast<std::size_t>(sent));

	/* If the buffer is now empty, disable POLLOUT */
	if (!client->hasPendingOutput())
	{
		for (std::size_t i = 0; i < _pollFds.size(); ++i)
		{
			if (_pollFds[i].fd == fd)
			{
				_pollFds[i].events &= ~POLLOUT;
				break;
			}
		}
	}
}
