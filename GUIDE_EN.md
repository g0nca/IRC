# Complete Guide — ft_irc (42)

Everything that needed to be learned to build an IRC server in C++98:
protocols, networking, sockets, system functions and implementation decisions.

---

## 1. What is IRC and how the protocol works

**IRC (Internet Relay Chat)** is a real-time messaging protocol defined in
**RFC 1459 (1993)** and updated in **RFC 2812 (2000)**. It is a text-based protocol:
all communication consists of text lines terminated with `\r\n` (CR LF).

### 1.1 Client-server architecture

IRC uses a **client-server** architecture. The server is the central hub: clients
connect to it and communicate through it. There is no direct communication between
clients — everything goes through the server.

```
Client A ──→ IRC Server ←── Client B
                 │
             Channel #test
             (members: A, B)
```

### 1.2 IRC message format

Each message follows this format (fields in `[ ]` are optional):

```
[":" prefix " "] COMMAND [" " params] [" :" trailing] "\r\n"
```

- **prefix** — who sent it (`nick!user@host`). Clients rarely include it; the server
  adds it to messages sent on behalf of a client.
- **COMMAND** — can be a word (`NICK`, `JOIN`, `PRIVMSG`) or a 3-digit number
  (`001`, `433`).
- **params** — space-separated. Up to 14 normal parameters are allowed.
- **trailing** — starts with ` :` and may contain spaces. It is the last "field" of
  the message.

**Examples:**
```
PASS secret\r\n
NICK alice\r\n
USER alice 0 * :Alice Smith\r\n
JOIN #test\r\n
PRIVMSG #test :Hello everyone!\r\n
:alice!alice@127.0.0.1 PRIVMSG #test :Hello everyone!\r\n
:ircserv 001 alice :Welcome to the server!\r\n
```

### 1.3 Registration handshake

For a client to become "registered" (accepted by the server) it must send exactly
these three commands, in this order:

1. `PASS <password>` — server password
2. `NICK <nickname>` — unique name on the server
3. `USER <username> 0 * :<realname>` — user information

Only after receiving all three (and all valid) does the server send the welcome replies
`001 002 003 004` and the client is "registered". Before that, almost all other
commands are rejected.

### 1.4 Numeric replies

The server uses 3-digit numbers to respond:

| Range | Type |
|-------|------|
| 001–099 | Welcome and informational replies |
| 200–399 | Successful command replies |
| 400–499 | Client errors (bad input) |
| 500–599 | Server errors |

Key codes in this project:

| Code | Name | When |
|------|------|------|
| 001 | RPL_WELCOME | Registration complete |
| 331 | RPL_NOTOPIC | Channel has no topic |
| 332 | RPL_TOPIC | Channel has a topic |
| 353 | RPL_NAMREPLY | Channel member list |
| 366 | RPL_ENDOFNAMES | End of member list |
| 401 | ERR_NOSUCHNICK | Nick does not exist |
| 403 | ERR_NOSUCHCHANNEL | Channel does not exist |
| 433 | ERR_NICKNAMEINUSE | Nick already in use |
| 441 | ERR_USERNOTINCHANNEL | User is not in channel |
| 442 | ERR_NOTONCHANNEL | You are not in the channel |
| 451 | ERR_NOTREGISTERED | Not registered |
| 461 | ERR_NEEDMOREPARAMS | Missing parameters |
| 462 | ERR_ALREADYREGISTERED | Already registered |
| 464 | ERR_PASSWDMISMATCH | Wrong password |
| 471 | ERR_CHANNELISFULL | Channel full (+l) |
| 473 | ERR_INVITEONLYCHAN | Channel +i, no invite |
| 475 | ERR_BADCHANNELKEY | Wrong key (+k) |
| 482 | ERR_CHANOPRIVSNEEDED | Not a channel operator |

### 1.5 Channel modes

Modes control the behaviour of a channel. They are enabled with `+` and disabled with
`-`. The subject requires support for:

| Mode | Name | Description |
|------|------|-------------|
| `+i` | invite-only | Only invited clients may join |
| `+t` | topic restricted | Only operators may change the topic |
| `+k <key>` | channel key | A password is required to join |
| `+o <nick>` | operator | Grants/revokes operator privileges to a nick |
| `+l <n>` | user limit | Maximum number of members |

