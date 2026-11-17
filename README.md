# 8 Ball Reis (PS2)

Jogo de sinuca 8 ball para PS2 (PS2SDK + gsKit + libpad).

- Criador: Diego Reis
- Versão: 1.9.0

## Menu inicial (animado)
- MODO HISTÓRIA (em breve), JOGAR, PLAYER DE MÚSICAS e **OPÇÕES**
- **OPÇÕES**: Som, Música (liga/desliga), Volume da música, Efeitos da mesa, Dicas na tela e **CRÉDITOS** (criador e versão do jogo)
- Cima/Baixo escolhe, X confirma
- JOGAR abre a tela **Configurar partida**, agora em **4 etapas** (barra no topo): **1 Partida** (adversário, tempo ou dificuldade, modo), **2 Mesa e bolas**, **3 Bola branca e guia** e **4 Personagens**. Cima/Baixo escolhe, Esq/Dir muda, **X ou START em "PROXIMA ETAPA"** avança, **O** volta uma etapa (na primeira, volta ao menu) e **L1/R1** trocam de etapa. A última etapa tem **INICIAR PARTIDA**. Cada etapa mostra uma prévia (mesa, bola branca e linha guia, personagens). Detalhes de cada opção:
- (opções, como funcionam:) (Cima/Baixo escolhe a linha, Esquerda/Direita muda o valor, X ou START inicia, O volta):
  - ADVERSÁRIO: BOT ou JOGADOR 2. Ao iniciar abre a tela para **digitar o nome do JOGADOR 1** e, se o adversário for o JOGADOR 2, depois o nome dele (teclado na tela: direcional move, X escolhe a letra, O apaga, START confirma, SELECT volta; até 10 caracteres; vazio = "JOGADOR 1" / "JOGADOR 2"; os nomes ficam guardados até desligar)
  - TEMPO POR JOGADA (só contra o JOGADOR 2): SEM TEMPO, 15, 20, 25 ou 30 segundos. O contador e a barra aparecem no painel de quem joga; passou o tempo, **perde a vez**
  - DIFICULDADE (só contra o BOT): FÁCIL, MÉDIO, DIFÍCIL ou PROFISSIONAL
    - Fácil: erra mais a mira e a força, pensa devagar e faz jogadas ruins de vez em quando
    - Médio: comportamento padrão
    - Difícil: mira quase perfeita e responde rápido
    - Profissional: mira praticamente perfeita, responde na hora, prefere tacadas retas e não perdoa erro
  - MODO: BOLA 8 ou SINUCA BRASILEIRA
  - MESA (17 opções): Clássica, Fênix (feltro verde e madeira preta), Oceano, Vinho, Royal, **Preta Ouro** (feltro preto com detalhes dourados), **Neon** (luzes correndo em volta, faixas de luz, estrelas e caçapas brilhando) e as novas:
    - **Fogo**: chamas na moldura, brasas subindo pelo feltro e caçapas em brasa
    - **Gelo**: feltro azul-gelo, neve caindo, brilho de cristais e flocos nos cantos
    - **Galáxia**: estrelas piscando, nebulosas passando, estrela cadente e caçapas viram buracos negros
    - **Matrix**: chuva de código verde, moldura neon verde
    - **Vegas**: lâmpadas piscando em volta da mesa, feltro vermelho com detalhes dourados
    - **Arco-íris**: faixas de cor que mudam no feltro e moldura em cores correndo
    - **Rosa** e **Turquesa**: mesas coloridas (sem animação)
    - **Noite Lua**: céu escuro com lua crescente, estrelas piscando, nuvens passando devagar, estrela cadente e caçapas com brilho azulado
    - **Faroeste**: feltro cor de areia, moldura de tábuas com pregos e corda, estrela de xerife no centro, cactos nos cantos, poeira soprando e uma bola de mato rolando pela mesa
    - As animações param se a opção **Efeitos** estiver desligada
  - COR DAS BOLAS (só na Bola 8, **16 temas**): Clássicas, Neon, Pastel, Gelo, Fogo, Floresta, Ouro, Arco-íris, Candy, Oceano, Metal, Tropical, Pôr do sol, Lavanda, Retrô e Ácido (mostra as 8 cores do tema; vale para lisas e listradas)
  - COR JOGADOR 1 / 2 (só na Sinuca Brasileira, agora **32 cores**): Azul, Vermelha, Amarela, Verde, Roxa, Laranja, Rosa, Ciano, Marrom, Cinza, Lima, Dourada, Turquesa, Bordô, Salmão, Verde escuro, Azul claro, Violeta, Rosa choque, Bege, Prata, Coral, Oliva, Lavanda, Mostarda, Menta, Magenta, Marinho, Grafite, Pêssego, Chocolate ou Azul bebê (cada jogador usa uma cor diferente; padrão: vermelha x amarela)
  - **BOLA BRANCA** (12 cores: Branca, Marfim, Vermelha, Azul, Verde, Amarela, Rosa, Ciano, Laranja, Roxa, Dourada e Prata) e **LINHA GUIA** (10 cores: Branca, Vermelha, Verde, Azul, Amarela, Rosa, Ciano, Laranja, Roxa e Arco-íris, que muda de cor sozinha). Na tela de configurar valem como cor inicial dos dois jogadores
  - Embaixo aparece a **prévia da mesa** escolhida (com as bolas na posição inicial do modo, a cor da bola branca escolhida e as animações das mesas)

