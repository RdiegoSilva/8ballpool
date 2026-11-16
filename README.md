# 8ballpool (PS2)

Jogo de sinuca 8 ball para PS2 (PS2SDK + gsKit + libpad).

- Criador: Diego Reis
- Versão: 1.1.0

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
  - COR JOGADOR 1 / 2 (só na Sinuca Brasileira): Azul, Vermelha, Amarela, Verde, Roxa ou Laranja (cada jogador usa uma cor diferente; padrão: amarela x vermelha)
- CRÉDITOS: mostra o criador e a versão do jogo

## Modos de jogo
- **Bola 8**: as regras de sempre (lisas x listradas, bola 8 por último)
- **Sinuca Brasileira**: a branca começa no meio da mesa e cada jogador tem 4 bolas lisas (sem número) da cor escolhida, agrupadas cada uma no seu lado da mesa, como numa mesa de verdade
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

- Força: velocidade da bola na tacada
- Efeito: quanto a bola branca curva com L2/R2
- Mira: tamanho da linha de mira (no máximo cerca de 60% da mesa, e ela some aos poucos no fim)
- Controle: velocidade da barra (maior = barra mais lenta, mais fácil de acertar)

## Compilar
```
make
```
Gera `pool8.elf` (requer PS2SDK e gsKit; rode no PCSX2 ou no PS2).
