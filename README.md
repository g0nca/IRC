*This project has been created as part of the 42 curriculum by ggomes-v, joaomart.*

# ft_irc

## Description

ft_irc is a fully functional IRC server written in C++98, developed as part of the 42 school curriculum. The server handles multiple simultaneous clients over TCP using a single non-blocking `poll()` event loop — no threads, no `fork()`.

The server implements the core IRC protocol (RFC 1459) and is compatible with standard IRC clients such as HexChat, irssi, and WeeChat. It supports:

- Password-protected connection (`PASS`)
- Nick and user registration (`NICK`, `USER`)
- Text channels with broadcast messaging (`JOIN`, `PART`, `PRIVMSG`, `NOTICE`)
- Channel topic management (`TOPIC`)
- Channel operator commands (`KICK`, `INVITE`, `MODE`)
- Channel modes: `+i` (invite-only), `+t` (topic restricted), `+k` (key/password), `+o` (operator), `+l` (user limit)
- Partial packet reassembly (TCP stream framing)
- IRCv3 capability negotiation (`CAP LS`) with graceful rejection

## Instructions

**Requirements:** a C++98-compatible compiler (`g++` or `clang++`) and `make`.

**Compilation:**
```
make
```

**Running the server:**
```
./ircserv <port> <password>
```

- `<port>` — TCP port to listen on (1–65535)
- `<password>` — connection password that every client must send via `PASS`

**Example:**
```
./ircserv 6667 secret
```

**Connecting with a reference client (HexChat):**
1. Open HexChat → Network List → Add network → set server to `localhost/6667`
2. In the network settings, enter `secret` in the Password field
3. Connect — HexChat sends `PASS`, `NICK` and `USER` automatically
4. Use `/join #channel` to join a channel

**Connecting with netcat (for testing):**
```
nc localhost 6667
PASS secret
NICK testuser
USER testuser 0 * :Real Name
```

**Makefile targets:**
| Target  | Description                        |
|---------|------------------------------------|
| `all`   | Build the `ircserv` binary         |
| `clean` | Remove object files                |
| `fclean`| Remove object files and binary     |
| `re`    | `fclean` + `all`                   |

## Resources

**IRC Protocol**
- [RFC 1459](https://datatracker.ietf.org/doc/html/rfc1459) — original IRC protocol specification
- [RFC 2812](https://datatracker.ietf.org/doc/html/rfc2812) — IRC client protocol update
- [Modern IRC documentation](https://modern.ircdocs.horse) — readable reference for numeric replies and commands

**Networking (C/C++)**
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) — sockets, `bind`, `listen`, `accept`, `recv`, `send`
- `man 2 poll`, `man 2 fcntl`, `man 2 recv`, `man 2 send`

**Use of AI (Claude) in this project**

Claude was used as a development assistant throughout the project. Specific tasks where AI assistance was applied:

- **Architecture planning** — discussing the split between the network layer (Person A: `Server`, `Client`, `poll()` loop) and the protocol layer (Person B: `CommandHandler`, `Channel`, commands), and defining the contract interface between both.
- **Debugging** — diagnosing why `nc` (netcat) did not trigger command processing (the `\r\n` vs `\n` line-terminator mismatch in `extractMessage`), and diagnosing why HexChat would not complete the login handshake (missing `CAP LS` response causing the client to stall).
- **Code review** — verifying correctness of the `poll()` loop, the non-blocking I/O model, and the IRC numeric replies.
- **Comment translation** — translating all in-code comments from Portuguese to English to meet the subject's language requirement.

All code in this repository was written by the team and reviewed before submission. AI-generated suggestions were studied and validated — no code was committed that the authors cannot explain.
