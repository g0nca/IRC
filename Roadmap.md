# ROADMAP ft_irc — Guia passo-a-passo (do zero ao defense)

> Companheiro do `PLAN_ft_irc.md`. O plano diz **o quê** e **quem**; este roteiro diz
> **em que ordem**, **como testar cada etapa** e **qual o output exato que devem ver**
> antes de passar à etapa seguinte. Se o output não bate certo, **não avancem** — o erro
> é mais barato de apanhar aqui do que na integração.

---

## Como usar este guia

Cada etapa tem sempre quatro campos:

- **Objetivo** — o que fica feito no fim da etapa.
- **Testar** — o comando/ação concreta para validar.
- **Output esperado** — exatamente o que devem ver. É o "teste de aceitação" da etapa.
- **Precisa de / Desbloqueia** — dependências e o que esta etapa liberta na outra track.

Marcadores:

- `[A]` — tarefa do **Pessoa A** (camada de rede: socket, poll, recv/send, buffers).
- `[B]` — tarefa do **Pessoa B** (camada de protocolo: parsing, auth, comandos, canais).
- `[A+B]` — feito em conjunto (ou ponto de integração).

As duas tracks correm **em paralelo**. A e B só se cruzam nos **checkpoints `INT-x`**.
Até lá, cada um testa o seu lado com um *duplo* do outro (A com um *stub* de dispatch,
B com um *mock* de servidor). Esse é o objetivo do contrato que já está congelado nos headers.

---

## A costura A ↔ B (o que atravessa a fronteira)

Só **uma** chamada liga as duas camadas:

```
CommandHandler::dispatch(Client& client, const Message& msg);
```

Já está demonstrado, hard-coded, na `main.cpp` (`demoContract()`). É o vosso contrato vivo.
Correr `./ircserv 6667 pass` imprime exatamente isto — é o **primeiro output esperado de todos**:

```
[1] RAW INPUT  (Pessoa A reads from the socket):
    "PRIVMSG #42 :Hello team!\r\n"

[2] PARSED Message  (Pessoa B receives this struct):
    command  = "PRIVMSG"
    params   = ["#42"]
    trailing = "Hello team!"  (present=true)

[3] REPLY  (Pessoa B builds it, Pessoa A sends via send()):
    ":alice!alice@localhost PRIVMSG #42 :Hello team!\r\n"
```

- A produz `[1]` (bytes crus) e envia `[3]` (bytes crus). **Nunca olha para dentro do `Message`.**
- B recebe `[2]` (`Message` já parseado) e devolve a resposta. **Nunca toca em sockets.**

Enquanto este formato não mudar, podem trabalhar separados sem se bloquearem.

---

## Fase 0 — Setup (ambos, ~meia tarde) `[A+B]`

**Objetivo:** repositório, estrutura de pastas, `Makefile` e o scaffold a compilar nas duas máquinas.

**Testar:**

```bash
make            # compila sem warnings
./ircserv                       # sem argumentos
./ircserv abc pass              # porta inválida
./ircserv 6667 pass             # válido
```

**Output esperado:**

```
$ make
c++ -Wall -Wextra -Werror -std=c++98 ... -o ircserv
$ ./ircserv
Usage: ./ircserv <port> <password>
$ ./ircserv abc pass
Error: <port> must be an integer in [1, 65535].
$ ./ircserv 6667 pass
ircserv ready to start on port 6667 (password defined, 4 chars).
======================================================== ...
```

(seguido do bloco `CONTRACT DEMO` acima)

> Regra de ouro do `Makefile`: `-Wall -Wextra -Werror -std=c++98`. Se compila com warnings, não está feito.

**Desbloqueia:** tudo. A partir daqui A e B separam-se.

---

# TRACK A — Camada de Rede

Meta da track: um servidor que aceita N clientes em simultâneo num **único `poll()`**, com fds
**não-bloqueantes**, que reconstrói linhas partidas, entrega cada linha completa ao dispatcher
e devolve as respostas — sem nunca crashar nem bloquear.

> Durante toda a Track A, B ainda não está ligado. Usa um **stub de dispatch**: em vez de
> `_commands.dispatch(...)`, chama uma função que só faz *echo* da linha de volta ao cliente.
> Assim testas a rede de ponta a ponta sem dependeres do protocolo.

---

### A1 — Argumentos e esqueleto `[A]`

**Objetivo:** validar `argc`, porta (1–65535) e password não-vazia. (Já feito na `main.cpp`.)

**Testar:** os quatro comandos da Fase 0.

**Output esperado:** ver Fase 0. Casos de erro devolvem código de saída `1`; o caso válido imprime a linha `ready`.

