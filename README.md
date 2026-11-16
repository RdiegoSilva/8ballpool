# 8ballpool (PS2)

Jogo de sinuca 8 ball para PS2 (PS2SDK + gsKit + libpad).

- Criador: Diego Reis
- Versão: 1.0.0

## Menu inicial (animado)
- 1 JOGADOR (VS BOT), 2 JOGADORES, DIFICULDADE e CRÉDITOS
- Cima/Baixo escolhe, X confirma
- DIFICULDADE: Esquerda/Direita (ou X) troca entre FÁCIL, MÉDIO e DIFÍCIL (vale para o BOT no modo 1 jogador)
  - Fácil: erra mais a mira e a força, pensa devagar e faz jogadas ruins de vez em quando
  - Médio: comportamento padrão
  - Difícil: mira quase perfeita e responde rápido
- CRÉDITOS: mostra o criador e a versão do jogo

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
- Mira: comprimento das linhas depois do impacto (bola alvo e desvio da branca). A linha até a bola sempre aparece inteira, com até 2 rebotes nas tabelas; a caçapa de destino da bola alvo fica verde
- Controle: velocidade da barra (maior = barra mais lenta, mais fácil de acertar)

## Compilar
```
make
```
Gera `pool8.elf` (requer PS2SDK e gsKit; rode no PCSX2 ou no PS2).