## Personagens (caricaturas)
Em **Configurar partida** há duas linhas novas: **SEU PERSONAGEM** e **PERSONAGEM DO BOT** (ou **JOGADOR 1 / JOGADOR 2** no versus). São 10 caricaturas desenhadas no próprio jogo: **Pinguim**, Robô, Pirata, Ninja, Alien, Cowboy, Vampiro, Mago, Viking e Gato. Cada jogador usa um personagem diferente (padrão: Pinguim x Robô).
- Embaixo da tela de configuração aparecem os dois escolhidos; o da linha selecionada comemora
- O rosto do personagem fica no canto do painel de cada jogador durante a partida

## Animações de vitória e derrota
Quando a partida termina:
- **Quem venceu**: o personagem pula de alegria com os braços pro alto, com raios de luz girando, fogos de artifício, chuva de confete e um troféu brilhando no centro
- **Quem perdeu**: o personagem fica cabisbaixo, chorando, debaixo de uma nuvem de chuva
- Contra o BOT: se você ganha aparece **VITORIA!**; se o bot ganha aparece **DERROTA**. No versus aparece "NOME VENCEU!"
- X = revanche (mantém o placar), START = voltar ao menu

## Modos de jogo
- **Bola 8**: as regras de sempre (lisas x listradas, bola 8 por último)
- **Sinuca Brasileira**: a branca começa no meio da mesa e cada jogador tem 4 bolas lisas (sem número) da cor escolhida, como numa mesa de verdade: 2 bolas juntas no centro da cabeceira de cada jogador e 1 bola em cada tabela comprida
  - Vence quem encaçapar as 4 bolas da sua cor primeiro
  - Encaçapou uma bola sua sem errar? Joga de novo
  - **Errou a sua bola** (tocou primeiro na bola do adversário ou não tocou em nenhuma): o adversário **tira da mesa uma das PRÓPRIAS bolas** (só as da cor dele; Setas escolhem a bola, X tira) e joga em seguida. O bot tira a própria bola mais difícil
  - **Suicídio** (branca na caçapa): quem fez perde na hora
  - **Meteu a última bola com falta**: se faltava só 1 bola para o jogador e ele a encaçapa tocando primeiro na bola do adversário (ou sem tocar em nada), ele **perde mesmo assim**, ainda que o adversário tenha as 4 bolas na mesa
  - Se a última bola de um jogador sair da mesa (por tacada ou por bola tirada), a partida acaba

## Placar
- Contra o BOT e contra o JOGADOR 2 têm placares separados de vitórias no topo da partida, entre os painéis dos jogadores
- Fim de jogo: X = revanche (mantém o placar), START = voltar ao menu
- O placar zera ao desligar o jogo

