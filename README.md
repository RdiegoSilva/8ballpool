# 8 Ball Reis (PS2)

Jogo de sinuca 8 ball para PS2 (PS2SDK + gsKit + libpad).

- Criador: Diego Reis
- Versão: 3.1.3

## Física nas bordas das caçapas (novo)
As **pontas das tabelas** em volta de cada caçapa agora **colidem com a bola**: quando ela bate na ponta, rebate no ângulo certo (antes atravessava) e toca o som de tabela
- **Visual no estilo 8 Ball Pool**: a **boca dos cantos ficou bem aberta** (ponta da tabela de 13 para 30 px do canto), as pontas são **retas, cortadas na diagonal**, o buraco do canto ficou **um pouco maior** e **sem a chapa prateada** (direto na madeira). Desenho e física usam o mesmo valor, então a bola bate exatamente onde a tabela termina
- São 12 pontas (2 por caçapa). Tacada que só roça a ponta rebate; bola bem na boca entra. O acerto de tacada bem mirada ficou praticamente igual ao de antes
- Ajustes no `main.c` (perto de `jawHit()`): `PK_TIP` (maior = canto mais aberto), `KN_R` (ponta mais gorda = mais difícil), `KN_E` (quique) e `KN_FT` (atrito de lado). `KN_ON 0` desliga a colisão. O tamanho do buraco é `PR[]`
- Com o poder **CHAMINE** a colisão das pontas fica desligada. Funções: `knuckleHit()` e `pocketKnuckles()`

## Barra de força nova: medidor em blocos (novo)
A barra de força (lado esquerdo) foi refeita do zero: não tem mais o taco vertical. Agora é um **medidor de LEDs com 20 blocos** que acendem de baixo para cima: **verde** (tacada leve) > **amarelo** > **laranja** > **vermelho** (força máxima). O bloco do topo acende por partes, então a subida é suave. Uma **linha branca com setinhas** marca a força exata, **tracinhos** mostram 25/50/75%, e o **%** aparece num mostrador embaixo, na cor da zona. O rótulo **FORCA** e a marca **IDEAL** do taco Hack continuam. Fica em `drawPowerBar()` e `pbZone()` no `main.c`.

## Configurar partida / torneio mais organizado (novo)
- **Etapa 1 (Partida)**: tirei os personagens, o "VS" e o texto "MATA-MATA" que ficavam desenhados por cima do painel (os personagens já aparecem na etapa 2). Agora só sobra uma linha de resumo embaixo do painel
- Linhas com mais folga, **divisória fina** entre elas e um respiro antes do botão verde
- **Setas < >** douradas na linha escolhida e apagadas nas outras; o valor fica em destaque
- A **dica** da opção escolhida agora fica numa **faixa fixa logo acima do rodapé**, em vez de flutuar embaixo do painel. Ganharam dica também ADVERSARIO e DIFICULDADE (Facil/Medio/Dificil)
- As prévias da mesa (etapas 3 e 4) subiram um pouco para não encostar na faixa de dica
- Vale para o versus e para o torneio (os dois usam `drawSetupScreen()`)

## Graficos mais nitidos (novo)
- **Bolas** com malha 16x8 (antes 12x6): contorno mais redondo e os numeros nao entortam ao girar
- **Circulos** (discos, aneis, brilhos) usam mais segmentos: a borda deixa de parecer quadradona
- Chave **`GFX_HQ`** no `main.c` (antes de `circ()`): 1 = visual novo, 0 = visual leve de antes. Gasta mais do PS2: confira o FPS no medidor do canto da tela e, se cair de 60, ponha 0

## Medidor de desempenho (novo)
Com `DEBUG_FPS 1`, no canto inferior esquerdo da tela aparece **FPS**, o tempo medio que a **MUSICA** gasta por quadro e o **PICO** (pior quadro) da ultima volta de 60 quadros. Serve para achar a causa do taco lento na primeira partida:
- FPS abaixo de 60 e MUSICA com varios ms (ou PICO alto): a lentidao vem da musica/USB (`musicUpdate()` no `music.c`)
- FPS abaixo de 60 e MUSICA perto de 0: a lentidao vem do desenho (mesa pesada como a Pool Azul)
- Fica em `dbgNow()`, `dbgFrameEnd()` e `dbgDraw()` no `main.c`. Vem **desligado** por padrao; para mostrar, defina `DEBUG_FPS 1`

## Tela de VS antes da partida (novo, 3.1.3)
Antes de **toda partida** aparece a tela de **VERSUS** no estilo do 8 Ball Pool: o mesmo cenario dos menus (por do sol, cidade e mesa), os **dois personagens em retratos quadrados com moldura escura** (sem a estrela de raridade, removida), **VS dourado** no meio, o **nome** embaixo de cada retrato e, embaixo do nome, o **premio em moedas** de quem ganhar. O nome da **mesa** fica no topo e uma miniatura dela na caixinha do centro.
- Aparece em **JOGAR** (contra o BOT e 2 jogadores), em **TORNEIO** (toda partida, depois do chaveamento), na **revanche** e em **Reiniciar partida**. O **MODO TREINO** nao tem adversario, entao vai direto para a mesa
- **X ou START** comeca na hora (depois de uma pausinha); se ninguem apertar, comeca sozinha em 5 segundos (barra no pe da tela)
- Fica em `vsBegin()` (chamada nos pontos de inicio de partida) e `drawVsScreen()` no `main.c`; o tempo da tela e `VS_LEN`

## Texto legivel + 60 FPS (novo)
- **Fonte nova** em todo o jogo: letras nitidas (antisserrilhadas) com **contorno escuro**, no lugar da fonte padrao do gsKit, que ficava borrada. Nos textos pequenos a letra ficou um pouco mais alta (de 0.5 para cima nao muda). Se faltar VRAM, o jogo volta sozinho para a fonte antiga
- Fica em `initFontTex()`, `textRaw()` e `legH()` no `main.c` (o atlas das letras e o `FNT4`). Minusculas saem em maiusculas
- **Menus PAUSADO, PLAYER DE MUSICAS e CONTROLES** no visual novo dos outros menus: faixa de titulo, painel, barra de selecao, chaves liga/desliga, barra de volume e rodape com os botoes do controle (o player tem rodape de 2 linhas). Nada estoura mais a caixa: nome de musica comprido e texto de dica se ajustam sozinhos
- A letra nova tem a **mesma largura** da fonte antiga e fica so um pouco mais alta nos textos pequenos, entao nenhum menu muda de tamanho (`legH()`)
- **Chaves de teste** no `main.c`, logo antes da funcao `circ()`: `USE_NEW_FONT` (0 = fonte antiga) e `FAST_DISC` (0 = discos como antes). Servem para isolar a causa de algum problema no PS2 sem desfazer o resto
- **Números das bolas** (ajustado na 3.1.3, estavam grandes demais): círculo branco com raio 0,50 da bola e dígitos proporcionais (1 dígito: ~0,48 da bola de largura), preto sobre branco. Fica em `renderSkin()`, `digitInk()` e `drawNumber()` no `main.c`
- **Bolas** com a borda mais escura (da volume, sem desenhar nada a mais) e brilho em 3 camadas; o **feltro** tem um degrade de luz mais suave (14 faixas)
- **Menos desenho por quadro** (o jogo so segura 60 FPS se o PS2 desenhar cada quadro em menos de 16,6 ms): `discN` agora usa so os segmentos que o olho ve (erro menor que 0,4 px) e discos de 1-2 px viram um quadradinho. A mesa **Pool Azul** (a padrao) usava ~8500 primitivas por quadro e agora usa ~5400 (-36%); no pior caso (Neon) caiu de ~6600 para ~6050. Nada muda no visual: so deixou de gastar triangulos que nao aparecem (ex.: os ~200 botoes do estofado da Pool Azul tinham 14 triangulos cada, com 1-2 px)

