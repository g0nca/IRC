# Guia Completo — ft_irc (42)

Tudo o que foi preciso aprender para construir um servidor IRC em C++98:
protocolos, redes, sockets, funções de sistema e decisões de implementação.

---

## 1. O que é o IRC e como funciona o protocolo

**IRC (Internet Relay Chat)** é um protocolo de mensagens em tempo real definido no
**RFC 1459 (1993)** e actualizado no **RFC 2812 (2000)**. É um protocolo baseado em
texto: toda a comunicação é feita com linhas de texto terminadas em `\r\n` (CR LF).

### 1.1 Arquitectura cliente-servidor

O IRC usa uma arquitectura **cliente-servidor**. O servidor é o ponto central: os
clientes ligam-se a ele e comunicam através dele. Não há comunicação directa entre
clientes — tudo passa pelo servidor.

```
Cliente A ──→ Servidor IRC ←── Cliente B
                   │
               Canal #test
               (membros: A, B)
```

### 1.2 Formato de uma mensagem IRC

Cada mensagem tem o seguinte formato (os campos entre `[ ]` são opcionais):

```
[":" prefixo " "] COMANDO [" " parâmetros] [" :" trailing] "\r\n"
```

- **prefixo** — quem enviou (`nick!user@host`). Os clientes raramente o incluem; o
  servidor adiciona-o nas mensagens que envia em nome de um cliente.
- **COMANDO** — pode ser uma palavra (`NICK`, `JOIN`, `PRIVMSG`) ou um número de 3
  dígitos (`001`, `433`).
- **parâmetros** — separados por espaços. Pode haver até 14 parâmetros normais.
- **trailing** — começa com ` :` e pode conter espaços. É o último "campo" da mensagem.

**Exemplos:**
```
PASS secret\r\n
NICK alice\r\n
USER alice 0 * :Alice Smith\r\n
JOIN #test\r\n
PRIVMSG #test :Olá a todos!\r\n
:alice!alice@127.0.0.1 PRIVMSG #test :Olá a todos!\r\n
:ircserv 001 alice :Welcome to the server!\r\n
```

### 1.3 Handshake de registo

Para um cliente ficar "registado" (aceite pelo servidor) tem de enviar exactamente estes
três comandos, por esta ordem:

1. `PASS <password>` — palavra-passe do servidor
2. `NICK <nickname>` — nome único no servidor
3. `USER <username> 0 * :<realname>` — informação de utilizador

Só depois de receber os três (e todos válidos) o servidor envia as respostas de
boas-vindas `001 002 003 004` e o cliente está "registado". Antes disso, quase todos
os outros comandos são rejeitados.

### 1.4 Numeric replies (respostas numéricas)

O servidor usa números de 3 dígitos para responder:

| Intervalo | Tipo |
|-----------|------|
| 001–099   | Respostas de boas-vindas e informação |
| 200–399   | Respostas a comandos com sucesso |
| 400–499   | Erros do cliente (bad input) |
| 500–599   | Erros do servidor |

Exemplos importantes no projecto:

| Código | Nome | Quando |
|--------|------|--------|
| 001 | RPL_WELCOME | Registo completo |
| 331 | RPL_NOTOPIC | Canal sem tópico |
| 332 | RPL_TOPIC | Canal com tópico |
| 353 | RPL_NAMREPLY | Lista de membros do canal |
| 366 | RPL_ENDOFNAMES | Fim da lista de membros |
| 401 | ERR_NOSUCHNICK | Nick não existe |
| 403 | ERR_NOSUCHCHANNEL | Canal não existe |
| 433 | ERR_NICKNAMEINUSE | Nick já em uso |
| 441 | ERR_USERNOTINCHANNEL | Utilizador não está no canal |
| 442 | ERR_NOTONCHANNEL | Tu não estás no canal |
| 451 | ERR_NOTREGISTERED | Não registado |
| 461 | ERR_NEEDMOREPARAMS | Faltam parâmetros |
| 462 | ERR_ALREADYREGISTERED | Já registado |
| 464 | ERR_PASSWDMISMATCH | Password errada |
| 471 | ERR_CHANNELISFULL | Canal cheio (+l) |
| 473 | ERR_INVITEONLYCHAN | Canal +i sem convite |
| 475 | ERR_BADCHANNELKEY | Key errada (+k) |
| 482 | ERR_CHANOPRIVSNEEDED | Não és operador |

