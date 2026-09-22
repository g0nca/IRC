/*
** Channel.cpp — Sala de chat IRC.
**
** Armazena membros, operadores, convidados e todos os modos do canal.
** Membros são representados como file descriptors (int); o Server resolve
** o fd para um Client* quando precisa de enviar. Isto evita dangling
** pointers quando um cliente desliga.
*/

#include "Channel.hpp"

/* ─── Orthodox Canonical Form ─────────────────────────────────────────────── */

/*
** Channel()
** Construtor por defeito. Cria um canal sem nome e com todos os modos off.
*/
Channel::Channel()
	: _name(),
	  _topic(),
	  _key(),
	  _inviteOnly(false),
	  _topicRestricted(false),
	  _hasKey(false),
	  _hasUserLimit(false),
	  _userLimit(0),
	  _members(),
	  _operators(),
	  _invited()
{}

/*
** Channel(const std::string& name)
** Construtor principal. Cria um canal com o nome dado (deve incluir '#').
** Recebe: nome do canal.
*/
Channel::Channel(const std::string& name)
	: _name(name),
	  _topic(),
	  _key(),
	  _inviteOnly(false),
	  _topicRestricted(false),
	  _hasKey(false),
	  _hasUserLimit(false),
	  _userLimit(0),
	  _members(),
	  _operators(),
	  _invited()
{}

/*
** Channel(const Channel& other)
** Construtor de cópia.
*/
Channel::Channel(const Channel& other)
	: _name(other._name),
	  _topic(other._topic),
	  _key(other._key),
	  _inviteOnly(other._inviteOnly),
	  _topicRestricted(other._topicRestricted),
	  _hasKey(other._hasKey),
	  _hasUserLimit(other._hasUserLimit),
	  _userLimit(other._userLimit),
	  _members(other._members),
	  _operators(other._operators),
	  _invited(other._invited)
{}

/*
** operator=
** Atribuição por cópia. Protege auto-atribuição.
*/
Channel& Channel::operator=(const Channel& other)
{
	if (this != &other)
	{
		_name            = other._name;
		_topic           = other._topic;
		_key             = other._key;
		_inviteOnly      = other._inviteOnly;
		_topicRestricted = other._topicRestricted;
		_hasKey          = other._hasKey;
		_hasUserLimit    = other._hasUserLimit;
		_userLimit       = other._userLimit;
		_members         = other._members;
		_operators       = other._operators;
		_invited         = other._invited;
	}
	return *this;
}

/*
** ~Channel()
** Destrutor trivial (os sets de int não têm gestão dinâmica de memória).
*/
Channel::~Channel() {}

/* ─── Name ─────────────────────────────────────────────────────────────────── */

/*
** getName
** Devolve: referência constante para o nome do canal (com '#').
*/
const std::string& Channel::getName() const { return _name; }

/* ─── Members ──────────────────────────────────────────────────────────────── */

/*
** addMember
** Adiciona um fd ao conjunto de membros do canal.
** Recebe: fd do cliente a adicionar.
*/
void Channel::addMember(int fd)
{
	_members.insert(fd);
}

/*
** removeMember
** Remove um fd dos membros, operadores e lista de convidados.
** É seguro chamar mesmo que o fd não esteja no canal.
** Recebe: fd do cliente a remover.
*/
void Channel::removeMember(int fd)
{
	_members.erase(fd);
	_operators.erase(fd);
	_invited.erase(fd);
}

/*
** isMember
** Verifica se um fd está actualmente no canal.
** Recebe: fd do cliente.
** Devolve: true se o fd é membro, false caso contrário.
*/
bool Channel::isMember(int fd) const
{
	return _members.count(fd) != 0;
}

/*
** isEmpty
** Indica se o canal não tem membros (pode ser destruído).
** Devolve: true se _members está vazio.
*/
bool Channel::isEmpty() const
{
	return _members.empty();
}

/*
** memberCount
** Devolve: número actual de membros.
*/
std::size_t Channel::memberCount() const
{
	return _members.size();
}

/*
** getMembers
** Devolve: referência constante ao conjunto de fds membros.
**          Usado pelo Server para iterar e fazer broadcast.
*/
const std::set<int>& Channel::getMembers() const
{
	return _members;
}

/* ─── Operators (+o) ───────────────────────────────────────────────────────── */