## Chapolin: corneta so 1 rodada + rever replay (novo)
- **CORNETA** paralisa a bola do adversario apenas por **1 rodada**: ela fica parada ate o fim da proxima jogada dele e depois volta ao normal (nao dura mais ate o fim da partida)
- Na tela **Configurar partida, etapa 1 (Partida)** aparece a linha **VER REPLAY ANTERIOR** — **so quando existe replay** da partida que acabou de terminar. **X** assiste (a ultima bola, em camera lenta, com a bola normal, sem aros de corneta/algemas) e **X ou START** volta as opcoes. A linha some assim que a proxima partida comeca a primeira tacada
- No replay, a mesa, as cores e os personagens sao os da partida anterior, mesmo que voce tenha mudado as opcoes

## Jogo mais rápido (3.1.1)
- A tacada sai bem mais forte (velocidade máxima da branca de 640 para 1200) e o atrito da mesa é um pouco menor, então as bolas correm muito mais e **andam mais longe** antes de parar
- Os números ficam em `MAXV`, `FRIC_K` e `FRIC_C` no começo do `main.c`: para ficar ainda mais rápido aumente `MAXV`; para as bolas pararem antes, aumente `FRIC_K` ou `FRIC_C`
- O BOT e o planejador do HACK foram ajustados para a nova física (a força que eles escolhem continua certa)

## HUD do topo no estilo 8 Ball Pool (3.1.2)
- Os retratos dos jogadores ganham **moldura dourada** com pinos nos cantos (quem joga brilha mais)
- O bloco central vira uma caixa dourada: o placar de vitórias em cima e, embaixo, uma **pilha de moedas com o prêmio** que o jogador ganha se vencer a partida (o mesmo valor que ele recebe no fim)

## Modo Treino (novo)
No menu inicial, **MODO TREINO** (primeiro cartão, no lugar do antigo Modo História) deixa você jogar **sozinho na mesa**, sem adversário, tempo ou fim de partida.
- Configura igual ao JOGAR, só que sem adversário/dificuldade/tempo/habilidades do rival: **etapa 1** escolhe o modo de jogo (Bola 8, Brasileira, Mini...), **etapa 2** o seu personagem, **etapa 3** mesa e bolas e **etapa 4** bola branca e guia. Os tacos e poderes do personagem funcionam normalmente
- A vez **nunca passa**: acertou ou errou, você joga de novo. Não existe falta nem derrota
- Bola encaçapada sai da mesa. **Branca na caçapa** = bola na mão (você põe onde quiser)
- Quando a mesa fica limpa, as bolas são arrumadas de novo ("MESA LIMPA! NOVA RODADA")
- O centro do topo mostra **quantas bolas faltam** na mesa. Não dá moedas nem mexe no placar
- Fica em `training` / `resolveTrain()` no `main.c`

## Replay da tacada inteira (novo)
O replay agora mostra a **tacada inteira**, do toque do taco na branca até todas as bolas pararem, e não só o pedaço em volta da última bola que caiu. Perto da caçapa onde a última bola cai continua em câmera lenta. Vale para o replay do menu de pausa, o do fim da partida e o **VER REPLAY ANTERIOR**. A chave `REP_WHOLE` no `main.c` (1 = tacada inteira, 0 = só o trecho em volta da bola, como antes) controla isso. Se a tacada durar mais de 10 s (600 quadros, `REPN`), mostra os últimos 10 s.

## Replay no menu de pausa (novo)
- Em **START (pausa)** aparece a opção **REPLAY**: apertando **X** você vê de novo **a última bola que caiu** (câmera lenta perto da caçapa). **X ou START** volta para o jogo
- O jogo guarda sozinho o replay da última tacada em que alguma bola caiu (`repKeep` no `main.c`); se a tacada seguinte não encaçapar nada, o replay anterior continua valendo. Antes de cair a primeira bola aparece "AINDA NAO CAIU NENHUMA BOLA"
- A opção **REPLAY das Opções** (SÓ NO FIM / TODA JOGADA / DESLIGADO) foi **removida**: não há mais escolha de modo de replay
- O replay automático ao fim da partida e o **VER REPLAY ANTERIOR** na tela de configurar continuam como antes

## Replay da vitória
Quando uma tacada termina a partida, o jogo **mostra de novo a última bola que caiu** antes da tela de fim (a bola 8 na vitória normal, ou a bola da falta/derrota): começa uns 2,5 segundos antes, passa em **câmera lenta** perto da caçapa e segue um pouco depois. Aparece a etiqueta **REPLAY** no canto da mesa e a dica **X: PULAR O REPLAY**; **X ou START** pula (vale para os dois jogadores, também quando o BOT vence).
- Cada quadro da tacada é gravado num anel de 600 quadros (10 s) em `repBuf` no `main.c`; os números ficam em `REP_PRE` (quadros antes), `REP_POST` (depois), `REP_SLOW` (velocidade perto da caçapa) e `REPN`
- Funciona em todos os modos (Bola 8, Sinuca Brasileira, torneio, 2 jogadores). Se ninguém encaçapou nada na última tacada (ex.: tempo esgotado), não há replay

## Visual dos menus (novo)
Todos os menus (inicial, Configurar partida, Nome, Opções, Créditos e Chaveamento do torneio) seguem o mesmo padrão:
- **Faixa de título** dourada no topo, com brilho que corre e enfeites nos lados
- **Rodapé com os botões** do controle (X, O, START, SELECT, direcional, L1/R1) e o que cada um faz
- **Menu inicial**: cartões com ícone para cada modo (JOGAR, TORNEIO, PLAYER DE MÚSICAS, LOJA, OPÇÕES) e um **painel de detalhes** ao lado, que mostra o que tem em cada modo (e quantas músicas foram encontradas)
- **Configurar partida / torneio**: barra de 4 etapas com bolinhas numeradas (etapa atual em dourado, concluídas em verde)
- **Opções** em grupos (**AUDIO**, **VISUAL**, **SOBRE**): chaves liga/desliga, volume em barras e uma caixa com a descrição da opção selecionada