## Controles
- Esquerda/Direita: girar a mira | L1/R1: **ajuste de precisão**: ao apertar a mira anda bem devagar (um toque move só um pouquinho) e, se você segurar, acelera aos poucos até um limite baixo
- X: a barra de força (lado esquerdo) sobe e desce sozinha; X de novo dispara a tacada
- O: cancelar a tacada
- QUADRADO: passar giz no taco (veja abaixo)
- L2/R2: efeito lateral na bola branca (esquerda/direita), indicador no canto inferior esquerdo
- CIMA/BAIXO (na mira): efeito vertical, **rola** (cima) ou **puxa** (baixo). O ponto vermelho do indicador mostra os dois efeitos
- TRIÂNGULO: liga/desliga o **PULO** da bola branca (veja abaixo). A palavra PULO no canto inferior esquerdo acende quando está ligado
- START: pausa o jogo (veja abaixo)
- SELECT: abre o menu de tacos (Esq/Dir escolhe, X equipa, O ou SELECT volta)
- Bola na mão: direcional move a bola branca, X confirma

## Efeito e pulo (novo na 1.6)
- **Efeito lateral (L2/R2)**: a bola branca curva no caminho e, quando bate na tabela, o efeito também muda o ângulo do rebote
- **Rola / Puxa (CIMA/BAIXO)**: depois de bater em outra bola, a branca **segue em frente** (rola) ou **volta** (puxa). Dá para parar a branca, mandá-la para frente ou trazê-la de volta. O efeito de cada taco aumenta a força do rola/puxa
- **Pulo (TRIÂNGULO)**: a branca sobe, **passa por cima das outras bolas** e cai mais adiante. Quanto mais força na barra, mais alto e mais longe ela vai. O taco com mais **Efeito** pula mais alto (o **Efeito** do taco também conta). No ar ela não cai na caçapa e não bate em ninguém; ao pousar quica de leve. Com o pulo ligado aparece uma **marca verde** mostrando onde a bola deve cair (muda com a força da barra)
- Pular e usar efeito gastam mais giz. O BOT não usa pulo nem efeito vertical
- A barra de dicas alterna sozinha entre duas páginas de teclas

## Tacos
| Taco       | Força | Efeito | Mira | Controle |
|------------|-------|--------|------|----------|
| Clássico   | 55    | 30     | 60   | 60       |
| Potência   | 100   | 20     | 45   | 35       |
| Efeito     | 50    | 100    | 50   | 55       |
| Mira Longa | 50    | 30     | 100  | 65       |
| Precisão   | 45    | 40     | 70   | 100      |
| Sniper     | 40    | 10     | 100  | 100      |
| Dragão     | 80    | 40     | 40   | 35       |
| Gelo       | 45    | 60     | 65   | 70       |
| Ouro       | 60    | 40     | 55   | 55       |
| Neon       | 55    | 55     | 55   | 55       |
| Samurai    | 75    | 30     | 50   | 45       |
| Oceano     | 50    | 70     | 45   | 55       |
| Pistoleiro | 70    | 35     | 60   | 55       |

- **Sniper**: laser vermelho que mostra a tacada inteira (até 3 rebotes e o caminho completo da bola alvo), mas bate fraco e quase não faz efeito
- **Pistoleiro** (cowboy): taco de couro e latão com brilho de bala correndo pelo taco e estrela de xerife piscando; bate forte e tem boa mira
- Alguns tacos têm animação: **Dragão** e **Ouro** (brilho pulsando), **Gelo** e **Oceano** (faíscas correndo pelo taco), **Neon** (fita e detalhe mudando de cor)
- Força: velocidade da bola na tacada
- Efeito: quanto a bola branca curva com L2/R2
- Mira: tamanho da linha de mira (no máximo cerca de 60% da mesa, e ela some aos poucos no fim)
- Controle: velocidade da barra (maior = barra mais lenta, mais fácil de acertar)

