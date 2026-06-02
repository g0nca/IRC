
# Plano de Trabalho — ft_irc (42)

> **Projeto:** servidor IRC em C++98 (`ircserv`) · **Equipa:** 2 devs · **Ritmo:** ~2-3 h/dia cada · **Âmbito:** só parte obrigatória · **Início:** Seg, 1 Jun 2026 · **Alvo de entrega:** Dom, 28 Jun 2026 (~4 semanas).

---

## 0. TL;DR — a estratégia em 6 frases

1. **Primeiro aprende-se redes**, não C++ (a base de C++ já está, com os CPP feitos). O desconhecido aqui é: sockets, `poll()`, I/O não-bloqueante, protocolo IRC e *framing* de mensagens.
2. **O "ponto de encontro" é um contrato de interfaces** (headers) que ambos escrevem juntos no Dia 3-4, *antes* de qualquer lógica. A partir daí cada um programa contra esse contrato.
3. **Divide-se em duas camadas:** Pessoa A = **Rede/Infraestrutura** (socket, poll, buffers, ciclo de vida do cliente); Pessoa B = **Protocolo/Lógica IRC** (parser, autenticação, canais, comandos de operador).
4. **Cada um escreve um "duplo hard-coded" do outro** para testar sozinho: A faz um *stub* do dispatcher de comandos; B faz um *mock server* que injeta strings cruas e captura respostas.
5. **Integra-se cedo e em contínuo:** primeiro merge ao fim da Semana 1 (esqueleto compila), integração end-to-end a meio da Semana 2, e daí PRs contínuos.
6. **A metodologia chama-se Scrum**, e a reunião diária de 10-15 min que descreveste é o **Daily Stand-up** (a.k.a. *Daily Scrum*) — secção 8.

---

## 1. O que precisam de aprender primeiro

C++/OOP está tratado pelos CPP. O esforço de estudo vai quase todo para **rede e protocolo**. Regra de ouro: **2 a 3 dias de estudo + um "spike" (servidor de eco mínimo)**, e depois aprende-se o resto a construir. Não percam uma semana só a ler.

### 1.1 Protocolo IRC (o "o quê")
Como é uma mensagem IRC, o *handshake* de registo e os *numeric replies*. Formato de uma mensagem:

```
[:prefixo] COMANDO [parametros...] [:trailing]\r\n
```

- Toda a mensagem termina em `\r\n` (CR LF). Isto é central para o *framing* (1.5).
- Handshake de registo (ordem que o cliente envia): `PASS <password>` → `NICK <nick>` → `USER <user> 0 * :<realname>`. Só depois de PASS+NICK+USER válidos o cliente está "registado" e recebem-se os numerics de boas-vindas `001 002 003 004`.
- Estudar os comandos que **têm** de implementar: `PASS, NICK, USER, JOIN, PART, PRIVMSG, NOTICE, QUIT, KICK, INVITE, TOPIC, MODE` e respostas de erro (`401 no such nick`, `403 no such channel`, `461 need more params`, `464 password mismatch`, `433 nick in use`, `442 not on channel`, `482 not channel operator`, etc.).

### 1.2 Sockets TCP/IP (o "como" da ligação)
A API Berkeley sockets em C: `socket()`, `setsockopt(SO_REUSEADDR)`, `bind()`, `listen()`, `accept()`, `recv()`, `send()`, `close()`; estruturas `sockaddr_in`, `htons`. Perceber o fluxo servidor: criar socket → opções → bind à porta → listen → aceitar ligações.

### 1.3 I/O não-bloqueante
`fcntl(fd, F_SETFL, O_NONBLOCK)`. Porque é que um `recv` bloqueante paralisa o servidor inteiro, e o que significam `EAGAIN`/`EWOULDBLOCK`. (No macOS o subject só autoriza esta flag exata.)

### 1.4 Multiplexação com `poll()` — o coração do projeto
`struct pollfd` (`fd`, `events`, `revents`), os eventos `POLLIN`/`POLLOUT`/`POLLHUP`/`POLLERR`, e o modelo de **um único `poll()` para tudo** (escutar, ler e escrever). É proibido fazer `recv`/`send` num fd sem passar pelo `poll()` — isso é **nota 0**. Proibido `fork()`.