**Desbloqueia:** A2.

---

### A2 — Socket de escuta `[A]`

**Objetivo:** `setupSocket()` — `socket()` → `setsockopt(SO_REUSEADDR)` → `bind()` → `listen()`.
Logo a seguir, marcar o fd de escuta como não-bloqueante.

**Testar:** correr o servidor e, noutro terminal:

```bash
ss -tlnp | grep 6667        # ou: lsof -i :6667
```

**Output esperado:**

```
LISTEN 0  128  0.0.0.0:6667  0.0.0.0:*  users:(("ircserv",pid=...,fd=3))
```

A porta aparece em estado `LISTEN`. Reiniciar o servidor imediatamente **não** dá
`bind: Address already in use` (prova que o `SO_REUSEADDR` está lá).

**Desbloqueia:** A3.

---

### A3 — Loop `poll()` + aceitar clientes `[A]`

**Objetivo:** o único `poll()` da aplicação. Vigia o fd de escuta; em `POLLIN` nele,
`acceptNewClient()` (accept → `O_NONBLOCK` → criar `Client*` → registar no `_pollFds` e no `_clients`).

**Testar:**

```bash
nc localhost 6667        # terminal 2
nc localhost 6667        # terminal 3 (segundo cliente em simultâneo)
```

**Output esperado (logs do servidor):**

```
[+] client connected: fd=4 (total: 1)
[+] client connected: fd=5 (total: 2)
```

O servidor **continua vivo** com os dois ligados ao mesmo tempo. Nada bloqueia.

**Desbloqueia:** A4.

---

### A4 — recv + buffer por cliente + framing `\r\n` + stub de dispatch `[A]`  ⭐

**Objetivo (a etapa mais importante da Track A):** em `POLLIN` de um cliente, `recv()` para um
buffer **por cliente**; sempre que houver uma linha terminada em `\r\n`, extraí-la (`Client::extractMessage`)
e entregá-la. Por agora, ao **stub**: imprime a linha e faz echo dela de volta.

Pontos críticos:
- TCP é um *stream*: uma chamada `recv` pode trazer meia linha, ou duas linhas e meia. Só processas linhas **completas**; o resto fica no buffer à espera do próximo `recv`.
- `recv` devolve `0` → o cliente fechou. Trata como desconexão (A6).

**Testar (o teste clássico do subject):**

```bash
nc -C localhost 6667
PRIVMSG #42 :Hello team!        # Enter manda "...\r\n" (por causa do -C)
```

Depois, o teste de **pacote partido**: escrever uma linha **sem** Enter e carregar `Ctrl+D`,
e só mais tarde completar com `Ctrl+D` de novo.

**Output esperado:**

Linha completa → o servidor regista e ecoa:

```
[recv fd=4] line: "PRIVMSG #42 :Hello team!"
```
e o `nc` recebe de volta `PRIVMSG #42 :Hello team!`.

Texto **sem** `\r\n` → **nada** é processado; fica em buffer. Só quando chegar o `\r\n`
é que a linha aparece. (Se processares dados sem ter o terminador, falhaste esta etapa.)

**Precisa de:** A3. **Desbloqueia:** INT-1 (com B2) e A5.

---

### A5 — Envio: out-buffer + `POLLOUT` `[A]`

**Objetivo:** `sendToClient()` acumula bytes num **out-buffer** do cliente e liga `POLLOUT`
no `poll()` desse fd. Em `POLLOUT`, `flushClientOutput()` faz `send()` do que houver; se enviar
só uma parte, guarda o resto; quando o buffer esvazia, desliga `POLLOUT`.

**Testar:** repetir o teste de A4 — mas agora o echo passa pelo out-buffer, não por um `send()` direto.

**Output esperado:** o `nc` recebe a linha de volta, igual a A4. A diferença é interna: o envio
nunca bloqueia mesmo que o cliente seja lento. (Verificar com um cliente que não lê: o servidor
não congela à espera.)

**Precisa de:** A4. **Desbloqueia:** INT-2 (com B3).

---

### A6 — Desconexão, limpeza e sinais `[A]`

**Objetivo:** `disconnectClient()` (tirar de canais via contrato, `close(fd)`, remover do `poll`
e do `_clients`, `delete`); ignorar `SIGPIPE`; `SIGINT` (Ctrl+C) levanta a flag `_running=false`
para sair do loop **limpo**.

Pontos críticos (causas clássicas de crash no defense):
- **`SIGPIPE`**: escrever para um socket que o cliente fechou mata o processo por defeito. `signal(SIGPIPE, SIG_IGN)` no arranque.
- **Não mutar `_pollFds` a meio da iteração**: marca para remover e limpa no fim da volta.