---

## 2. Networking — TCP/IP

### 2.1 The TCP/IP model

IRC runs over **TCP (Transmission Control Protocol)**, which in turn runs over
**IP (Internet Protocol)**. It helps to understand the layers:

```
Application — the IRC protocol (text lines \r\n)
Transport   — TCP (guarantees ordered, lossless delivery)
Network     — IP (addressing and routing)
Link        — Ethernet, Wi-Fi, etc.
```

### 2.2 TCP — what it guarantees

TCP is a **connection-oriented** and **stream** protocol:

- **Connection-oriented** — before exchanging data, client and server perform a
  handshake (SYN / SYN-ACK / ACK) to establish the connection.
- **Stream** — there are no visible "messages" or "packets" at the application level.
  It is a continuous flow of bytes. A single `recv()` call may return half an IRC
  message, or two and a half messages. This is why **framing** is necessary (see
  section 4.2).
- **Guarantees delivery** — if a packet is lost, TCP retransmits it automatically.
- **Guarantees order** — bytes always arrive in the order they were sent.

### 2.3 Addressing — IP and ports

Each TCP connection is identified by 4 values:
- Source IP + source port (client side)
- Destination IP + destination port (server side)

**Ports** are 16-bit numbers (1–65535). IRC conventionally uses port 6667. Ports below
1024 require root privileges. In testing, any port ≥ 1024 can be used.

### 2.4 Non-blocking I/O

By default, calls like `recv()` and `send()` are **blocking**: they wait until data is
available. On a server with multiple clients, that would be catastrophic — one slow
client would block all others.

The solution is to make sockets **non-blocking** with:
```c
fcntl(fd, F_SETFL, O_NONBLOCK);
```

With non-blocking sockets:
- `recv()` returns `-1` with `errno == EAGAIN` if there is no data (instead of
  blocking).
- `send()` returns `-1` with `errno == EAGAIN` if the kernel's send buffer is full
  (instead of blocking).
- `poll()` must be used to know *when* a socket is ready for reading or writing.

---

## 3. Sockets — what they are and how to use them

A **socket** is a network communication endpoint. On Unix/Linux, a socket is
represented by a **file descriptor** (fd) — an integer that identifies an open
resource. Just as a file is read with `read(fd, ...)`, a socket is read with
`recv(fd, ...)`.

### 3.1 Server socket lifecycle

```
socket()        — create the fd
setsockopt()    — configure options (SO_REUSEADDR)
bind()          — bind to a port
listen()        — start listening for connections
accept()        — accept a connection → creates a new fd for the client
recv() / send() — communicate with the client
close()         — close the connection
```

### 3.2 Client socket lifecycle

```
socket()        — create the fd
connect()       — connect to the server (address + port)
send() / recv() — communicate
close()         — close
```

Our server never calls `connect()` — that is the responsibility of the IRC client
(HexChat, nc, etc.).

### 3.3 Functions in detail

#### `socket(domain, type, protocol)`

Creates a new socket and returns a fd.

```c
int fd = socket(AF_INET, SOCK_STREAM, 0);
```

- `AF_INET` — IPv4 address family (use `AF_INET6` for IPv6).
- `SOCK_STREAM` — TCP type (connection-oriented stream). `SOCK_DGRAM` would be UDP.
- `0` — let the kernel pick the protocol (TCP for SOCK_STREAM).

#### `setsockopt(fd, level, optname, &value, size)`

Sets options on the socket. The most important one in this project:

```c
int opt = 1;
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

`SO_REUSEADDR` allows a port to be reused immediately after the server exits.
Without it, restarting the server causes `bind()` to fail with "Address already in
use" because the kernel still holds the port in the TIME_WAIT state.

#### `bind(fd, addr, addrlen)`

Binds the socket to a local address and port.

```c
struct sockaddr_in addr;
memset(&addr, 0, sizeof(addr));
addr.sin_family      = AF_INET;
addr.sin_addr.s_addr = INADDR_ANY;   // listen on all interfaces
addr.sin_port        = htons(6667);  // convert to big-endian (network byte order)