## Menu inicial (animado)
- **MODO TREINO** (no lugar do Modo História), JOGAR, **TORNEIO**, PLAYER DE MÚSICAS, **LOJA** e **OPÇÕES**
- **OPÇÕES**: Som, Música (liga/desliga), Volume da música, Efeitos da mesa, Dicas na tela e **CRÉDITOS** (criador e versão do jogo)
- Cima/Baixo escolhe, X confirma
- JOGAR abre a tela **Configurar partida**, agora em **4 etapas** (barra no topo): **1 Partida** (adversário, tempo ou dificuldade, modo), **2 Personagens**, **3 Mesa e bolas** e **4 Bola branca e guia**. Cima/Baixo escolhe, Esq/Dir muda, **X ou START em "PROXIMA ETAPA"** avança, **O** volta uma etapa (na primeira, volta ao menu) e **L1/R1** trocam de etapa. A última etapa (Bola branca e guia) tem **INICIAR PARTIDA**. Cada etapa mostra uma prévia (personagens com o taco e a mesa de cada um, mesa, bola branca e linha guia). Detalhes de cada opção:
- (opções, como funcionam:) (Cima/Baixo escolhe a linha, Esquerda/Direita muda o valor, X ou START inicia, O volta):
  - ADVERSÁRIO: BOT ou JOGADOR 2. Ao iniciar abre a tela para **digitar o nome do JOGADOR 1** e, se o adversário for o JOGADOR 2, depois o nome dele (teclado na tela: direcional move, X escolhe a letra, O apaga, START confirma, SELECT volta; até 10 caracteres; vazio = "JOGADOR 1" / "JOGADOR 2"; os nomes ficam guardados até desligar)
  - TEMPO POR JOGADA (só contra o JOGADOR 2): SEM TEMPO, 15, 20, 25 ou 30 segundos. O contador e a barra aparecem no painel de quem joga; passou o tempo, **perde a vez**
  - DIFICULDADE (só contra o BOT): FÁCIL, MÉDIO, DIFÍCIL ou PROFISSIONAL
    - Fácil: erra mais a mira e a força, pensa devagar e faz jogadas ruins de vez em quando
    - Médio: comportamento padrão
    - Difícil: mira quase perfeita e responde rápido
    - Profissional: mira praticamente perfeita, responde na hora, prefere tacadas retas e não perdoa erro
    - **Mestre** (nova): joga como o Profissional, mas o BOT **usa tacos e poderes** e troca conforme a situação: taco de POTENCIA na quebra; em tacada difícil usa ENTERRO, PUXÃO (Vampiro) ou CHAMINE; sem tacada boa usa NOVA VIDA ou ALGEMAS; em tacada fácil usa ALGEMAS (na sua bola mais perto da caçapa), RESSUSCITAR, MALDIÇÃO ou SIRENE. Respeita a recarga de 3 tacadas. No torneio só usa poderes se a regra HABILIDADES estiver LIGADA
  - MODO: BOLA 8 ou SINUCA BRASILEIRA
  - **MESA DO PERSONAGEM** (SIM/NÃO): usa automaticamente a mesa do personagem do Jogador 1 (veja a tabela de kits abaixo). Escolher uma mesa na mão desliga essa opção
  - **HABILIDADES** (só no versus): LIGADAS ou DESLIGADAS. Contra o BOT as habilidades ficam sempre ligadas para você
  - MESA (28 opções): Clássica, Fênix (feltro verde e madeira preta), Oceano, Vinho, Royal, **Preta Ouro** (feltro preto com detalhes dourados), **Neon** (luzes correndo em volta, faixas de luz, estrelas e caçapas brilhando) e as novas:
    - **Fogo**: chamas na moldura, brasas subindo pelo feltro e caçapas em brasa
    - **Gelo**: feltro azul-gelo, neve caindo, brilho de cristais e flocos nos cantos
    - **Galáxia**: estrelas piscando, nebulosas passando, estrela cadente e caçapas viram buracos negros
    - **Matrix**: chuva de código verde, moldura neon verde
    - **Vegas**: lâmpadas piscando em volta da mesa, feltro vermelho com detalhes dourados
    - **Arco-íris**: faixas de cor que mudam no feltro e moldura em cores correndo
    - **Rosa** e **Turquesa**: mesas coloridas (sem animação)
    - **Noite Lua**: céu escuro com lua crescente, estrelas piscando, nuvens passando devagar, estrela cadente e caçapas com brilho azulado
    - **Faroeste**: feltro cor de areia, moldura de tábuas com pregos e corda, estrela de xerife no centro, cactos nos cantos, poeira soprando e uma bola de mato rolando pela mesa
    - **Halloween**: feltro roxo, moldura preta com luzes laranja e verdes piscando, aboboras de Halloween nos cantos, teias de aranha, aranha descendo da tabela, fantasma flutuando, morcegos voando, neblina, lua alaranjada e caçapas com brilho verde de poção
    - **Vampiro**: feltro vinho, espinhos góticos, velas nos cantos, lua pálida, morcegos e sangue escorrendo da tabela
    - **Natal**: feltro verde com moldura vermelha, pisca-pisca colorido, bolas de Natal nos cantos, neve, estrelas e o **trenó do Papai Noel** cruzando a mesa
    - **Hack**: grade e varredura ciano, chuva de bits ciano/magenta, pacotes de dados correndo na moldura, falhas (glitch), "HACK" piscando e mira nas caçapas
    - **Chapolin**: feltro vermelho, moldura vermelha e amarela, corações nos cantos e subindo pelo feltro, grande coração amarelo no centro, estrelas e antenas saltitantes
    - **Grinch**: feltro verde, moldura vermelha com luzes piscando em verde, vermelho e branco, carinha do Grinch nos cantos, grande coração vermelho no centro que cresce, fumaça verde subindo, neve e presentes roubados flutuando
    - **Abismo**: fundo do mar com raios de luz e ondas no feltro, bolhas subindo, peixes passando, água correndo na moldura, corais nos cantos e redemoinhos nas caçapas
    - **Pool Azul** (nova, agora é a **mesa padrão**): feltro azul com luz no centro, madeira vermelha **acolchoada com losangos e botões** e brilho de verniz, tabelas azuis com brilho claro no gume, diamantes claros, placas metálicas escuras nos cantos, caçapas pretas com **aro prateado** e linha da cabeceira bem visível (sem animação)
    - As animações param se a opção **Efeitos** estiver desligada
  - COR DAS BOLAS (só na Bola 8, **16 temas**): Clássicas, Neon, Pastel, Gelo, Fogo, Floresta, Ouro, Arco-íris, Candy, Oceano, Metal, Tropical, Pôr do sol, Lavanda, Retrô e Ácido (mostra as 8 cores do tema; vale para lisas e listradas)
  - COR JOGADOR 1 / 2 (só na Sinuca Brasileira, agora **32 cores**): Azul, Vermelha, Amarela, Verde, Roxa, Laranja, Rosa, Ciano, Marrom, Cinza, Lima, Dourada, Turquesa, Bordô, Salmão, Verde escuro, Azul claro, Violeta, Rosa choque, Bege, Prata, Coral, Oliva, Lavanda, Mostarda, Menta, Magenta, Marinho, Grafite, Pêssego, Chocolate ou Azul bebê (cada jogador usa uma cor diferente; padrão: vermelha x amarela)
  - **BOLA BRANCA** (12 cores: Branca, Marfim, Vermelha, Azul, Verde, Amarela, Rosa, Ciano, Laranja, Roxa, Dourada e Prata) e **LINHA GUIA** (10 cores: Branca, Vermelha, Verde, Azul, Amarela, Rosa, Ciano, Laranja, Roxa e Arco-íris, que muda de cor sozinha). Na tela de configurar valem como cor inicial dos dois jogadores
  - Embaixo aparece a **prévia da mesa** escolhida (com as bolas na posição inicial do modo, a cor da bola branca escolhida e as animações das mesas)

