# 8 Ball Reis (PS2)

Jogo de sinuca 8 ball para PS2 (PS2SDK + gsKit + libpad).

- Criador: Diego Reis
- Versão: 2.9.0

## Visual dos menus (novo)
Todos os menus (inicial, Configurar partida, Nome, Opções, Créditos e Chaveamento do torneio) seguem o mesmo padrão:
- **Faixa de título** dourada no topo, com brilho que corre e enfeites nos lados
- **Rodapé com os botões** do controle (X, O, START, SELECT, direcional, L1/R1) e o que cada um faz
- **Menu inicial**: cartões com ícone para cada modo (JOGAR, TORNEIO, PLAYER DE MÚSICAS, OPÇÕES) e um **painel de detalhes** ao lado, que mostra o que tem em cada modo (e quantas músicas foram encontradas)
- **Configurar partida / torneio**: barra de 4 etapas com bolinhas numeradas (etapa atual em dourado, concluídas em verde)
- **Opções** em grupos (**AUDIO**, **VISUAL**, **SOBRE**): chaves liga/desliga, volume em barras e uma caixa com a descrição da opção selecionada

## Menu inicial (animado)
- MODO HISTÓRIA (em breve), JOGAR, **TORNEIO**, PLAYER DE MÚSICAS e **OPÇÕES**
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
  - MESA (27 opções): Clássica, Fênix (feltro verde e madeira preta), Oceano, Vinho, Royal, **Preta Ouro** (feltro preto com detalhes dourados), **Neon** (luzes correndo em volta, faixas de luz, estrelas e caçapas brilhando) e as novas:
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
    - As animações param se a opção **Efeitos** estiver desligada
  - COR DAS BOLAS (só na Bola 8, **16 temas**): Clássicas, Neon, Pastel, Gelo, Fogo, Floresta, Ouro, Arco-íris, Candy, Oceano, Metal, Tropical, Pôr do sol, Lavanda, Retrô e Ácido (mostra as 8 cores do tema; vale para lisas e listradas)
  - COR JOGADOR 1 / 2 (só na Sinuca Brasileira, agora **32 cores**): Azul, Vermelha, Amarela, Verde, Roxa, Laranja, Rosa, Ciano, Marrom, Cinza, Lima, Dourada, Turquesa, Bordô, Salmão, Verde escuro, Azul claro, Violeta, Rosa choque, Bege, Prata, Coral, Oliva, Lavanda, Mostarda, Menta, Magenta, Marinho, Grafite, Pêssego, Chocolate ou Azul bebê (cada jogador usa uma cor diferente; padrão: vermelha x amarela)
  - **BOLA BRANCA** (12 cores: Branca, Marfim, Vermelha, Azul, Verde, Amarela, Rosa, Ciano, Laranja, Roxa, Dourada e Prata) e **LINHA GUIA** (10 cores: Branca, Vermelha, Verde, Azul, Amarela, Rosa, Ciano, Laranja, Roxa e Arco-íris, que muda de cor sozinha). Na tela de configurar valem como cor inicial dos dois jogadores
  - Embaixo aparece a **prévia da mesa** escolhida (com as bolas na posição inicial do modo, a cor da bola branca escolhida e as animações das mesas)

## Personagens (caricaturas)
Em **Configurar partida** há duas linhas novas: **SEU PERSONAGEM** e **PERSONAGEM DO BOT** (ou **JOGADOR 1 / JOGADOR 2** no versus). São 18 caricaturas desenhadas no próprio jogo: **Pinguim**, Robô, Pirata, Ninja, Alien, Cowboy, Vampiro, Mago, Viking, Gato, **Chapolin**, **Hacker** (capuz escuro, óculos ciano e moletom com 01) e **Papai Noel** (gorro, barba e traje vermelho), **Mestre do Tempo**, **Grinch** (pelo verde, olhos amarelos e gorro roubado do Noel) **Poseidon** (cabelo e barba azuis, coroa de coral e tridente dourado) **Coveiro** (chapéu preto, rosto pálido e pá) e **Policial** (boné azul com distintivo dourado, bigode e algemas no cinto). Cada jogador usa um personagem diferente (padrão: Pinguim x Robô).
- Embaixo da tela de configuração aparecem os dois escolhidos; o da linha selecionada comemora
- O rosto do personagem fica no canto do painel de cada jogador durante a partida

