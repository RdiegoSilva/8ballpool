# 8ballpool (PS2)

Jogo de sinuca 8 ball para PS2 (PS2SDK + gsKit + libpad).

- Criador: Diego Reis
- Versão: 1.3.0

## Menu inicial (animado)
- MODO HISTÓRIA (em breve), JOGAR e CRÉDITOS
- Cima/Baixo escolhe, X confirma
- JOGAR abre a tela **Configurar partida** (Cima/Baixo escolhe a linha, Esquerda/Direita muda o valor, X ou START inicia, O volta):
  - ADVERSÁRIO: BOT ou JOGADOR 2. Com JOGADOR 2, ao iniciar abre a tela para **digitar o nome** (teclado na tela: direcional move, X escolhe a letra, O apaga, START confirma, SELECT volta; até 10 caracteres; vazio = "JOGADOR 2")
  - DIFICULDADE (só contra o BOT): FÁCIL, MÉDIO, DIFÍCIL ou PROFISSIONAL
    - Fácil: erra mais a mira e a força, pensa devagar e faz jogadas ruins de vez em quando
    - Médio: comportamento padrão
    - Difícil: mira quase perfeita e responde rápido
    - Profissional: mira praticamente perfeita, responde na hora, prefere tacadas retas e não perdoa erro
  - MODO: BOLA 8 ou SINUCA BRASILEIRA
  - MESA: Clássica, Fênix (feltro verde e madeira preta), Oceano, Vinho, Royal, **Preta Ouro** (feltro preto com detalhes dourados) ou **Neon** (animada: luzes correndo em volta, faixas de luz, estrelas e caçapas brilhando)
  - COR JOGADOR 1 / 2 (só na Sinuca Brasileira): Azul, Vermelha, Amarela, Verde, Roxa, Laranja, Rosa, Ciano, Marrom, Cinza, Lima ou Dourada (cada jogador usa uma cor diferente; padrão: vermelha x amarela)
  - Embaixo aparece a **prévia da mesa** escolhida (com as bolas na posição inicial do modo e as animações da mesa Neon)
- CRÉDITOS: mostra o criador e a versão do jogo

## Modos de jogo
- **Bola 8**: as regras de sempre (lisas x listradas, bola 8 por último)
- **Sinuca Brasileira**: a branca começa no meio da mesa e cada jogador tem 4 bolas lisas (sem número) da cor escolhida, como numa mesa de verdade: 2 bolas juntas no centro da cabeceira de cada jogador e 1 bola em cada tabela comprida
  - Vence quem encaçapar as 4 bolas da sua cor primeiro
  - Encaçapou uma bola sua sem errar? Joga de novo
  - **Errou a sua bola** (tocou primeiro na bola do adversário ou não tocou em nenhuma): o adversário **tira qualquer uma bola da mesa** (Setas escolhem a bola, X tira) e joga em seguida. O bot tira a própria bola mais difícil
  - **Suicídio** (branca na caçapa): quem fez perde na hora
  - Se a última bola de um jogador sair da mesa (por tacada ou por bola tirada), a partida acaba

## Placar
- Contra o BOT e contra o JOGADOR 2 têm placares separados de vitórias no topo da partida, entre os painéis dos jogadores
- Fim de jogo: X = revanche (mantém o placar), START = voltar ao menu
- O placar zera ao desligar o jogo

## Controles
- Esquerda/Direita: girar a mira | L1/R1: ajuste fino
- X: a barra de força (lado esquerdo) sobe e desce sozinha; X de novo dispara a tacada
- O: cancelar a tacada
- L2/R2: efeito na bola branca (esquerda/direita), indicador no canto inferior esquerdo
- START: pausa o jogo (veja abaixo)
- SELECT: abre o menu de tacos (Esq/Dir escolhe, X equipa, O ou SELECT volta)
- Bola na mão: direcional move a bola branca, X confirma

## Tacos
| Taco       | Força | Efeito | Mira | Controle |
|------------|-------|--------|------|----------|
| Clássico   | 55    | 30     | 60   | 60       |
| Potência   | 100   | 20     | 45   | 35       |
| Efeito     | 50    | 100    | 50   | 55       |
| Mira Longa | 50    | 30     | 100  | 65       |
| Precisão   | 45    | 40     | 70   | 100      |
| Sniper     | 40    | 10     | 100  | 100      |

- **Sniper**: laser vermelho que mostra a tacada inteira (até 3 rebotes e o caminho completo da bola alvo), mas bate fraco e quase não faz efeito
- Força: velocidade da bola na tacada
- Efeito: quanto a bola branca curva com L2/R2
- Mira: tamanho da linha de mira (no máximo cerca de 60% da mesa, e ela some aos poucos no fim)
- Controle: velocidade da barra (maior = barra mais lenta, mais fácil de acertar)

## Pausa (START)
Continuar, **Som**, **Música** (liga/desliga), **Volume da música**, **Player de músicas**, **Efeitos** (animações das mesas e brilho da mira), **Dicas na tela** (barra de teclas e dicas da mira), Controles e dicas, Reiniciar partida e Menu principal.
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