## Personagens (estilo adulto)
Em **Configurar partida** há duas linhas novas: **SEU PERSONAGEM** e **PERSONAGEM DO BOT** (ou **JOGADOR 1 / JOGADOR 2** no versus). São 18 personagens desenhados no próprio jogo, em **estilo adulto**: proporções de gente de verdade (cabeça menor, rosto com maxilar, olhos e sobrancelhas finos), cores sóbrias e sombreado. Movimento sutil: respiram, balançam a cabeça de leve e piscam de vez em quando (sem pulos nem braços balançando). O personagem escolhido ou vencedor cruza os braços; quem perdeu abaixa a cabeça. Os desenhos ficam em `drawChar()` no `main.c`. Os personagens: **Pinguim**, Robô, Pirata, Ninja, Alien, Cowboy, Vampiro, Mago, Viking, Gato, **Chapolin**, **Hacker** (capuz escuro, óculos ciano e moletom com 01) e **Papai Noel** (gorro, barba e traje vermelho), **Mestre do Tempo**, **Grinch** (pelo verde, olhos amarelos e gorro roubado do Noel) **Poseidon** (cabelo e barba azuis, coroa de coral e tridente dourado) **Coveiro** (chapéu preto, rosto pálido e pá) e **Policial** (boné azul com distintivo dourado, bigode e algemas no cinto). Cada jogador usa um personagem diferente (padrão: Pinguim x Robô).
- Embaixo da tela de configuração aparecem os dois escolhidos; o da linha selecionada comemora
- O rosto do personagem fica no canto do painel de cada jogador durante a partida

## HUD (layout novo)
O topo da tela agora é uma faixa baixa de vidro com três blocos: **retrato + nome, grupo e bolas** do jogador 1, **placar** no centro e o bloco do jogador 2 espelhado. Quem joga tem borda dourada, brilho e filete na cor do jogador (azul/vermelho); o outro fica apagado. O relógio de jogada vira um filete no pé do bloco, e a barra de sangue do taco Vampiro fica logo abaixo.

## Tela de fim de partida (refeita na 3.1.3, igual a do VS)
Quando a partida termina aparece o mesmo visual da tela de VS (cenario do por do sol, retratos quadrados com moldura, nome embaixo):
- **Quem venceu**: retrato com brilho dourado, rostinho feliz pulando, o titulo **VENCEDOR** em dourado em cima (no torneio, quem ganha a final ve **CAMPEAO**), raios de luz, fogos de artificio e confete
- **Quem perdeu**: retrato mais escuro, com o rostinho chorando e o nome apagado
- **Moedas ganhas** (`+N` com a moeda) aparecem embaixo do nome; no meio ficam o **VS** e o trofeu na caixinha escura
- **Coveiro (especial)**: se o Coveiro vence, o perdedor ganha o aviso "DESCANSE EM PAZ" (a animacao da pa e do monte de terra foi retirada)
- Contra o BOT: se voce ganha aparece **VOCE VENCEU O BOT!**; se o bot ganha, **O BOT VENCEU... TENTE DE NOVO!**. No versus aparece "PARABENS, NOME!" e o placar. Cada texto fica na sua linha (o "+N MOEDAS" nao fica mais em cima do placar)
- X = revanche (mantem o placar, passa pela tela de VS), START = voltar ao menu
- Fica em `drawOverScreen()` e `vsPortrait()` no `main.c`

## Torneio (novo na 2.0)
No menu inicial, **TORNEIO** abre uma disputa **mata-mata com 8 jogadores**: Quartas de final, Semifinal e Final.
- **Perdeu, está fora!** Se o BOT vencer a sua partida, você é eliminado e vê quem ficou com o título
- Configuração em etapas: dificuldade do BOT e modo (Bola 8 ou Sinuca Brasileira), mesa e bolas, bola branca e guia, e o seu personagem. Os 7 rivais (nomes e personagens) são sorteados
- **Regras do torneio** (etapa 1 da configuração): **HABILIDADES** (padrão DESLIGADAS) e **TEMPO POR JOGADA** (SEM TEMPO, 15, 20, 25 ou 30 s; o relógio só corre nas suas jogadas e, se acabar, você perde a vez). Também vale a dificuldade do BOT e o modo (Bola 8 ou Sinuca Brasileira)
- **Habilidades DESLIGADAS:** todos jogam com o taco CLASSICO e a troca de taco (SELECT) fica bloqueada
- **Habilidades LIGADAS:** você começa com o taco do seu personagem, pode trocar de taco (SELECT) e usar o poder (O na mira). Os rivais controlados pelo BOT jogam sem poderes
- **A mesa muda a cada partida** do torneio (sorteada, nunca a mesma da partida anterior) e aparece no chaveamento; por isso não há escolha de mesa na configuração do torneio
- O chaveamento mostra o **nome de cada personagem** (cor da raridade) embaixo do nome do jogador, o confronto da partida (ex.: PINGUIM X ROBO) e um resumo das regras ativas
- Entre as partidas aparece o **chaveamento**: X joga a próxima partida, SELECT sai do torneio. As partidas dos outros jogadores são decididas automaticamente
- No torneio não há revanche nem reiniciar partida; o placar do topo mostra a fase (QUARTAS, SEMI ou FINAL)
- Ganhou a final: tela de **CAMPEÃO** com troféu

## Mira automática no início da jogada (novo)
Quando a sua vez começa (depois da tacada do adversário, da bola na mão ou de tirar uma bola), a mira **já aponta para a bola sua mais perto que tenha caminho livre**. Se houver bola do adversário (ou outra bola) na frente de todas, a mira fica como estava. Você continua podendo girar a mira normalmente. Na **Sinuca Brasileira**, quando a bola mais perto faz par com outra bola sua colada em cima ou embaixo dela (as 2 bolas da cabeceira), a mira já começa **reta no meio das duas**.