### 1.5 *Framing* / leituras parciais
TCP é um **fluxo de bytes**, não mensagens. Um `recv` pode trazer meia mensagem ou duas e meia. Têm de **acumular num buffer por cliente** e só processar comandos completos (até ao `\r\n`). É exatamente o teste do subject:

```
$> nc -C 127.0.0.1 6667
com^Dman^Dd      # enviado em 3 pedaços: 'com', 'man', 'd\n'
```

O servidor tem de reconstruir `command` a partir dos 3 pedaços.

### 1.6 Sinais
`signal`/`sigaction` para apanhar `SIGINT` (Ctrl-C) e desligar limpo (fechar sockets, libertar memória) sem crashar.

### 1.7 Cliente de referência
Escolher **um** cliente e usá-lo sempre (é o que conta na defesa). Recomendado: **irssi** ou **WeeChat** (terminal, leves) ou **HexChat** (GUI). Aprender a ligar (`/connect 127.0.0.1 6667 <password>`) e a ver o tráfego cru (modo raw/debug) ajuda imenso a perceber o que o cliente realmente envia.

### Recursos
| Tema | Recurso |
|---|---|
| Sockets (o clássico) | *Beej's Guide to Network Programming* |
| Protocolo IRC | RFC 1459 (original) e RFC 2812 (cliente); `modern.ircdocs.horse` (muito legível) |
| poll / fcntl / send | `man 2 poll`, `man 2 fcntl`, `man 2 recv` |
| Numerics | `modern.ircdocs.horse` (lista de respostas numéricas) |

> ⚠️ **Nota do subject sobre IA:** usem IA para acelerar tarefas repetitivas e para *entender* conceitos, mas **só metam código que conseguem explicar na defesa**. Revisão entre pares antes de cada merge. Na defesa pode ser pedida uma pequena modificação ao vivo — têm de dominar o que está no repo.

---

## 2. O "ponto de encontro" — o contrato de interfaces

Esta é a parte mais importante para trabalharem em paralelo. **No Dia 3-4 sentam-se juntos e fixam os headers** (assinaturas das classes e o struct `Message`). Esses headers são o contrato: depois cada um programa contra eles sem precisar do código do outro.

### 2.1 Fluxo de dados (entender antes de dividir)

```
            ┌──────────────────────── CAMADA REDE (Pessoa A) ────────────────────────┐
 socket ──▶ poll() ──▶ recv() ──▶ buffer de entrada do Client ──▶ extrair linha \r\n ─┐
                                                                                       │
            ┌──────────────────── CAMADA PROTOCOLO (Pessoa B) ────────────────────────┘
            ▼
   Parser (linha crua → Message) ──▶ CommandHandler.dispatch(client, msg) ──▶ comando
            (PASS/NICK/JOIN/PRIVMSG/MODE...) gera resposta
                                                                                       │
            ┌──────────────────────── CAMADA REDE (Pessoa A) ◀────────────────────────┘
            ▼
  Client.appendToOutBuffer(resp) ──▶ poll() sinaliza POLLOUT ──▶ send()
```

O contrato vive **nas setas que cruzam as camadas**:
- Rede → Protocolo: `CommandHandler::dispatch(Client& c, const Message& m)`
- Protocolo → Rede: o handler chama métodos do `Server`/`Client` (ex.: `server.sendToClient(fd, txt)`, `client.appendToOutBuffer(txt)`, `server.getClientByNick(...)`, `server.getOrCreateChannel(...)`).

### 2.2 Como cada um escreve a "parte hard-coded" do outro

Depois dos headers fixados, cada um cria um **duplo falso** do lado do colega para se desbloquear:

**Pessoa A (Rede) precisa de algo que processe comandos → escreve um _stub_ do dispatcher:**
```cpp
// stub_dispatch.cpp — substitui temporariamente a lógica da Pessoa B
void CommandHandler::dispatch(Client& c, const Message& m) {
    // hard-coded: só ecoa de volta para validar o loop poll/recv/send
    c.appendToOutBuffer("ECHO " + m.command + "\r\n");
}
```
Com isto a Pessoa A consegue testar `poll()`, aceitação de clientes, *framing* parcial (`nc` com `^D`) e envio — **sem depender da lógica IRC**.