**Testar:**

```bash
# cliente sai (Ctrl+C no nc)        -> servidor deteta recv()==0
# matar um cliente a meio de um envio -> servidor NÃO crasha (SIGPIPE ignorado)
# Ctrl+C no servidor                 -> shutdown limpo
valgrind --leak-check=full ./ircserv 6667 pass
```

**Output esperado:**

```
[-] client disconnected: fd=4 (total: 0)
^C
[*] shutting down, closing all sockets...
...
==12345== All heap blocks were freed -- no leaks are possible
==12345== ERROR SUMMARY: 0 errors from 0 contexts
```

**Precisa de:** A4. **Desbloqueia:** INT-3.

---

# TRACK B — Camada de Protocolo

Meta da track: dado um `Message` já parseado, executar o comando correto, gerir registo,
clientes e canais, e produzir as respostas IRC exatas (numéricas e mensagens com prefixo).

> Durante toda a Track B, A ainda não está ligado. Usa um **mock de servidor**: uma classe
> pequena que implementa o contrato (`getClientByNick`, `sendToClient`, `broadcastToChannel`, ...)
> guardando as respostas num `std::vector<std::string>` em memória, mais um `main` de teste que
> cria `Client`s falsos e alimenta `Message`s à mão. Assim verificas cada comando sem rede.

---

### B1 — Harness de teste + parser de linha `[B]`

**Objetivo:** o `main` de teste (mock server + clientes falsos) e a função que transforma uma
linha crua num `Message` (prefixo opcional, comando, params, trailing após o primeiro ` :`).

**Testar:** alimentar a linha do contrato e imprimir o `Message` resultante.

```cpp
Message m = parse("PRIVMSG #42 :Hello team!\r\n");
```

**Output esperado (tem de bater certo com o bloco `[2]` do contrato):**

```
command  = "PRIVMSG"
params   = ["#42"]
trailing = "Hello team!"  (present=true)
```

Casos a cobrir já aqui: linha sem trailing (`JOIN #chan` → params=["#chan"], present=false),
trailing com espaços, múltiplos params (`MODE #chan +o bob`).

**Desbloqueia:** B2.

---

### B2 — Registo: PASS / NICK / USER + boas-vindas `[B]`

**Objetivo:** handshake `PASS` → `NICK` → `USER`. Validar password contra o contrato
(`_server.getPassword()`), nick único e válido, marcar `registered` só quando os três passos
estão completos, e então enviar os numéricos 001–004.

**Testar:** alimentar, em sequência:

```
PASS pass
NICK alice
USER alice 0 * :Alice Real
```

**Output esperado (após o USER):**

```
:ircserv 001 alice :Welcome to the IRC network, alice
:ircserv 002 alice :Your host is ircserv, running version 1.0
:ircserv 003 alice :This server was created for the 42 ft_irc project
:ircserv 004 alice ircserv 1.0 o itkol
```

Casos de erro (verificar um a um):

```
PASS errada           -> :ircserv 464 * :Password incorrect
NICK (sem nada)       -> :ircserv 431 * :No nickname given
NICK alice (repetido) -> :ircserv 433 alice alice :Nickname is already in use
PRIVMSG antes de registar -> :ircserv 451 * :You have not registered
USER sem params       -> :ircserv 461 alice USER :Not enough parameters
```

> Nota: antes do `NICK`, o nick ainda não existe — usa-se `*` como placeholder nas numéricas.

**Desbloqueia:** INT-1 (com A4). É o primeiro comando que faz sentido testar com cliente real.

---

### B3 — Mensagens: PRIVMSG / NOTICE `[B]`

**Objetivo:** encaminhar para um nick ou para um canal. `NOTICE` é igual mas **nunca** gera
resposta de erro automática (regra do protocolo).

**Testar:** com `alice` e `bob` registados, `alice` manda `PRIVMSG bob :hi`.

**Output esperado (o que `bob` recebe):**

```
:alice!alice@localhost PRIVMSG bob :hi
```

Para canal (`PRIVMSG #chan :olá a todos`), todos os membros **menos o emissor** recebem:

```
:alice!alice@localhost PRIVMSG #chan :olá a todos
```

Erros:

```
PRIVMSG (sem destino) -> :ircserv 411 alice :No recipient given (PRIVMSG)
PRIVMSG bob (sem texto) -> :ircserv 412 alice :No text to send
PRIVMSG ninguem :oi   -> :ircserv 401 alice ninguem :No such nick/channel
```