## Sorteio (novo)
- Na linha **MESA** a última opção é **ALEATORIA**: a mesa é sorteada ao iniciar a partida
- Nas linhas de **personagem** (SEU PERSONAGEM / PERSONAGEM DO BOT / JOGADOR 1 e 2) a última opção é **ALEATORIO**: os personagens são sorteados (sempre diferentes entre si). **MESA DO PERSONAGEM = SIM** tem prioridade sobre a mesa aleatória

## Modos de jogo
- **Bola 8**: as regras de sempre (lisas x listradas, bola 8 por último)
- **Bola 8 Mini** (novo): partida rápida com só 3 lisas, 3 listradas e a bola 8 num triângulo pequeno; as regras são as da Bola 8
- **Corrida de 3** (novo): quem encaçapar **3 bolas** (sem falta) primeiro vence; mesa aberta, sem lisas/listradas; o placar aparece no painel de cada jogador. Encaçapar a 8 é derrota
- **Bola 8 Duelo** (novo): só 2 lisas, 2 listradas e a bola 8; regras da Bola 8
- **Morte Súbita** (novo): quem encaçapar **primeiro** uma bola (qualquer uma, menos a 8) **sem fazer falta** vence na hora. Falta dá bola na mão; encaçapar a 8 é derrota
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
- Esquerda/Direita: girar a mira (começa fino e acelera aos poucos enquanto você segura) | L1/R1: **ajuste de precisão**: ao apertar a mira anda bem devagar (um toque move só um pouquinho) e, se você segurar, acelera aos poucos até um limite baixo
- X: a barra de força (lado esquerdo) sobe e desce sozinha; X de novo dispara a tacada
- O: cancelar a tacada | **O (na mira): usar a habilidade do taco de personagem**
- QUADRADO: passar giz no taco (veja abaixo)
- L2/R2: efeito lateral na bola branca (esquerda/direita), indicador no canto inferior esquerdo
- CIMA/BAIXO (na mira): efeito vertical, **rola** (cima) ou **puxa** (baixo). O ponto vermelho do indicador mostra os dois efeitos
- TRIÂNGULO: liga/desliga o **PULO** da bola branca (veja abaixo). A palavra PULO no canto inferior esquerdo acende quando está ligado
- START: pausa o jogo (veja abaixo)
- SELECT: abre o menu de tacos (Esq/Dir escolhe, X equipa, O ou SELECT volta)
- Bola na mão: direcional move a bola branca, X confirma
- **Analógico** (sugestão dos jogadores): o jogo liga o modo analógico do controle sozinho (o botão ANALOG ainda liga/desliga) e o direcional continua funcionando. **Stick esquerdo**: gira a mira (quanto mais inclina, mais rápido; perto do centro o ajuste é bem fino e o movimento é suavizado), move a bola branca na bola na mão e navega nos menus. **Stick direito**: efeito (esquerda/direita = lateral, cima = rola, baixo = puxa). L1/R1 continuam sendo o ajuste de precisão

## Efeito e pulo (novo na 1.6)
- **Efeito lateral (L2/R2)**: a bola branca curva no caminho e, quando bate na tabela, o efeito também muda o ângulo do rebote
- **Rola / Puxa (CIMA/BAIXO)**: depois de bater em outra bola, a branca **segue em frente** (rola) ou **volta** (puxa). Dá para parar a branca, mandá-la para frente ou trazê-la de volta. O efeito de cada taco aumenta a força do rola/puxa
- **Pulo (TRIÂNGULO)**: a branca sobe, **passa por cima das outras bolas** e cai mais adiante. Quanto mais força na barra, mais alto e mais longe ela vai. O taco com mais **Efeito** pula mais alto (o **Efeito** do taco também conta). No ar ela não cai na caçapa e não bate em ninguém; ao pousar quica de leve. Com o pulo ligado aparece uma **marca verde** mostrando onde a bola deve cair (muda com a força da barra)
- Pular e usar efeito gastam mais giz. O BOT não usa pulo nem efeito vertical
- A barra de dicas alterna sozinha entre duas páginas de teclas

## Raridade dos personagens
Cada personagem tem uma raridade, mostrada na escolha de personagens e no catálogo de tacos (os tacos de personagem ficam agrupados por raridade):
- **ULTRA LENDÁRIO** (nova raridade, a mais alta; o nome muda de cor em arco-íris): Coveiro
- **LENDÁRIO**: Grinch e Policial
- **MÍSTICO**: Vampiro, Mago, Papai Noel e Mestre do Tempo
- **ÉPICO**: Hacker e Poseidon
- **SUPER RARO**: Viking, Ninja, Chapolin, Pirata e Gato
- **RARO**: Pinguim, Robô, Alien e Cowboy

## Kits dos personagens (taco + habilidade + mesa)
Novo combo pack **GRINCH** (personagem + taco + mesa), o único **LENDÁRIO** (categoria própria no catálogo de tacos). O poder é **sorteado antes** (puxado pela situação da mesa) e **aparece na tela** ("O: PUXÃO VERDE" ou "O: BAGUNÇA"), então você decide se usa (O na mira). **Se tacar sem usar, o próximo poder é sorteado de novo** na jogada seguinte. Recarrega em 3 tacadas:
- **Puxão verde**: depois da batida, a bola alvo é puxada para a caçapa mais próxima (mais provável com poucas bolas na mesa)
- **Bagunça**: as bolas do adversário levam um empurrão em direções aleatórias e ficam espalhadas (mais provável quando o adversário está perto de ganhar ou tem muitas bolas)

Cada personagem tem o seu taco especial e a sua mesa (base para o modo história). Todos os personagens têm taco e habilidade própria.