**Pessoa B (Protocolo) precisa de um servidor que lhe forneça clientes e receba respostas → escreve um _mock server_ / arnês de teste:**
```cpp
// mock_server.cpp — main de teste que NÃO usa sockets
int main() {
    MockServer srv;                          // implementa a mesma interface de Server
    Client a(1), b(2);                        // "clientes" falsos com fds fictícios
    srv.addClient(&a); srv.addClient(&b);

    CommandHandler h(srv);
    h.dispatch(a, parse("PASS 1234\r\n"));
    h.dispatch(a, parse("NICK alice\r\n"));
    h.dispatch(a, parse("USER alice 0 * :Alice\r\n"));
    h.dispatch(a, parse("JOIN #test\r\n"));
    h.dispatch(a, parse("PRIVMSG #test :ola\r\n"));

    std::cout << a.takeOutBuffer();           // inspecionar o que "seria enviado"
    std::cout << b.takeOutBuffer();
}
```
Com isto a Pessoa B desenvolve **toda** a lógica (auth, canais, comandos, MODE) e testa-a deterministicamente, **sem sockets**, injetando strings cruas e lendo o output capturado.

> Estes ficheiros (`stub_dispatch.cpp`, `mock_server.cpp`) ficam em `tests/` e **não vão na entrega** (o subject diz que testes não são submetidos nem avaliados), mas são ouro para a defesa.

### 2.3 Quando juntar as partes (merge checkpoints)
- **M1 (fim Sem 1):** headers fixados + esqueleto compila em `develop`. A tem servidor que aceita ligações e ecoa via stub; B tem parser+auth a passar no mock.
- **Integração grande (meio Sem 2):** trocar o *stub* pelo `CommandHandler` real e o *mock* pelo `Server` real. Como ambos programaram contra o **mesmo contrato**, deve compilar e ligar. Primeiro teste end-to-end com o cliente real: autenticar + PRIVMSG entre dois clientes.
- **Daí em diante:** integração contínua — cada feature acabada entra em `develop` por PR revisto pelo outro; `main` só recebe milestones estáveis.

---

## 3. Estruturas de dados (classes)

Esboços C++98 (respeitar a **Orthodox Canonical Form**: construtor por defeito, cópia, `operator=`, destrutor). São o ponto de partida do contrato — afinem juntos.

### `Message` (forma parseada de uma linha crua)
```cpp
struct Message {
    std::string              prefix;    // opcional (raramente usado server-side)
    std::string              command;   // "PASS", "JOIN", "PRIVMSG", ...
    std::vector<std::string> params;    // parâmetros antes do trailing
    std::string              trailing;  // parte após " :" (pode ter espaços)
};
```

### `Client`
```cpp
class Client {
private:
    int         _fd;
    std::string _nickname;
    std::string _username;
    std::string _realname;
    std::string _hostname;
    std::string _inBuffer;     // acumula recv parciais (framing)
    std::string _outBuffer;    // dados pendentes para send (POLLOUT)
    bool        _passReceived; // recebeu PASS correto?
    bool        _registered;   // PASS+NICK+USER completos?
public:
    Client();
    explicit Client(int fd);
    Client(const Client&);
    Client& operator=(const Client&);
    ~Client();

    // getters/setters
    int  getFd() const;
    const std::string& getNickname() const;
    void setNickname(const std::string&);
    bool isRegistered() const;
    void setRegistered(bool);
    // ... (username, realname, passReceived, hostname)

    // framing / I/O (usados pela Rede; o Protocolo usa appendToOutBuffer)
    void appendToInBuffer(const std::string& data);
    bool extractMessage(std::string& lineOut); // retira 1 linha terminada em \r\n
    void appendToOutBuffer(const std::string& data);
    std::string& outBuffer();                  // a Rede consome no send()
};
```