### 1.5 Modos de canal

Os modos controlam o comportamento de um canal. São activados com `+` e desactivados
com `-`. O subject exige suporte para:

| Modo | Nome | Descrição |
|------|------|-----------|
| `+i` | invite-only | Só quem foi convidado pode entrar |
| `+t` | topic restricted | Só operadores podem mudar o tópico |
| `+k <key>` | channel key | É necessária uma password para entrar |
| `+o <nick>` | operator | Dá/remove privilégios de operador a um nick |
| `+l <n>` | user limit | Limite máximo de membros |

---

## 2. Redes — TCP/IP

### 2.1 O modelo TCP/IP

O IRC corre sobre **TCP (Transmission Control Protocol)**, que por sua vez corre sobre
**IP (Internet Protocol)**. É útil entender as camadas:

```
Aplicação   — o protocolo IRC (linhas de texto \r\n)
Transporte  — TCP (garante entrega ordenada e sem perdas)
Rede        — IP (endereçamento e roteamento)
Ligação     — Ethernet, Wi-Fi, etc.
```

### 2.2 TCP — o que garante

TCP é um protocolo **orientado à ligação** e **de stream**:

- **Orientado à ligação** — antes de trocar dados, cliente e servidor fazem um
  handshake (SYN / SYN-ACK / ACK) para estabelecer a ligação.
- **Stream** — não há "mensagens" nem "pacotes" visíveis para a aplicação. É um
  fluxo contínuo de bytes. Um `recv()` pode devolver metade de uma mensagem IRC, ou
  duas mensagens e meia. Por isso precisamos de **framing** (ver secção 4.2).
- **Garante entrega** — se um pacote se perder, o TCP reencaminha-o automaticamente.
- **Garante ordem** — os bytes chegam sempre pela ordem em que foram enviados.

### 2.3 Endereçamento — IP e portas

Cada ligação TCP é identificada por 4 valores:
- IP de origem + porta de origem (cliente)
- IP de destino + porta de destino (servidor)

**Portas** são números de 16 bits (1–65535). O IRC usa convencionalmente a porta 6667.
Portas abaixo de 1024 requerem privilégios de root. No projecto usamos qualquer porta
≥ 1024 nos testes.

### 2.4 I/O não-bloqueante

Por defeito, chamadas como `recv()` e `send()` são **bloqueantes**: ficam à espera
até haver dados disponíveis. Num servidor com múltiplos clientes, isso seria
catastrófico — um cliente lento bloquearia todos os outros.

A solução é tornar os sockets **não-bloqueantes** com:
```c
fcntl(fd, F_SETFL, O_NONBLOCK);
```

Com sockets não-bloqueantes:
- `recv()` devolve `-1` com `errno == EAGAIN` se não há dados (em vez de bloquear).
- `send()` devolve `-1` com `errno == EAGAIN` se o buffer de envio do kernel está
  cheio (em vez de bloquear).
- É preciso usar `poll()` para saber *quando* um socket está pronto para leitura ou
  escrita.

---

## 3. Sockets — o que são e como se usam

Um **socket** é um endpoint de comunicação de rede. Em Unix/Linux, um socket é
representado por um **file descriptor** (fd) — um inteiro que identifica um recurso
aberto. Assim como se lê um ficheiro com `read(fd, ...)`, lê-se de um socket com
`recv(fd, ...)`.