| Personagem | Taco | Habilidade | Mesa |
|------------|------|------------|------|
| Pinguim    | Gelo       | **CONGELAR**: o gelo cobre a mesa e as bolas deslizam muito mais longe (quase sem atrito) | Gelo |
| Cowboy     | Pistoleiro | **TIRO**: a branca sai como uma bala (+50% de velocidade, sem efeito) com clarão de disparo | Faroeste |
| Vampiro    | Vampiro    | **SUGAR**: depois da batida, a bola alvo é puxada para a caçapa mais próxima | Vampiro |
| Papai Noel | Noel       | **CHAMINÉ**: todas as caçapas ficam **45% maiores** nesta tacada, com aro dourado (a branca também pode cair: cuidado com o suicídio) | Natal |
| Chapolin   | Marreta Biônica | **MARRETA**: a bola alvo leva uma marretada do chipote e sai com +70% de velocidade | Chapolin |
| Hacker     | Hack       | **HACK**: calcula a melhor tacada (confere na simulação da mesa, igual à tacada real; se nenhuma confirma, ajusta fino ângulo e força e testa de novo), trava mira, efeito e pulo, mostra o caminho todo e **X atira direto na força ideal exata** (não precisa acertar a barra) | Hack |
| Poseidon   | Tridente   | **MARÉ ALTA**: uma onda gigante empurra **todas as bolas** (menos a branca) na direção da caçapa mais próxima de cada uma, com força fraca: só as bolas que já estão perto de uma caçapa chegam lá | Abismo |
| Alien      | Ovni       | **RASTREIO**: a branca curva sozinha e **persegue a bola certa** mais perto da sua mira (a sua bola; mesa aberta: qualquer uma, menos a 8), até a primeira batida | Galáxia |
| Robô       | Robô       | **ÍMÃ**: as **suas bolas** são puxadas para a caçapa mais próxima de cada uma, com força fraca (mesa aberta: todas, menos a 8) | Neon |
| Mago       | Varinha    | **MALDIÇÃO**: amaldiçoa o adversário: na **próxima tacada dele** os botões de mirar ficam **invertidos** (direcional, stick e L1/R1) e a **força falha** às vezes (35% de chance: marcou 100, sai 10). O bot amaldiçoado só erra mais a mira. O painel do amaldiçoado ganha uma aura roxa e aparece "AMALDIÇOADO!" | Noite Lua |
| Gato       | Gato       | **NOVA VIDA** (agora junta o PRESENTE): você **não perde a vez** (se não encaçapar, **joga de novo**) e a **falta e o suicídio são perdoados** (branca na caçapa volta para a mesa, e na bola 8 antes da hora ela também volta). Se encaçapar uma bola sua sem falta, a habilidade **não gasta** | Rosa |
| Grinch     | Grinch     | **GRINCH**: ao apertar O na mira, a habilidade é **sorteada** entre Puxão verde e Bagunça conforme a situação da mesa (veja abaixo da tabela) | Grinch |
| Viking     | Dragão     | **FOGO**: a bola alvo explode e empurra as bolas que estão perto | Fogo |
| Ninja      | Samurai    | **CORTE**: +20% de força e a branca para na hora ao bater na bola | Preta Ouro |
| Pirata     | Oceano     | **MARÉ**: a branca faz uma curva forte (efeito x2,5) sem precisar de L2/R2 | Oceano |

- **Usar a habilidade: O na mira.** Na maioria, o primeiro toque arma a habilidade (aparece "ARMADO!" no canto) e vale para a próxima tacada; tocar de novo cancela. O **HACK** age na hora
  - **HACK (melhorado)**: só mostra tacadas que **realmente encaçapam** (cada uma é testada na física do jogo: bola certa primeiro, branca não cai). Funciona na Bola 8 e na Sinuca Brasileira. **O de novo** troca para a próxima melhor opção (até 4) e, depois da última, libera a mira. Com o HACK ligado a mira, o efeito, o pulo e a troca de taco ficam travados, a tacada não sofre desvio de giz, a barra de força anda mais devagar e **gruda na força ideal** (±6%). Quando há poucas tacadas diretas, ele também procura **tacadas de tabela** (1 rebote na tacada). Depois da batida, a bola alvo ainda **curva para a caçapa** (tacada certeira). **Se você ligar e desligar o HACK sem atirar, a recarga é devolvida.** Sem tacada possível, ele sugere uma jogada de segurança (acerta a bola certa sem arriscar a branca). Na quebra mira reto no triângulo com força total
  - **CHAMINÉ**: vale para a Bola 8 e para a Sinuca Brasileira; o efeito dura só a tacada e depois a recarga é de 3 tacadas
- Depois de usar, a habilidade **recarrega em 3 tacadas** (o contador aparece ao lado do nome do taco)
- O BOT não usa habilidades. No torneio só você usa habilidades, e só se a regra HABILIDADES estiver LIGADA (padrão: DESLIGADAS, taco Clássico)
- No **versus** você escolhe na configuração se a partida é **com ou sem habilidades** (HABILIDADES: LIGADAS/DESLIGADAS). Desligadas, os tacos valem só pelos atributos

## Catálogo de tacos (SELECT na mira)
Tela no estilo do 8 Ball Pool: **abas** no topo (**PADRÃO**, **PAÍSES**, **RARO**, **ÉPICO**, **LENDÁRIO**, **ULTRA** (ultra lendários: Coveiro e Chapolin)), com um **cartão por taco**: nome, raridade, o taco comprido desenhado, barras de **FORÇA, MIRA, GIRO e TEMPO** (10 gomos) e o botão **USANDO** (taco equipado) ou **EQUIPAR**. Embaixo aparece a **habilidade** do taco selecionado (descrição, recarga, caricatura do dono e mesa do personagem). **Cima/Baixo** escolhe o taco, **Esq/Dir** ou **L1/R1** troca de aba, **X** equipa, **O** ou **SELECT** volta. Em `drawCueMenu()` no `main.c` (abas em `CATT_S` / `CATT_N`).

## Tacos de países
Categoria própria no catálogo: tacos com as cores da bandeira (faixas que giram e brilhos), só de atributos, sem habilidade.

| Taco      | Força | Efeito | Mira | Controle |
|-----------|-------|--------|------|----------|
| Brasileiro | 60   | 50     | 60   | 60       |
| Mexicano  | 58    | 52     | 58   | 57       |
| Argentino | 56    | 56     | 58   | 58       |
| Americano | 70    | 38     | 50   | 50       |
| Italiano  | 55    | 60     | 55   | 55       |
| Japonês   | 48    | 45     | 67   | 72       |

## Tacos
| Taco       | Força | Efeito | Mira | Controle |
|------------|-------|--------|------|----------|
| Clássico   | 55    | 30     | 60   | 60       |
| Potência   | 100   | 20     | 45   | 35       |
| Efeito     | 50    | 100    | 50   | 55       |
| Mira Longa | 50    | 30     | 100  | 65       |
| Precisão   | 45    | 40     | 70   | 100      |
| Sniper     | 40    | 10     | 100  | 100      |
| Dragão     | 90    | 50     | 60   | 60       |
| Gelo       | 45    | 60     | 65   | 70       |
| Ouro       | 60    | 40     | 55   | 55       |
| Neon       | 55    | 55     | 55   | 55       |
| Samurai    | 85    | 45     | 65   | 65       |
| Oceano     | 65    | 80     | 60   | 60       |
| Pistoleiro | 70    | 35     | 60   | 55       |
| Brasileiro | 60    | 50     | 60   | 60       |
| Vampiro    | 90    | 80     | 80   | 80       |
| Noel       | 80    | 85     | 80   | 85       |
| Grinch     | 90    | 95     | 85   | 90       |
| Hack       | 60    | 60     | 90   | 90       |
| Tridente   | 70    | 85     | 70   | 75       |
| Ovni       | 52    | 55     | 60   | 58       |
| Robô       | 62    | 35     | 65   | 65       |
| Varinha    | 75    | 95     | 80   | 80       |
| Gato       | 70    | 80     | 70   | 75       |

### Atributos por raridade (ajustados na 3.1.3)
Os tacos de personagem ficam mais fortes conforme a raridade do dono (soma dos 4 atributos, maximo 400). Cada taco continua com a sua especialidade (ex.: Dragao e Samurai batem mais forte, Oceano e Varinha fazem mais efeito).