### `Channel`
```cpp
class Channel {
private:
    std::string        _name;
    std::string        _topic;
    std::string        _key;            // modo k (password)
    bool               _inviteOnly;     // modo i
    bool               _topicRestricted;// modo t (só ops mudam topic)
    bool               _hasUserLimit;   // modo l
    size_t             _userLimit;      // modo l
    std::set<int>      _members;        // fds dos membros
    std::set<int>      _operators;      // fds dos operadores do canal
    std::set<int>      _invited;        // fds convidados (para modo i)
public:
    Channel();
    explicit Channel(const std::string& name);
    Channel(const Channel&);
    Channel& operator=(const Channel&);
    ~Channel();

    const std::string& getName() const;
    // membros
    void addMember(int fd);
    void removeMember(int fd);
    bool isMember(int fd) const;
    bool isEmpty() const;
    // operadores
    void addOperator(int fd);
    void removeOperator(int fd);
    bool isOperator(int fd) const;
    // convites / modos
    void invite(int fd);
    bool isInvited(int fd) const;
    void setInviteOnly(bool); bool isInviteOnly() const;
    void setTopicRestricted(bool); bool isTopicRestricted() const;
    void setKey(const std::string&); void removeKey(); const std::string& getKey() const;
    void setUserLimit(size_t); void removeUserLimit();
    // topic
    void setTopic(const std::string&); const std::string& getTopic() const;
    // difusão
    const std::set<int>& getMembers() const; // a Rede/handler envia a todos
};
```

### `Server` (dono de tudo; a sua interface pública É o contrato para os comandos)
```cpp
class Server {
private:
    int                              _listenFd;
    int                              _port;
    std::string                      _password;
    std::vector<struct pollfd>       _pollFds;
    std::map<int, Client*>           _clients;   // fd -> Client
    std::map<std::string, Channel*>  _channels;  // nome -> Channel
    static bool                      _running;   // posto a false no SIGINT
public:
    Server(int port, const std::string& password);
    ~Server();

    void run();                                  // setup + loop poll() (Pessoa A)

    // ---- CONTRATO usado pelos comandos (Pessoa B chama, Pessoa A implementa) ----
    const std::string& getPassword() const;
    Client*  getClientByNick(const std::string& nick);
    Client*  getClientByFd(int fd);
    Channel* getChannel(const std::string& name);
    Channel* getOrCreateChannel(const std::string& name);
    void     removeChannelIfEmpty(const std::string& name);
    void     sendToClient(int fd, const std::string& msg);   // -> appendToOutBuffer
    void     disconnectClient(int fd);                       // fecha fd + limpa estado
private:
    void setupSocket();          // socket/setsockopt/bind/listen (A)
    void acceptNewClient();      // accept + O_NONBLOCK + add ao poll (A)
    void handleClientData(int fd);// recv -> buffer -> extractMessage -> dispatch (A→B)
    void flushClientOutput(int fd);// send a partir do outBuffer (A)
};
```

> **Decisão a tomar juntos:** identificar clientes/canais por `int fd` (simples, mostrado acima) ou por `Client*`. Recomendo `fd` nos `std::set` dos canais e `std::map<int,Client*>` no servidor — evita ponteiros pendurados quando um cliente sai.

---

## 4. Divisão de trabalho

| | **Pessoa A — Rede / Infraestrutura** | **Pessoa B — Protocolo / Lógica IRC** |
|---|---|---|
| Foco | Sockets, `poll()`, buffers, ciclo de vida | Parser, comandos, canais, MODE |
| Classes | `Server` (socket/poll), `Client` (I/O+framing) | `CommandHandler`, `Channel`, `Message` |
| Tarefas | `main.cpp` + validação de args; setup do socket; loop `poll()` único; `accept`; `recv`+buffer de entrada; *framing* parcial; `send`+buffer de saída (POLLOUT); desconexão e limpeza; `SIGINT`; sem leaks/crashes | parser linha→`Message`; dispatcher; **auth** (PASS/NICK/USER + numerics 001-004); **PRIVMSG/NOTICE**; **JOIN/PART/QUIT**; difusão no canal; **TOPIC**; **KICK/INVITE**; **MODE i/t/k/o/l**; respostas de erro |
| Ficheiros | `src/server/*`, `src/client/*`, `main.cpp` | `src/command/*`, `src/channel/*`, `include/Message.hpp` |
| Testa com | `nc` + *stub* do dispatcher | *mock server* + strings cruas |
| Duplo que escreve p/ o outro | `stub_dispatch.cpp` | `mock_server.cpp` |

