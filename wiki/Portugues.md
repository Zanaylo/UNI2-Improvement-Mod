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

O `UNI2_IM.ini` é criado com os padrões na primeira vez que o mod roda, na pasta `UNI2-IM` ao lado
da DLL. A partir daí ele se conserta sozinho: a cada execução, toda chave ou seção que estiver
faltando é acrescentada com o valor padrão, e nada que você editou é tocado. Uma versão nova que
adicione configurações passa a acrescentá-las no seu arquivo, e um arquivo que você reduziu a duas
linhas na mão é preenchido de volta. Apague-o para voltar ao padrão. Cada chave está documentada em
[The ini file](The-ini-file).

O medidor de quadros desenha com a arte de painel e a fonte do próprio jogo. O mod extrai esses
arquivos do arquivo `d` do jogo para `UNI2-IM\Assets` na primeira execução, então não há nada para
extrair na mão e nenhum dado do jogo no download. Apague a pasta e ela é refeita; se o arquivo não
puder ser lido, o medidor continua funcionando, desenhado em cores chapadas.

Para encadear outro wrapper de `dinput8.dll`, ponha o caminho completo dele em
`[Mod] DinputDllWrapper`.

## Linux e Steam Deck (Proton)

Copie o `dinput8.dll` para o lado do `uni2.exe` exatamente como no Windows e faça o único passo que
o Windows não precisa - avisar o Wine para carregá-lo:

1. Na sua biblioteca Steam, clique com o botão direito em **UNDER NIGHT IN-BIRTH II Sys:Celes** e
   abra **Propriedades**.
2. Em **Geral**, no campo **Opções de inicialização**, ponha esta linha exatamente assim:

```
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

3. Abra o jogo e aperte **F1**.

É só isso. Nada é renomeado e nada mais é copiado.

**Por que é necessário.** O Wine escolhe qual `dinput8.dll` carregar por uma configuração do prefixo,
não pela pasta em que o arquivo está, então sem essa linha a DLL fica ao lado do `uni2.exe` e
simplesmente nunca é carregada - é isso que o relato "o mod não faz nada no Linux" quer dizer. `n,b`
quer dizer *primeiro o nativo, depois o embutido*: a cópia do mod carrega, e o `dinput8` do próprio
Wine continua respondendo tudo o que o mod repassa para ele, que é o motivo de o controle do jogo
continuar funcionando.

O Proton 9 e mais novos já fazem isso sozinhos com o `dinput8.dll` de um mod, então neles a linha não
muda nada e pode ficar. O Proton antigo não faz, e sem ela não carrega o mod.

No Linux o mod liga sozinho o **modo de compatibilidade**: sem reescrever a taxa de atualização em
tela cheia, sem opt-out de power throttling e sem substituir o `Sleep`. Esses três são ajustes para
o agendador e o compositor do Windows; no Linux o DXVK e o kernel já fazem esse trabalho com
informação melhor. Ponha `[Compat] WineSafeMode = 0` para tê-los de volta - e para descobrir se um
deles é o que está atrapalhando, caso algo esteja.

Se não acontecer absolutamente nada, procure uma pasta `UNI2-IM` ao lado do `uni2.exe`. Se ela não
existir, a DLL nunca foi carregada, então o problema é o passo acima e não o mod. Para tirar um log
de uma máquina onde ele carrega, crie o `UNI2-IM/UNI2_IM.ini` na mão com só estas duas linhas e abra
o jogo uma vez - o mod preenche o resto do arquivo sozinho:

```ini
[Debug]
Logging = 1
```

## RivaTuner Statistics Server, MSI Afterburner e outros overlays

Dois overlays no mesmo jogo são dois motores de hook nas mesmas funções do Direct3D, e o final
costuma ser um escrevendo o salto dele por cima do do outro. O RTSS verifica se o salto dele ainda
está lá e o repõe quando não está, o que leva junto o hook do outro motor - o overlay do mod desenha
um quadro e nunca mais, ou o jogo fecha na inicialização.

O mod não escreve mais por cima de ninguém. Cada hook segue a cadeia de saltos que já está na frente
da função e se instala no fim dela, que é exatamente o que o autor do RTSS pede que terceiros façam.
Os dois overlays acabam numa cadeia só que funciona, e a ordem de carregamento deixa de importar. É
a mesma mudança que fez o overlay da Steam, que hooka as mesmas funções, conviver bem também.

Se mesmo assim algo der errado, ponha `[Debug] Logging = 1` no `UNI2_IM.ini` e rode o jogo uma vez: o
log em `UNI2-IM/Logs` diz qual hook foi e o que aconteceu com ele, e a janela Debug mostra o mesmo
estado ao vivo. Duas opções do RTSS resolvem o resto:

- **Settings / General / Injection properties, "Use Microsoft Detours API hooking".** Isso muda o
  RTSS para um modelo de hook feito para conviver com outros motores.
- **No perfil do jogo no RTSS, Application detection level, None.** Aí o RTSS deixa o jogo em paz de
  vez - e o overlay dele junto.

## Funções

## Interface

**F1**. Tudo fica numa janela só, com três seções — Training, Custom e Config — mais janelas
separadas para o visualizador de hitbox, a legenda do medidor de quadros, o Player Control e o
Performance.

Enquanto você digita em qualquer campo de texto, o teclado pertence à interface e o jogo não recebe
nada; o mesmo vale enquanto uma tecla está sendo capturada. Uma tecla ainda pressionada quando o
campo é liberado fica mascarada até você soltá-la, para que nada saia como golpe.

## Visualizador de hitbox

**F2**. Desenha todas as caixas que a engine tem, para personagens **e** projéteis, nas cores do
próprio jogo. A decoração é filtrada pela flag `_Exist_NoHantei` da própria engine, em vez de
adivinhada, então o que você vê é o que o jogo lê.

Não existe caixa para agarrão, para o D Shield nem para proximity guard — o jogo não tem. A região
de captura de um agarrão é uma caixa de ataque comum, num quadro que carrega atributo de agarrão.

## Medidor de quadros

**F3**. Medidor com uma faixa por estado, a contagem de quadros impressa dentro de cada faixa
concluída, a troca totalizada numa linha própria e uma linha de status nomeando toda invencibilidade
em vigor naquele quadro. A vantagem de quadros é medida contra o campo de vantagem do próprio jogo,
e não inferida da animação.

Ele começa centralizado perto da base da tela e pode ser arrastado com o mouse. É desenhado direto no
back buffer e não tem nenhuma detecção de clique da interface, então um clique que caia sobre ele o
move, independentemente do que você estivesse fazendo.

## Pausa e avanço de quadro

**F5** pausa, **F6** avança um quadro. Segurar F6 repete.

Há dois modos de congelamento. O *Tick stop* suprime o quadro inteiro do jogo e é o padrão. O
*Hitstun Stop* reaproveita o hitstop da própria engine, o que mantém os menus vivos mas desenha os
efeitos errados — isso é inerente à técnica, não um defeito.

O auto pause pode parar o jogo sozinho num ataque, num golpe com armadura ou num acerto específico
do combo, e voltar depois de uma contagem regressiva.

## Player Control

Uma janela própria. Mostra o que os dois lados estão inserindo, ao vivo, como um direcional numpad e
quatro luzes de botão. Você pode apontar seu próprio controle ou teclado para qualquer um dos
personagens, segurar uma direção ou um botão no dummy, dar um toque de alguns quadros ou rodar um
script escrito para cada lado.

Ele também mede o input lag: o tempo de relógio entre um toque físico e a mudança do campo de input
daquele personagem. O teclado e cada controle são amostrados cerca de mil vezes por segundo numa
thread própria, em vez de lidos da varredura de uma vez por quadro do jogo, que quantizaria toda
resposta em 16,7 ms e não mediria nada. As duas APIs de controle que o jogo usa estão cobertas.

## Lado do teclado

*Config → Keyboard.* Escolha se o teclado joga como **1P** ou **2P**: ele passa a ser um jogador
próprio, com as teclas que você já configurou. Feito para jogo local em torneio, onde os dois
jogadores dividem a mesma máquina e um deles está no teclado.

O jogo dá ao teclado e ao primeiro controle o mesmo número de jogador, então no versus local os
dois movem o mesmo personagem. Escolher um lado aqui move o **controle** para o outro; o teclado
fica exatamente como está, com as teclas que você já configurou.

Ele nunca escreve nas suas configurações de tecla, e nunca move o teclado entre os dois jogadores
de teclado do jogo — as duas coisas foram tentadas e as duas quebraram algo. Se você tem um segundo
jogador de teclado configurado nas opções do jogo, aquelas teclas respondem no lado do controle;
coloque *Keyboard Player Number* em 1 lá para desligá-las.

*Hold the side during a match* continua escrevendo as portas dos dois lados durante a partida local,
para que o lado escolhido seja o lado que você recebe. Desligue para deixar o jogo decidir.

Nada disso roda online.

## Paletas

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

## Player Card

Edita o cartão que o jogo publica para o seu oponente: as quatro camadas da placa e o título. O
título é texto livre — as três palavras da loja são apenas o estado do seletor — então qualquer
frase escrita ali é salva e chega ao oponente exatamente como foi escrita.

## Seletor de BGM

Em uma janela própria, aberta pelo item **Music** no menu do mod.

Toda tela com música passa por um único ponto de entrada no jogo, então qualquer uma pode receber
outra faixa: o tema de batalha de um personagem, a seleção de personagem, a tela de VS, um menu. O
jogo traz três temas de confronto; isto é a mesma ideia sem o limite.

**Soundpacks** instala a trilha inteira de um jogo de uma vez e aponta todas as telas para ela.
**Get OST from French-Bread games** lê a trilha de uma cópia que você já tem — aponte para a pasta
com `UNIclr.exe` ou `UNIst.exe`, `MBTL.exe`, ou `MBAA.exe` — e instala aqui com títulos das músicas
e pontos de loop. Nada é baixado, e nenhum áudio acompanha o mod. Rodar duas vezes no mesmo jogo
substitui o que foi adicionado em vez de duplicar. Export e Import levam e trazem seus packs em um
único zip, para um amigo ter o mesmo conjunto sem repetir nada disso.

**Browse** lista todas as faixas que o jogo pode tocar, com busca e filtro por origem, e Play para
iniciar uma e segurá-la — o jogo recupera a música dele quando você aperta Stop. O **Randomizer**
entrega uma faixa aleatória dessa lista toda vez que o jogo pede música, e cada faixa tem um
interruptor, então o que você desliga nunca é sorteado.

**Rules** é a versão manual: toque esta faixa neste confronto, para este personagem, ou no lugar
desta tela. As regras também podem ser exportadas e importadas, e importar soma à sua lista em vez
de substituí-la.

Sua própria música também funciona: coloque arquivos `.ogg` em uma pasta dentro de `UNI2-IM/Music`
e eles aparecem na lista junto com o resto.

## Performance

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

## POTATO MODE

A aba **POTATO MODE** da janela Performance, para uma máquina que não consegue segurar 60.

O jogo rasteriza todo quadro em cinco render targets de 1280x720 fixos e só depois escala o
resultado para o seu display, então quase todo pixel que ele paga é um desses. O POTATO MODE abaixa
esse tamanho. Metade é um quarto dos pixels nas cinco passagens; um quarto é um dezesseis avos.
**O cenário continua sendo desenhado em todos os níveis** — a imagem fica suave, não fica vazia.

| Nível | Desenha em | E também |
|---|---|---|
| Off | o que a opção Display do jogo pedir | nada |
| Balanced | 960x540 | multisampling do back buffer desligado, espera pelo handshake de quadro |
| Potato | 480p, 360p, 240p ou 144p | e Character Visual Improvements desligado |

**É um tamanho, não uma fração da janela.** 640x360 continua 640x360 com a janela em 720p ou em
1440p, então você pode deixar a janela do tamanho que quiser que o jogo continua desenhando só essa
quantidade de pixels — o Direct3D estica o resultado até preencher. A engine desenha exatamente como
sempre desenhou, então nada nela precisa saber e nada pode acabar no lugar errado; a imagem só fica
suave.

**Off é off**: o mod para de mexer no tamanho e a janela volta ao que a opção Display do próprio jogo
disser.

Só em janela e janela sem borda. O tamanho vale a partir da próxima vez que o jogo montar o display —
reinicie, ou mexa em qualquer opção de vídeo no menu dele.

*Character Visual Improvements* é uma opção do próprio jogo. Ela seleciona as técnicas de filtro do
shader de personagem, que resolvem nove consultas à paleta por pixel em vez de uma, e o que isso
compra é um borrão de um texel de origem — cerca de um pixel de tela em 720p. Desligá-la é o ganho
real mais barato numa placa fraca, e o mod a mantém desligada, porque a tela de opções do jogo
reescreve a mesma configuração.

*Multisampling do back buffer* não custa nada perder: uma textura Direct3D 9 não pode ser
multisampled, então o Antialias do jogo nunca alcança a borda de um sprite. Subir a resolução
interna acima de 100% é o único anti-aliasing que esta engine aceita, e o ini continua permitindo.

**Nada aqui alcança a simulação.** A partida é a mesma partida e ninguém no online percebe.

O jogo constrói os render targets uma vez, então mudar a resolução só vale depois de reiniciar, ou
depois de mexer em qualquer opção de vídeo no menu do próprio jogo. O resto é imediato. A aba relata
o que está **de fato** em vigor e avisa quando um pedido não pegou. Se algo parecer errado, volte o
nível para Off: o mod devolve cada byte que alterou.

## Improvements

A aba **Improvements** da mesma janela, e é o POTATO MODE ao contrário: o quadro é desenhado *maior*
que a sua janela e o Direct3D encaixa de volta, então cada borda é amostrada várias vezes.

| Nível | Desenha em |
|---|---|
| Off | o que a opção Display do jogo pedir |
| 1440p | 2560x1440 |
| 4K | 3840x2160 |

**Seja claro sobre o que isso faz e o que não faz.** O jogo rasteriza personagens e cenário em cinco
render targets de 1280x720 fixos antes de tudo isso, e isso não é tocado — um sprite não ganha
detalhe nenhum. O que melhora é tudo que é desenhado direto no back buffer: o HUD, os menus, as
bordas da composição e o próprio overlay do mod. É supersampling, não resolução interna.

Custa fill rate proporcional ao tamanho — 4K é nove vezes 720p. **A aba só aparece com o POTATO MODE
em Off**, porque os dois decidem o mesmo tamanho por lados opostos. Só em janela e janela sem borda.

## Memory debug

Desligado por padrão; coloque `[Debug] MemoryDebug = 1` e use **Ctrl+F1**. Leituras cruas e as
ferramentas de busca usadas para construir o resto do mod.

## O que vem depois

- **Paletas no lobby**, e não apenas na partida.

## O arquivo ini

As chaves são as mesmas listadas na seção em inglês acima — [The ini file](The-ini-file) — com os
mesmos padrões. Chaves ausentes assumem o padrão, então você pode apagar o que não está mudando, e o
arquivo inteiro pode ser apagado para recomeçar.

## Créditos

- [Under Night BR](https://discord.gg/Az7uQUU)
- [BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod) — referência de arquitetura
- [Hantei-kun](https://github.com/Zanaylo/Hantei-kun) — referência dos formatos HA6 / CG / PAL
- [undernightinbirth wiki](https://github.com/Fatih120/undernightinbirth) — documentação de modding
- [Dear ImGui](https://github.com/ocornut/imgui), [MinHook](https://github.com/TsudaKageyu/minhook)

## Agradecimentos especiais

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
