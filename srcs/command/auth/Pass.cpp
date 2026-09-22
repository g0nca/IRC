/*
** Pass.cpp — Handler for the PASS command.
**
** PASS <password>
** Must be the first command sent by the client. Sets whether the password
** is correct; without a valid PASS the registration cannot complete.
*/

#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Replies.hpp"

/*
** CommandHandler::handlePass
** Validates the supplied password against the server password.
** Can only be called before registration is complete.
**
** Receives: client — who sent PASS.
**           msg    — msg.params[0] must contain the password.
**
** Possible replies:
**   462 ERR_ALREADYREGISTERED — already registered
**   461 ERR_NEEDMOREPARAMS    — no argument supplied
**   464 ERR_PASSWDMISMATCH    — wrong password
**   (none on success — the flag is stored internally)
*/
void CommandHandler::handlePass(Client& client, const Message& msg)
{
	/* Already registered: cannot re-register */
	if (client.isRegistered())
	{
		_server.sendToClient(client.getFd(),
		                     ERR_ALREADYREGISTERED(client.getNickname()));
		return;
	}

	/* No argument */
	if (msg.params.empty())
	{
		std::string nick = client.getNickname().empty() ? "*" : client.getNickname();
		_server.sendToClient(client.getFd(), ERR_NEEDMOREPARAMS(nick, "PASS"));
		return;
	}

	/* Check the password */
	if (msg.params[0] != _server.getPassword())
	{
		std::string nick = client.getNickname().empty() ? "*" : client.getNickname();
		_server.sendToClient(client.getFd(), ERR_PASSWDMISMATCH(nick));
		return;
	}

	/* Correct password: set the flag */
	client.setPassReceived(true);
}