| Raridade        | Soma  | Tacos (Força / Efeito / Mira / Controle) |
|-----------------|-------|------------------------------------------|
| RARO            | ~225  | Gelo, Pistoleiro, Ovni, Robô (sem mudança) |
| SUPER RARO      | ~260  | Dragão 90/50/60/60, Samurai 85/45/65/65, Oceano 65/80/60/60 |
| ÉPICO           | ~300  | Hack 60/60/90/90, Gato 70/80/70/75, Tridente 70/85/70/75 |
| MÍSTICO         | ~330  | Vampiro 90/80/80/80, Noel 80/85/80/85, Varinha 75/95/80/80, Mestre do Tempo 80/80/85/85 |
| LENDÁRIO        | ~360  | Grinch 90/95/85/90, Cassetete (Policial) 95/80/90/95 |
| ULTRA LENDÁRIO  | ~385  | Coveiro 100/90/100/100, Marreta Biônica 100/85/95/100 |

Os tacos PADRÃO e de PAÍSES não mudaram. Os valores ficam na tabela `CUES` do `main.c`.

- **Sniper**: laser vermelho que mostra a tacada inteira (até 3 rebotes e o caminho completo da bola alvo), mas bate fraco e quase não faz efeito
- **Pistoleiro** (cowboy): taco de couro e latão com brilho de bala correndo pelo taco e estrela de xerife piscando; bate forte e tem boa mira
- **Tridente** (Poseidon): azul profundo com ondas turquesa correndo pelo taco, ponta brilhante e detalhe dourado
- **Ovni** (Alien): verde com luzes de disco voador correndo pelo taco; **Robô**: aço com luzinhas de LED piscando; **Varinha** (Mago): roxo com estrelinhas douradas; **Gato**: rosa com bolinhas pulando e ponta pulsando
- **Vampiro**: ponta vermelha pulsando e gotas de sangue; **Noel**: listras de bengala doce e brilhos de neve; **Hack**: pacotes de dados ciano e magenta correndo pelo taco, com falhas
- **Brasileiro**: verde, amarelo e azul da bandeira, com faixas que mudam de cor e estrelinhas correndo pelo taco; equilibrado, com um pouco mais de efeito
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
Continuar, **REPLAY**, **Som**, **Música** (liga/desliga), **Volume da música**, **Cor da linha guia**, **Player de músicas**, **Efeitos** (animações das mesas e brilho da mira), **Dicas na tela** (barra de teclas e dicas da mira), Controles e dicas (agora com rola/puxa e pulo), Reiniciar partida e Menu principal.
Obs.: a opção **Som** liga/desliga os efeitos sonoros (veja abaixo); a opção **Música** e o **Volume** só afetam as músicas.

## Efeitos sonoros (novo)
Os sons são **gerados pelo próprio jogo** (não precisa de nenhum arquivo): tacada, bola contra bola, tabela, caçapa, falta, giz, habilidade, sons de menu (mover, confirmar, voltar) e jingles de **vitória** e **derrota**. A força do som depende da velocidade da bola. Eles são misturados com a música por um mixer em `music.c` e a opção **Som** liga/desliga todos. O volume da música não altera o volume dos efeitos.

## Música durante a partida
O jogo toca as suas músicas enquanto você joga (toca sozinho ao abrir, em ordem aleatória, e segue para a próxima faixa).
- Formatos: **MP3** e **WAV** (PCM 8/16 bits). Nomes de arquivo ficam em maiúsculas e sem acentos na tela
- Onde colocar: uma pasta **MUSIC** (ou MUSICA, MUSICAS, MP3) na raiz do pen drive / HD externo USB, ou uma pasta **MUSIC** ao lado do `pool8.elf`. Subpastas (até 2 níveis) também são lidas. No PCSX2 também vale a pasta `host:MUSIC`
- Se a pasta não existir, o jogo procura direto na raiz do pen drive
- O USB demora alguns segundos para aparecer: o jogo tenta sozinho por cerca de 30 segundos. Também dá para apertar TRIÂNGULO na tela do player
- START → **Player de músicas**: CIMA/BAIXO escolhe, X toca, QUADRADO pausa/continua, L1/R1 faixa anterior/próxima, ESQ/DIR volume, TRIÂNGULO procura de novo
- Ao trocar de faixa aparece o nome no topo da tela
- HD interno da PS2 e rede (SMB) ainda não são lidos

## Loja (novo na 3.0)
No menu inicial, **LOJA** gasta as **moedas** ganhas nas partidas em itens de visual (nenhum dá vantagem no jogo). Esq/Dir ou L1/R1 trocam de aba, Cima/Baixo escolhe o item, **X** compra e equipa (ou só equipa, se já comprou), **O** volta. Cada item tem uma prévia animada ao lado.
- **Moedas**: contra o BOT vence = 25 + 15 por nível de dificuldade (derrota = 8); torneio: 60 por vitória, **200 pelo título** (derrota = 10); 2 jogadores: 15 para a partida. O ganho aparece na tela de fim de jogo (`shopAward` no `main.c`)
- **GIZ** (16): cor do cubo e da poeira ao passar o giz: Azul (padrão), Verde, Rosa, Dourado, Roxo Mágico (com faíscas) e Arco-íris (muda de cor) e **10 novos**: Vermelho, Laranja, Ciano, Verde Ácido, Obsidiana, Prata, Vinho, Neve e os animados **Fogo** (vermelho, laranja e amarelo) e **Galáxia** (azul, roxo e rosa)
- **BOLAS** (5): Clássicas, **Caveira**, **Planetas**, **Frutas** e **Brilho no escuro**. Não valem na Sinuca Brasileira (bolas lisas)
- **RASTRO** (5): traço colorido atrás das bolas que rolam: Fogo, Gelo, Neon Verde e Arco-íris
- **CAÇAPA** (11): efeito quando uma bola cai: **Confete**, **Fogo**, **Raio** e os novos **Estrelas** (chuva de estrelas douradas), **Corações** (sobem da caçapa), **Gelo** (cacos voando), **Moedas** (pulam para fora), **Bolhas**, **Fogos** (fogos de artifício coloridos) e **Magia** (faíscas girando). Ficam em `pfxBurst()`, `pfxUpdate()` e `pfxDraw()` no `main.c`
- **TÍTULOS** (24): apelido sob o nome do jogador 1 no placar. Os 5 clássicos (Novato, Tubarão, Mestre das Caçapas, **Rei da Sinuca**, Lenda) e **18 títulos de personagens**, com a frase de cada um, mais caros conforme a raridade (100 a 900 moedas): Pinguim **NOOT NOOT**, Robô **BIP BOP**, Alien **VIM EM PAZ**, Cowboy **YEEHAW**, Viking **VALHALLA**, Ninja **SILENCIO**, Chapolin **NAO CONTAVAM**, Pirata **ARRR MATEY**, Gato **MIAU**, Hacker **ACESSO TOTAL**, Poseidon **FURIA DO MAR**, Vampiro **NOITE ETERNA**, Mago **ABRACADABRA**, Papai Noel **HO HO HO**, Mestre do Tempo **HORA CERTA**, Grinch **ODEIO NATAL**, Policial **ESTA PRESO** e Coveiro **SETE PALMOS**. A lista da loja **rola** (setas ▲▼ no painel) e os itens novos ficam no fim, então o save antigo continua valendo
- **MOLDURAS** (15): borda no painel do jogador 1: Dourada, Neon Azul, Fogo, Gelo e Arco-íris e as novas **Rosa Neon**, **Verde Neon**, **Roxa** (faíscas correndo), **Natal** (pisca-pisca), **Corações**, **Estrelas**, **Elétrica** (raios na borda), **Sangue** (pingos escorrendo) e **Prata** (brilho correndo). Ficam em `shopFrameDraw()` no `main.c`
- **Save**: moedas, itens comprados e equipados ficam no **memory card 1** (pasta `8BALLREIS`, arquivo `loja.sav`). Sem cartão a loja funciona, mas o progresso não é salvo
- **Save sem biblioteca extra**: usa funções POSIX em `mc0:`, então o Makefile não precisa de `-lmc`. Para desligar o save, defina `USE_MEMCARD 0` no `main.c`
- Os preços e itens ficam nas tabelas `SH_IT` / `SH_N` no `main.c`