### 3.1 Ciclo de vida de um socket servidor

```
socket()        — criar o fd
setsockopt()    — configurar opções (SO_REUSEADDR)
bind()          — associar a uma porta
listen()        — ficar à escuta de ligações
accept()        — aceitar uma ligação → cria um novo fd para o cliente
recv() / send() — comunicar com o cliente
close()         — fechar a ligação
```

### 3.2 Ciclo de vida de um socket cliente

```
socket()        — criar o fd
connect()       — ligar ao servidor (endereço + porta)
send() / recv() — comunicar
close()         — fechar
```

No nosso servidor não há `connect()` — essa é a responsabilidade do cliente IRC
(HexChat, nc, etc.).

### 3.3 Funções detalhadas

#### `socket(domain, type, protocol)`

Cria um novo socket e devolve um fd.

```c
int fd = socket(AF_INET, SOCK_STREAM, 0);
```

- `AF_INET` — família de endereços IPv4 (usar `AF_INET6` para IPv6).
- `SOCK_STREAM` — tipo TCP (stream orientado à ligação). `SOCK_DGRAM` seria UDP.
- `0` — protocolo automático (TCP para SOCK_STREAM).

#### `setsockopt(fd, level, optname, &value, size)`

Define opções no socket. A mais importante no projecto:

```c
int opt = 1;
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

`SO_REUSEADDR` permite reutilizar imediatamente uma porta após o servidor terminar.
Sem isto, ao reiniciar o servidor, o `bind()` falha com "Address already in use"
porque o kernel ainda tem a porta em estado TIME_WAIT.

#### `bind(fd, addr, addrlen)`

Associa o socket a um endereço e porta locais.

```c
struct sockaddr_in addr;
memset(&addr, 0, sizeof(addr));
addr.sin_family      = AF_INET;
addr.sin_addr.s_addr = INADDR_ANY;   // escutar em todas as interfaces
addr.sin_port        = htons(6667);  // converter para big-endian (network byte order)

bind(fd, (struct sockaddr*)&addr, sizeof(addr));
```

`htons()` — *host to network short* — converte um inteiro de 16 bits do byte order
do processador (little-endian em x86) para big-endian (que o protocolo de rede usa).
Sem esta conversão a porta ficaria "trocada" em máquinas little-endian.

#### `listen(fd, backlog)`

Coloca o socket em modo de escuta. `backlog` é o número máximo de ligações pendentes
(à espera de `accept()`) na fila.

```c
listen(fd, 128);
```

#### `accept(fd, addr, addrlen)`

Aceita uma ligação pendente. **Bloqueia** até chegar uma ligação (a não ser que o
socket seja não-bloqueante). Devolve um **novo fd** para comunicar especificamente
com esse cliente.

```c
struct sockaddr_in clientAddr;
socklen_t addrLen = sizeof(clientAddr);
int clientFd = accept(listenFd, (struct sockaddr*)&clientAddr, &addrLen);
```

O fd original (`listenFd`) continua a escutar; o `clientFd` é o canal de comunicação
com aquele cliente específico.

#### `recv(fd, buf, len, flags)`

Lê dados do socket para um buffer. Com socket não-bloqueante, pode devolver:
- `n > 0` — leu `n` bytes.
- `0` — o cliente fechou a ligação (EOF).
- `-1` com `errno == EAGAIN` — não há dados agora, tentar mais tarde.
- `-1` com outro errno — erro real.

```c
char buf[4096];
ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
```

#### `send(fd, buf, len, flags)`

Envia dados pelo socket. Pode enviar menos bytes do que pedido (envio parcial), por
isso é boa prática usar um buffer de saída e verificar `sent < len`.

```c
ssize_t sent = send(clientFd, message.c_str(), message.size(), 0);
```

#### `close(fd)`

Fecha o socket e liberta o fd. Após `close()`, o kernel envia um FIN ao outro lado,
sinalizando o fim da ligação.

#### `fcntl(fd, F_SETFL, O_NONBLOCK)`

Torna o fd não-bloqueante. No subject, esta é a única flag de `fcntl` autorizada.

```c
fcntl(fd, F_SETFL, O_NONBLOCK);
```

#### `inet_ntoa(addr.sin_addr)`

Converte um endereço IP de formato binário (`in_addr`) para string legível
(`"127.0.0.1"`). Usado para registar o IP do cliente que se ligou.

---

## 4. `poll()` — multiplexação de I/O

### 4.1 O problema

Com múltiplos clientes, não podemos fazer `recv()` bloqueante num cliente (ficamos
presos). Precisamos de monitorizar **todos** os fds simultaneamente e agir apenas
quando houver actividade.

### 4.2 Como funciona `poll()`

```c
#include <poll.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

