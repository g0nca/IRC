/*
** ServerSocket.cpp — Criação do socket de escuta e gestão do array poll().
**
** Toda a interacção com sockets a nível de sistema (socket, bind, listen,
** accept, fcntl) está aqui. A lógica do loop de eventos está em ServerLoop.cpp.
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
** Cria o socket de escuta TCP e prepara-o para aceitar conexões:
**   1. socket()       — cria o fd
**   2. setsockopt()   — SO_REUSEADDR (evita "Address already in use" no restart)
**   3. bind()         — associa à porta pedida em todas as interfaces
**   4. listen()       — coloca o socket em modo de escuta
**   5. fcntl(O_NONBLOCK) — torna o fd não-bloqueante
**   6. addToPoll()    — regista no array do poll() para POLLIN
** Lança std::runtime_error em qualquer falha do sistema.
*/
void Server::setupSocket()
{
	/* 1. Criar socket TCP */
	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd < 0)
		throw std::runtime_error("socket() failed");

	/* 2. Reutilização do endereço (para restart imediato) */
	int opt = 1;
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		close(_listenFd);
		throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
	}

	/* 3. Bind à porta pedida em todas as interfaces */
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

	/* 4. Começar a escutar (backlog = 128) */
	if (listen(_listenFd, 128) < 0)
	{
		close(_listenFd);
		throw std::runtime_error("listen() failed");
	}

	/* 5. Modo não-bloqueante */
	if (fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(_listenFd);
		throw std::runtime_error("fcntl(O_NONBLOCK) failed on listen socket");
	}

	/* 6. Registar no poll() para novas ligações */
	addToPoll(_listenFd, POLLIN);

	std::cout << "[*] listening on port " << _port << " (fd=" << _listenFd << ")\n";
}

/*
** Server::acceptNewClient
** Aceita uma nova ligação TCP no socket de escuta:
**   1. accept()       — cria o fd do cliente
**   2. fcntl(O_NONBLOCK) — torna-o não-bloqueante
**   3. Cria Client*   — regista nome de host pela addr do peer
**   4. addToPoll()    — passa a vigiar POLLIN para dados do cliente
** Chamada quando _pollFds[0] (o listen fd) tem POLLIN.
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

	/* Modo não-bloqueante para o fd do cliente */
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(clientFd);
		return;
	}

	/* Criar o objecto Client e registar */
	Client* newClient = new Client(clientFd);
	newClient->setHostname(inet_ntoa(clientAddr.sin_addr));
	_clients[clientFd] = newClient;
	addToPoll(clientFd, POLLIN);

	std::cout << "[+] client connected: fd=" << clientFd
	          << " (total: " << _clients.size() << ")\n";
}

/*
** Server::addToPoll
** Adiciona um fd ao array _pollFds que o poll() vigia.
** Recebe: fd a adicionar, máscara de eventos a vigiar (ex: POLLIN, POLLIN|POLLOUT).
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
** Remove um fd do array _pollFds. Usa a técnica swap-and-pop para O(n)
** sem realocar (a ordem não importa para o poll()).
** Recebe: fd a remover.
*/
void Server::removeFromPoll(int fd)
{
	for (std::size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd == fd)
		{
			/* Substituir pelo último elemento e encolher o vector */
			_pollFds[i] = _pollFds.back();
			_pollFds.pop_back();
			return;
		}
	}
}