## Animações de vitória e derrota
Quando a partida termina:
- **Quem venceu**: o personagem pula de alegria com os braços pro alto, com raios de luz girando, fogos de artifício, chuva de confete e um troféu brilhando no centro
- **Quem perdeu**: o personagem fica cabisbaixo, chorando, debaixo de uma nuvem de chuva
- **Coveiro (especial)**: se o Coveiro vence, ele joga areia com a pá no adversário, que vai sendo enterrado até sumir debaixo de um montinho de terra com uma cruz e o aviso "DESCANSE EM PAZ"
- Contra o BOT: se você ganha aparece **VITORIA!**; se o bot ganha aparece **DERROTA**. No versus aparece "NOME VENCEU!"
- X = revanche (mantém o placar), START = voltar ao menu

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
Quando a sua vez começa (depois da tacada do adversário, da bola na mão ou de tirar uma bola), a mira **já aponta para a bola sua mais perto que tenha caminho livre**. Se houver bola do adversário (ou outra bola) na frente de todas, a mira fica como estava. Você continua podendo girar a mira normalmente.

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
| Hacker     | Hack       | **HACK**: calcula a melhor tacada (confere na simulação da mesa), trava mira, efeito e pulo, mostra o caminho todo e marca a **força ideal** na barra | Hack |
| Poseidon   | Tridente   | **MARÉ ALTA**: uma onda gigante empurra **todas as bolas** (menos a branca) na direção da caçapa mais próxima de cada uma, com força fraca: só as bolas que já estão perto de uma caçapa chegam lá | Abismo |
| Alien      | Ovni       | **RASTREIO**: a branca curva sozinha e **persegue a bola certa** mais perto da sua mira (a sua bola; mesa aberta: qualquer uma, menos a 8), até a primeira batida | Galáxia |
| Robô       | Robô       | **ÍMÃ**: as **suas bolas** são puxadas para a caçapa mais próxima de cada uma, com força fraca (mesa aberta: todas, menos a 8) | Neon |
| Mago       | Varinha    | **MALDIÇÃO**: amaldiçoa o adversário: na **próxima tacada dele** os botões de mirar ficam **invertidos** (direcional, stick e L1/R1) e a **força falha** às vezes (35% de chance: marcou 100, sai 10). O bot amaldiçoado só erra mais a mira. O painel do amaldiçoado ganha uma aura roxa e aparece "AMALDIÇOADO!" | Noite Lua |
| Gato       | Gato       | **NOVA VIDA**: se a tacada **não encaçapar** e não for falta, você **joga de novo**. Se encaçapar, a habilidade **não gasta** | Rosa |
| Grinch     | Grinch     | **GRINCH**: ao apertar O na mira, a habilidade é **sorteada** entre Puxão verde e Bagunça conforme a situação da mesa (veja abaixo da tabela) | Grinch |
| Viking     | Dragão     | **FOGO**: a bola alvo explode e empurra as bolas que estão perto | Fogo |
| Ninja      | Samurai    | **CORTE**: +20% de força e a branca para na hora ao bater na bola | Preta Ouro |
| Pirata     | Oceano     | **MARÉ**: a branca faz uma curva forte (efeito x2,5) sem precisar de L2/R2 | Oceano |

- **Usar a habilidade: O na mira.** Na maioria, o primeiro toque arma a habilidade (aparece "ARMADO!" no canto) e vale para a próxima tacada; tocar de novo cancela. O **HACK** age na hora
  - **HACK (melhorado)**: só mostra tacadas que **realmente encaçapam** (cada uma é testada na física do jogo: bola certa primeiro, branca não cai). Funciona na Bola 8 e na Sinuca Brasileira. **O de novo** troca para a próxima melhor opção (até 4) e, depois da última, libera a mira. Com o HACK ligado a mira, o efeito, o pulo e a troca de taco ficam travados, a tacada não sofre desvio de giz, a barra de força anda mais devagar e **gruda na força ideal** (±6%). Sem tacada possível, ele sugere uma jogada de segurança (acerta a bola certa sem arriscar a branca). Na quebra mira reto no triângulo com força total
  - **CHAMINÉ**: vale para a Bola 8 e para a Sinuca Brasileira; o efeito dura só a tacada e depois a recarga é de 3 tacadas
- Depois de usar, a habilidade **recarrega em 3 tacadas** (o contador aparece ao lado do nome do taco)
- O BOT não usa habilidades. No torneio só você usa habilidades, e só se a regra HABILIDADES estiver LIGADA (padrão: DESLIGADAS, taco Clássico)
- No **versus** você escolhe na configuração se a partida é **com ou sem habilidades** (HABILIDADES: LIGADAS/DESLIGADAS). Desligadas, os tacos valem só pelos atributos

