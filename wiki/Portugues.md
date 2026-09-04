# Português

Mod de treino e conveniência para **UNDER NIGHT IN-BIRTH II Sys:Celes** (Steam, `uni2.exe`).

Carrega como proxy de `dinput8.dll` e desenha uma interface Dear ImGui dentro do renderizador
Direct3D 9 do jogo.

## Sobre o online

**Nada neste mod foi feito para dar vantagem no online, e nada nele dá.**

Toda ferramenta de treino que pode mudar o que a simulação faz é travada nos modos offline: avanço
quadro a quadro, congelar, dirigir um personagem na mão, os scripts de dummy. A trava é o próprio
tráfego ponto a ponto do jogo. Se o jogo mandou um pacote para um oponente nos últimos três
segundos, essas ferramentas não rodam. O jogo usa rollback GGPO, e mexer no estado da simulação
durante uma partida dessincroniza.

O que roda online é cosmético e só de leitura: as paletas, que viajam ao lado do jogo pela Steam e
não alcançam a partida, e as opções de desempenho, que só mudam como o quadro chega no seu monitor.

**O seletor de patch é a exceção, e é só offline.** Um patch muda o que o jogo simula. As tabelas de
golpes e as constantes de sistema são lidas uma vez, quando o jogo abre, e ficam a sessão inteira.
Se você abriu num patch, continua nele no online, em qualquer menu. Só funciona contra quem escolheu
o mesmo patch. **Não use em ranked, nem contra quem não está nele. Vai dessincronizar.** Reinicie no
jogo instalado antes.

Achou algo aqui que dá vantagem numa partida de verdade? É bug. Reporta.

## Instalando

Extraia o zip da release na pasta do `uni2.exe`:

```
<Steam>\steamapps\common\UNDER NIGHT IN-BIRTH II Sys Celes\
```

São dois arquivos. `dinput8.dll` é o mod. `UNI2IMUpdater.exe` instala as versões seguintes; sozinho
não faz nada e o mod funciona sem ele.

**F1** no jogo abre a interface. Para desinstalar, apague os dois arquivos.

O `UNI2_IM.ini` é criado com os padrões na primeira execução, na pasta `UNI2-IM` ao lado da DLL.
Depois disso ele se conserta sozinho: a cada execução, chave ou seção faltando é acrescentada com o
padrão, e nada que você editou é tocado. Versão nova acrescenta as configurações novas no seu
arquivo. Apague para voltar ao padrão. Todas as chaves estão em [The ini file](The-ini-file).

O medidor de quadros usa a arte de painel e a fonte do próprio jogo. O mod tira esses arquivos do
arquivo `d` para `UNI2-IM\Assets` na primeira execução, então não tem nada para extrair na mão e
nenhum dado do jogo vem no download. Apague a pasta e ela é refeita. Se o arquivo não puder ser
lido, o medidor continua funcionando em cores chapadas.

Para encadear outro wrapper de `dinput8.dll`, ponha o caminho completo em `[Mod] DinputDllWrapper`.

## Linux e Steam Deck (Proton)

Copie o `dinput8.dll` para a pasta do `uni2.exe` igual no Windows. Falta um passo só:

1. Na biblioteca da Steam, clique com o direito em **UNDER NIGHT IN-BIRTH II Sys:Celes** →
   **Propriedades**.
2. Em **Geral**, em **Opções de inicialização**, cole exatamente isto:

