# Mega Stars

Arena brawler 3x3 no modo **Coleta de Cristais**: pegue 10 cristais e segure a
contagem regressiva até o fim. Personagens, arte, nomes e mecânicas são
originais — nada é copiado de outros jogos.

## Edições

| Edição | Linguagem | Plataformas | Binário |
| --- | --- | --- | --- |
| **JS Edition** | JavaScript | Navegador (desktop e celular) | servidor Node + cliente canvas |
| **PC Edition** | C++17 | Windows 10+, macOS 10.13 High Sierra+ (iMac 2011), Ubuntu | `mega-stars` |
| **Linux ASCII** | C++17 | Ubuntu (terminal) | `mega-stars --headless` |
| **Mobile Edition** | C++17 | Android (Redmi Note 8+), iOS/iPadOS (iPad 9+) | `mega-stars-mobile` |
| **Pocket Edition** | C++17 | Android antigo (Galaxy J5 Prime), iPhone 6 | `mega-stars-pocket` |

Todas as edições nativas compartilham o mesmo núcleo de simulação
(`core/`), então as regras são idênticas em qualquer aparelho. O que muda entre
elas é só o orçamento de desempenho (`app/src/Profile.cpp`): resolução, taxa de
quadros, taxa de simulação, partículas e controles de toque.

## JS Edition

```bash
cd editions/js
npm install
npm start            # http://localhost:8780
```

O mesmo processo abre duas portas:

- **8780** – site + WebSocket para o navegador;
- **8781** – TCP com o mesmo protocolo JSON para as edições nativas.

Ou seja, quem está no navegador joga na mesma partida de quem está no PC ou no
celular. Variáveis de ambiente: `PORT`, `TCP_PORT`, `SEED`, `TEAM_SIZE`.

Controles no navegador: **WASD** move, **mouse** mira, **clique** atira,
**E / botão direito** solta o super. No celular aparecem joystick e botões.

## Edições nativas

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DMEGA_BUILD_MOBILE=ON -DMEGA_BUILD_POCKET=ON
cmake --build build -j
./build/bin/mega-stars                                # offline contra bots
./build/bin/mega-stars --server=meuservidor.com:8781  # online
./build/bin/mega-stars --headless                     # terminal em ASCII
./build/bin/mega-stars --brawlers                     # lista os personagens
```

Detalhes por plataforma (Windows, macOS High Sierra, Android, iOS) e a
integração com o Epic Online Services estão em [`docs/build.md`](docs/build.md).
O protocolo de rede está em [`docs/protocol.md`](docs/protocol.md).

## Personagens

| Id | Nome | Estilo |
| --- | --- | --- |
| `faisca` | Faísca | tiro reto equilibrado |
| `bruto` | Bruto | tanque com leque de curto alcance |
| `estopim` | Estopim | bombas que passam por cima das paredes |
| `rajada` | Rajada | rápida, rajadas curtas |
| `mira` | Mira | franco-atiradora de dano alto |
| `pilha` | Pilha | suporte que cura aliados |

## Testes

```bash
cd editions/js && npm test && npx eslint .   # JS Edition
ctest --test-dir build --output-on-failure   # núcleo C++
```
