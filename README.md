# UNI2 Improvement Mod

Training and quality-of-life mod for **UNDER NIGHT IN-BIRTH II Sys:Celes** (Steam, `uni2.exe`).

It loads as a `dinput8.dll` proxy and draws a Dear ImGui overlay inside the game's Direct3D 9
renderer. Inspired by, and architecturally indebted to,
[BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod).

Donwload the .dll from the [Release](https://github.com/Zanaylo/UNI2-Improvement-Mod/releases)

**[English](#english) · [Português](#português) · [日本語](#日本語)**

---

## A note on online play

**Nothing in this mod is built to give anyone an advantage online, and nothing in it does.**

Every training tool that can alter what the simulation does - frame stepping, freezing, driving a
character by hand, the dummy scripts - is hard-gated to offline modes. The gate is the game's own
peer-to-peer traffic: while the game has sent a packet to an opponent in the last three seconds,
those tools refuse to run. The game uses GGPO rollback netcode, and anything that touches simulation
state during a match desynchronises it.

What does run online is cosmetic and read-only: the custom palettes, which travel beside the game
over Steam rather than through the netcode and cannot affect the match, and the performance options,
which only change how the frame reaches your monitor.

If you find something here that gives an edge in a real match, that is a bug. Report it.

---

# English

## Installing

Copy `dinput8.dll` next to `uni2.exe`:

```
<Steam>\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes\
```

Press **F1** in game for the overlay. To uninstall, delete `dinput8.dll`.

`UNI2_IM.ini` is written with defaults the first time the mod runs, next to the DLL. Delete it to go
back to defaults. Every key in it is documented under [The ini file](#the-ini-file).

The frame meter draws with the game's own panel art and font. The mod lifts those files out of the
game's `d` archive into `UNI2-IM\Assets` on first run, so there is nothing to extract by hand and no
game data in the download. Delete the folder and it is rebuilt; if the archive cannot be read the
meter still works, drawn in flat colours.

To chain-load another `dinput8.dll` wrapper, put its full path in `[Mod] DinputDllWrapper`.

## Features

### Overlay

**F1**. Everything lives in one window with three sections - Training, Custom and Config - plus
separate windows for the hitbox viewer, the frame meter legend, Player Control and Performance.

While you are typing into any text field the keyboard belongs to the overlay and the game is handed
nothing, and the same is true while a keybind is being captured. A key still held when a field lets
go is masked until you release it, so nothing comes out as a move.

### Hitbox viewer

**F2**. Draws every box the engine has, for characters **and** projectiles, in the game's own
colours. Decoration is filtered out by the engine's own `_Exist_NoHantei` flag rather than guessed
at, so what you see is what the game reads.

There is no box for a throw, for the D Shield, or for proximity guard - the game does not have one.
A throw's catch region is an ordinary attack box on a frame carrying a throw attribute.

### Frame meter

**F3**. A meter with a band per state, the frame count printed inside each finished band,
the exchange totalled on its own line, and a status row naming every invincibility in force on that
frame. Frame advantage is measured against the game's own advantage field, not inferred from the
animation.

It starts centred near the bottom of the screen and can be dragged with the mouse. It is drawn
straight onto the back buffer and has none of the overlay's hit testing, so a click that lands on it
moves it whatever else you were doing.

### Pause and frame stepping

**F5** pauses, **F6** steps one frame. Holding F6 repeats.

Two freeze modes. *Tick stop* suppresses the game's whole frame and is the default. *Hitstun Stop*
reuses the engine's own hitstop, which keeps menus live but renders effects wrong - that is inherent
to the technique rather than a defect.

Auto pause can stop the game by itself on an attack, on an armoured move, or at a given hit of a
combo, and resume after a countdown.

### Player Control

A window of its own. It shows what both sides are inputting, live, as a numpad stick and four button
lights. You can point your own pad or keyboard at either character, hold a direction or a button on
the dummy, tap one for a few frames, or run a written script per side.

It also measures input lag: the wall clock from a physical press to that character's own input field
changing. The keyboard and every pad are sampled about a thousand times a second on a thread of
their own, rather than read from the game's once-a-frame poll, which would quantise every answer to
16.7 ms and measure nothing. Both of the pad APIs the game uses are covered.

### Palettes

Any colour on either character, applied live. A colour belongs to the character rather than to a
half of the screen, so it stays with them across a crossover, and what each character wears is
remembered and put back on automatically next time.

Palettes are saved as ordinary `.pal` files - the game's own format, which Hantei-kun also writes -
under `UNI2-IM\Palettes\<character>`, with a name, an author and a description. Effect colours are
part of a palette and are saved and loaded with it. The character palette list has a **Default**
entry that puts the game's own colours back.

Online, your palette is sent to the other player beside the game over Steam. **See the other
player's colours** decides their side in one switch: on, their palette is read as it arrives and
worn, so you see what they chose; off, their packets are dropped and their side is left the way the
game gives it. Yours is sent either way, and a player without the mod sees nothing.

**Palette Nativa** is a different feature: the game's own colour customiser, driven from the
overlay. It builds a colour out of the character's stock palettes, one per part, so it cannot be any
colour you like - but the game saves it and **every** opponent sees it, mod or no mod.

### Player Card

Edits the card the game publishes to your opponent: the four plate layers and the plate title. The
title is free text - the three shop words are only the picker's state - so any phrase you write is
saved and reaches the opponent as written.

### Performance

Its own window, opened from Config. It exists because the engine's frame pacing has a specific,
findable problem: the game runs its message pump on one thread and its frame on another, and every
frame the frame thread blocks on a message that only the pump can answer while the pump is asleep.
Windows also takes back the millisecond timer resolution while the game sits in the background,
which is what an alt-tab leaves behind.

Three options, each with its real trade-off written next to it, and two presets. The window reports
what is **actually** in force, read back from the device rather than from what the mod asked for.

The **Metrics** tab measures. Frame interval with its spread, a quarter-millisecond histogram around
the target, two-cluster detection for the judder a median cannot see, how long Present blocks, and a
paste-ready summary for bug reports.

An earlier build of this mod made the game feel worse, and did so by default: it added a second back
buffer for everyone and dragged high-refresh monitors down to 60 Hz in fullscreen, which cost a
frame of input latency and bought nothing. It no longer touches the display unless asked.

### Memory debug

Off by default; set `[Debug] MemoryDebug = 1`, then **Ctrl+F1**. Raw readouts and the search tools
used to build the rest of the mod: entity list, hitbox dump, move state, a global scanner, a diff
search, a pointer follower and a struct viewer.

## Coming later

- **BGM selector**, with classic French Bread music.
- **POTATO MODE** - the game renders every frame at a fixed 1280x720 and only then scales it to your
  display, so rendering at half of that is a quarter of the pixels through all five of its
  full-screen passes. Built and compiled in, held back until the resolution change has been checked
  in a real match.
- **Palettes in the lobby**, not only in the match.

## The ini file

`UNI2_IM.ini` sits next to the DLL. Missing keys take their defaults, so you can delete anything you
are not changing, and the whole file can be deleted to start over.

### `[Mod]`

| Key | Default | What it does |
|---|---|---|
| `DinputDllWrapper` | empty | Full path to another `dinput8.dll` to chain-load. Empty uses the system one. |

### `[Keybinds]`

| Key | Default | What it does |
|---|---|---|
| `ToggleOverlay` | `F1` | Opens and closes the main window. |
| `ToggleHitboxOverlay` | `F2` | Hitbox viewer. |
| `ToggleFrameMeter` | `F3` | Frame meter. |
| `FreezeFrame` | `F5` | Pause and resume. |
| `StepForward` | `F6` | One frame forward; hold to repeat. |

### `[Training]`

| Key | Default | What it does |
|---|---|---|
| `FreezeMode` | `0` | 0 tick stop, 1 hitstun stop. Tick stop freezes everything; hitstun stop keeps menus live but distorts effects. |
| `AutoPauseOnAttack` | `0` | Bit field: 1 watch P1, 2 watch P2, 4 on attacks, 8 on armoured moves. 0 is off. |
| `AutoPauseComboStops` | `3` | Hit counts to stop at, comma separated. `3,20` stops on the third hit and the twentieth. |
| `AutoPauseBlockStops` | `3` | The same, for blocked hits. |
| `ResumeDelayFrames` | `60` | Countdown before the game resumes after an auto pause. |
| `StepRepeatDelayMs` | `250` | How long the next-frame key must be held before it repeats. |
| `StepRepeatIntervalMs` | `90` | How long between repeated steps. |
| `RecordFrameCounterRva` | `0` | Advanced. RVA of the recorder's frame counter; 0 disables it. |

### `[FrameMeter]`

| Key | Default | What it does |
|---|---|---|
| `PlaceAutomatically` | `0` | Keeps the meter centred near the bottom of whatever resolution the game runs at, ignoring the position below. |
| `PositionX` / `PositionY` | `-1` | Top-left corner in pixels. `-1` means it has never been placed: the meter takes the automatic spot once, writes it here, and is draggable from there. |
| `Scale` | `1.5` | Size of the meter. |
| `BandCounts` | `1` | Print the length of every finished band inside the bar. |
| `LineTotals` | `1` | The exchange totalled - blockstun, hitstun and the gap - on its own line. |
| `AttributeRow` | `1` | The thin row under each bar naming every invincibility in force. |
| `Opacity` | `100` | How solid the meter is drawn, as a percentage. |
| `MouseDrag` | `1` | Whether a click that lands on the meter drags it. |

### `[Palette]`

| Key | Default | What it does |
|---|---|---|
| `ShowOnlinePalettes` | `1` | The other player's side, in one decision. On, their palette is read as it arrives and worn. Off, their packets are dropped and their side is left as the game gives it. Yours is sent either way. |
| `Creator` | empty | The author name written into palettes you save. The overlay fills this in as you type it. |
| `CompanionCharacters` | `15` | Characters whose companion draws before the fighters do, by the game's own numbering. Chaos is 15. Comma separated. |
| `OwnersFromDraws` | `1` | Take texture owners from the renderer's own draw calls instead of the bind-order guess. A mirror match needs this on. |
| `IdentifyByColours` | `0` | Let the colour comparison name a side. Off, and off for a measured reason: it has come out backwards every time it was tried. |
| `PaintOutOfMatch` | `0` | Paint chosen palettes outside a match too - character select portrait, lobby avatar. |
| `PaintEffectRows` | `1` | Reserved; it no longer does anything. Left so an older ini still loads. |
| `ShowLegacyTab` | `0` | Shows the first palette system's tab. Kept for its machinery only. |
| `GroupByPart` | `1` | Group palette entries the way the game's own colour screen does - hair, skin, and so on. |
| `FlashEntry` | `1` | Picking an entry darkens everything else and blinks that entry on the character. |
| `FilterJunk` | `1` | Hide the entries that are not really colours: the black padding, the green the unused slots are filled with, and anything repeating an entry above it. |

### `[Video]`

| Key | Default | What it does |
|---|---|---|
| `TimerResolution` | `1` | Hold Windows' 1 ms timer and ask again when the window regains focus. The game asks once at startup and never again, and Windows takes it back in the background - which is what an alt-tab leaves behind. |
| `PowerThrottlingOptOut` | `1` | Opt the process out of EcoQoS and of the background clamp on timer resolution. The other half of the same fix. |
| `PumpWait` | `0` | Wait on the frame thread's message instead of on the clock, and put the engine's other short sleeps on a high resolution timer. No CPU cost, no engine code patched. |
| `PumpWaitAllInput` | `0` | Wake that wait on every message rather than only on the handshake. Shortens window message latency and costs CPU in proportion to how much the mouse moves. |
| `DisplayTuning` | `1` | Let the mod choose the fullscreen display parameters below. Off leaves exactly what the game asked for. |
| `FullscreenRefreshHz` | `0` | 0 leaves the desktop's own mode alone. With the game's vsync on and a rate that is not a multiple of 60, 0 picks the highest listed multiple of 60 at or below the desktop rate. Exclusive fullscreen only. |
| `ExtraBackBuffer` | `0` | A second back buffer. Only helps in exclusive fullscreen with the game's vsync on, and costs up to a frame of input latency. Ignored windowed and with vsync off. |
| `FlatStage` | `0` | Replace the stage with a flat colour, for keying a capture. |
| `FlatStageColour` | `65280` | That colour, as `0xRRGGBB` in decimal. |

### `[Overlay]`

| Key | Default | What it does |
|---|---|---|
| `UiScale` | `1.0` | Overlay scale. 1.0 is native. |
| `Notifications` | `1` | The line that slides across the top when the mod loads. 0 silences it. |
| `BlockGameMouse` | `0` | Stop the game seeing the mouse at all, so clicking the overlay cannot disturb it. |

### `[Debug]`

| Key | Default | What it does |
|---|---|---|
| `MemoryDebug` | `0` | Loads the Memory debug window, opened with Ctrl+F1. |
| `Profiler` | `0` | Frame interval and per-section timing, shown in the Performance window's Metrics tab. |
| `MeterTrace` | `0` | The frame meter's diagnostic capture and its CSV. |

## Building

Needs **Visual Studio 2026** - the project targets platform toolset **v145**, which the 2022
install does not have and fails on with `MSB8020`. Everything else is vendored under `depends/`
(Dear ImGui, MinHook). The DirectX SDK is not needed; `d3d9.h` ships with the Windows SDK.

```
MSBuild UNI2_IM.slnx /p:Configuration=Release /p:Platform=Win32
```

`UNI2_IM.sln` is kept beside it for older tooling; both build the same project.

Output is `bin\Release\dinput8.dll`. **Win32 only** - the game is 32-bit. Add
`/p:EnableLogging=true` for a runtime log next to the DLL. The game locks the DLL while it runs, so
close it before building over an installed copy.

## Layout

```
src/Core/       DLL entry, dinput8 forwarding, settings, logging, crash dumps
src/Hooks/      Signature scanning, MinHook wrappers, RTTI vtable lookup, input hooks
src/D3D9/       Direct3DCreate9 and device vtable hooks, present tuning
src/Overlay/    ImGui setup, window registry, individual windows
src/Game/       Game memory layer, structures and resolved pointers
src/Training/   Frame stepper, frame meter, player control, input lag meter
src/Palette/    Palette identity, painting and sharing
```

## Credits

- [Under Night BR](https://discord.gg/Az7uQUU)
- [BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod) - architecture reference
- [Hantei-kun](https://github.com/Zanaylo/Hantei-kun) - HA6 / CG / PAL format ground truth
- [undernightinbirth wiki](https://github.com/Fatih120/undernightinbirth) - modding documentation
- [Dear ImGui](https://github.com/ocornut/imgui), [MinHook](https://github.com/TsudaKageyu/minhook)

### Special thanks

- Pescador Cearense
- Eon
- Listentothebirds - Rafael
- Willyofruit
- Sky Leite
- Excel
- ZateFGC
- Yorezordd (Velho fudido)
- Thiago
- Tanasinn [AZ]
- Licensed Grappler
- Anklegator

---

# Português

Mod de treino e conveniência para **UNDER NIGHT IN-BIRTH II Sys:Celes** (Steam, `uni2.exe`).

Ele carrega como um proxy de `dinput8.dll` e desenha uma interface Dear ImGui dentro do renderizador
Direct3D 9 do jogo.

## Sobre o online

**Nada neste mod foi feito para dar vantagem a ninguém no online, e nada nele dá.**

Toda ferramenta de treino capaz de alterar o que a simulação faz — avançar quadro a quadro,
congelar, dirigir um personagem na mão, os scripts de dummy — é travada nos modos offline. A trava é
o próprio tráfego ponto a ponto do jogo: enquanto ele tiver enviado um pacote a um oponente nos
últimos três segundos, essas ferramentas se recusam a rodar. O jogo usa netcode de rollback GGPO, e
qualquer coisa que toque o estado da simulação durante uma partida a dessincroniza.

O que roda online é cosmético e apenas de leitura: as paletas personalizadas, que viajam ao lado do
jogo pela Steam em vez de passar pelo netcode e não têm como afetar a partida, e as opções de
desempenho, que só mudam como o quadro chega ao seu monitor.

Se você encontrar aqui algo que dê vantagem numa partida de verdade, isso é um bug. Reporte.

## Instalando

Copie `dinput8.dll` para a pasta do `uni2.exe`:

```
<Steam>\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes\
```

**F1** no jogo abre a interface. Para desinstalar, apague `dinput8.dll`.

O `UNI2_IM.ini` é criado com os padrões na primeira vez que o mod roda, ao lado da DLL. Apague-o
para voltar ao padrão. Cada chave dele está documentada em [The ini file](#the-ini-file).

O medidor de quadros desenha com a arte de painel e a fonte do próprio jogo. O mod extrai esses
arquivos do arquivo `d` do jogo para `UNI2-IM\Assets` na primeira execução, então não há nada para
extrair na mão e nenhum dado do jogo no download. Apague a pasta e ela é refeita; se o arquivo não
puder ser lido, o medidor continua funcionando, desenhado em cores chapadas.

Para encadear outro wrapper de `dinput8.dll`, ponha o caminho completo dele em
`[Mod] DinputDllWrapper`.

## Funções

### Interface

**F1**. Tudo fica numa janela só, com três seções — Training, Custom e Config — mais janelas
separadas para o visualizador de hitbox, a legenda do medidor de quadros, o Player Control e o
Performance.

Enquanto você digita em qualquer campo de texto, o teclado pertence à interface e o jogo não recebe
nada; o mesmo vale enquanto uma tecla está sendo capturada. Uma tecla ainda pressionada quando o
campo é liberado fica mascarada até você soltá-la, para que nada saia como golpe.

### Visualizador de hitbox

**F2**. Desenha todas as caixas que a engine tem, para personagens **e** projéteis, nas cores do
próprio jogo. A decoração é filtrada pela flag `_Exist_NoHantei` da própria engine, em vez de
adivinhada, então o que você vê é o que o jogo lê.

Não existe caixa para agarrão, para o D Shield nem para proximity guard — o jogo não tem. A região
de captura de um agarrão é uma caixa de ataque comum, num quadro que carrega atributo de agarrão.

### Medidor de quadros

**F3**. Medidor com uma faixa por estado, a contagem de quadros impressa dentro de cada faixa
concluída, a troca totalizada numa linha própria e uma linha de status nomeando toda invencibilidade
em vigor naquele quadro. A vantagem de quadros é medida contra o campo de vantagem do próprio jogo,
e não inferida da animação.

Ele começa centralizado perto da base da tela e pode ser arrastado com o mouse. É desenhado direto no
back buffer e não tem nenhuma detecção de clique da interface, então um clique que caia sobre ele o
move, independentemente do que você estivesse fazendo.

### Pausa e avanço de quadro

**F5** pausa, **F6** avança um quadro. Segurar F6 repete.

Há dois modos de congelamento. O *Tick stop* suprime o quadro inteiro do jogo e é o padrão. O
*Hitstun Stop* reaproveita o hitstop da própria engine, o que mantém os menus vivos mas desenha os
efeitos errados — isso é inerente à técnica, não um defeito.

O auto pause pode parar o jogo sozinho num ataque, num golpe com armadura ou num acerto específico
do combo, e voltar depois de uma contagem regressiva.

### Player Control

Uma janela própria. Mostra o que os dois lados estão inserindo, ao vivo, como um direcional numpad e
quatro luzes de botão. Você pode apontar seu próprio controle ou teclado para qualquer um dos
personagens, segurar uma direção ou um botão no dummy, dar um toque de alguns quadros ou rodar um
script escrito para cada lado.

Ele também mede o input lag: o tempo de relógio entre um toque físico e a mudança do campo de input
daquele personagem. O teclado e cada controle são amostrados cerca de mil vezes por segundo numa
thread própria, em vez de lidos da varredura de uma vez por quadro do jogo, que quantizaria toda
resposta em 16,7 ms e não mediria nada. As duas APIs de controle que o jogo usa estão cobertas.

### Paletas

Qualquer cor em qualquer um dos personagens, aplicada ao vivo. Uma cor pertence ao personagem, e não
a um lado da tela, então ela o acompanha mesmo numa troca de lado; e o que cada personagem veste é
lembrado e recolocado automaticamente na próxima vez.

As paletas são salvas como arquivos `.pal` comuns — o formato do próprio jogo, que o Hantei-kun
também escreve — em `UNI2-IM\Palettes\<personagem>`, com nome, autor e descrição. As cores de efeito
fazem parte da paleta e são salvas e carregadas junto. A lista de paletas do personagem tem uma
entrada **Default**, que devolve as cores do próprio jogo.

No online, sua paleta é enviada ao outro jogador ao lado do jogo, pela Steam. **See the other
player's colours** decide o lado dele numa única chave: ligada, a paleta dele é lida quando chega e
vestida, então você vê o que ele escolheu; desligada, os pacotes dele são descartados e o lado dele
fica do jeito que o jogo dá. A sua é enviada de qualquer forma, e quem não tem o mod não vê nada.

**Palette Nativa** é outra coisa: o customizador de cores do próprio jogo, controlado pela interface.
Ele monta uma cor a partir das paletas de fábrica do personagem, uma por parte, então não pode ser
qualquer cor — mas o jogo a salva e **todo** oponente a vê, com mod ou sem.

### Player Card

Edita o cartão que o jogo publica para o seu oponente: as quatro camadas da placa e o título. O
título é texto livre — as três palavras da loja são apenas o estado do seletor — então qualquer
frase escrita ali é salva e chega ao oponente exatamente como foi escrita.

### Performance

Janela própria, aberta pelo Config. Ela existe porque o ritmo de quadros da engine tem um problema
específico e localizável: o jogo roda a fila de mensagens numa thread e o quadro em outra, e a cada
quadro a thread do quadro bloqueia numa mensagem que só a fila pode responder — enquanto a fila está
dormindo. O Windows também retoma a resolução de timer de 1 ms enquanto o jogo fica em segundo
plano, que é o que um alt+tab deixa para trás.

São três opções, cada uma com a sua contrapartida real escrita ao lado, e dois presets. A janela
relata o que está **de fato** em vigor, lido de volta do dispositivo, e não o que o mod pediu.

A aba **Metrics** mede: intervalo entre quadros com a sua dispersão, um histograma de um quarto de
milissegundo em torno do alvo, detecção de dois agrupamentos para o tremor que uma mediana não vê,
quanto tempo o Present bloqueia, e um resumo pronto para colar num relatório de bug.

Uma versão anterior deste mod deixou o jogo pior, e por padrão: ela acrescentava um segundo back
buffer para todo mundo e puxava monitores de alta taxa de atualização para 60 Hz em tela cheia, o
que custava um quadro de latência e não trazia nada em troca. Ele não mexe mais no display sem ser
solicitado.

### Memory debug

Desligado por padrão; coloque `[Debug] MemoryDebug = 1` e use **Ctrl+F1**. Leituras cruas e as
ferramentas de busca usadas para construir o resto do mod.

## O que vem depois

- **Seletor de BGM**, com músicas clássicas da French Bread.
- **POTATO MODE** — o jogo renderiza todo quadro num tamanho fixo de 1280x720 e só depois o escala
  para o seu display, então renderizar na metade disso é um quarto dos pixels em todas as cinco
  passagens de tela cheia. Já construído e compilado, guardado até que a mudança de resolução seja
  verificada numa partida de verdade.
- **Paletas no lobby**, e não apenas na partida.

## O arquivo ini

As chaves são as mesmas listadas na seção em inglês acima — [The ini file](#the-ini-file) — com os
mesmos padrões. Chaves ausentes assumem o padrão, então você pode apagar o que não está mudando, e o
arquivo inteiro pode ser apagado para recomeçar.

## Créditos

- [Under Night BR](https://discord.gg/Az7uQUU)
- [BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod) — referência de arquitetura
- [Hantei-kun](https://github.com/Zanaylo/Hantei-kun) — referência dos formatos HA6 / CG / PAL
- [undernightinbirth wiki](https://github.com/Fatih120/undernightinbirth) — documentação de modding
- [Dear ImGui](https://github.com/ocornut/imgui), [MinHook](https://github.com/TsudaKageyu/minhook)

### Agradecimentos especiais

- Pescador Cearense
- Eon
- Listentothebirds - Rafael
- Willyofruit
- Sky Leite
- Excel
- ZateFGC
- Yorezordd (Velho fudido)
- Thiago
- Tanasinn [AZ]
- Licensed Grappler
- Anklegator

---

# 日本語

> **注意: この節は機械翻訳です。** 正確な内容は英語版を参照してください。
> (Machine translation. Refer to the English section for the authoritative text.)

**UNDER NIGHT IN-BIRTH II Sys:Celes** (Steam, `uni2.exe`) 用のトレーニング・利便性向上 MOD です。

`dinput8.dll` のプロキシとして読み込まれ、ゲームの Direct3D 9 レンダラー内に Dear ImGui のオーバー
レイを描画します。

## オンラインについて

**この MOD には、オンラインで有利になるように作られた機能はひとつもありません。**

シミュレーションの動作を変えうるトレーニング機能 — フレーム送り、停止、キャラクターの手動操作、
ダミースクリプト — はすべてオフライン専用に固定されています。判定にはゲーム自身の P2P 通信を使い、
直近 3 秒以内に対戦相手へパケットを送っていれば、これらの機能は動作を拒否します。本作は GGPO の
ロールバックネットコードを採用しており、対戦中にシミュレーション状態へ触れるものは同期ずれを起こし
ます。

オンラインで動作するのは外見に関わる読み取り専用の機能だけです。カスタムパレットはネットコードでは
なく Steam 経由でゲームと並行して送られるため対戦に影響を与えられず、パフォーマンス設定は映像が
モニターへ届くまでの経路を変えるだけです。

実際の対戦で有利になるものを見つけた場合、それは不具合です。報告してください。

## インストール

`dinput8.dll` を `uni2.exe` と同じフォルダーにコピーします。

```
<Steam>\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes\
```

ゲーム中に **F1** でオーバーレイを開きます。アンインストールは `dinput8.dll` を削除するだけです。

`UNI2_IM.ini` は初回起動時に既定値で DLL の隣に作成されます。削除すれば既定値に戻ります。各キーの
説明は [The ini file](#the-ini-file) にあります。

## 機能

### オーバーレイ

**F1**。Training・Custom・Config の 3 セクションが 1 つのウィンドウにまとまり、ヒットボックス
表示、フレームメーターの凡例、Player Control、Performance は別ウィンドウになります。

テキスト欄に入力している間、キーボードはオーバーレイが占有し、ゲームには何も渡されません。キー
設定の取得中も同様です。欄を離れた時点で押されたままのキーは、離すまでマスクされます。

### ヒットボックス表示

**F2**。キャラクターと飛び道具の両方について、エンジンが持つすべてのボックスをゲーム自身の配色で
描画します。装飾はエンジン自身の `_Exist_NoHantei` フラグで除外しており、推測ではありません。

投げ、D シールド、近距離ガードにボックスは存在しません。投げの捕捉範囲は、投げ属性を持つフレームの
通常の攻撃ボックスです。

### フレームメーター

**F3**。状態ごとの帯、確定した帯の中に表示されるフレーム数、独立した行に出るやり取りの合計、その
フレームで有効な無敵をすべて記した状態行を表示します。フレーム有利はアニメーションからの推測では
なく、ゲーム自身の有利フレーム値と照合しています。

初期位置は画面下部の中央付近で、マウスでドラッグできます。バックバッファーへ直接描画されるため、
上をクリックすると何をしていても移動します。

### 一時停止とフレーム送り

**F5** で停止、**F6** で 1 フレーム進みます。F6 を押し続けると連続します。

停止方式は 2 つあります。既定の *Tick stop* はゲームのフレーム処理全体を止めます。*Hitstun Stop*
はエンジン自身のヒットストップを流用するためメニューは動き続けますが、エフェクトの描画が崩れます。
これは手法上の性質であり、不具合ではありません。

自動停止は、攻撃・アーマー付き技・コンボの指定ヒット数で自動的にゲームを止め、カウント後に再開でき
ます。

### Player Control

専用ウィンドウです。両サイドの入力をテンキー表記のレバーと 4 つのボタンランプでリアルタイムに表示
します。自分のパッドやキーボードをどちらのキャラクターへも割り当てられ、ダミーに方向やボタンを
押させ続けたり、数フレームだけ入力させたり、サイドごとに記述したスクリプトを実行したりできます。

入力遅延の計測も行います。物理的な入力から、そのキャラクター自身の入力フィールドが変化するまでの
実時間です。キーボードと各パッドは専用スレッドで毎秒約 1000 回サンプリングしています。ゲームの
1 フレームに 1 回のポーリングを読むと、すべての結果が 16.7 ms 単位に丸められて計測にならないため
です。ゲームが使う 2 種類のパッド API の両方に対応しています。

### パレット

どちらのキャラクターにも任意の色をリアルタイムで適用できます。色は画面のサイドではなくキャラクター
に紐づくため、サイドが入れ替わっても一緒に移動し、各キャラクターが着ている色は記憶されて次回自動的
に適用されます。

パレットは通常の `.pal` ファイルとして `UNI2-IM\Palettes\<キャラクター>` に保存されます。ゲーム
自身の形式で、Hantei-kun も同じ形式を書き出します。名前・作者・説明を保持し、エフェクトの色も
パレットの一部として一緒に保存・読み込みされます。キャラクターのパレット一覧にある **Default** を
選ぶと、ゲーム本来の色に戻ります。

オンラインでは、自分のパレットは Steam 経由でゲームと並行して相手へ送られます。**See the other
player's colours** は相手サイドの扱いを 1 つのスイッチで決めます。オンなら届いたパレットを読んで
着せるため相手が選んだ色が見え、オフなら受信パケットを破棄して相手サイドはゲーム本来のままになり
ます。自分のパレットはどちらでも送信され、MOD を入れていない相手には何も見えません。

**Palette Nativa** は別の機能で、ゲーム自身のカラーカスタマイズをオーバーレイから操作します。
パーツごとにキャラクターの既定パレットから色を組み立てるため任意の色にはできませんが、ゲームが保存
するので MOD の有無にかかわらず **すべての** 対戦相手に見えます。

### Player Card

ゲームが対戦相手へ公開するカードを編集します。4 層のプレートとプレートタイトルです。タイトルは
自由入力で、ショップの 3 単語は選択 UI の状態にすぎないため、書いた文言がそのまま保存され相手に
届きます。

### Performance

Config から開く専用ウィンドウです。エンジンのフレーム進行には特定可能な問題があります。ゲームは
メッセージポンプと描画を別スレッドで動かしており、毎フレーム、描画スレッドはポンプだけが応答できる
メッセージで待たされます。そのポンプは眠っています。さらに Windows は、ゲームがバックグラウンドに
ある間 1 ms のタイマー精度を取り上げます。これが alt+tab の後に残る症状です。

3 つの選択肢があり、それぞれに実際の代償が併記されています。プリセットは 2 つです。ウィンドウは
MOD が要求した値ではなく、デバイスから読み戻した **実際に有効な** 値を表示します。

**Metrics** タブが計測を行います。フレーム間隔とそのばらつき、目標値周辺の 0.25 ms 刻みヒスト
グラム、中央値では見えないカクつきを捉える 2 クラスター検出、Present がブロックする時間、そして
不具合報告にそのまま貼れる要約です。

以前のビルドは既定でゲームの体感を悪化させていました。全員に 2 枚目のバックバッファーを追加し、
高リフレッシュレートのモニターをフルスクリーンで 60 Hz へ落としていたためで、入力遅延を 1 フレーム
増やすだけで何の利点もありませんでした。現在は指示がない限りディスプレイ設定に触れません。

### Memory debug

既定では無効です。`[Debug] MemoryDebug = 1` を設定し **Ctrl+F1** で開きます。MOD の他の部分を
作るために使った生の表示と検索ツールです。

## 今後の予定

- **BGM セレクター**。French Bread の往年の楽曲を収録予定。
- **POTATO MODE** — 本作は毎フレームを固定の 1280x720 で描画してから画面へ拡大するため、その半分
  で描画すれば 5 つの全画面パスすべてでピクセル数が 4 分の 1 になります。実装・コンパイル済みです
  が、解像度変更を実戦で確認するまで無効にしてあります。
- **ロビーでのパレット表示**。対戦中だけでなく。

## ini ファイル

キーと既定値は上の英語版 [The ini file](#the-ini-file) と同じです。記載のないキーは既定値が使われ
るため、変更しない項目は削除して構いませんし、ファイルごと削除してやり直すこともできます。

## クレジット

- [Under Night BR](https://discord.gg/Az7uQUU)
- [BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod) — 設計の参考
- [Hantei-kun](https://github.com/Zanaylo/Hantei-kun) — HA6 / CG / PAL 形式の基準
- [undernightinbirth wiki](https://github.com/Fatih120/undernightinbirth) — MOD 制作の資料
- [Dear ImGui](https://github.com/ocornut/imgui), [MinHook](https://github.com/TsudaKageyu/minhook)

### スペシャルサンクス

- Pescador Cearense
- Eon
- Listentothebirds - Rafael
- Willyofruit
- Sky Leite
- Excel
- ZateFGC
- Yorezordd (Velho fudido)
- Thiago
- Tanasinn [AZ]
- Licensed Grappler
- Anklegator
