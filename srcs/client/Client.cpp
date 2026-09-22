/*
** Client.cpp — Representa um utilizador TCP ligado ao servidor.
**
** A camada de rede (Person A) gere o ciclo de vida (criação no accept,
** destruição no quit/disconnect) e preenche os buffers de I/O.
** A camada de protocolo (Person B) lê os campos de identidade e muda
** os flags de registo.
*/

#include "Client.hpp"
#include <cstddef>

/* ─── Orthodox Canonical Form ─────────────────────────────────────────────── */

/*
** Client()
** Construtor por defeito. Cria um Client sem socket associado.
** _fd = -1 sinaliza "sem socket". Todos os flags inicializados a false.
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
** Construtor principal. Liga o Client ao file descriptor de um socket aceite.
** Recebe: fd — o file descriptor do socket TCP.
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
** Construtor de cópia. Copia todos os campos incluindo buffers.
** Recebe: referência constante para o Client a copiar.
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
** Atribuição por cópia. Protege auto-atribuição.
** Recebe: referência constante para o Client fonte.
** Devolve: referência para this.
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
** Destrutor. NÃO fecha o fd — é responsabilidade do Server fechar o socket
** antes de apagar o Client.
*/
Client::~Client() {}

/* ─── Identity getters ─────────────────────────────────────────────────────── */

/*
** getFd / getNickname / getUsername / getRealname / getHostname
** Acessores simples para os campos de identidade do cliente.
** Devolvem: referência constante ou valor inteiro do campo pedido.
*/
int                Client::getFd()       const { return _fd; }
const std::string& Client::getNickname() const { return _nickname; }
const std::string& Client::getUsername() const { return _username; }
const std::string& Client::getRealname() const { return _realname; }
const std::string& Client::getHostname() const { return _hostname; }

/* ─── Identity setters ─────────────────────────────────────────────────────── */

/*
** setNickname / setUsername / setRealname / setHostname
** Mutadores simples. Chamados pela camada de protocolo durante o registo
** e durante mudanças de nick em runtime.
** Recebem: string com o novo valor do campo.
*/
void Client::setNickname(const std::string& nickname) { _nickname = nickname; }
void Client::setUsername(const std::string& username) { _username = username; }
void Client::setRealname(const std::string& realname) { _realname = realname; }
void Client::setHostname(const std::string& hostname) { _hostname = hostname; }

/* ─── Registration state ───────────────────────────────────────────────────── */

/*
** hasReceivedPass / setPassReceived
** Flag que marca se o client já enviou um PASS válido.
** Devolve/recebe: bool.
*/
bool Client::hasReceivedPass() const        { return _passReceived; }
void Client::setPassReceived(bool value)    { _passReceived = value; }

/*
** isRegistered / setRegistered
** Flag que marca se o handshake PASS+NICK+USER foi completado com sucesso.
** Só após setRegistered(true) é que os comandos normais são aceites.
*/
bool Client::isRegistered() const           { return _registered; }
void Client::setRegistered(bool value)      { _registered = value; }

/* ─── Prefix ───────────────────────────────────────────────────────────────── */

/*
** getPrefix
** Constrói o prefixo "nick!user@host" usado no início das mensagens
** que o servidor envia em nome deste cliente.
** Devolve: string no formato "nick!user@host".
*/
std::string Client::getPrefix() const
{
	std::string user = _username.empty() ? "unknown" : _username;
	return _nickname + "!" + user + "@" + _hostname;
}

/* ─── Input framing ────────────────────────────────────────────────────────── */

/*
** appendToInBuffer
** Acrescenta bytes recebidos pelo recv() ao buffer de entrada.
** O TCP é um stream, por isso os dados podem chegar em pedaços;
** este buffer acumula-os até ter uma linha completa.
** Recebe: string com os bytes lidos do socket.
*/
void Client::appendToInBuffer(const std::string& data)
{
	_inBuffer += data;
}

/*
** extractMessage
** Extrai UMA linha completa (terminada em "\r\n") do buffer de entrada.
** Remove o terminador da string resultante.
** Recebe: referência para string que receberá a linha extraída.
** Devolve: true se foi extraída uma linha; false se o buffer ainda não tem
**          um terminador completo.
*/
bool Client::extractMessage(std::string& lineOut)
{
	std::size_t pos = _inBuffer.find("\r\n");
	if (pos == std::string::npos)
		return false;

	lineOut = _inBuffer.substr(0, pos);     /* linha sem o \r\n */
	_inBuffer.erase(0, pos + 2);            /* remove linha + terminador */
	return true;
}

/* ─── Output queue ─────────────────────────────────────────────────────────── */

/*
** appendToOutBuffer
** Acrescenta dados ao buffer de saída. O server activa POLLOUT para este fd
** imediatamente a seguir. O envio real acontece em flushClientOutput().
** Recebe: string com os bytes a enfileirar.
*/
void Client::appendToOutBuffer(const std::string& data)
{
	_outBuffer += data;
}

/*
** getOutBuffer
** Devolve referência constante ao buffer de saída (usado pelo Server para
** chamar send() com os dados pendentes).
** Devolve: referência constante para _outBuffer.
*/
const std::string& Client::getOutBuffer() const
{
	return _outBuffer;
}

/*
** consumeOutBuffer
** Remove 'count' bytes do início do buffer de saída após um send() parcial
** ou total ter sido bem-sucedido.
** Recebe: número de bytes confirmados enviados.
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
** Indica se há bytes por enviar no buffer de saída.
** Devolve: true se o buffer tem conteúdo, false se estiver vazio.
*/
bool Client::hasPendingOutput() const
{
	return !_outBuffer.empty();
}