**Juntos:** contrato/headers (Dia 3-4), `Makefile`, `.gitignore`, README, revisões de PR, integração e prep de defesa. Sugiro **Pessoa A = tu** (és quem está a conduzir o planeamento) e B = o colega — mas troquem conforme a preferência.

---

## 5. Calendário e deadlines (4 sprints)

Início Seg 1 Jun, ~2-3 h/dia por pessoa. Semanas Seg→Dom. Alvo: **Dom 28 Jun**.

### Sprint 0 — Aprender + Setup · **1–7 Jun**
- Dias 1-3: estudar rede/protocolo (secção 1) + cada um faz um *spike* de servidor de eco com `poll()`.
- Dias 3-4: **juntos** — fixar contrato/headers, estrutura de pastas, `Makefile`, repo + branches, README esqueleto, board de tarefas.
- Dias 5-7: arranque paralelo. A: esqueleto Server+poll com *stub*. B: parser + mock + PASS/NICK/USER.
- **🏁 M1 (7 Jun):** repo montado, contrato fixado, A aceita ligações e ecoa via `nc`, B passa auth no mock.

### Sprint 1 — Núcleo · **8–14 Jun**
- A: *framing* parcial robusto, buffer de saída + POLLOUT, ciclo de vida/limpeza, `SIGINT`.
- B: fluxo de registo completo + numerics de boas-vindas, PRIVMSG (cliente-a-cliente), erros.
- **Integração grande (~10-11 Jun):** ligar `CommandHandler` real ao `Server` real.
- **🏁 M2 (14 Jun):** cliente de referência liga, autentica, define nick/user, e dois clientes trocam PRIVMSG.

### Sprint 2 — Canais · **15–21 Jun**
- B (lead): `Channel`, JOIN, PART, difusão de PRIVMSG no canal, TOPIC (ver/definir).
- A (apoio): garantir envio a múltiplos fds, limpar membros em desconexão, corrigir bugs de buffer.
- **🏁 M3 (21 Jun):** vários clientes entram num canal e conversam; topic funciona.

### Sprint 3 — Operadores + MODE + Polish · **22–28 Jun**
- B (lead): modelo de operador, KICK, INVITE, MODE `i`/`t`/`k`/`o`/`l`.
- A: stress test (pacotes parciais, muitos clientes, input malformado), **valgrind** (zero leaks), garantir "não crasha nunca", `Makefile` sem relink.
- Ambos: README final, passagem de documentação, **code review cruzado**, prep de defesa.
- **🏁 M4 (28 Jun):** parte obrigatória completa, sem leaks, sem crashes, README pronto. Pronto a submeter/defender.

> Folga: se algo escorregar, há 2-3 dias de almofada até início de Julho antes de marcar a defesa. Bónus só **depois** de tudo isto perfeito (fora deste plano, conforme pediste).

---

## 6. Estrutura de pastas

```
ft_irc/
├── Makefile                 # NAME, all, clean, fclean, re — sem relink
├── README.md                # requisitos da secção 9.2
├── .gitignore               # *.o, ircserv
├── docs/
│   ├── PLAN.md              # este plano
│   ├── ARCHITECTURE.md      # o contrato de interfaces + diagrama de fluxo
│   └── COMMANDS.md          # referência de comandos + casos de teste
├── include/
│   ├── Server.hpp
│   ├── Client.hpp
│   ├── Channel.hpp
│   ├── CommandHandler.hpp
│   ├── Message.hpp
│   ├── Replies.hpp          # macros dos numeric replies (RPL_/ERR_)
│   └── Utils.hpp
├── src/
│   ├── main.cpp
│   ├── server/              # Pessoa A
│   │   ├── Server.cpp
│   │   ├── ServerSocket.cpp # setup do socket
│   │   └── ServerLoop.cpp   # poll/accept/recv/send
│   ├── client/              # Pessoa A
│   │   └── Client.cpp
│   ├── channel/             # Pessoa B
│   │   └── Channel.cpp
│   ├── command/             # Pessoa B
│   │   ├── CommandHandler.cpp
│   │   ├── Parser.cpp
│   │   ├── auth/            # Pass.cpp, Nick.cpp, User.cpp
│   │   ├── channel/         # Join.cpp, Part.cpp, Topic.cpp
│   │   ├── messaging/       # Privmsg.cpp, Notice.cpp
│   │   └── oper/            # Kick.cpp, Invite.cpp, Mode.cpp
│   └── utils/
│       └── Utils.cpp
└── tests/                   # NÃO submetido — só para vocês
    ├── stub_dispatch.cpp
    └── mock_server.cpp
```

