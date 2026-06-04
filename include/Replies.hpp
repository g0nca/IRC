#ifndef REPLIES_HPP
#define REPLIES_HPP

#include <string>

/**
 * @file Replies.hpp
 * @brief Builders for the IRC numeric replies used by the mandatory part.
 *
 * Each macro returns a std::string already terminated by "\r\n", ready to be
 * passed to Server::sendToClient(). They assume std::string arguments.
 *
 * Format of a numeric reply:   :<server> <code> <nick> <params> :<text>\r\n
 *
 * NOTE (Person B): treat these as a starting template. Verify the exact text
 * and parameters against your reference client (irssi / WeeChat / HexChat),
 * because clients can be picky about spacing and order.
 */

#define SERVER_NAME "ircserv"
#define CRLF "\r\n"

// ---- connection registration (sent right after PASS+NICK+USER succeed) ----
#define RPL_WELCOME(nick) \
	(std::string(":" SERVER_NAME " 001 ") + (nick) + " :Welcome to the IRC network, " + (nick) + CRLF)
#define RPL_YOURHOST(nick) \
	(std::string(":" SERVER_NAME " 002 ") + (nick) + " :Your host is " SERVER_NAME ", running version 1.0" CRLF)
#define RPL_CREATED(nick) \
	(std::string(":" SERVER_NAME " 003 ") + (nick) + " :This server was created for the 42 ft_irc project" CRLF)
#define RPL_MYINFO(nick) \
	(std::string(":" SERVER_NAME " 004 ") + (nick) + " " SERVER_NAME " 1.0 o itkol" CRLF)

// ---- channel / topic / names ----
#define RPL_CHANNELMODEIS(nick, chan, modes) \
	(std::string(":" SERVER_NAME " 324 ") + (nick) + " " + (chan) + " " + (modes) + CRLF)
#define RPL_NOTOPIC(nick, chan) \
	(std::string(":" SERVER_NAME " 331 ") + (nick) + " " + (chan) + " :No topic is set" CRLF)
#define RPL_TOPIC(nick, chan, topic) \
	(std::string(":" SERVER_NAME " 332 ") + (nick) + " " + (chan) + " :" + (topic) + CRLF)
#define RPL_INVITING(nick, target, chan) \
	(std::string(":" SERVER_NAME " 341 ") + (nick) + " " + (target) + " " + (chan) + CRLF)
#define RPL_NAMREPLY(nick, chan, names) \
	(std::string(":" SERVER_NAME " 353 ") + (nick) + " = " + (chan) + " :" + (names) + CRLF)
#define RPL_ENDOFNAMES(nick, chan) \
	(std::string(":" SERVER_NAME " 366 ") + (nick) + " " + (chan) + " :End of /NAMES list" CRLF)

// ---- errors ----
#define ERR_NOSUCHNICK(nick, target) \
	(std::string(":" SERVER_NAME " 401 ") + (nick) + " " + (target) + " :No such nick/channel" CRLF)
#define ERR_NOSUCHCHANNEL(nick, chan) \
	(std::string(":" SERVER_NAME " 403 ") + (nick) + " " + (chan) + " :No such channel" CRLF)
#define ERR_NORECIPIENT(nick, cmd) \
	(std::string(":" SERVER_NAME " 411 ") + (nick) + " :No recipient given (" + (cmd) + ")" CRLF)
#define ERR_NOTEXTTOSEND(nick) \
	(std::string(":" SERVER_NAME " 412 ") + (nick) + " :No text to send" CRLF)
#define ERR_UNKNOWNCOMMAND(nick, cmd) \
	(std::string(":" SERVER_NAME " 421 ") + (nick) + " " + (cmd) + " :Unknown command" CRLF)
#define ERR_NONICKNAMEGIVEN(nick) \
	(std::string(":" SERVER_NAME " 431 ") + (nick) + " :No nickname given" CRLF)
#define ERR_ERRONEUSNICKNAME(nick, bad) \
	(std::string(":" SERVER_NAME " 432 ") + (nick) + " " + (bad) + " :Erroneous nickname" CRLF)
#define ERR_NICKNAMEINUSE(nick, bad) \
	(std::string(":" SERVER_NAME " 433 ") + (nick) + " " + (bad) + " :Nickname is already in use" CRLF)
#define ERR_USERNOTINCHANNEL(nick, target, chan) \
	(std::string(":" SERVER_NAME " 441 ") + (nick) + " " + (target) + " " + (chan) + " :They aren't on that channel" CRLF)
#define ERR_NOTONCHANNEL(nick, chan) \
	(std::string(":" SERVER_NAME " 442 ") + (nick) + " " + (chan) + " :You're not on that channel" CRLF)
#define ERR_USERONCHANNEL(nick, target, chan) \
	(std::string(":" SERVER_NAME " 443 ") + (nick) + " " + (target) + " " + (chan) + " :is already on channel" CRLF)
#define ERR_NOTREGISTERED(nick) \
	(std::string(":" SERVER_NAME " 451 ") + (nick) + " :You have not registered" CRLF)
#define ERR_NEEDMOREPARAMS(nick, cmd) \
	(std::string(":" SERVER_NAME " 461 ") + (nick) + " " + (cmd) + " :Not enough parameters" CRLF)
#define ERR_ALREADYREGISTERED(nick) \
	(std::string(":" SERVER_NAME " 462 ") + (nick) + " :You may not reregister" CRLF)
#define ERR_PASSWDMISMATCH(nick) \
	(std::string(":" SERVER_NAME " 464 ") + (nick) + " :Password incorrect" CRLF)
#define ERR_CHANNELISFULL(nick, chan) \
	(std::string(":" SERVER_NAME " 471 ") + (nick) + " " + (chan) + " :Cannot join channel (+l)" CRLF)
#define ERR_UNKNOWNMODE(nick, ch) \
	(std::string(":" SERVER_NAME " 472 ") + (nick) + " " + (ch) + " :is unknown mode char to me" CRLF)
#define ERR_INVITEONLYCHAN(nick, chan) \
	(std::string(":" SERVER_NAME " 473 ") + (nick) + " " + (chan) + " :Cannot join channel (+i)" CRLF)
#define ERR_BADCHANNELKEY(nick, chan) \
	(std::string(":" SERVER_NAME " 475 ") + (nick) + " " + (chan) + " :Cannot join channel (+k)" CRLF)
#define ERR_CHANOPRIVSNEEDED(nick, chan) \
	(std::string(":" SERVER_NAME " 482 ") + (nick) + " " + (chan) + " :You're not channel operator" CRLF)

#endif // REPLIES_HPP