bind(fd, (struct sockaddr*)&addr, sizeof(addr));
```

`htons()` — *host to network short* — converts a 16-bit integer from the processor's
byte order (little-endian on x86) to big-endian (required by the network protocol).
Without this conversion the port number would be wrong on little-endian machines.

#### `listen(fd, backlog)`

Puts the socket into listening mode. `backlog` is the maximum number of pending
connections (waiting for `accept()`) in the queue.

```c
listen(fd, 128);
```

#### `accept(fd, addr, addrlen)`

Accepts a pending connection. **Blocks** until a connection arrives (unless the socket
is non-blocking). Returns a **new fd** for communicating specifically with that client.

```c
struct sockaddr_in clientAddr;
socklen_t addrLen = sizeof(clientAddr);
int clientFd = accept(listenFd, (struct sockaddr*)&clientAddr, &addrLen);
```

The original fd (`listenFd`) keeps listening; `clientFd` is the communication channel
with that specific client.

#### `recv(fd, buf, len, flags)`

Reads data from the socket into a buffer. With a non-blocking socket it may return:
- `n > 0` — read `n` bytes.
- `0` — the client closed the connection (EOF).
- `-1` with `errno == EAGAIN` — no data right now, try again later.
- `-1` with another errno — real error.

```c
char buf[4096];
ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
```

#### `send(fd, buf, len, flags)`

Sends data through the socket. It may send fewer bytes than requested (partial send),
so the correct approach is to keep an output buffer and check `sent < len`.

```c
ssize_t sent = send(clientFd, message.c_str(), message.size(), 0);
```

#### `close(fd)`

Closes the socket and releases the fd. After `close()`, the kernel sends a FIN to the
other side, signalling the end of the connection.

#### `fcntl(fd, F_SETFL, O_NONBLOCK)`

Makes the fd non-blocking. In the subject, this is the only `fcntl` flag allowed.

```c
fcntl(fd, F_SETFL, O_NONBLOCK);
```

#### `inet_ntoa(addr.sin_addr)`

Converts an IP address from binary format (`in_addr`) to a readable string
(`"127.0.0.1"`). Used to log the IP of the connecting client.

---

## 4. `poll()` — I/O multiplexing

### 4.1 The problem

With multiple clients, we cannot call blocking `recv()` on one client (we get stuck).
We need to monitor **all** fds simultaneously and act only when there is activity.

### 4.2 How `poll()` works

```c
#include <poll.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

- `fds` — array of `struct pollfd`, one per fd to watch.
- `nfds` — number of elements in the array.
- `timeout` — milliseconds to wait (-1 = block indefinitely, 0 = do not wait).
- Returns: number of fds with ready events, 0 on timeout, -1 on error.

```c
struct pollfd {
    int   fd;       // file descriptor to watch
    short events;   // events we want (POLLIN, POLLOUT, ...)
    short revents;  // events that occurred (filled in by the kernel)
};
```

**Important events:**

| Flag | Meaning |
|------|---------|
| `POLLIN` | Data available to read (or new connection on listen socket) |
| `POLLOUT` | Socket is ready for writing |
| `POLLERR` | Error on the fd |
| `POLLHUP` | The other side closed the connection |
| `POLLNVAL` | Invalid fd |

### 4.3 The server loop

```c
while (running) {
    int ready = poll(pollFds.data(), pollFds.size(), 1000);

    // fd[0] = listening socket
    if (pollFds[0].revents & POLLIN)
        acceptNewClient();

    // fd[1..n] = clients
    for (size_t i = 1; i < pollFds.size(); ++i) {
        if (pollFds[i].revents & POLLIN)
            handleClientData(pollFds[i].fd);   // recv
        if (pollFds[i].revents & POLLOUT)
            flushClientOutput(pollFds[i].fd);  // send
        if (pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            disconnectClient(pollFds[i].fd);
    }
}
```

### 4.4 Why POLLOUT?

`send()` may not be able to send all data at once (kernel buffer full). The correct
approach is:
1. Store data in a per-client output buffer.
2. Enable `POLLOUT` for the fd.
3. When `poll()` signals `POLLOUT`, call `send()` with whatever remains.
4. When the buffer is empty, disable `POLLOUT` (save CPU).