**Precisa de:** B2. **Desbloqueia:** INT-2 (com A5).

---

### B4 — Canais: Channel + JOIN / PART + broadcast `[B]`

**Objetivo:** classe `Channel` operacional; `JOIN` (cria canal se não existir, primeiro a entrar
fica **operador**), `PART`. Difundir entrada/saída a todos os membros.

**Testar:** `alice` faz `JOIN #chan`.

**Output esperado:**

```
:alice!alice@localhost JOIN #chan
:ircserv 353 alice = #chan :@alice
:ircserv 366 alice #chan :End of /NAMES list
```

(`@alice` porque é operadora — criou o canal.) Quando `bob` entra a seguir, **ambos** recebem
`:bob!bob@localhost JOIN #chan`, e `bob` recebe a lista `353` com `:@alice bob`.

`PART #chan :bye` → todos recebem `:alice!alice@localhost PART #chan :bye`.
Quando o último sai, o canal é destruído (`removeChannelIfEmpty`).

Erros: `JOIN` sem param → `461`; `PART` de canal onde não está → `:ircserv 442 alice #chan :You're not on that channel`.

**Precisa de:** B2. **Desbloqueia:** INT-3.

---

### B5 — TOPIC `[B]`

**Objetivo:** consultar e definir o tópico, respeitando o modo `+t` (só operador define).

**Testar:** `TOPIC #chan` (consulta) e `TOPIC #chan :Bem-vindos` (define).

**Output esperado:**

```
TOPIC #chan  (sem tópico)  -> :ircserv 331 alice #chan :No topic is set
TOPIC #chan  (com tópico)  -> :ircserv 332 alice #chan :Bem-vindos
TOPIC #chan :Bem-vindos    -> broadcast: :alice!alice@localhost TOPIC #chan :Bem-vindos
```

Com `+t` ativo e quem define não é operador → `:ircserv 482 bob #chan :You're not channel operator`.

**Precisa de:** B4. **Desbloqueia:** INT-3.

---

### B6 — Operador: KICK / INVITE `[B]`

**Objetivo:** `KICK` (operador expulsa membro) e `INVITE` (convidar; relevante com `+i`).

**Testar:** `alice` (op) faz `KICK #chan bob :spam` e `INVITE carol #chan`.

**Output esperado:**

```
KICK   -> broadcast no canal: :alice!alice@localhost KICK #chan bob :spam
INVITE -> ao alice:  :ircserv 341 alice carol #chan
          ao carol:  :alice!alice@localhost INVITE carol #chan
```

Erros: não-operador a dar `KICK` → `482`; `KICK` de alguém que não está no canal →
`:ircserv 441 alice bob #chan :They aren't on that channel`.

**Precisa de:** B4. **Desbloqueia:** INT-4.

---

### B7 — MODE: i / t / k / o / l `[B]`

**Objetivo:** os cinco modos obrigatórios — `+i/-i` (invite-only), `+t/-t` (tópico só-op),
`+k/-k <key>` (password do canal), `+o/-o <nick>` (dar/tirar op), `+l/-l <n>` (limite de utilizadores).

**Testar:** cada modo, com `alice` como operadora.

**Output esperado (broadcast no canal):**

```
MODE #chan +i        -> :alice!alice@localhost MODE #chan +i
MODE #chan +t        -> :alice!alice@localhost MODE #chan +t
MODE #chan +k segredo -> :alice!alice@localhost MODE #chan +k segredo
MODE #chan +o bob    -> :alice!alice@localhost MODE #chan +o bob
MODE #chan +l 10     -> :alice!alice@localhost MODE #chan +l 10
MODE #chan  (consulta) -> :ircserv 324 alice #chan +it
```

Efeitos a verificar depois: `+k` faz `JOIN #chan` sem key dar `:ircserv 475 ... :Cannot join channel (+k)`;
`+i` faz `JOIN` sem convite dar `:ircserv 473 ... :Cannot join channel (+i)`;
`+l 1` cheio dá `:ircserv 471 ... :Cannot join channel (+l)`.
Não-operador → `482`. Modo desconhecido → `:ircserv 472 alice X :is unknown mode char to me`.

**Precisa de:** B4, B6. **Desbloqueia:** INT-4.

---

# Pontos de integração `[A+B]`

Aqui é que se junta. Em cada `INT-x`: trocar o stub/mock pelo lado real do outro, compilar tudo
junto, e correr com um **cliente real**. Usar `nc` para os primeiros e um cliente IRC de referência
(**irssi**, **WeeChat** ou **HexChat**) para os comandos de operador.

### INT-1 — Autenticação real (depois de A4 + B2)

