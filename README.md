# 8ballpool (PS2)

Jogo de sinuca 8 ball para PS2 (PS2SDK + gsKit + libpad).

## Modos
- 1 jogador (contra o BOT)
- 2 jogadores (dois controles)

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
- Mira: tamanho da linha de mira
- Controle: velocidade da barra (maior = barra mais lenta, mais fácil de acertar)

## Compilar
```
make
```
Gera `pool8.elf` (requer PS2SDK e gsKit; rode no PCSX2 ou no PS2).
