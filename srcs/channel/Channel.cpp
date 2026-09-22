/*
** Channel.cpp — IRC chat room.
**
** Stores members, operators, invited clients and all channel modes.
** Members are represented as file descriptors (int); the Server resolves
** an fd to a Client* when it needs to send. This avoids dangling pointers
** when a client disconnects.
*/

#include "Channel.hpp"

/* ─── Orthodox Canonical Form ─────────────────────────────────────────────── */

/*
** Channel()
** Default constructor. Creates a channel with no name and all modes off.
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
** Main constructor. Creates a channel with the given name (must include '#').
** Receives: channel name.
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
** Copy constructor.
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
** Copy assignment. Guards against self-assignment.
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
** Trivial destructor (the int sets have no dynamic memory management).
*/
Channel::~Channel() {}

/* ─── Name ─────────────────────────────────────────────────────────────────── */

/*
** getName
** Returns: const reference to the channel name (including '#').
*/
const std::string& Channel::getName() const { return _name; }

/* ─── Members ──────────────────────────────────────────────────────────────── */

/*
** addMember
** Adds an fd to the channel member set.
** Receives: fd of the client to add.
*/
void Channel::addMember(int fd)
{
	_members.insert(fd);
}

/*
** removeMember
** Removes an fd from members, operators and the invite list.
** Safe to call even if the fd is not in the channel.
** Receives: fd of the client to remove.
*/
void Channel::removeMember(int fd)
{
	_members.erase(fd);
	_operators.erase(fd);
	_invited.erase(fd);
}

/*
** isMember
** Checks whether an fd is currently in the channel.
** Receives: client fd.
** Returns: true if the fd is a member, false otherwise.
*/
bool Channel::isMember(int fd) const
{
	return _members.count(fd) != 0;
}

/*
** isEmpty
** Indicates whether the channel has no members (can be destroyed).
** Returns: true if _members is empty.
*/
bool Channel::isEmpty() const
{
	return _members.empty();
}

/*
** memberCount
** Returns: current number of members.
*/
std::size_t Channel::memberCount() const
{
	return _members.size();
}

/*
** getMembers
** Returns: const reference to the set of member fds.
**          Used by the Server to iterate and broadcast.
*/
const std::set<int>& Channel::getMembers() const
{
	return _members;
}

/* ─── Operators (+o) ───────────────────────────────────────────────────────── */

/*
** addOperator / removeOperator / isOperator
** Manage the channel operator set (+o).
** Receive: client fd.
** isOperator returns: true if the fd holds operator privileges.
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
** Manage the list of explicitly invited fds (relevant with mode +i).
** Receive: client fd.
** isInvited returns: true if the fd has an invite.
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
** Manage the current channel topic.
** setTopic receives: string with the new topic (can be empty to clear it).
** getTopic returns: const reference to the topic.
** hasTopic returns: true if the topic is not empty.
*/
void               Channel::setTopic(const std::string& topic) { _topic = topic; }
const std::string& Channel::getTopic() const                   { return _topic; }
bool               Channel::hasTopic() const                   { return !_topic.empty(); }

/* ─── Mode +i ──────────────────────────────────────────────────────────────── */

/*
** setInviteOnly / isInviteOnly
** Controls mode +i: only invited clients may JOIN.
*/
void Channel::setInviteOnly(bool value) { _inviteOnly = value; }
bool Channel::isInviteOnly() const      { return _inviteOnly; }

/* ─── Mode +t ──────────────────────────────────────────────────────────────── */

/*
** setTopicRestricted / isTopicRestricted
** Controls mode +t: only operators may change the topic.
*/
void Channel::setTopicRestricted(bool value) { _topicRestricted = value; }
bool Channel::isTopicRestricted() const      { return _topicRestricted; }

/* ─── Mode +k ──────────────────────────────────────────────────────────────── */

/*
** setKey
** Sets the channel key (JOIN password). Activates mode +k.
** Receives: string with the key.
*/
void Channel::setKey(const std::string& key)
{
	_key    = key;
	_hasKey = true;
}

/*
** removeKey
** Removes the channel key. Deactivates mode +k.
*/
void Channel::removeKey()
{
	_key.clear();
	_hasKey = false;
}

/*
** hasKey / getKey
** hasKey returns: true if mode +k is currently active.
** getKey returns: const reference to the current key.
*/
bool               Channel::hasKey()  const { return _hasKey; }
const std::string& Channel::getKey()  const { return _key; }

/* ─── Mode +l ──────────────────────────────────────────────────────────────── */

/*
** setUserLimit
** Sets the maximum member count. Activates mode +l.
** Receives: maximum number of users.
*/
void Channel::setUserLimit(std::size_t limit)
{
	_userLimit    = limit;
	_hasUserLimit = true;
}

/*
** removeUserLimit
** Removes the user limit. Deactivates mode +l.
*/
void Channel::removeUserLimit()
{
	_userLimit    = 0;
	_hasUserLimit = false;
}

/*
** hasUserLimit / getUserLimit
** hasUserLimit returns: true if mode +l is currently active.
** getUserLimit returns: current limit value.
*/
bool        Channel::hasUserLimit() const { return _hasUserLimit; }
std::size_t Channel::getUserLimit() const { return _userLimit; }

/* ─── Mode string ──────────────────────────────────────────────────────────── */

/*
** getModeString
** Builds the active mode string in "+itk" format for 324 replies
** and MODE broadcasts.
** Returns: mode string (always starts with '+', or just "+" if no modes active).
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