Liga o `recv`/buffer/framing real (A) ao `dispatch` real (B), substituindo o stub de echo.

**Testar:**

```bash
nc -C localhost 6667
PASS pass
NICK alice
USER alice 0 * :Alice
```

**Output esperado:** o `nc` recebe os numéricos 001–004 (ver B2). É o primeiro fim-a-fim real:
bytes da socket → linha → `Message` → handler → resposta → bytes de volta.

### INT-2 — Mensagens entre dois clientes reais (depois de A5 + B3)

**Testar:** dois `nc` registados; um manda `PRIVMSG <nick_do_outro> :olá`.

**Output esperado:** o outro terminal mostra `:alice!alice@localhost PRIVMSG bob :olá`.

### INT-3 — Canais e desconexão (depois de A6 + B4/B5)

**Testar:** com irssi/WeeChat: `/join #chan`, `/msg #chan olá`, `/topic #chan tema`, sair.

**Output esperado:** o cliente de referência mostra a entrada no canal, a lista de nomes,
as mensagens difundidas e a mudança de tópico — **sem mensagens de erro de protocolo** no cliente.
Sair não crasha o servidor.

### INT-4 — Comandos de operador (depois de B6/B7)

**Testar:** com dois clientes de referência num canal: `/mode #chan +o`, `/kick`, `/invite`, `+i`, `+k`, `+l`, `+t`.

**Output esperado:** o cliente de referência reflete cada mudança de modo e ação de operador
corretamente; um não-operador a tentar é recusado com o erro `482`.

---

## Tabela de dependências (ordem segura)

| Etapa | Precisa de | Liberta | Sprint (PLAN) |
|-------|-----------|---------|---------------|
| Fase 0 | — | tudo | Sprint 1 |
| A1 | Fase 0 | A2 | Sprint 1 |
| A2 | A1 | A3 | Sprint 1 |
| A3 | A2 | A4 | Sprint 1–2 |
| A4 ⭐ | A3 | INT-1, A5 | Sprint 2 |
| A5 | A4 | INT-2 | Sprint 2 |
| A6 | A4 | INT-3 | Sprint 2–3 |
| B1 | Fase 0 | B2 | Sprint 1 |
| B2 | B1 | INT-1, B3 | Sprint 2 |
| B3 | B2 | INT-2 | Sprint 2 |
| B4 | B2 | INT-3, B5, B6 | Sprint 3 |
| B5 | B4 | INT-3 | Sprint 3 |
| B6 | B4 | INT-4 | Sprint 3–4 |
| B7 | B4, B6 | INT-4 | Sprint 4 |
| INT-1 | A4 + B2 | — | Sprint 2 |
| INT-2 | A5 + B3 | — | Sprint 2–3 |
| INT-3 | A6 + B4/B5 | — | Sprint 3 |
| INT-4 | B6/B7 | — | Sprint 4 |

**Caminho crítico:** Fase 0 → (A1→A2→A3→A4) ‖ (B1→B2) → **INT-1**. Assim que o INT-1 passa,
têm um servidor que autentica clientes reais — o esqueleto está provado e o resto é acrescentar comandos.

---

## Checklist final (antes do defense) `[A+B]`

```bash
# 1. Compila limpo, padrão certo
make            # -Wall -Wextra -Werror -std=c++98, zero warnings

# 2. Sem fugas de memória
valgrind --leak-check=full ./ircserv 6667 pass

# 3. Vários clientes em simultâneo (3+ terminais nc / clientes de referência)

# 4. Pacotes partidos (o teste do subject)
nc -C localhost 6667     # mandar comando aos bocados + Ctrl+D

# 5. Cliente de referência completo (irssi / WeeChat / HexChat)
#    registar, join, privmsg, topic, kick, invite, todos os modes i/t/k/o/l

# 6. Robustez: input lixo, linhas enormes, cliente morto a meio de um send
#    -> o servidor NUNCA crasha; só fecha em Ctrl+C (SIGINT)
```

Critérios que o avaliador vai confirmar:
- Um **único** `poll()` (ou equivalente) para todo o I/O; **todos** os fds não-bloqueantes.
- Sem `fork()`. Sem `recv`/`send` fora do fluxo do `poll()`.
- Nenhum crash em nenhum cenário; encerramento limpo só por decisão do operador.
- Comandos obrigatórios todos a funcionar com cliente de referência.
- C++98, Orthodox Canonical Form onde aplicável, `Makefile` com as regras do subject.

> Quando os seis passos do checklist passam com cliente de referência e `valgrind` limpo,
> o projeto está pronto para defender.