```
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

3. Abra o jogo e aperte **F1**.

É só isso. Nada é renomeado e nada mais é copiado.

**Por que precisa.** O Wine decide qual `dinput8.dll` carregar por uma configuração do prefixo, não
pela pasta onde o arquivo está. Sem essa linha a DLL fica lá do lado do `uni2.exe` e nunca é
carregada. `n,b` quer dizer nativa primeiro, depois a interna: a cópia do mod carrega e o `dinput8`
do Wine continua respondendo o que o mod repassa, que é por isso que o controle continua funcionando.

Proton 9 pra cima já faz isso sozinho com um `dinput8.dll` de mod, então lá a linha não muda nada e
pode ficar. Proton antigo não faz, e sem ela não carrega.

No Linux o mod liga o **modo seguro de compatibilidade** sozinho: sem reescrever taxa de atualização
em fullscreen, sem desligar o throttling de energia, sem substituir o `Sleep`. Esses três são ajuste
para o agendador e o compositor do Windows; no Linux o DXVK e o kernel já fazem isso com informação
melhor. Ponha `[Compat] WineSafeMode = 0` para tê-los de volta, ou para descobrir se um deles é o
problema.

Se não acontecer nada, procure uma pasta `UNI2-IM` ao lado do `uni2.exe`. Sem a pasta, a DLL nunca
foi carregada, então é o passo acima e não o mod. Para tirar um log de uma máquina onde ela carrega,
crie o `UNI2-IM/UNI2_IM.ini` na mão com essas duas linhas e abra o jogo uma vez:

```ini
[Debug]
Logging = 1
```

## RivaTuner, MSI Afterburner e outros overlays

Dois overlays no mesmo jogo é dois motores de hook nas mesmas funções do Direct3D, e normalmente um
escreve o jump dele por cima do outro. O RTSS confere se o jump dele ainda está lá e recoloca, o que
leva junto o hook do outro motor. Aí o overlay do mod aparece por um quadro e some, ou o jogo
crasha na abertura.

O mod não escreve mais por cima de ninguém. Todo hook segue a cadeia de jumps que já está na frente
da função e se instala no fim dela, que é o que o autor do RTSS pede. Os dois overlays acabam numa
cadeia só e a ordem de carga deixa de importar. Foi a mesma mudança que fez o overlay da Steam
funcionar junto.

Se ainda der problema, ponha `[Debug] Logging = 1` e abra o jogo uma vez. O log em `UNI2-IM/Logs`
diz qual hook e o que aconteceu com ele. Duas opções do RTSS resolvem o resto:

- **Settings → General → Injection properties → "Use Microsoft Detours API hooking".**
- **No perfil do jogo no RTSS → Application detection level → None.**

## Funções

### Interface

**F1**. Uma janela com três seções: Training, Custom e Config. Visualizador de hitbox, legenda do
medidor de quadros, Player Control, Performance, Music e Stages abrem em janelas próprias.

Enquanto você digita num campo de texto o teclado é da interface e o jogo não recebe nada. Igual
enquanto uma tecla está sendo capturada. Tecla ainda segurada quando o campo solta fica mascarada
até você largar, então nunca sai como golpe.

### Visualizador de hitbox

**F2**. Desenha todas as caixas que o engine tem, de personagens e projéteis, nas cores do próprio
jogo. Decoração é filtrada pela flag `_Exist_NoHantei` do engine, não no chute.

Não existe caixa de agarrão, de D Shield nem de proximity guard. O jogo não tem. A área de agarrão é
uma caixa de ataque comum num quadro com atributo de agarrão.

### Medidor de quadros

**F3**. Uma faixa por estado, a contagem de quadros dentro de cada faixa fechada, a troca somada numa
linha própria e uma linha de status com toda invencibilidade ativa naquele quadro. A vantagem de
quadro vem do campo de vantagem do próprio jogo, não de ler a animação.

Começa perto de baixo da tela e arrasta com o mouse. É desenhado direto no back buffer e não tem o
hit testing da interface, então um clique que cair nele move ele, esteja você fazendo o que for.

### Pausa e avanço de quadro

**F5** pausa. **F6** avança um quadro, segure para repetir.

Dois modos de congelamento. *Tick stop* suprime o quadro inteiro do jogo e é o padrão. *Hitstun Stop*
reaproveita o hitstop do engine, o que mantém os menus vivos mas desenha efeitos errado. Isso vem
com a técnica, não é defeito.

O auto pause consegue parar num ataque, num golpe com armor, ou num hit específico do combo, e
voltar depois de uma contagem.

### Player Control

Janela própria. Mostra o que os dois lados estão apertando, ao vivo, como um direcional numérico e
quatro luzes de botão. Aponte seu controle ou teclado para qualquer um dos personagens, segure uma
direção ou botão no dummy, toque por alguns quadros, ou rode um script escrito por lado.

Também mede input lag: o tempo de relógio entre apertar de verdade e o campo de input daquele
personagem mudar. O teclado e todo controle são lidos umas mil vezes por segundo numa thread
própria. Ler a leitura de uma vez por quadro do jogo arredondaria toda resposta para 16,7 ms e não
mediria nada. As duas APIs de controle que o jogo usa estão cobertas.

### Lado do teclado

*Config → Keyboard.* Escolha se o teclado joga de **1P** ou **2P**. Ele vira um jogador próprio, com
as teclas que você já configurou. Feito para jogar local em torneio: dois jogadores, uma máquina, um
deles no teclado.

O jogo dá ao teclado e ao primeiro controle o mesmo número de jogador, então no versus local os dois
dirigem o mesmo personagem. Escolher um lado aqui move o **controle** para o outro. O teclado não sai
do lugar e as teclas não mudam.

Nunca escreve nas suas teclas, e nunca move o teclado entre os dois jogadores de teclado do jogo. Os
dois foram tentados e os dois quebraram alguma coisa. Se você tem um segundo jogador de teclado
configurado nas opções do jogo, aquelas teclas respondem do lado do controle; ponha *Keyboard Player
Number* em 1 lá para desligar.

*Hold the side during a match* continua escrevendo os dois slots de controle enquanto a partida local
roda, então o lado que você escolheu é o lado que você tem. Desligue para deixar o jogo decidir.

Nada aqui roda online.

### Paletas

Qualquer cor nos dois personagens, aplicada ao vivo. A cor é do personagem, não de um lado da tela,
então acompanha ele no crossover. O que cada um usa fica gravado e volta sozinho na próxima.

As paletas são arquivos `.pal` normais, o formato do próprio jogo, o mesmo que o Hantei-kun escreve.
Ficam em `UNI2-IM\Palettes\<personagem>` com nome, autor e descrição. Cores de efeito fazem parte da
paleta e vão junto. **Default** na lista devolve as cores do jogo.

No online, sua paleta vai para o outro jogador ao lado do jogo, pela Steam. **See the other player's
colours** decide o lado dele: ligado, você vê o que ele escolheu; desligado, os pacotes dele são
descartados e o lado dele fica como o jogo entrega. A sua vai dos dois jeitos. Quem não tem o mod não
vê nada.

**Palette Nativa** é outra coisa: o customizador de cores do próprio jogo, controlado pela interface.
Ele monta a cor a partir das paletas de fábrica do personagem, uma por parte, então não dá para
escolher qualquer cor. Mas o jogo salva e **todo** oponente vê, com mod ou sem.

### Vozes e sons

Troca a voz de um personagem ou um efeito sonoro. Só aquele personagem muda.

Na aba **Replace**, escolha o personagem e aperte **Load voices and sounds**. Vem tudo que ele tem:
voz de batalha, falas de história, falas de vitória, locutor, menu e seleção, e os efeitos
compartilhados que ele usa. A maioria das falas de batalha mostra o texto ao lado, lido da lista de
sons do próprio jogo.

**Get this voice from UNI...** puxa a voz de um personagem da sua cópia do jogo antigo, a pasta com
`UNIclr.exe`, `UNIst.exe` ou `UNIEL.exe`. Como os dois jogos não nomeiam os arquivos igual, o
casamento é feito pela fala em si: os dois escrevem o texto ao lado de cada entrada. Cerca de dois
terços das falas acham par. O resto fica com a gravação do UNI2. Tsurugi, Uzuki, Kaguya, Kuon, Ogre
e Izumi não existem no UNI.

Suas trocas ficam num pack, em `UNI2-IM\Sounds`. **Use Ogg Vorbis.** WAV e MP3 são convertidos uma
vez e guardados em cache. **Export** gera um zip para mandar para alguém e **Import** põe de volta.
Nada de áudio vem no mod.

### Player Card

Edita o cartão que o jogo publica para o oponente: as quatro camadas de placa e o título. O título é
texto livre. As três palavras da loja são só o estado do seletor, então qualquer frase que você
escrever é salva e chega no oponente como escrita.

### Seletor de BGM

Janela própria, aberta em **Music**. Toda tela com música passa por um ponto só no jogo, então
qualquer uma pode receber outra faixa: tema de personagem, seleção, tela de VS, menu. O jogo tem três
temas de matchup; aqui é a mesma ideia sem o limite.

**Get OST from French-Bread games** lê a trilha de uma cópia que você já tem. Aponte para a pasta com
`UNIclr.exe` ou `UNIst.exe`, `MBTL.exe`, ou `MBAA.exe`. Instala com títulos e pontos de loop. Nada é
baixado e nenhum áudio vem no mod.

**Browse** lista toda faixa que o jogo pode tocar, com busca e filtro. O **Randomizer** entrega uma
faixa aleatória toda vez que o jogo pede música, e toda faixa tem um interruptor. Cada faixa tem
**Volume** próprio, guardado em `UNI2-IM/bgm.ini`. Ele só consegue segurar a faixa: 100% é o nível em
que ela foi gravada e não tem como passar disso.

**Rules** é a versão manual: toque essa faixa nesse matchup, nesse personagem, ou no lugar dessa
tela. **Import music** aceita MP3, OGG ou WAV, ou jogue os arquivos em `UNI2-IM/Music`.

### Palcos

**Stages** na janela principal. Duas coisas: os palcos que o UNI2 esconde do próprio seletor, e
palcos trazidos de outro jogo da French-Bread que você tem.

O jogo lê a lista de palcos uma vez, na abertura, então o que você mudar aqui aparece depois de
reiniciar. O painel avisa e oferece o botão.

Os escondidos são dois: **煌朧の祭壇**, o altar, e o **palco de debug**, que não tem cenário, só a
grade.

Para trazer um palco, **Get stages from French-Bread games** e escolha a pasta com `MBTL.exe`,
`UNIclr.exe` ou `UNIst.exe`. MBTL e UNI guardam palco do mesmo jeito que o UNI2, então nada é
convertido: os arquivos são lidos da sua cópia e escritos como um palco novo. Nada é baixado e nada
que o jogo traz é substituído. Os nomes ficam em inglês e a miniatura vem do jogo de origem.

MBAACC não dá. Os cenários dele são camadas 2D num formato que não tem nada a ver com o do UNI2. A
**música** dele importa normal.

### Performance

Janela própria, aberta em Config. Existe porque o ritmo de quadro do engine tem um problema
específico: o jogo roda a bomba de mensagens numa thread e o quadro em outra, e todo quadro a thread
do quadro trava esperando uma mensagem que só a bomba responde, enquanto a bomba está dormindo. O
Windows também toma de volta a resolução de milissegundo do timer enquanto o jogo fica em segundo
plano, que é o que sobra depois de um alt-tab.

Três opções, cada uma com o custo escrito do lado, e dois presets. A janela mostra o que está **de
fato** valendo, lido do dispositivo. A aba **Metrics** mede: intervalo de quadro e dispersão,
histograma de um quarto de milissegundo, detecção de dois grupos para o tranco que a mediana não vê,
quanto o Present trava, e um resumo pronto para colar num relatório.

### POTATO MODE

Aba da janela de Performance, para máquina que não segura 60.

**O palco continua desenhando em todo nível, e nada disso muda o tamanho da imagem.**

| Nível | Desenha em | Também |
|---|---|---|
| Off | a opção Display do jogo | nada |
| Balanced | 960x540 | multisampling do back buffer desligado |
| Potato | **480p, 360p, 240p ou 144p** | e Character Visual Improvements desligado |

**O tamanho é um tamanho, não uma fração da sua janela.** 640x360 continua 640x360 com a janela em
720p ou em 1440p. O Direct3D estica o resultado. O engine desenha exatamente como sempre, então nada
nele precisa saber e nada cai no lugar errado. A imagem só fica mole.

Em fullscreen exclusivo o tamanho é arredondado para cima, porque o back buffer tem que nomear um
modo de vídeo que a placa realmente tem. A aba diz no que ele parou.

O tamanho vale na próxima vez que o jogo montar a tela: reinicie, ou mexa em qualquer opção de vídeo
no menu do jogo. O resto é imediato. **Nada aqui chega na simulação.**

### Improvements

A mesma janela, ao contrário: o quadro é desenhado **maior** que a sua janela e o Direct3D encaixa de
volta, então toda borda é amostrada várias vezes.

O jogo rasteriza personagens e palco em cinco render targets fixos de 1280x720 antes disso, e isso
não é tocado. Sprite não ganha detalhe. O que melhora é tudo desenhado direto no back buffer: HUD,
menus, as bordas da composição e a interface do mod. É supersampling, não resolução interna maior.

**Sharpening** está na mesma aba e é a metade mais útil. A moleza que você vê está no upscale, não na
arte. Isso devolve o contraste de borda. 40-60% é a faixa útil.

Só em janela e borderless.

### Memory debug

Desligado por padrão. Ponha `[Debug] MemoryDebug = 1` e aperte **Ctrl+F1**. Leituras cruas e as
ferramentas de busca com que o resto do mod foi feito.

## O arquivo ini

`UNI2_IM.ini` na pasta `UNI2-IM` ao lado da DLL. Toda chave está listada em
[The ini file](The-ini-file), em inglês.

## Créditos

- [Under Night BR](https://discord.gg/Az7uQUU)
- [BBCF-Improvement-Mod](https://github.com/libreofficecalc/BBCF-Improvement-Mod) - referência de arquitetura
- [Hantei-kun](https://github.com/Zanaylo/Hantei-kun) - formatos HA6 / CG / PAL
- [undernightinbirth wiki](https://github.com/Fatih120/undernightinbirth) - documentação de modding
- [Dear ImGui](https://github.com/ocornut/imgui), [MinHook](https://github.com/TsudaKageyu/minhook)

## Agradecimentos especiais

Pescador Cearense, Eon, Listentothebirds - Rafael, Willyofruit, Sky Leite, Excel, ZateFGC, Yorezordd
(Velho fudido), Thiago, Tanasinn [AZ], Licensed Grappler e Anklegator.