- `fds` — array de `struct pollfd`, um por fd a vigiar.
- `nfds` — número de elementos no array.
- `timeout` — milissegundos a esperar (-1 = bloquear indefinidamente, 0 = não esperar).
- Devolve: número de fds com eventos prontos, 0 se timeout, -1 se erro.

```c
struct pollfd {
    int   fd;       // file descriptor a vigiar
    short events;   // eventos que queremos (POLLIN, POLLOUT, ...)
    short revents;  // eventos que aconteceram (preenchido pelo kernel)
};
```

**Eventos importantes:**

| Flag | Significado |
|------|-------------|
| `POLLIN` | Há dados para ler (ou nova ligação no listen socket) |
| `POLLOUT` | O socket está pronto para escrita |
| `POLLERR` | Erro no fd |
| `POLLHUP` | O outro lado fechou a ligação |
| `POLLNVAL` | Fd inválido |

### 4.3 O loop do servidor

```c
while (running) {
    int ready = poll(pollFds.data(), pollFds.size(), 1000);

    // fd[0] = socket de escuta
    if (pollFds[0].revents & POLLIN)
        acceptNewClient();

    // fd[1..n] = clientes
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

### 4.4 Porquê POLLOUT?

`send()` pode não conseguir enviar todos os dados de uma vez (buffer do kernel cheio).
A solução correcta é:
1. Guardar os dados num buffer de saída por cliente.
2. Activar `POLLOUT` para o fd.
3. Quando `poll()` sinaliza `POLLOUT`, chamar `send()` com o que resta.
4. Quando o buffer esvaziou, desactivar `POLLOUT` (poupar CPU).

Se nunca activarmos `POLLOUT`, perdemos dados. Se o deixarmos sempre activo, o
`poll()` fica sempre a acordar (busy loop desnecessário).

---

## 5. Framing de mensagens (leituras parciais)

O TCP é um **stream de bytes** — não há garantia que um `recv()` devolva exactamente
uma mensagem IRC. Podem acontecer:

- **Fragmentação**: `"NICK ali"` num recv, `"ce\r\n"` no próximo.
- **Agregação**: `"NICK alice\r\nUSER alice 0 * :Al"` num só recv.

A solução é um **buffer de entrada por cliente**: cada vez que `recv()` devolve dados,
acrescentamos ao buffer. Depois extraímos linhas completas (terminadas em `\r\n`) uma
a uma.

```cpp
void appendToInBuffer(const std::string& data) {
    _inBuffer += data;
}

