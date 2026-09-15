# Português

Mod de treino e conveniência para **UNDER NIGHT IN-BIRTH II Sys:Celes** (Steam, `uni2.exe`).

Ele carrega como proxy de `dinput8.dll` e desenha uma interface Dear ImGui dentro do jogo
(Direct3D 9).

## Sobre o online

**Nada neste mod dá vantagem no online.**

Toda ferramenta que muda a simulação só funciona offline: avanço quadro a quadro, congelar, controlar
um personagem na mão e os scripts de dummy. Se o jogo mandou um pacote para um oponente nos últimos
três segundos, essas ferramentas não rodam. O jogo usa rollback GGPO, e mexer na simulação durante
uma partida dessincroniza.

Online só roda o que é cosmético ou só leitura: as paletas, que vão pela Steam por fora da partida,
e as opções de desempenho, que só mudam como o quadro chega no seu monitor.

**O seletor de patch é a exceção, e é só offline.** Um patch muda o que o jogo simula. O jogo lê as
tabelas de golpes e as constantes de sistema uma vez, ao abrir, e usa essas a sessão inteira. Se você
abriu num patch, continua nele no online, em qualquer menu. Só funciona contra quem escolheu o mesmo
patch. **Não use em ranked, nem contra quem não está nele. Vai dessincronizar.** Antes, reinicie no
jogo instalado.

Achou algo que dá vantagem numa partida de verdade? É bug. Reporte.

## Instalando

Extraia o zip da release na pasta do `uni2.exe`:

```
<Steam>\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes\
```

São dois arquivos. `dinput8.dll` é o mod. `UNI2IMUpdater.exe` instala as próximas versões. Sozinho
ele não faz nada, e o mod funciona sem ele.

No jogo, aperte **F1** para abrir a interface. Para desinstalar, apague os dois arquivos.

Na primeira execução o mod cria o `UNI2_IM.ini` com os padrões, na pasta `UNI2-IM` ao lado da DLL.
Depois ele se conserta sozinho: a cada execução, toda chave ou seção faltando volta com o padrão, e
nada que você editou é mexido. Uma versão nova acrescenta as configurações novas no seu arquivo.
Apague o arquivo para voltar ao padrão. Todas as chaves estão em [The ini file](The-ini-file).

O medidor de quadros usa a arte de painel e a fonte do próprio jogo. Na primeira execução o mod tira
esses arquivos do arquivo `d` para `UNI2-IM\Assets`. Não precisa extrair nada na mão, e o download
não traz nenhum dado do jogo. Se apagar a pasta, ela é refeita. Se o arquivo não puder ser lido, o
medidor funciona com cores chapadas.

Para encadear outro wrapper de `dinput8.dll`, ponha o caminho completo em `[Mod] DinputDllWrapper`.

## Linux e Steam Deck (Proton)

Copie o `dinput8.dll` para a pasta do `uni2.exe`, igual no Windows. Depois é só um passo:

1. Na biblioteca da Steam, clique com o direito em **UNDER NIGHT IN-BIRTH II Sys:Celes** →
   **Propriedades**.
2. Em **Geral**, em **Opções de inicialização**, cole exatamente isto:

```
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

3. Abra o jogo e aperte **F1**.

Pronto. Não precisa renomear nem copiar mais nada.

**Por que precisa.** O Wine escolhe qual `dinput8.dll` carregar por uma configuração do prefixo, e
não pela pasta. Sem essa linha a DLL fica ao lado do `uni2.exe` e nunca carrega. `n,b` quer dizer
nativa primeiro, depois a interna: carrega a cópia do mod, e o `dinput8` do Wine continua atendendo o
que o mod repassa. Por isso o controle continua funcionando.

No Proton 9 ou mais novo isso já acontece sozinho, então a linha não muda nada e pode ficar. Proton
antigo precisa dela para carregar.

No Linux o mod liga sozinho o **modo seguro de compatibilidade**: não reescreve a taxa de atualização
em fullscreen, não desliga o throttling de energia e não substitui o `Sleep`. Esses três são ajustes
para o Windows. No Linux o DXVK e o kernel já cuidam disso. Ponha `[Compat] WineSafeMode = 0` para
ligar de volta, ou para testar se um deles é o problema.

Se nada acontecer, procure a pasta `UNI2-IM` ao lado do `uni2.exe`. Sem a pasta, a DLL nunca
carregou, então o problema é o passo acima e não o mod. Para tirar um log numa máquina onde ela
carrega, crie o `UNI2-IM/UNI2_IM.ini` na mão com estas duas linhas e abra o jogo uma vez:

```ini
[Debug]
Logging = 1
```

## RivaTuner, MSI Afterburner e outros overlays

Dois overlays no mesmo jogo fazem hook nas mesmas funções do Direct3D, e um costuma apagar o do
outro. O overlay do mod aparece por um quadro e some, ou o jogo crasha ao abrir.

O mod agora se instala no fim da cadeia de hooks que já existe, como o autor do RTSS pede. Os dois
overlays convivem e a ordem de carga não importa mais. Foi isso que também fez o overlay da Steam
funcionar junto.

Se ainda der problema, ponha `[Debug] Logging = 1` e abra o jogo uma vez. O log em `UNI2-IM/Logs`
mostra qual hook falhou e por quê. Estas duas opções do RTSS resolvem o resto:

- **Settings → General → Injection properties → "Use Microsoft Detours API hooking".**
- **No perfil do jogo no RTSS → Application detection level → None.**

## Funções

### Interface

Aperte **F1**. Abre uma janela com três seções: Training, Custom e Config. Visualizador de hitbox,
legenda do medidor de quadros, Player Control, Performance, Music e Stages abrem em janelas próprias.

Enquanto você digita num campo de texto, o teclado fica com a interface e o jogo não recebe nada. O
mesmo vale enquanto uma tecla está sendo capturada. Uma tecla ainda segurada quando o campo solta fica
bloqueada até você soltar, então nunca vira golpe.

### Visualizador de hitbox

Aperte **F2**. Desenha todas as caixas de personagens e projéteis, nas cores do próprio jogo.
Decoração é filtrada pela flag `_Exist_NoHantei` do engine.

Não existe caixa de agarrão, de D Shield nem de proximity guard, porque o jogo não tem. O agarrão é
uma caixa de ataque comum num quadro com atributo de agarrão.

### Medidor de quadros

Aperte **F3**. Mostra uma faixa por estado, com a contagem de quadros dentro de cada faixa fechada, a
troca somada numa linha própria e uma linha de status com toda invencibilidade ativa naquele quadro.
A vantagem de quadro vem do próprio jogo.

Ele começa perto da parte de baixo da tela e dá para arrastar com o mouse. Como é desenhado direto na
tela, fora da interface, qualquer clique em cima dele move ele.

### Pausa e avanço de quadro

**F5** pausa. **F6** avança um quadro. Segure para repetir.

Há dois modos de congelar. *Tick stop* para o quadro inteiro do jogo e é o padrão. *Hitstun Stop* usa
o hitstop do engine: os menus continuam funcionando, mas os efeitos saem errados. Isso é da técnica,
não é defeito.

O auto pause pode parar num ataque, num golpe com armor ou num hit específico do combo, e voltar
depois de uma contagem.

### Player Control

Janela própria. Mostra ao vivo o que os dois lados estão apertando, com um direcional numérico e
quatro luzes de botão. Você pode controlar qualquer um dos personagens com seu controle ou teclado,
segurar uma direção ou botão no dummy, tocar inputs por alguns quadros ou rodar um script para cada
lado.

Também mede input lag: o tempo real entre apertar o botão e o input do personagem mudar. O teclado e
os controles são lidos cerca de mil vezes por segundo, então a medida não fica presa em 16,7 ms. As
duas APIs de controle que o jogo usa funcionam.

### Lado do teclado

*Config → Keyboard.* Escolha se o teclado joga de **1P** ou **2P**. Ele vira um jogador separado, com
as teclas que você já configurou. Serve para jogar local em torneio: dois jogadores numa máquina, um
deles no teclado.

O jogo dá o mesmo número de jogador ao teclado e ao primeiro controle, então no versus local os dois
controlam o mesmo personagem. Escolher um lado aqui manda o **controle** para o outro lado. O teclado
fica onde está e as teclas não mudam.

O mod nunca altera suas teclas e nunca troca o teclado entre os dois jogadores de teclado do jogo. Se
você tem um segundo jogador de teclado nas opções do jogo, essas teclas respondem do lado do
controle. Para desligar, ponha *Keyboard Player Number* em 1 lá.

*Hold the side during a match* mantém os dois slots de controle no lugar durante a partida local, então
você fica no lado que escolheu. Desligue para deixar o jogo decidir.

Nada aqui roda online.

### Paletas

Qualquer cor nos dois personagens, aplicada na hora. A cor é do personagem, não do lado da tela,
então ela acompanha o personagem no crossover. A escolha de cada um fica salva e volta sozinha na
próxima vez.

As paletas são arquivos `.pal` normais, o formato do próprio jogo, o mesmo do Hantei-kun. Ficam em
`UNI2-IM\Palettes\<personagem>` com nome, autor e descrição. As cores de efeito fazem parte da paleta
e vão junto. **Default** na lista volta às cores do jogo.

No online, sua paleta vai para o outro jogador pela Steam. **See the other player's colours** decide
o lado dele. Ligado, você vê o que ele escolheu. Desligado, você ignora a paleta dele e vê as cores
normais do jogo. A sua é enviada nos dois casos. Quem não tem o mod não vê nada.

**Palette Nativa** é outra coisa: o customizador de cores do próprio jogo, controlado pela interface.
Ele monta a cor com as paletas de fábrica do personagem, uma por parte, então não dá para escolher
qualquer cor. Em troca, o jogo salva e **todo** oponente vê, com ou sem mod.

### Vozes e sons

Troca a voz de um personagem ou um efeito sonoro. Só aquele personagem muda.

Na aba **Replace**, escolha o personagem e aperte **Load voices and sounds**. Aparece tudo dele: voz
de batalha, falas de história, falas de vitória, locutor, menu e seleção, e os efeitos compartilhados
que ele usa. A maioria das falas de batalha mostra o texto ao lado, lido da lista de sons do jogo.

**Get this voice from UNI...** pega a voz de um personagem da sua cópia do jogo antigo (a pasta com
`UNIclr.exe`, `UNIst.exe` ou `UNIEL.exe`). Os dois jogos dão nomes diferentes aos arquivos, então o
mod casa as falas pelo texto. Cerca de dois terços das falas acham par. O resto fica com a gravação do
UNI2. Tsurugi, Uzuki, Kaguya, Kuon, Ogre e Izumi não existem no UNI.

Suas trocas ficam num pack em `UNI2-IM\Sounds`. **Use Ogg Vorbis.** WAV e MP3 são convertidos uma vez
e guardados em cache. **Export** gera um zip para mandar para alguém, e **Import** carrega de volta.
O mod não traz nenhum áudio.

### Player Card

Edita o cartão que o jogo mostra para o oponente: as quatro camadas de placa e o título. O título é
texto livre. Qualquer frase que você escrever é salva e chega no oponente do jeito que você escreveu.

### Seletor de BGM

Janela própria, aberta em **Music**. Qualquer tela com música pode receber outra faixa: tema de
personagem, seleção, tela de VS, menu. O jogo tem três temas de matchup. Aqui é a mesma ideia, sem
limite.

**Get OST from French-Bread games** lê a trilha de uma cópia que você já tem. Aponte para a pasta com
`UNIclr.exe` ou `UNIst.exe`, `MBTL.exe` ou `MBAA.exe`. As faixas são instaladas com títulos e pontos
de loop. Nada é baixado e o mod não traz nenhum áudio.

**Browse** lista todas as faixas que o jogo pode tocar, com busca e filtro. O **Randomizer** toca uma
faixa aleatória toda vez que o jogo pede música, e cada faixa pode ser ligada ou desligada. Cada faixa
tem seu **Volume**, salvo em `UNI2-IM/bgm.ini`. O volume só abaixa: 100% é o nível original da
gravação e não passa disso.

**Rules** é a versão manual: toque esta faixa neste matchup, neste personagem ou no lugar desta tela.
**Import music** aceita MP3, OGG ou WAV. Você também pode jogar os arquivos em `UNI2-IM/Music`.

### Palcos

**Stages** na janela principal. Serve para duas coisas: liberar os palcos que o UNI2 esconde do
seletor e trazer palcos de outro jogo da French-Bread que você tem.

O jogo lê a lista de palcos só ao abrir, então as mudanças aparecem depois de reiniciar. O painel
avisa e mostra o botão.

Os palcos escondidos são dois: **煌朧の祭壇** (o altar) e o **palco de debug**, que não tem cenário,
só a grade.

Para trazer um palco, aperte **Get stages from French-Bread games** e escolha a pasta com `MBTL.exe`,
`UNIclr.exe` ou `UNIst.exe`. MBTL e UNI guardam palcos do mesmo jeito que o UNI2, então nada é
convertido: os arquivos são lidos da sua cópia e salvos como um palco novo. Nada é baixado e nenhum
palco do jogo é substituído. Os nomes ficam em inglês e a miniatura vem do jogo de origem.

MBAACC não funciona. Os cenários dele são camadas 2D num formato totalmente diferente. A **música**
dele importa normalmente.

### Performance

Janela própria, aberta em Config. Corrige travadas no ritmo de quadros do jogo, inclusive as que
aparecem depois de um alt-tab.

São três opções, cada uma com o custo escrito ao lado, e dois presets. A janela mostra o que está
**realmente** valendo, lido do dispositivo. A aba **Metrics** mede o intervalo de quadro e a
variação, mostra um histograma de um quarto de milissegundo, detecta travadas que a mediana esconde,
mede quanto o Present trava e gera um resumo pronto para colar num relatório.

### POTATO MODE

Aba da janela de Performance, para máquinas que não seguram 60 FPS.

**O palco continua aparecendo em todos os níveis, e o tamanho da imagem na tela não muda.**

| Nível | Desenha em | Também |
|---|---|---|
| Off | a opção Display do jogo | nada |
| Balanced | 960x540 | multisampling do back buffer desligado |
| Potato | **480p, 360p, 240p ou 144p** | e Character Visual Improvements desligado |

**O tamanho é fixo, não uma fração da sua janela.** 640x360 continua 640x360 com a janela em 720p ou
em 1440p. O Direct3D estica o resultado. O jogo desenha como sempre, então nada sai do lugar. A imagem
só fica mais borrada.

Em fullscreen exclusivo o tamanho é arredondado para cima, para bater com um modo de vídeo que a placa
tem. A aba mostra o tamanho final.

O tamanho vale na próxima vez que o jogo montar a tela: reinicie ou mude qualquer opção de vídeo no
menu do jogo. O resto vale na hora. **Nada aqui afeta a simulação.**

### Improvements

Na mesma janela, o contrário: o quadro é desenhado **maior** que a sua janela e o Direct3D reduz de
volta, o que suaviza as bordas.

Personagens e palco continuam em 1280x720, então os sprites não ganham detalhe. O que melhora é o que
é desenhado direto na tela: HUD, menus, as bordas da imagem e a interface do mod. É supersampling, não
aumento de resolução interna.

**Sharpening** fica na mesma aba e é a parte mais útil. O borrado que você vê vem do upscale, não da
arte, e o sharpening devolve o contraste das bordas. A faixa útil é 40-60%.

Só em janela e borderless.

### Memory debug

Desligado por padrão. Ponha `[Debug] MemoryDebug = 1` e aperte **Ctrl+F1**. Mostra leituras cruas
de memória e as ferramentas de busca usadas para criar o mod.

## O arquivo ini

`UNI2_IM.ini`, na pasta `UNI2-IM` ao lado da DLL. Todas as chaves estão em
[The ini file](The-ini-file), em inglês.

## Créditos

- [Under Night BR](https://discord.gg/Az7uQUU)
- [BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod): referência de arquitetura
- [Hantei-kun](https://github.com/Zanaylo/Hantei-kun): formatos HA6 / CG / PAL
- [undernightinbirth wiki](https://github.com/Fatih120/undernightinbirth): documentação de modding
- [Dear ImGui](https://github.com/ocornut/imgui), [MinHook](https://github.com/TsudaKageyu/minhook)

## Agradecimentos especiais

Pescador Cearense, Eon, Listentothebirds (Rafael), Willyofruit, Sky Leite, Excel, ZateFGC, Yorezordd
(Velho fudido), Thiago, Tanasinn [AZ], Licensed Grappler e Anklegator.
