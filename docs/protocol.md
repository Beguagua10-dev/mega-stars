# Protocolo de rede

Mensagens JSON. No navegador elas trafegam em um WebSocket (porta 8780); nas
edições nativas, as mesmas mensagens trafegam em TCP puro separadas por `\n`
(porta 8781). Por isso navegador, PC e celular jogam na mesma partida.

## Cliente → servidor

```jsonc
{ "t": "join", "name": "Danilo", "brawler": "faisca" }

{ "t": "input", "mx": 0.0, "my": -1.0,   // direção do movimento (normalizada)
                "ax": 1.0, "ay": 0.0,    // direção da mira (normalizada)
                "s": 1,                  // atirando
                "u": 0 }                 // usando o super
```

## Servidor → cliente

```jsonc
{ "t": "welcome",
  "id": 7,                 // id do seu jogador
  "seed": 1337,
  "w": 30, "h": 34,
  "tiles": [0,1,2,...],    // 0 chão, 1 parede, 2 arbusto, 3 cerca
  "roster": [ ... ] }      // personagens disponíveis

{ "t": "state",
  "match": { "ph": 1,      // 0 aquecimento, 1 em jogo, 2 contagem, 3 fim
             "pt": 132.5,  // tempo restante da fase
             "el": 18.2,
             "g0": 3, "g1": 5 },
  "players": [ { "id": 7, "n": "Danilo", "b": "faisca", "tm": 0,
                 "x": 15.2, "y": 17.8, "ax": 1, "ay": 0,
                 "hp": 3600, "mhp": 3600, "am": 2.4, "ac": 3,
                 "sc": 40, "gm": 2, "rt": 0, "k": 1, "d": 0, "bot": false } ],
  "proj": [ { "id": 91, "tm": 1, "x": 12.0, "y": 9.5 } ],
  "gems": [ { "id": 88, "x": 15.0, "y": 17.0 } ],
  "feed": ["Nina eliminou Rui"] }
```

O servidor é autoritativo: o cliente só envia intenção e desenha o `state` que
recebe (20 snapshots por segundo).

## Backend EOS

`--server=eos:<lobby>` usa o Epic Online Services em vez do TCP. O binário
precisa ter sido compilado com `-DMEGA_EOS_SDK=<caminho do SDK>` e as
credenciais vêm das variáveis `MEGA_EOS_PRODUCT_ID`, `MEGA_EOS_SANDBOX_ID`,
`MEGA_EOS_DEPLOYMENT_ID`, `MEGA_EOS_CLIENT_ID` e `MEGA_EOS_CLIENT_SECRET`.