bool extractMessage(std::string& lineOut) {
    size_t pos = _inBuffer.find("\r\n");
    if (pos == std::string::npos)
        return false;          // linha ainda incompleta
    lineOut = _inBuffer.substr(0, pos);
    _inBuffer.erase(0, pos + 2);
    return true;
}
```

O subject testa explicitamente isto com `nc` e `Ctrl+D` para enviar partes de um
comando em fragmentos separados.

---

## 6. Arquitectura do servidor

### 6.1 Divisão em camadas

O projecto divide-se em duas camadas com uma interface clara:

```
CAMADA DE REDE (Person A)          CAMADA DE PROTOCOLO (Person B)
────────────────────────           ────────────────────────────────
Server + Client                    CommandHandler + Channel
socket, poll, recv, send           parser, auth, canais, comandos
```

O ponto de encontro é:
```cpp
CommandHandler::dispatch(Client& client, const Message& msg);
```

### 6.2 Estruturas de dados principais

**`struct Message`** — forma parseada de uma linha IRC:
```cpp
struct Message {
    std::string              prefix;      // quem enviou (raramente usado)
    std::string              command;     // "NICK", "JOIN", "001", ...
    std::vector<std::string> params;      // parâmetros normais
    std::string              trailing;    // texto após " :"
    bool                     hasTrailing; // distingue "" de "sem trailing"
};
```

**`class Client`** — um utilizador TCP ligado:
```cpp
class Client {
    int         _fd;           // socket fd
    std::string _nickname;
    std::string _username;
    std::string _realname;
    std::string _hostname;
    std::string _inBuffer;     // bytes recebidos mas não processados
    std::string _outBuffer;    // bytes pendentes para envio
    bool        _passReceived; // recebeu PASS correcto
    bool        _registered;   // PASS + NICK + USER completos
};
```

**`class Channel`** — uma sala de chat:
```cpp
class Channel {
    std::string   _name;
    std::string   _topic;
    std::string   _key;            // modo +k
    bool          _inviteOnly;     // modo +i
    bool          _topicRestricted;// modo +t
    bool          _hasKey;
    bool          _hasUserLimit;   // modo +l
    std::size_t   _userLimit;
    std::set<int> _members;        // fds dos membros
    std::set<int> _operators;      // fds dos operadores
    std::set<int> _invited;        // fds convidados
};
```

**`class Server`** — dono de tudo:
```cpp
class Server {
    int                              _listenFd;
    int                              _port;
    std::string                      _password;
    std::vector<struct pollfd>       _pollFds;
    std::map<int, Client*>           _clients;   // fd → Client
    std::map<std::string, Channel*>  _channels;  // nome → Channel
    CommandHandler                   _commands;
    static bool                      _running;
};
```

### 6.3 Ciclo de vida de uma mensagem

```
1. poll() → POLLIN num fd de cliente
2. recv() → bytes chegam ao buffer de entrada do Client
3. extractMessage() → extrai uma linha completa ("\r\n")
4. parseMessage()   → linha → struct Message
5. dispatch()       → chama o handler correcto (handleJoin, handlePrivmsg, ...)
6. handler chama server.sendToClient(fd, resposta)
7. sendToClient → appendToOutBuffer + activa POLLOUT
8. poll() → POLLOUT no fd
9. send() → envia os bytes do buffer de saída
```

---

## 7. Sinais

### SIGPIPE

Quando o servidor tenta fazer `send()` para um cliente que já fechou a ligação, o
kernel envia `SIGPIPE` ao processo, que por defeito o termina imediatamente. Para
evitar isto:

```c
signal(SIGPIPE, SIG_IGN);
```

Com `SIGPIPE` ignorado, o `send()` devolve `-1` com `errno == EPIPE` e o servidor
pode tratar o erro graciosamente.

### SIGINT (Ctrl+C)

Para o servidor fechar de forma limpa (fechar sockets, libertar memória) quando o
utilizador pressiona Ctrl+C:

```c
signal(SIGINT, handlerFunction);
// no handler:
Server::_running = false;
```

O loop principal verifica `_running` em cada iteração e sai do loop, permitindo que
o destrutor do `Server` limpe tudo.

---

## 8. IRCv3 — CAP (negociação de capacidades)

Clientes modernos como o HexChat enviam `CAP LS 302` logo ao ligar. Este comando
faz parte do **IRCv3**, uma extensão moderna ao protocolo IRC.

O cliente está a perguntar: *"Que capacidades avançadas suportas?"*. Se o servidor
não responder, o HexChat fica à espera indefinidamente — não avança para o login.

A nossa solução mínima:
```
Cliente → CAP LS 302
Servidor → :ircserv CAP * LS :    (lista vazia — sem capacidades)
Cliente → PASS / NICK / USER      (avança para o login normal)
```

Se o cliente pedir uma capacidade específica (`CAP REQ :sasl`), o servidor responde
com `NAK` (rejeitar):
```
Cliente → CAP REQ :sasl
Servidor → :ircserv CAP * NAK :sasl
```

---

## 9. Estruturas C relevantes

```c
// Endereço IPv4
struct sockaddr_in {
    sa_family_t    sin_family;   // AF_INET
    in_port_t      sin_port;     // porta em network byte order (usar htons())
    struct in_addr sin_addr;     // endereço IP
};