---

## 7. GitHub: estrutura e branches

### Modelo de branches (Git-flow leve, ideal para 2 pessoas)
- **`main`** — só código estável que **compila e funciona sempre**. Protegida; só recebe merges via PR. Recebe milestones (M1…M4).
- **`develop`** — branch de **integração**. É aqui que as features se juntam e onde respondem à pergunta "quando juntamos as nossas partes": **continuamente, em `develop`, por PR**.
- **`feature/*`** — uma por tarefa, criada a partir de `develop`.

### Branches concretas e para quê
| Branch | Dono | Propósito |
|---|---|---|
| `main` | ambos | código estável/entregável |
| `develop` | ambos | integração contínua |
| `feature/project-setup` | ambos | Makefile, headers/contrato, esqueleto, .gitignore |
| `feature/net-socket` | A | socket/setsockopt/bind/listen |
| `feature/net-poll-loop` | A | loop `poll()`, accept, recv/send, buffers |
| `feature/net-client-lifecycle` | A | classe `Client`, conexão/desconexão, SIGINT |
| `feature/proto-parser` | B | linha crua → `Message` |
| `feature/proto-auth` | B | PASS/NICK/USER + numerics de registo |
| `feature/proto-messaging` | B | PRIVMSG/NOTICE |
| `feature/proto-channel` | B | `Channel` + JOIN/PART/TOPIC |
| `feature/proto-oper` | B | KICK/INVITE/MODE i,t,k,o,l |
| `fix/<descrição>` | quem achar | correções de bugs |
| `docs/readme` | ambos | README + docs |

### Convenções
- **Nomes de branch:** `tipo/descrição-curta-em-kebab-case` (`feature/`, `fix/`, `docs/`, `refactor/`, `test/`).
- **Commits (Conventional Commits):** `tipo: mensagem no imperativo`. Ex.: `feat: add poll loop`, `fix: handle partial recv`, `docs: update README`, `refactor: split Server into socket/loop`. Histórico limpo = fácil de explicar na defesa.
- **Pull Requests:** sempre revistos pelo **outro** antes de merge (alinha com a ênfase do subject em revisão por pares). Descrição com: *o quê*, *porquê*, *como testei*, e checklist (compila com `-Wall -Wextra -Werror -std=c++98`? sem leaks? sem relink?).
- **Issues + Project board:** transformem as tarefas da secção 4/5 em *Issues* e usem um *GitHub Project* com colunas **To Do · In Progress · Review · Done**. É o vosso quadro Scrum.
- **`.gitignore`:** `*.o`, o binário `ircserv`, ficheiros de editor.
- **Proteção de `main`:** exigir PR + 1 aprovação (Settings → Branches), para nunca partir o `main`.

---

## 8. Metodologia de trabalho — Scrum & Daily Stand-up

O método que descreveste **tem nome: é o _Daily Stand-up_ (ou _Daily Scrum_)**, a cerimónia diária do **Scrum** (framework ágil). As três perguntas que mencionaste são exatamente o formato canónico:

1. **O que fiz desde o último stand-up?**
2. **O que vou fazer hoje?**
3. **Tenho algum impedimento (*blocker*)?**

### Como aplicar a 2 pessoas a trabalhar full-time
- **Stand-up assíncrono escrito** (10-15 min), uma vez por dia, num canal fixo (Discord/WhatsApp/Notion). Como têm horários apertados, escrito > presencial: cada um posta as 3 respostas à mesma hora combinada (ex.: 21h00). Impedimentos resolvem-se logo a seguir, em chamada se preciso.
- **Sprints semanais** = os Sprints 0-3 da secção 5. Cada um tem um objetivo claro (o *milestone*).
- **Sprint Review + Retro (fim de cada semana, ~20 min):** o que está feito (demo rápida), e o que melhorar na semana seguinte (o quê correu bem/mal). Ajusta o plano com dados reais.
- **Backlog = GitHub Issues**; o **board** (secção 7) é o radiador de informação. Mover cartões To Do → In Progress → Review → Done dá a "definição de pronto".
- **Definition of Done** por tarefa: compila com as flags exigidas, sem leaks (valgrind), revista pelo par, e fundida em `develop`.

