/*
** Server.cpp — Construtor, destrutor e métodos do contrato público do Server.
**
** O contrato é a superfície que o CommandHandler (Person B) usa para
** interagir com a camada de rede sem tocar em sockets directamente.
** A lógica de socket/poll está em ServerSocket.cpp e ServerLoop.cpp.
*/

#include "Server.hpp"
#include <unistd.h>     /* close() */
#include <iostream>
#include <vector>

/* Definição do membro estático (flag do SIGINT) */
bool Server::_running = true;

/* ─── Constructor / Destructor ────────────────────────────────────────────── */

/*
** Server(int port, const std::string& password)
** Inicializa o servidor com a porta e password fornecidas.
** O socket é criado em setupSocket(), chamado por run().
** _commands é inicializado com *this (referência — válido porque CommandHandler
** só guarda a referência, não a usa durante a sua própria construção).
** Recebe: port (1–65535), password (não vazia).
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
** Destrói o servidor: fecha todos os fds dos clientes, apaga os objectos
** Client* e Channel*, fecha o socket de escuta.
** Não envia mensagens — o cleanup é silencioso.
*/
Server::~Server()
{
	/* Fechar e apagar todos os clientes */
	for (std::map<int, Client*>::iterator it = _clients.begin();
	     it != _clients.end(); ++it)
	{
		close(it->first);
		delete it->second;
	}
	_clients.clear();

	/* Apagar todos os canais */
	for (std::map<std::string, Channel*>::iterator it = _channels.begin();
	     it != _channels.end(); ++it)
		delete it->second;
	_channels.clear();

	/* Fechar o socket de escuta */
	if (_listenFd != -1)
	{
		close(_listenFd);
		_listenFd = -1;
	}
}

/* ─── Static signal helpers ────────────────────────────────────────────────── */

/*
** requestStop
** Chamado pelo handler de SIGINT. Baixa a flag _running para que o loop
** principal saia de forma limpa na próxima iteração.
*/
void Server::requestStop() { _running = false; }

/*
** isRunning
** Devolve: true enquanto o servidor não recebeu sinal de paragem.
*/
bool Server::isRunning()   { return _running; }

/* ─── Contract: getters de configuração ─────────────────────────────────────── */

/*
** getPassword
** Devolve: referência constante à password que os clientes devem enviar via PASS.
*/
const std::string& Server::getPassword() const { return _password; }

/* ─── Contract: lookup de clientes ─────────────────────────────────────────── */

/*
** getClientByFd
** Procura um cliente pelo seu file descriptor.
** Recebe: fd do socket do cliente.
** Devolve: Client* se encontrado, NULL caso contrário.
*/
Client* Server::getClientByFd(int fd)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	return (it != _clients.end()) ? it->second : NULL;
}

/*
** getClientByNick
** Procura um cliente pelo seu nickname (comparação case-sensitive, como
** definido no RFC — o servidor não faz conversão).
** Recebe: nickname a procurar.
** Devolve: Client* se encontrado, NULL caso contrário.
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

/* ─── Contract: lookup de canais ───────────────────────────────────────────── */

/*
** getChannel
** Procura um canal pelo nome (case-sensitive).
** Recebe: nome do canal (com '#').
** Devolve: Channel* se existir, NULL caso contrário.
*/
Channel* Server::getChannel(const std::string& name)
{
	std::map<std::string, Channel*>::iterator it = _channels.find(name);
	return (it != _channels.end()) ? it->second : NULL;
}

/*
** getOrCreateChannel
** Devolve o canal com o nome dado. Se não existir, cria-o e regista-o.
** Recebe: nome do canal (deve ser válido).
** Devolve: ponteiro para o Channel (nunca NULL).
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
** Apaga o canal se não tiver membros. Chama-se após cada PART/KICK/QUIT.
** Recebe: nome do canal.
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

/* ─── Contract: envio de mensagens ─────────────────────────────────────────── */

/*
** sendToClient
** Enfileira uma mensagem no buffer de saída do cliente e activa POLLOUT
** para que o servidor a envie assim que o socket estiver pronto.
** Recebe: fd do destinatário, string da mensagem (deve terminar em "\r\n").
*/
void Server::sendToClient(int fd, const std::string& message)
{
	Client* c = getClientByFd(fd);
	if (!c)
		return;

	c->appendToOutBuffer(message);

	/* Activar POLLOUT para este fd */
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
** Envia 'message' a todos os membros do canal, excepto ao fd indicado
** em 'exceptFd' (passar -1 para enviar a todos).
** Recebe: nome do canal, mensagem, fd a excluir (-1 = ninguém excluído).
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

/* ─── Contract: desconexão ──────────────────────────────────────────────────── */

/*
** disconnectClient
** Remove o cliente de todos os canais, fecha o fd, remove-o do poll(),
** elimina-o do registo e liberta a memória.
** NÃO envia notificações — use quitClient() para isso.
** Recebe: fd do cliente a desligar.
*/
void Server::disconnectClient(int fd)
{
	Client* c = getClientByFd(fd);
	if (!c)
		return;

	/* Remover dos canais; destruir canais vazios */
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
** Notifica os canais comuns com uma mensagem QUIT, depois chama
** disconnectClient(). Usado pelo handler QUIT e pelo recv()==0.
** Recebe: fd do cliente, razão do QUIT.
*/
void Server::quitClient(int fd, const std::string& reason)
{
	Client* c = getClientByFd(fd);
	if (!c)
		return;

	/* Só envia QUIT se o cliente já estava registado (tem nick) */
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
** Devolve os nomes de todos os canais onde o fd está actualmente como membro.
** Usado pelo handler NICK (para broadcast de mudança de nick) e pelo QUIT.
** Recebe: fd do cliente.
** Devolve: vector de nomes de canal.
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
