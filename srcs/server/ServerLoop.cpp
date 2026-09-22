/*
** ServerLoop.cpp — Loop de eventos principal (único poll()) e leitura/escrita.
**
** Regras do subject (obrigatórias):
**   - Um único poll() para todo o I/O; todos os fds não-bloqueantes.
**   - Sem fork(). Sem recv()/send() fora do fluxo do poll().
**   - SIGPIPE ignorado (cliente fecha socket durante um send).
**   - SIGINT (Ctrl+C) baixa _running para saída limpa.
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
** Ponto de entrada do servidor após a construção. Sequência:
**   1. Configura sinais (SIGPIPE ignorado, SIGINT → requestStop)
**   2. Cria o socket de escuta via setupSocket()
**   3. Entra no loop poll() único enquanto _running for true
**   4. Ao sair, imprime mensagem de shutdown (o destrutor limpa os recursos)
**
** O loop:
**   - poll() com timeout de 1 s (para reagir ao SIGINT mesmo sem actividade)
**   - fd[0] = listen socket: POLLIN → acceptNewClient()
**   - fd[1..n] = clientes:   POLLIN → handleClientData(); POLLOUT → flushClientOutput()
**   - POLLERR/POLLHUP/POLLNVAL → disconnectClient()
*/
void Server::run()
{
	/* Ignorar SIGPIPE: send() para socket fechado devolve -1, não mata o processo */
	signal(SIGPIPE, SIG_IGN);
	/* SIGINT (Ctrl+C): baixa a flag de forma segura */
	signal(SIGINT, reinterpret_cast<void (*)(int)>(&Server::requestStop));

	setupSocket();

	while (_running)
	{
		/* poll() com timeout de 1000 ms para não bloquear indefinidamente */
		int ready = poll(_pollFds.data(),
		                 static_cast<nfds_t>(_pollFds.size()),
		                 1000);

		if (ready < 0)
		{
			if (errno == EINTR)     /* interrompido por sinal — verificar _running */
				continue;
			std::cerr << "[!] poll() error\n";
			break;
		}
		if (ready == 0)             /* timeout sem actividade */
			continue;

		/* ── 1. Socket de escuta: nova ligação ──────────────────────────── */
		if (_pollFds[0].revents & POLLIN)
			acceptNewClient();

		/* ── 2. Sockets de clientes ─────────────────────────────────────── */
		/* Nota: _pollFds pode crescer (acceptNewClient) ou encolher
		   (disconnectClient). Por isso usamos índice em vez de iterator
		   e não incrementamos i quando um elemento foi removido.         */
		for (std::size_t i = 1; i < _pollFds.size(); )
		{
			int   fd      = _pollFds[i].fd;
			short revents = _pollFds[i].revents;

			/* Erro ou hang-up: desligar imediatamente */
			if (revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				quitClient(fd, "connection error");
				/* element removed — não incrementar i */
				continue;
			}

			/* Dados disponíveis para ler */
			if (revents & POLLIN)
			{
				handleClientData(fd);
				/* handleClientData pode chamar quitClient/disconnectClient */
				if (!getClientByFd(fd))
					continue;           /* elemento removido */
			}

			/* Socket pronto para escrever (buffer de saída pendente) */
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
** Lê bytes do socket do cliente com recv(), acrescenta ao buffer de entrada
** e processa todas as linhas completas ("\r\n") que ficaram disponíveis.
** Cada linha é parseada em Message e entregue ao CommandHandler::dispatch().
**
** Se recv() devolver 0 → cliente fechou a ligação → quitClient().
** Se recv() devolver <0 e errno != EAGAIN → erro → quitClient().
**
** Recebe: fd do socket do cliente.
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
		/* Cliente fechou a ligação de forma limpa */
		quitClient(fd, "connection closed");
		return;
	}
	if (n < 0)
	{
		/* EAGAIN/EWOULDBLOCK: sem dados por agora, não é erro fatal */
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		quitClient(fd, "read error");
		return;
	}

	client->appendToInBuffer(std::string(buf, static_cast<std::size_t>(n)));
	std::cerr << "[DBG] recv fd=" << fd << " bytes=" << n << " raw=|" << std::string(buf, static_cast<std::size_t>(n)) << "|\n";

	/* Processar todas as linhas completas acumuladas no buffer */
	std::string line;
	while (client->extractMessage(line))
	{
		std::cerr << "[DBG] extracted line=|" << line << "|\n";
		Message msg = parseMessage(line);
		std::cerr << "[DBG] parsed cmd=|" << msg.command << "| params=" << msg.params.size() << " hasTrailing=" << msg.hasTrailing << "\n";
		if (msg.command.empty())
			continue;

		_commands.dispatch(*client, msg);

		/* dispatch() pode ter desligado este cliente (ex: QUIT) */
		if (!getClientByFd(fd))
			return;
	}
}

/*
** Server::flushClientOutput
** Envia o máximo possível do buffer de saída do cliente usando send().
** send() não-bloqueante pode enviar apenas parte dos dados; o resto fica
** no buffer para a próxima activação de POLLOUT.
** Quando o buffer esvazia, desactiva POLLOUT para este fd.
**
** Recebe: fd do socket do cliente.
*/
void Server::flushClientOutput(int fd)
{
	Client* client = getClientByFd(fd);
	if (!client || !client->hasPendingOutput())
	{
		/* Não há nada para enviar: desactivar POLLOUT */
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
			return;                     /* tentar de novo na próxima iteração */
		quitClient(fd, "write error");
		return;
	}

	client->consumeOutBuffer(static_cast<std::size_t>(sent));

	/* Se o buffer ficou vazio, desligar POLLOUT */
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