/*
** addOperator / removeOperator / isOperator
** Gerem o conjunto de operadores do canal (+o).
** Recebem: fd do cliente.
** isOperator devolve: true se o fd tem privilégios de operador.
*/
void Channel::addOperator(int fd)    { _operators.insert(fd); }
void Channel::removeOperator(int fd) { _operators.erase(fd); }
bool Channel::isOperator(int fd) const
{
	return _operators.count(fd) != 0;
}

/* ─── Invites (+i) ─────────────────────────────────────────────────────────── */

/*
** addInvite / removeInvite / isInvited
** Gerem a lista de fds explicitamente convidados (relevante com modo +i).
** Recebem: fd do cliente.
** isInvited devolve: true se o fd tem convite.
*/
void Channel::addInvite(int fd)    { _invited.insert(fd); }
void Channel::removeInvite(int fd) { _invited.erase(fd); }
bool Channel::isInvited(int fd) const
{
	return _invited.count(fd) != 0;
}

/* ─── Topic ────────────────────────────────────────────────────────────────── */

/*
** setTopic / getTopic / hasTopic
** Gerem o tópico actual do canal.
** setTopic recebe: string com o novo tópico (pode ser vazia para remover).
** getTopic devolve: referência constante ao tópico.
** hasTopic devolve: true se o tópico não está vazio.
*/
void               Channel::setTopic(const std::string& topic) { _topic = topic; }
const std::string& Channel::getTopic() const                   { return _topic; }
bool               Channel::hasTopic() const                   { return !_topic.empty(); }

/* ─── Mode +i ──────────────────────────────────────────────────────────────── */

/*
** setInviteOnly / isInviteOnly
** Controla o modo +i: apenas convidados podem fazer JOIN.
*/
void Channel::setInviteOnly(bool value) { _inviteOnly = value; }
bool Channel::isInviteOnly() const      { return _inviteOnly; }

/* ─── Mode +t ──────────────────────────────────────────────────────────────── */

/*
** setTopicRestricted / isTopicRestricted
** Controla o modo +t: apenas operadores podem alterar o tópico.
*/
void Channel::setTopicRestricted(bool value) { _topicRestricted = value; }
bool Channel::isTopicRestricted() const      { return _topicRestricted; }

/* ─── Mode +k ──────────────────────────────────────────────────────────────── */

/*
** setKey
** Define a chave do canal (password de JOIN). Activa o modo +k.
** Recebe: string com a chave.
*/
void Channel::setKey(const std::string& key)
{
	_key    = key;
	_hasKey = true;
}

/*
** removeKey
** Remove a chave do canal. Desactiva o modo +k.
*/
void Channel::removeKey()
{
	_key.clear();
	_hasKey = false;
}

/*
** hasKey / getKey
** hasKey devolve: true se o modo +k está activo.
** getKey devolve: referência constante à chave actual.
*/
bool               Channel::hasKey()  const { return _hasKey; }
const std::string& Channel::getKey()  const { return _key; }

/* ─── Mode +l ──────────────────────────────────────────────────────────────── */

/*
** setUserLimit
** Define o limite máximo de membros. Activa o modo +l.
** Recebe: número máximo de utilizadores.
*/
void Channel::setUserLimit(std::size_t limit)
{
	_userLimit    = limit;
	_hasUserLimit = true;
}

/*
** removeUserLimit
** Remove o limite de utilizadores. Desactiva o modo +l.
*/
void Channel::removeUserLimit()
{
	_userLimit    = 0;
	_hasUserLimit = false;
}

/*
** hasUserLimit / getUserLimit
** hasUserLimit devolve: true se o modo +l está activo.
** getUserLimit devolve: valor actual do limite.
*/
bool        Channel::hasUserLimit() const { return _hasUserLimit; }
std::size_t Channel::getUserLimit() const { return _userLimit; }

/* ─── Mode string ──────────────────────────────────────────────────────────── */

/*
** getModeString
** Constrói a string de modos activos no formato "+itk" para respostas 324
** e para broadcasts de MODE.
** Devolve: string de modos (começa sempre com '+', ou "+" se nenhum modo activo).
*/
std::string Channel::getModeString() const
{
	std::string modes = "+";
	if (_inviteOnly)      modes += 'i';
	if (_topicRestricted) modes += 't';
	if (_hasKey)          modes += 'k';
	if (_hasUserLimit)    modes += 'l';
	return modes;
}