> Posso, se quiseres, criar-vos o board no Notion e/ou agendar um lembrete diário do stand-up — vê o fim da mensagem.

---

## 9. Documentação

### 9.1 Documentar funções e código
Usem blocos de comentário **estilo Doxygen** nos headers (mesmo sem correr Doxygen — o formato é standard e legível). Documentem o **porquê**, não o óbvio "o quê".

```cpp
/**
 * @brief Extrai uma mensagem completa (terminada em \r\n) do buffer de entrada.
 *
 * O TCP entrega um fluxo de bytes; uma mensagem pode chegar fragmentada.
 * Esta função consome do _inBuffer apenas se existir um \r\n completo.
 *
 * @param lineOut  recebe a linha sem o \r\n final.
 * @return true se extraiu uma mensagem; false se ainda está incompleta.
 */
bool Client::extractMessage(std::string& lineOut);
```

Regras práticas:
- Cada **classe** leva um comentário a dizer o que representa e quem é dono dela.
- Cada **função não-trivial** leva `@brief`, `@param`, `@return` e, se aplicável, efeitos colaterais (ex.: "fecha o fd").
- Nomes claros > comentários: `handlePartialRecv()` é melhor que `func2()` com comentário.
- Comentar *decisões* não-óbvias (ex.: porquê `SO_REUSEADDR`, porquê `fd` em vez de `Client*`).
- Lembrar o aviso do subject: na defesa têm de **explicar tudo**. Não comitem código (vosso ou de IA) que não conseguem justificar.

### 9.2 Documentar o GitHub
**README.md na raiz** (obrigatório pelo subject, em **inglês**), com no mínimo:
- **Primeira linha em itálico, exatamente:** *This project has been created as part of the 42 curriculum by `<login1>`, `<login2>`.*
- Secção **Description** — objetivo e visão geral do projeto.
- Secção **Instructions** — como compilar/instalar/executar (`make`, depois `./ircserv <port> <password>`).
- Secção **Resources** — referências clássicas (RFCs, Beej's Guide, ircdocs) **e** descrição de **como a IA foi usada** (em que tarefas/partes). O subject exige isto explicitamente.
- Extras recomendados: lista de funcionalidades/comandos suportados, exemplos de uso com o cliente de referência, escolhas técnicas (porquê `poll`, modelo de dados).

Para além do README, documentem o repo com: **Issues** bem escritas (o backlog), **PRs** descritivos e revistos, **commits** convencionais, e os docs em `docs/` (`ARCHITECTURE.md` com o contrato/diagrama, `COMMANDS.md` com casos de teste). Tudo isto torna o projeto fácil de entender por peers/staff/recrutadores — que é o propósito do README segundo o subject.

---

## 10. Checklist de defesa (critérios obrigatórios)

- [ ] Compila com `c++ -Wall -Wextra -Werror -std=c++98`, sem warnings.
- [ ] `Makefile`: `all`, `clean`, `fclean`, `re`, `$(NAME)` — **sem relink** desnecessário.
- [ ] Sem bibliotecas externas nem Boost.
- [ ] **Um único `poll()`** (ou equivalente) para tudo; **nunca** `recv`/`send` sem passar pelo poll.
- [ ] Todos os fds **não-bloqueantes**; **sem `fork()`**.
- [ ] Lida com **dados parciais** (teste `nc` com `^D`) — reconstrói o comando.
- [ ] **Nunca crasha** (input malformado, cliente que cai, out-of-memory) e **sem leaks** (valgrind).
- [ ] Autenticação por password; NICK; USER; JOIN; PRIVMSG (e difusão no canal).
- [ ] Operadores vs utilizadores normais.
- [ ] KICK, INVITE, TOPIC, MODE (`i`, `t`, `k`, `o`, `l`).
- [ ] Liga sem erros com o **cliente de referência** escolhido.
- [ ] README com as secções obrigatórias (incl. uso de IA), em inglês.
- [ ] Ambos conseguem **explicar qualquer linha** e fazer uma pequena modificação ao vivo.
```