If `POLLOUT` is never enabled, data is lost. If it is always enabled, `poll()` wakes
up constantly (unnecessary busy loop).

---

## 5. Message framing (partial reads)

TCP is a **byte stream** — there is no guarantee that a single `recv()` call returns
exactly one IRC message. Several things can happen:

- **Fragmentation**: `"NICK ali"` in one recv, `"ce\r\n"` in the next.
- **Aggregation**: `"NICK alice\r\nUSER alice 0 * :Al"` in a single recv.

The solution is a **per-client input buffer**: every time `recv()` returns data, we
append it to the buffer. Then we extract complete lines (terminated by `\r\n`) one by
one.

```cpp
void appendToInBuffer(const std::string& data) {
    _inBuffer += data;
}

bool extractMessage(std::string& lineOut) {
    size_t pos = _inBuffer.find("\r\n");
    if (pos == std::string::npos)
        return false;          // line still incomplete
    lineOut = _inBuffer.substr(0, pos);
    _inBuffer.erase(0, pos + 2);
    return true;
}
```

The subject explicitly tests this with `nc` and `Ctrl+D` to send parts of a command
in separate fragments.

---

## 6. Server architecture

### 6.1 Two-layer design

The project is split into two layers with a clear interface:

```
NETWORK LAYER (Person A)           PROTOCOL LAYER (Person B)
────────────────────────           ─────────────────────────
Server + Client                    CommandHandler + Channel
socket, poll, recv, send           parser, auth, channels, commands
```

The meeting point is:
```cpp
CommandHandler::dispatch(Client& client, const Message& msg);
```

### 6.2 Main data structures

**`struct Message`** — parsed form of one IRC line:
```cpp
struct Message {
    std::string              prefix;      // who sent it (rarely used server-side)
    std::string              command;     // "NICK", "JOIN", "001", ...
    std::vector<std::string> params;      // normal parameters
    std::string              trailing;    // text after " :"
    bool                     hasTrailing; // distinguishes "" from "no trailing"
};
```

**`class Client`** — a connected TCP user:
```cpp
class Client {
    int         _fd;           // socket fd
    std::string _nickname;
    std::string _username;
    std::string _realname;
    std::string _hostname;
    std::string _inBuffer;     // received bytes not yet processed
    std::string _outBuffer;    // bytes pending to be sent
    bool        _passReceived; // received a correct PASS
    bool        _registered;   // PASS + NICK + USER complete
};
```

**`class Channel`** — a chat room:
```cpp
class Channel {
    std::string   _name;
    std::string   _topic;
    std::string   _key;            // mode +k
    bool          _inviteOnly;     // mode +i
    bool          _topicRestricted;// mode +t
    bool          _hasKey;
    bool          _hasUserLimit;   // mode +l
    std::size_t   _userLimit;
    std::set<int> _members;        // fds of members
    std::set<int> _operators;      // fds of operators
    std::set<int> _invited;        // fds of invited clients
};
```

**`class Server`** — owns everything:
```cpp
class Server {
    int                              _listenFd;
    int                              _port;
    std::string                      _password;
    std::vector<struct pollfd>       _pollFds;
    std::map<int, Client*>           _clients;   // fd → Client
    std::map<std::string, Channel*>  _channels;  // name → Channel
    CommandHandler                   _commands;
    static bool                      _running;
};
```

### 6.3 Lifecycle of a message

```
1. poll() → POLLIN on a client fd
2. recv() → bytes arrive in the Client's input buffer
3. extractMessage() → extracts a complete line ("\r\n")
4. parseMessage()   → line → struct Message
5. dispatch()       → calls the correct handler (handleJoin, handlePrivmsg, ...)
6. handler calls server.sendToClient(fd, reply)
7. sendToClient → appendToOutBuffer + enable POLLOUT
8. poll() → POLLOUT on the fd
9. send() → sends bytes from the output buffer
```

---

## 7. Signals

### SIGPIPE

When the server tries to `send()` to a client that has already closed the connection,
the kernel delivers `SIGPIPE` to the process, which by default terminates it
immediately. To prevent this:

```c
signal(SIGPIPE, SIG_IGN);
```

With `SIGPIPE` ignored, `send()` returns `-1` with `errno == EPIPE` and the server can
handle the error gracefully.

### SIGINT (Ctrl+C)

To allow the server to shut down cleanly (close sockets, free memory) when the user
presses Ctrl+C:

```c
signal(SIGINT, handlerFunction);
// in the handler:
Server::_running = false;
```

The main loop checks `_running` on each iteration and exits, allowing the `Server`
destructor to clean everything up.

---

## 8. IRCv3 — CAP (capability negotiation)

Modern clients such as HexChat send `CAP LS 302` immediately upon connecting. This
command is part of **IRCv3**, a modern extension to the IRC protocol.

The client is asking: *"What advanced capabilities do you support?"*. If the server
does not reply, HexChat waits indefinitely — it does not proceed to login.

Our minimal solution:
```
Client → CAP LS 302
Server → :ircserv CAP * LS :    (empty list — no capabilities)
Client → PASS / NICK / USER     (proceeds to normal login)
```

If the client requests a specific capability (`CAP REQ :sasl`), the server replies
with `NAK` (reject):
```
Client → CAP REQ :sasl
Server → :ircserv CAP * NAK :sasl
```

---

## 9. Relevant C structures

```c
// IPv4 address
struct sockaddr_in {
    sa_family_t    sin_family;   // AF_INET
    in_port_t      sin_port;     // port in network byte order (use htons())
    struct in_addr sin_addr;     // IP address
};

struct in_addr {
    uint32_t s_addr;             // address in network byte order
};

// fd monitoring with poll()
struct pollfd {
    int   fd;       // file descriptor
    short events;   // events to watch (POLLIN, POLLOUT, ...)
    short revents;  // events that occurred (filled in by the kernel)
};
```

---

## 10. System functions used in the project

| Function | Header | Purpose |
|----------|--------|---------|
| `socket()` | `<sys/socket.h>` | Create a socket |
| `setsockopt()` | `<sys/socket.h>` | Set socket options |
| `bind()` | `<sys/socket.h>` | Bind socket to address/port |
| `listen()` | `<sys/socket.h>` | Put socket in listening mode |
| `accept()` | `<sys/socket.h>` | Accept a new connection |
| `recv()` | `<sys/socket.h>` | Read data from socket |
| `send()` | `<sys/socket.h>` | Send data through socket |
| `close()` | `<unistd.h>` | Close fd/socket |
| `fcntl()` | `<fcntl.h>` | Control fd flags (O_NONBLOCK) |
| `poll()` | `<poll.h>` | Multiplex I/O on multiple fds |
| `htons()` | `<arpa/inet.h>` | Host to network byte order (16-bit) |
| `inet_ntoa()` | `<arpa/inet.h>` | Binary IP → readable string |
| `memset()` | `<cstring>` | Initialise memory block to zero |
| `signal()` | `<csignal>` | Set handler for a signal |
| `atoi()` | `<cstdlib>` | String → integer |

---

## 11. Common mistakes and how to avoid them

### `EAGAIN` / `EWOULDBLOCK`
With non-blocking sockets, `recv()` and `send()` may return `-1` with this errno.
**This is not an error** — it simply means there is no data right now. Treat it as
normal and continue.

### Partial send with `send()`
`send()` may send fewer bytes than requested. Always check the return value and save
the remainder in a buffer to send later (when `POLLOUT` is signalled).

### Fds growing in the `poll()` vector
When a client disconnects during the loop over `_pollFds`, the vector shrinks. Use an
index instead of an iterator and do not increment when an element was removed.

### Modifying `_pollFds` during iteration
`acceptNewClient()` adds elements to the vector. Therefore the loop uses `.size()` on
each iteration (it does not cache the size before the loop).

### Memory leaks from `Client*` and `Channel*`
The `Server` owns all pointers. The destructor and `disconnectClient()` must `delete`
them and erase them from their maps. Verify with `valgrind` before the defence.

### Closing fd before erasing from map
Always `close(fd)` before `delete client` and `_clients.erase(fd)`. Never use the fd
after `close()`.