## Catálogo de tacos (SELECT na mira)
Tela reorganizada: lista à esquerda (com rolagem) separada em **PADRÃO**, **PAÍSES** e **PERSONAGENS**; à direita, a foto do taco sobre o feltro da mesa do personagem, os atributos, a **caricatura do dono do taco**, a **habilidade** (com descrição e recarga) e a **mesa do personagem**. Cima/Baixo escolhe, X equipa, O ou SELECT volta.

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
| Dragão     | 80    | 40     | 40   | 35       |
| Gelo       | 45    | 60     | 65   | 70       |
| Ouro       | 60    | 40     | 55   | 55       |
| Neon       | 55    | 55     | 55   | 55       |
| Samurai    | 75    | 30     | 50   | 45       |
| Oceano     | 50    | 70     | 45   | 55       |
| Pistoleiro | 70    | 35     | 60   | 55       |
| Brasileiro | 60    | 50     | 60   | 60       |
| Vampiro    | 65    | 60     | 55   | 50       |
| Noel       | 55    | 60     | 55   | 65       |
| Grinch     | 58    | 62     | 55   | 60       |
| Hack       | 50    | 50     | 80   | 80       |
| Tridente   | 56    | 64     | 55   | 60       |
| Ovni       | 52    | 55     | 60   | 58       |
| Robô       | 62    | 35     | 65   | 65       |
| Varinha    | 48    | 70     | 55   | 55       |
| Gato       | 52    | 58     | 55   | 62       |

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
Continuar, **Som**, **Música** (liga/desliga), **Volume da música**, **Cor da linha guia**, **Player de músicas**, **Efeitos** (animações das mesas e brilho da mira), **Dicas na tela** (barra de teclas e dicas da mira), Controles e dicas (agora com rola/puxa e pulo), Reiniciar partida e Menu principal.
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

## Compilar
```
make
```
Gera `pool8.elf` (requer PS2SDK e gsKit; rode no PCSX2 ou no PS2).
O player de músicas usa audsrv, fileXio, USB (BDM) e o decodificador MP3 minimp3 (domínio público, já incluso em `minimp3.h`). Os módulos `.irx` do PS2SDK são embutidos no `.elf` pelo Makefile.
Arquivos: `main.c`, `music.c`, `music.h`, `minimp3.h`, `Makefile`.

## Coveiro (ultra lendário) e mesa Cemitério

Combo **COVEIRO** (personagem + taco + mesa **Cemitério**: grade de ferro, lápides, névoa e fogo-fátuo). Diferente do Grinch, **você escolhe** o poder: na mira, **O** arma o poder e **O de novo troca** (ENTERRO > COVA > RESSUSCITAR > cancelar). Recarrega em 3 tacadas.

- **ENTERRO**: a bola alvo (a primeira que a branca toca) tem a trajetória desviada **30%** em direção à caçapa mais próxima no momento da batida (`ENTERRO_PUXAO`); não é mais garantido, só ajuda. Se cair, conta como encaçapada; se errar, fica na mesa. Só a bola 8 não é enterrada antes de você limpar as suas (seria derrota). Para voltar à regra "só na última bola", ponha `ENTERRO_SO_ULTIMA` em 1 no `main.c`.
- **COVA**: abre uma caçapa extra (agora bem menor, raio 12 em vez de 20) no meio da mesa durante a tacada. A bola que cair lá (menos a branca) **conta como encaçapada** e não volta.
- **RESSUSCITAR**: traz de volta à mesa a última bola do adversário que foi encaçapada (contra-ataque). A bola que volta é um **fantasma**: nesta jogada a branca e as outras bolas **passam por ela, sem física**; na rodada seguinte ela volta ao normal.

## Policial (lendário) e mesa Delegacia

Combo **POLICIAL** (personagem + taco **Cassetete** + mesa **Delegacia**: fita de isolamento, sirenes vermelha/azul e holofotes). Como o Coveiro, **você escolhe** o poder: na mira, **O** arma e **O de novo troca** (ALGEMAS > SIRENE > cancelar). Recarrega em 3 tacadas.

- **ALGEMAS**: prende **qualquer bola do adversário** por 1 jogada (até o fim da próxima tacada dele). A bola presa **não se mexe de jeito nenhum**: nenhum poder a move, não cai na caçapa e quem bate nela rebate como em uma parede. **QUADRADO** troca a bola que será presa (começa pela mais perto de uma caçapa). O BOT não usa a bola presa como alvo.
- **SIRENE**: na próxima jogada o adversário joga **sem linha guia** (o BOT erra mais a mira).

## Vampiro e Grinch (PUXÃO)

O puxão ficou mais suave: em vez de um ímã que arrastava a bola para a caçapa mais próxima, a bola alvo agora **curva aos poucos** para a caçapa que ela já está mirando (até ~57°), sem ganhar velocidade extra.
