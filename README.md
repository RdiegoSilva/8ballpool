# 8ballpool (PS2)

Jogo de sinuca 8 ball para PS2 (PS2SDK + gsKit + libpad).

## Modos
- 1 jogador (contra o BOT)
- 2 jogadores (dois controles)

## Controles
- Esquerda/Direita: girar a mira | L1/R1: ajuste fino
- Segurar X: encher a barra de força (lado esquerdo) e puxar o taco
- Soltar X: tacada (a barra volta a zero)
- O: cancelar a tacada
- Bola na mão: direcional move a bola branca, X confirma

## Compilar
```
make
```
Gera `pool8.elf` (requer PS2SDK e gsKit; rode no PCSX2 ou no PS2).