## Compilar
```
make
```
Gera `pool8.elf` (requer PS2SDK e gsKit; rode no PCSX2 ou no PS2).
O player de músicas usa audsrv, fileXio, USB (BDM) e o decodificador MP3 minimp3 (domínio público, já incluso em `minimp3.h`). Os módulos `.irx` do PS2SDK são embutidos no `.elf` pelo Makefile.
Arquivos: `main.c`, `music.c`, `music.h`, `minimp3.h`, `Makefile`.

## Coveiro (ultra lendário) e mesa Cemitério

Combo **COVEIRO** (personagem + taco + mesa **Cemitério**: grade de ferro, lápides, névoa e fogo-fátuo). Diferente do Grinch, **você escolhe** o poder: na mira, **O** arma o poder e **O de novo troca** (ENTERRO > COVA > RESSUSCITAR > cancelar). Recarrega em 3 tacadas.

- **ENTERRO** (nerfado): depois da batida a bola alvo (a primeira que a branca toca) é desviada **35%** (`ENTERRO_PUXAO`) para a caçapa **mais alinhada com o rumo dela** (até ~57°, `ENTERRO_COS`; se não houver caçapa na frente o poder falha e a recarga volta) e a mais de ~340 de distância (`ENTERRO_ALCANCE`) também não funciona. Depois ela é guiada só um pouco (`ENTERRO_GIRO` 1,6 rad/s por `ENTERRO_TEMPO` 260 passos), **sem velocidade extra e sem ajuda para não morrer antes da caçapa**: precisa de boa mira para cair. Só a bola 8 não é enterrada antes de você limpar as suas (seria derrota).
- **COVA**: abre uma caçapa extra (agora bem menor, raio 12 em vez de 20) no meio da mesa durante a tacada. A bola que cair lá (menos a branca) **conta como encaçapada** e não volta.
- **RESSUSCITAR**: traz de volta à mesa a última bola do adversário que foi encaçapada (contra-ataque). A bola que volta é um **fantasma**: nesta jogada a branca e as outras bolas **passam por ela, sem física**; na rodada seguinte ela volta ao normal.

## Policial (lendário) e mesa Delegacia

Combo **POLICIAL** (personagem + taco **Cassetete** + mesa **Delegacia**: fita de isolamento, sirenes vermelha/azul e holofotes). Como o Coveiro, **você escolhe** o poder: na mira, **O** arma e **O de novo troca** (ALGEMAS > SIRENE > cancelar). Recarrega em 3 tacadas.

- **ALGEMAS**: prende **qualquer bola do adversário** por 1 jogada (até o fim da próxima tacada dele). A bola presa **não se mexe de jeito nenhum**: nenhum poder a move, não cai na caçapa e quem bate nela rebate como em uma parede. **QUADRADO** troca a bola que será presa (começa pela mais perto de uma caçapa). O BOT não usa a bola presa como alvo.
- **SIRENE**: na próxima jogada o adversário joga **sem linha guia** (o BOT erra mais a mira).

## Vampiro: barra de sangue (SEDE DE SANGUE)
Quando você joga com o **taco VAMPIRO** (com HABILIDADES ligadas), aparece uma **barra de sangue com 3 gomos** embaixo do seu painel (a barra é do taco, não do personagem; o BOT com o personagem Vampiro também usa):
- Cada tacada que **encaçapa uma bola sua** enche 1 gomo
- Se a tacada **não encaçapar** (ou der falta), a barra **não se perde**: os gomos ficam guardados
- Com **3 bolas** (não precisam ser seguidas) a barra enche e pulsa: a **4ª tacada vira SEDE DE SANGUE**, com **50% mais força** e a bola alvo **curva bem mais forte para a caçapa** (PUXÃO reforçado, mira até ~75°)
- Depois da tacada de sangue a barra zera. Vale também para o Vampiro controlado pelo BOT
- **MORCEGOS:** na jogada seguinte do adversário, **6 morcegos** aparecem na mesa e voam por ela. As bolas **batem neles com física** (quicam e desviam) e a branca não pode ser posicionada em cima de um. Eles somem assim que o adversário termina a tacada

## Vampiro e Grinch (PUXÃO)

O puxão ficou mais suave: em vez de um ímã que arrastava a bola para a caçapa mais próxima, a bola alvo agora **curva aos poucos** para a caçapa que ela já está mirando (até ~57°), sem ganhar velocidade extra.

## Rodape com icones (novo)
- O medidor de **FPS / MUSICA / PICO** foi desligado (`DEBUG_FPS 0` no `main.c`), entao nao aparece mais por cima do rodape. Para voltar a ver, ponha `DEBUG_FPS 1`
- Os botoes do rodape agora sao icones desenhados (funcao `uiKey()`): **X** (cruz), **O** (anel), **triangulo** e **quadrado** vazados, **START** com simbolo de play, **SELECT** com tracinhos, **direcional** em cruz (destaca cima/baixo ou esquerda/direita) e **L1/R1** como dois gatilhos

## Upgrade gráfico (visual profissional)

- Personagens com volume: cabeças, olhos, mãos e sapatos agora são esferas sombreadas (degradê Gouraud, luz vinda de cima-esquerda) em vez de formas chapadas; corpo e mantos com degradê claro→escuro.
- Contorno escuro suave na cabeça e no corpo, bochechas, brilho nos olhos e sombra no chão sob os personagens de corpo inteiro.
- Caixas e painéis do HUD com cantos arredondados e degradê contínuo (sem faixas).
- Céu, feltro da mesa de fundo e faixa do título com degradê suave.
- Chave `GFX_ROUND` (em main.c, padrão `1`). Se o FPS cair abaixo de 60 no PS2/PCSX2, compile com `GFX_ROUND=0` (ou mude o `#define`) para voltar ao visual antigo, mais leve.