## Giz no taco
O taco começa com a ponta cheia de giz (ponta azul no taco e barrinha **GIZ** no canto inferior direito da tela).
- Cada tacada gasta pouco giz (cerca de 28 tacadas normais até acabar; mais rápido quando você usa muito efeito ou pulo). Quanto menos giz, mais a tacada **sai torta**: a mira desvia um pouco e o efeito fica mais fraco
- Com pouquíssimo giz e muito efeito lateral o taco **escorrega**: desvio dobrado e tacada mais fraca
- Aperte **QUADRADO** na hora da mira para passar giz (animação rápida); o giz volta a 100% e a tacada fica certeira de novo
- Cada jogador tem o seu giz. O BOT não gasta giz
- Quando o giz está abaixo da metade a barra de dicas mostra a tecla do giz

## Cor da bola branca e da linha guia
A **cor da bola branca** é escolhida só no menu (**Configurar partida → BOLA BRANCA**) e vale para a partida inteira: não dá para trocar durante o jogo.
A **linha guia** continua podendo ser trocada a qualquer momento: aperte **START** durante a partida e mude em **LINHA GUIA** (Esquerda/Direita ou X). Contra o BOT aparece a opção do jogador 1; no versus aparecem **GUIA J1** e **GUIA J2**, e a cor vale quando é a vez do jogador. A linha guia colorida vale para a linha de mira, a bola fantasma e o caminho da bola alvo (o laser do taco Sniper continua vermelho enquanto a guia estiver em Branca).

## Visual (novo na 1.9)
- **Tela de abertura** com logo, barra de carregamento e fade (X ou START pula)
- **Transições suaves** (fade) entre menu, partida e fim de jogo
- **Menus**: painéis com degradê, cantoneiras douradas e sombra; barra de seleção com brilho; título com sombra, brilho metálico e bolas 8 nos lados; fundo com partículas de luz e vinheta
- **Mesa**: sombra do feltro junto das tabelas (mais profundidade), brilho na madeira e caçapas com aro de couro e latão mais detalhados
- **HUD**: caixas com sombra, brilho no topo e base mais escura

## Pausa (START)
Continuar, **Som**, **Música** (liga/desliga), **Volume da música**, **Cor da linha guia**, **Player de músicas**, **Efeitos** (animações das mesas e brilho da mira), **Dicas na tela** (barra de teclas e dicas da mira), Controles e dicas (agora com rola/puxa e pulo), Reiniciar partida e Menu principal.
Obs.: os sons dos efeitos (tacadas, caçapas) ainda não existem; a opção **Som** já fica salva no menu.

## Música durante a partida
O jogo toca as suas músicas enquanto você joga (toca sozinho ao abrir, em ordem aleatória, e segue para a próxima faixa).
- Formatos: **MP3** e **WAV** (PCM 8/16 bits). Nomes de arquivo ficam em maiúsculas e sem acentos na tela
- Onde colocar: uma pasta **MUSIC** (ou MUSICA, MUSICAS, MP3) na raiz do pen drive / HD externo USB, ou uma pasta **MUSIC** ao lado do `pool8.elf`. Subpastas (até 2 níveis) também são lidas. No PCSX2 também vale a pasta `host:MUSIC`
- Se a pasta não existir, o jogo procura direto na raiz do pen drive
- O USB demora alguns segundos para aparecer: o jogo tenta sozinho por cerca de 30 segundos. Também dá para apertar TRIÂNGULO na tela do player
- START → **Player de músicas**: CIMA/BAIXO escolhe, X toca, QUADRADO pausa/continua, L1/R1 faixa anterior/próxima, ESQ/DIR volume, TRIÂNGULO procura de novo
- Ao trocar de faixa aparece o nome no topo da tela
- HD interno da PS2 e rede (SMB) ainda não são lidos

## Compilar
```
make
```
Gera `pool8.elf` (requer PS2SDK e gsKit; rode no PCSX2 ou no PS2).
O player de músicas usa audsrv, fileXio, USB (BDM) e o decodificador MP3 minimp3 (domínio público, já incluso em `minimp3.h`). Os módulos `.irx` do PS2SDK são embutidos no `.elf` pelo Makefile.
Arquivos: `main.c`, `music.c`, `music.h`, `minimp3.h`, `Makefile`.
