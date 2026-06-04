#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <cstddef>

/**
 * @class Channel
 * @brief A chat room. Holds its members, its operators and its modes.
 *
 * Owned by the Server in std::map<std::string, Channel*> keyed by name.
 * Members are stored as file descriptors (int); the Server resolves an fd to
 * a Client* when it needs to send. This avoids dangling pointers when a client
 * disconnects: we only have to erase an int from the sets.
 *
 * Modes required by the subject (channel-operator only):
 *   +i  invite-only          -> _inviteOnly
 *   +t  topic restricted     -> _topicRestricted (only ops may change TOPIC)
 *   +k  key / password       -> _hasKey + _key
 *   +o  give/take operator   -> _operators set
 *   +l  user limit           -> _hasUserLimit + _userLimit
 */
class Channel
{
private:
	std::string   _name;            // Channel name, including the leading '#'.
	std::string   _topic;           // Current topic. Empty string = no topic.
	std::string   _key;             // Mode +k password. Meaningful only if _hasKey.
	bool          _inviteOnly;      // Mode +i: only invited clients may JOIN.
	bool          _topicRestricted; // Mode +t: only operators may change the topic.
	bool          _hasKey;          // Whether mode +k is currently set.
	bool          _hasUserLimit;    // Whether mode +l is currently set.
	std::size_t   _userLimit;       // Mode +l value: max number of members.
	std::set<int> _members;         // fds of every client currently in the channel.
	std::set<int> _operators;       // fds of clients holding the +o privilege.
	std::set<int> _invited;         // fds explicitly invited (needed when +i).

public:
	// --- Orthodox Canonical Form ---
	Channel();
	explicit Channel(const std::string& name);
	Channel(const Channel& other);
	Channel& operator=(const Channel& other);
	~Channel();

	const std::string& getName() const;

	// --- members ---
	void                 addMember(int fd);
	void                 removeMember(int fd);   // also drops it from ops & invites
	bool                 isMember(int fd) const;
	bool                 isEmpty() const;
	std::size_t          memberCount() const;
	const std::set<int>& getMembers() const;     // Server iterates this to broadcast

	// --- operators (+o) ---
	void addOperator(int fd);
	void removeOperator(int fd);
	bool isOperator(int fd) const;

	// --- invites (+i) ---
	void addInvite(int fd);
	void removeInvite(int fd);
	bool isInvited(int fd) const;

	// --- topic ---
	void               setTopic(const std::string& topic);
	const std::string& getTopic() const;
	bool               hasTopic() const;

	// --- mode +i ---
	void setInviteOnly(bool value);
	bool isInviteOnly() const;

	// --- mode +t ---
	void setTopicRestricted(bool value);
	bool isTopicRestricted() const;

	// --- mode +k ---
	void               setKey(const std::string& key);
	void               removeKey();
	bool               hasKey() const;
	const std::string& getKey() const;

	// --- mode +l ---
	void        setUserLimit(std::size_t limit);
	void        removeUserLimit();
	bool        hasUserLimit() const;
	std::size_t getUserLimit() const;

	/** @brief Builds a mode string like "+itk" for MODE / 324 replies. */
	std::string getModeString() const;
};

#endif // CHANNEL_HPP