struct in_addr {
    uint32_t s_addr;             // endereço em network byte order
};

// Monitorização de fd com poll()
struct pollfd {
    int   fd;       // file descriptor
    short events;   // eventos a vigiar (POLLIN, POLLOUT, ...)
    short revents;  // eventos que aconteceram (preenchido pelo kernel)
};
```

---

## 10. Funções do sistema usadas no projecto

| Função | Header | Para que serve |
|--------|--------|----------------|
| `socket()` | `<sys/socket.h>` | Criar um socket |
| `setsockopt()` | `<sys/socket.h>` | Configurar opções do socket |
| `bind()` | `<sys/socket.h>` | Associar socket a endereço/porta |
| `listen()` | `<sys/socket.h>` | Colocar em modo de escuta |
| `accept()` | `<sys/socket.h>` | Aceitar nova ligação |
| `recv()` | `<sys/socket.h>` | Ler dados do socket |
| `send()` | `<sys/socket.h>` | Enviar dados pelo socket |
| `close()` | `<unistd.h>` | Fechar fd/socket |
| `fcntl()` | `<fcntl.h>` | Controlar flags de fd (O_NONBLOCK) |
| `poll()` | `<poll.h>` | Multiplexar I/O em múltiplos fds |
| `htons()` | `<arpa/inet.h>` | Host to network byte order (16-bit) |
| `inet_ntoa()` | `<arpa/inet.h>` | IP binário → string legível |
| `memset()` | `<cstring>` | Inicializar bloco de memória a zero |
| `signal()` | `<csignal>` | Definir handler para um sinal |
| `atoi()` | `<cstdlib>` | String → inteiro |

---

## 11. Erros comuns e como evitá-los

### `EAGAIN` / `EWOULDBLOCK`
Com sockets não-bloqueantes, `recv()` e `send()` podem devolver `-1` com este errno.
**Não é um erro** — significa simplesmente que não há dados agora. Tratar como caso
normal e continuar.

### Envio parcial com `send()`
`send()` pode enviar menos bytes do que pedido. Sempre verificar o valor devolvido e
guardar o resto num buffer para enviar depois (quando `POLLOUT` for sinalizado).

### Fds a crescer no vector do `poll()`
Quando um cliente desliga durante o loop sobre `_pollFds`, o vector encolhe. Usar
índice em vez de iterator e não incrementar quando um elemento foi removido.

### Modificar o vector `_pollFds` durante iteração
`acceptNewClient()` adiciona elementos ao vector. Por isso o loop usa `.size()` em
cada iteração (não guarda o tamanho antes do loop).

### Memory leaks de `Client*` e `Channel*`
O `Server` é dono de todos os ponteiros. O destrutor e `disconnectClient()` têm de
fazer `delete` e apagar do map. Verificar com `valgrind` antes da defesa.

### Fechar o fd antes de apagar do map
Sempre `close(fd)` antes de `delete client` e `_clients.erase(fd)`. Nunca usar o fd
depois de `close()`.
