/* 8 BALL REIS para PS2 - PS2SDK + gsKit + libpad
 * Modos: 1 jogador (vs BOT, 4 niveis) e 2 jogadores (dois controles).
 * Criador: Diego Reis | Versao 2.3.0
 * Controles na mira:  Esq/Dir = girar | L1/R1 = ajuste fino
 *                     X = a barra de forca (esquerda) sobe e desce sozinha,
 *                         X de novo = tacada | O = cancelar a tacada
 *                     L2/R2 = efeito lateral | CIMA/BAIXO = rola/puxa
 *                     TRIANGULO = pulo da bola branca | SELECT = trocar de taco
 * Bola na mao:        direcional move a bola branca, X confirma
 */
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <libpad.h>
#ifndef USE_MEMCARD
#define USE_MEMCARD 1   /* 1 = salva moedas e itens da LOJA no memory card 1 (usa funcoes POSIX em mc0:, sem biblioteca extra); 0 = nao salva */
#endif
#if USE_MEMCARD
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "music.h"

#define R     8.5f                       /* raio das bolas (original 8): bolas um pouco maiores */
#define TX0   64.0f                      /* mesa maior (era 70..598 x 100..364): agora 64..604 x 86..364 */
#define TX1   604.0f
#define TY0   86.0f
#define TY1   364.0f
#define TCX   ((TX0 + TX1) * 0.5f)
#define TCY   ((TY0 + TY1) * 0.5f)
#define HEADX (TX0 + (TX1 - TX0) * 0.25f)
#define FOOTX (TX0 + (TX1 - TX0) * 0.75f)
#define MAXV  1200.0f                     /* velocidade maxima da branca (era 640): bolas bem mais rapidas */
#define FRIC_K 0.55f                      /* atrito proporcional a velocidade (era 0.6) */
#define FRIC_C 20.0f                      /* atrito constante (era 24) */
#define OLD_MAXV 640.0f                   /* fisica antiga: so o bot usa, para manter a mesma distancia de tacada */
#define OLD_FRIC_K 0.6f
#define OLD_FRIC_C 24.0f

enum { S_MENU, S_AIM, S_ROLL, S_PLACE, S_OVER, S_STRIKE, S_CUES, S_CREDITS, S_SETUP, S_NAME, S_TAKE, S_PLAYER, S_OPTS, S_BRACKET, S_REWIND, S_SHOP, S_REPLAY };
enum { GM_8BALL, GM_BR, GM_MINI, GM_SUDDEN, GM_RACE, GM_DUEL, NGM };            /* modos de sinuca (MINI e MORTE SUBITA usam as regras da bola 8) */
static const char *GMN[NGM] = { "BOLA 8", "SINUCA BRASILEIRA", "BOLA 8 MINI", "MORTE SUBITA", "CORRIDA DE 3", "BOLA 8 DUELO" };

typedef struct { float x, y, vx, vy; int in; float z, vz; } Ball;   /* z/vz: altura no pulo */

static GSGLOBAL *g;
static GSFONTM  *fm;
static float OX, OY;

static char padbuf0[256] __attribute__((aligned(64)));
static char padbuf1[256] __attribute__((aligned(64)));

static Ball b[16];
static int state = S_MENU, mode = 0, menuSel = 1;
static int cur, grp[2], breakShot, charging;
static float ang, pwr;
static int pocketed[16], firstHit, gAtStart, startOwnLeft;
static int winner, botTimer, botPlanned;
static int gmode = GM_8BALL, tableSel = 27;       /* modo de jogo e mesa escolhidos */
static int colSel[2] = { 1, 2 };                 /* cores das bolas no modo brasileiro */
static char pnames[2][12] = { "JOGADOR 1", "JOGADOR 2" };
static int takeList[16], takeN, takeSel;         /* bolas que podem ser tiradas apos uma falta (brasileira) */
static int timeSel, turnT, turnCur = -1, turnReset = 1;   /* tempo por jogada (versus): 0 = sem tempo */
static const int TIME_SEC[5] = { 0, 15, 20, 25, 30 };
#define NTIMES 5
static char overNote[48];                         /* motivo do fim de jogo */
static int winsBot[2], winsVs[2], training;   /* training: MODO TREINO (sozinho na mesa, sem adversario nem regras) */   /* placar de vitorias (vs bot / versus play) */
static float botAng, botPow;
static char msg[64];
static int msgTimer;

/* torneio mata-mata: 8 jogadores (quartas, semifinal, final); perdeu, esta fora */
static int tTable = -1;                           /* torneio: mesa da partida atual (muda a cada partida) */
static int tAbil;                                 /* regra do torneio: 1 = com habilidades (voce escolhe o taco), 0 = so taco CLASSICO */
static int tourney, tRound, tOpp, tPM, tRes, tChamp;   /* tRes: 0 antes da partida, 1 avancou, 2 eliminado, 3 campeao */
static int tSlot[4][8], tChar[8];                      /* tSlot[rodada][posicao] = competidor (0 = voce, -1 = ainda nao definido) */
static char tName[8][12];
/* habilidades dos tacos de personagem (O na mira usa a habilidade; recarrega em AB_COOL tacadas) */
enum { AB_NONE, AB_TIRO, AB_CONGELAR, AB_SUGAR, AB_CHAMINE, AB_HACK, AB_FOGO, AB_CORTE, AB_MARE, AB_MARRETA, AB_TEMPO, AB_GRINCH, AB_BAGUNCA, AB_MAREALTA, AB_RASTREIO, AB_IMA, AB_MALDICAO, AB_VIDAS, AB_COVEIRO, AB_ENTERRO, AB_COVA, AB_RESSUS, AB_POLICIAL, AB_PRENDER, AB_SIRENE, AB_CORNETA, AB_CHIQUI, AB_BIONICA, NAB };
static const char *AB_NAME[NAB] = { "", "TIRO", "CONGELAR", "SUGAR", "CHAMINE", "HACK", "FOGO", "CORTE", "MARE", "CHAPOLIN", "TEMPO", "GRINCH", "BAGUNCA", "MARE ALTA", "RASTREIO", "IMA", "MALDICAO", "NOVA VIDA", "COVEIRO", "ENTERRO", "COVA", "RESSUSCITAR", "POLICIAL", "ALGEMAS", "SIRENE", "CORNETA", "CHIQUITOLINA", "MARRETA BIONICA" };
#define AB_COOL 3
static int abilSel, abilOn = 1, tableAuto;           /* abilSel: versus 0 = com habilidades, 1 = sem */
#define FORGIVE_ON(f) ((f) == 1 || (f) == 4)          /* 1 = PRESENTE (Noel), 4 = NOVA VIDA do Gato (perdoa falta e suicidio e joga de novo) */
static int abilArm, abilKind, abilT, abilFx, abilForgive, hackOn, abilCD[2], grinchOn, abilExtra, rastT = -1, curse[2];   /* curse[p]: MALDICAO do mago sobre o jogador p */   /* grinchOn: a habilidade desta tacada veio do sorteio do Grinch */
#define ENTERRO_SO_ULTIMA 0                           /* 1 = ENTERRO so funciona na sua ultima bola (antes da 8); 0 = qualquer bola */
#define GRAVE_R 12.0f                                 /* raio da COVA (buraco extra no meio da mesa; era 20) */
#define ENTERRO_ALCANCE 340.0f                        /* NERF: distancia maxima ate a cacapa (antes sem limite) */
#define ENTERRO_PUXAO 0.35f                           /* NERF: desvia a bola alvo so 35% em direcao a cacapa na batida (antes 90%) */
#define ENTERRO_COS 0.55f                             /* NERF: so ajuda cacapas mais ou menos a frente da bola (ate ~57 graus); sem cacapa na frente, nao funciona */
#define ENTERRO_GIRO 1.6f                             /* NERF: velocidade da curva depois da batida, em rad/s (antes 7: a bola nunca errava) */
#define ENTERRO_TEMPO 260                             /* NERF: por quantos passos de fisica a bola continua sendo guiada (antes 1500) */
#define ENTERRO_RECARGA 3                             /* o ENTERRO recarrega em 3 tacadas */
static int graveSel, graveOpen, graveOn, graveIn[16], buryBall, buryP, buryT, potLog[32], potN;   /* COVEIRO: poder escolhido, bolas na cova, bola enterrada, cemiterio (bolas encacapadas) */
static float graveOX[16], graveOY[16];                /* COVA: onde cada bola estava antes da tacada */
static const int GV_KIND[3] = { AB_ENTERRO, AB_COVA, AB_RESSUS };
static const char *GV_NAME[3] = { "ENTERRO", "COVA", "RESSUSCITAR" };
/* POLICIAL (lendario): 2 poderes que o jogador escolhe (ALGEMAS, SIRENE). lockBall = bola presa (nao se mexe de jeito nenhum) */
static int raceCnt[2];                                  /* CORRIDA DE 3: bolas encacapadas por jogador */
static char raceBuf[2][16];
static const char *raceTxt(int pl) { sprintf(raceBuf[pl], "%d DE 3", raceCnt[pl]); return raceBuf[pl]; }
static int copSel, copOpen, copTgt, lockBall, lockOwner, sirenT[2], ghostBall;   /* ghostBall: bola ressuscitada (sem fisica ate o fim da jogada) */
static float lockX, lockY;
static const int CP_KIND[2] = { AB_PRENDER, AB_SIRENE };
static const char *CP_NAME[2] = { "ALGEMAS", "SIRENE" };
/* CHAPOLIN (ultra lendario): 3 poderes que o jogador escolhe.
   CORNETA PARALISADORA: uma bola do adversario nao se mexe por 1 rodada (ate o fim da proxima jogada dele)
   PASTILHAS DE CHIQUITOLINA: ate o fim da proxima jogada do adversario, as bolas dele ficam menores, mais lentas e a forca dele cai
   MARRETA BIONICA: nesta tacada as bolas saem mais rapidas e demoram para parar */
#define CHIQ_R    0.70f                               /* tamanho das bolas do adversario sob a CHIQUITOLINA */
#define CHIQ_POW  0.75f                               /* forca da tacada do adversario sob a CHIQUITOLINA */
#define CHIQ_DRAG 1.6f                                /* atrito extra das bolas do adversario (por segundo) */
#define BION_VEL  1.35f                               /* MARRETA BIONICA: velocidade da branca */
#define BION_FRIC 0.40f                               /* MARRETA BIONICA: atrito (menor = demora mais para parar) */
static int chSel, chOpen, cornBall, cornOwner, chiqOn, chiqOwner = -1;
static float cornX, cornY;
static const int CH_KIND[3] = { AB_CORNETA, AB_CHIQUI, AB_BIONICA };
static const char *CH_NAME[3] = { "CORNETA", "CHIQUITOLINA", "MARRETA BIONICA" };
#define LOCKED(i) ((i) > 0 && ((i) == lockBall || (i) == cornBall))   /* bola presa (algemas) ou paralisada (corneta): nada a move */
#define TAB_CEM 25                                    /* indice da mesa CEMITERIO em TABLES */
static float tabPock(void) { return 1.0f; }   /* todas as mesas (inclusive o cemiterio): cacapas e bordas no tamanho padrao da Pool Azul */
static int grinchPend;                                /* poder que o Grinch tem agora (0 = ainda nao sorteado); aparece na tela antes de usar */
static float fricMul = 1.0f, pockMul = 1.0f, hackPow, fxX, fxY;   /* pockMul: CHAMINE aumenta as cacapas */
static float hCandAng[10], hCandPow[10], hCandSc[10];   /* HACK: melhores tacadas encontradas (a mais facil primeiro) */
static int hCandN, hCandI;
static const char *tRoundName(int r) { return r == 0 ? "QUARTAS DE FINAL" : (r == 1 ? "SEMIFINAL" : "FINAL"); }

/* animacoes: tacada, barra de forca e bolas caindo na cacapa */
#define STRIKEN 6             /* frames da tacada (taco indo ate a bola) */
#define FOLLOWN 10            /* frames do taco depois da tacada */
#define FALLN   16            /* frames da bola caindo na cacapa */
static float barLevel, strikePwr, strikeD0, hitX, hitY;
static int   strikeK, follow;
static int   fallT[16], fallP[16];
/* MESTRE DO TEMPO: foto da mesa antes da tacada; se a tacada perder a partida, oferece voltar no tempo */
static Ball  tmB[16];
static int   tmSnap, tmArmed, tmNow, tmSel, tmEight, tmGrp[2];
static char  tmNote[48];
static float fallX[16], fallY[16];
static float spinSel, spinCur, spinK, barDir = 1.0f;   /* efeito e vai-e-vem da barra */
static int   cueSel[2], cueMenuSel, cueRet, hitCue;    /* taco de cada jogador / menu */
static float chalk[2] = { 1.0f, 1.0f }, cueChalk;      /* giz no taco (1 = cheio) e giz mostrado na ponta */
static int   chalkAnim;                                /* animacao de passar giz (frames) */
static int   fineHold, dpadHold;                                 /* tempo segurando L1/R1 (ajuste fino acelera devagar) */

/* pulo e efeito vertical (v1.6) */
#define JUMP_G     900.0f                          /* gravidade da bola no ar */
#define CHAP_HOP   160.0f                          /* CHAPULIN: impulso do pulo da bola alvo (pico ~14, no ar ~0,35 s) */
#define JUMP_CLEAR 11.0f                           /* acima desta altura a branca passa por cima das outras bolas */
static float spinV, spinVCur, spinVK;              /* efeito vertical: + rola (segue), - puxa (volta) */
static int   jumpArm;                              /* 1 = a proxima tacada faz a branca pular */
static int   charRand[2], tableRandom;            /* ALEATORIO: sorteia o personagem / a mesa ao iniciar a partida */
static int   charSel[2] = { 0, 1 }, overT;         /* personagens (caricaturas) e relogio da tela de fim */

#define POC 12.0f                                  /* quina: o buraco encosta no canto do feltro mas NAO invade a mesa (so cobre as pontas das tabelas) */
#define POM 14.2f                                   /* meio: o buraco fica na tabela, encostando no feltro sem invadir */
static const float PX[6] = { TX0 - POC, TCX, TX1 + POC, TX0 - POC, TCX, TX1 + POC };
static const float PY[6] = { TY0 - POC, TY0 - POM, TY0 - POC, TY1 + POC, TY1 + POM, TY1 + POC };
static const float PR[6] = { 14.5f, 12.0f, 14.5f, 14.5f, 12.0f, 14.5f };   /* raio DESENHADO do buraco (nao passa da borda do feltro) */
/* raio de captura (fisica): o buraco agora fica mais perto do feltro, entao o raio e menor mas a boca continua generosa */
static const float PRC[6] = { 31.0f, 17.0f, 31.0f, 31.0f, 17.0f, 31.0f };
#define POCK_PULL 220.0f                          /* "funil": puxao suave da bola que entra na boca da cacapa */

#define NBC 35
#define BC_INIT { \
    {255,255,255}, {240,200,20}, {30,60,200}, {210,30,30}, {120,40,150}, \
    {240,120,20},  {20,140,60},  {130,20,30}, {15,15,15}, \
    {255,110,185}, {0,200,225},  {125,72,30}, {150,155,165}, {165,230,40}, {215,175,70}, \
    {30,200,180}, {110,15,45}, {250,130,110}, {15,90,45}, {110,175,255}, {170,90,255}, {255,30,140}, {225,200,150}, \
    {200,205,215}, {255,100,80}, {130,140,40}, {190,160,240}, {210,170,30}, {140,240,190}, \
    {210,30,170}, {15,35,120}, {70,72,80}, {255,190,140}, {85,45,25}, {170,215,255} }
static const u8 BC0[NBC][3] = BC_INIT;     /* paleta padrao */
static u8       BC [NBC][3] = BC_INIT;     /* paleta em uso (bolas 1 a 8 mudam com o tema de cores) */

/* temas de cor das bolas 1 a 8 (as listradas 9 a 15 repetem as mesmas cores) - so no modo Bola 8 */
#define NTHEMES 16
static const char *THEME_NAME[NTHEMES] = { "CLASSICAS", "NEON", "PASTEL", "GELO", "FOGO", "FLORESTA", "OURO", "ARCO-IRIS",
                                       "CANDY", "OCEANO", "METAL", "TROPICAL", "POR DO SOL", "LAVANDA", "RETRO", "ACIDO" };
static const u8 THEME[NTHEMES][8][3] = {
    { {240,200,20}, {30,60,200},  {210,30,30},  {120,40,150}, {240,120,20}, {20,140,60},  {130,20,30}, {15,15,15} },
    { {255,240,0},  {0,160,255},  {255,40,90},  {190,60,255}, {255,130,0},  {0,255,120},  {255,0,200}, {25,25,40} },
    { {255,236,150},{150,190,255},{255,160,170},{200,170,235},{255,200,150},{160,225,175},{215,140,160},{80,80,92} },
    { {210,240,255},{80,150,255}, {120,205,255},{150,130,255},{175,235,235},{60,190,200}, {40,90,170}, {20,32,64} },
    { {255,235,70}, {255,150,20}, {235,45,20},  {255,90,130}, {255,180,50}, {200,70,10},  {140,20,10}, {28,10,10} },
    { {205,205,90}, {40,115,75},  {165,65,45},  {125,95,45},  {85,165,65},  {30,95,55},   {95,55,32},  {15,28,16} },
    { {255,220,100},{230,190,75}, {205,155,55}, {255,238,160},{185,135,45}, {235,205,115},{155,110,32},{32,24,12} },
    { {230,40,40},  {250,140,30}, {245,215,40}, {60,190,70},  {40,200,210}, {50,90,230},  {150,70,220},{15,15,20} },
    { {255,120,190},{120,220,255},{255,210,90}, {170,255,140},{200,140,255},{255,150,110},{240,60,130}, {40,20,50} },
    { {255,220,120},{20,110,200}, {240,90,80},  {90,70,170},  {255,150,60}, {20,160,140}, {120,40,60},  {8,24,40} },
    { {235,235,240},{150,170,200},{200,90,90},  {150,130,180},{210,170,110},{110,170,150},{110,80,80},  {30,30,36} },
    { {255,225,50}, {0,170,210},  {255,80,70},  {160,70,200}, {255,140,30}, {40,190,90},  {190,50,90},  {20,40,30} },
    { {255,225,120},{90,70,160},  {255,110,70}, {190,70,150}, {255,160,60}, {240,90,110}, {150,40,70},  {35,15,35} },
    { {250,230,160},{110,120,230},{240,120,160},{170,120,230},{250,170,120},{120,200,170},{150,80,140}, {35,28,50} },
    { {230,190,60}, {50,100,170}, {190,60,50},  {120,80,130}, {210,120,50}, {70,130,80},  {110,40,40},  {25,22,20} },
    { {230,255,0},  {0,230,255},  {255,0,120},  {150,0,255},  {255,120,0},  {0,255,70},   {255,0,60},   {10,10,10} },
};
static int ballTheme;
/* LOJA: estilo das bolas, moedas, itens comprados (mascara de bits) e equipados, particulas de efeito */
enum { SC_GIZ, SC_BOLA, SC_RASTRO, SC_CACAPA, SC_TITULO, SC_MOLDURA, SH_CATS };
static int ballStyle, skinStyleEff;                  /* 0 classica, 1 caveira, 2 planetas, 3 frutas, 4 brilho no escuro */
static int shCoins, shOwn[SH_CATS], shEq[SH_CATS], shLastEarn;
typedef struct { float x, y, vx, vy; int life, max, kind, c; } Pfx;
#define NPF 120
static Pfx pf[NPF];
static void pfxBurst(int kind, float x, float y);
static void shopFrameDraw(int st, float x1, float y1, float x2, float y2);

/* cor da bola branca e da linha guia: cada jogador escolhe a sua (contra o bot vale a do jogador 1) */
#define NWB 12
static const char *WB_NAME[NWB] = { "BRANCA", "MARFIM", "VERMELHA", "AZUL", "VERDE", "AMARELA", "ROSA", "CIANO", "LARANJA", "ROXA", "DOURADA", "PRATA" };
static const u8 WB_COL[NWB][3] = { {250,250,250}, {255,242,208}, {240,70,70}, {80,130,255}, {80,215,110}, {255,228,70},
                                   {255,130,195}, {90,230,240}, {255,155,60}, {180,110,245}, {238,198,86}, {190,196,208} };
#define NGD 10
static const char *GD_NAME[NGD] = { "BRANCA", "VERMELHA", "VERDE", "AZUL", "AMARELA", "ROSA", "CIANO", "LARANJA", "ROXA", "ARCO-IRIS" };
static const u8 GD_COL[NGD][3] = { {255,255,255}, {255,80,80}, {90,255,120}, {90,150,255}, {255,230,70},
                                   {255,120,200}, {80,235,245}, {255,160,60}, {190,120,255}, {255,255,255} };
static int wbSel[2], gdSel[2];
static int optWho(void) { return mode == 0 ? 0 : cur; }       /* de quem e a cor usada agora */

/* cores escolhaveis no modo brasileiro (indice em BC) */
#define NBRC 32
static const int   BRC[NBRC]      = { 2, 3, 1, 6, 4, 5, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                      23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34 };
static const char *BRC_NAME[NBRC] = { "AZUL", "VERMELHA", "AMARELA", "VERDE", "ROXA", "LARANJA",
                                      "ROSA", "CIANO", "MARROM", "CINZA", "LIMA", "DOURADA",
                                      "TURQUESA", "BORDO", "SALMAO", "VERDE ESC.", "AZUL CLARO",
                                      "VIOLETA", "ROSA CHOQUE", "BEGE",
                                      "PRATA", "CORAL", "OLIVA", "LAVANDA", "MOSTARDA", "MENTA",
                                      "MAGENTA", "MARINHO", "GRAFITE", "PESSEGO", "CHOCOLATE", "AZUL BEBE" };

/* estilo de cada bola: cor (indice em BC), listra e numero (mudam no modo brasileiro) */
static int skinCol[16], skinStripe[16], skinNum[16], skinKey = 0;
static void skinDefaults(void)
{
    int i;
    for (i = 0; i < 16; i++) { skinCol[i] = i > 8 ? i - 8 : i; skinStripe[i] = i > 8; skinNum[i] = i > 0; }
}

/* tacos: estatisticas de 0 a 100 + cores para o desenho */
typedef struct {
    const char *name;
    int force, effect, aim, ctl;          /* forca, efeito, linha de mira, controle da barra */
    u8 tip[3], wood[3], butt[3], inlay[3], wrap[3];
    int fx;                               /* animacao: 0 nenhuma, 1 brilho, 2 arco-iris, 3 faiscas */
} CueDef;
#define NCUES 32
#define CUE_SNIPER 5
static CueDef CUES[NCUES] = {
    { "CLASSICO",   55,  30,  60,  60, { 58,104,172}, {228,192,130}, { 94, 48, 22}, {208,168, 72}, { 30, 24, 24} },
    { "POTENCIA",  100,  20,  45,  35, {190, 60, 50}, {222,170,110}, {150, 26, 26}, {210,210,215}, { 25, 20, 20} },
    { "EFEITO",     50, 100,  50,  55, { 70,150,230}, {226,196,140}, { 28, 64,160}, {130,210,255}, { 20, 26, 50} },
    { "MIRA LONGA", 50,  30, 100,  65, { 60,170,100}, {238,218,166}, { 20,112, 62}, {236,230,120}, { 20, 40, 28} },
    { "PRECISAO",   45,  40,  70, 100, {200,200,210}, {176,178,186}, { 44, 44, 50}, {205,205,215}, { 20, 20, 24} },
    /* SNIPER: laser vermelho que mostra a tacada inteira (ate 3 rebotes e o caminho completo da bola alvo);
       em troca tem pouca forca e quase nenhum efeito */
    { "SNIPER",     40,  10, 100, 100, {235, 60, 55}, { 96,104, 92}, { 38, 44, 36}, {255, 70, 60}, { 18, 22, 18} },
    { "DRAGAO",     80,  40,  40,  35, {255,140, 40}, { 96, 36, 24}, {150, 20, 12}, {255,190, 50}, { 24, 10, 10}, 1 },
    { "GELO",       45,  60,  65,  70, {190,235,255}, {206,232,248}, { 70,130,200}, {235,248,255}, { 30, 60,100}, 3 },
    { "OURO",       60,  40,  55,  55, {255,220,110}, {230,196,120}, {170,128, 40}, {255,235,150}, { 60, 40, 12}, 1 },
    { "NEON",       55,  55,  55,  55, {255, 60,200}, { 40, 40, 56}, { 18, 18, 28}, {  0,255,200}, {255, 60,200}, 2 },
    { "SAMURAI",    75,  30,  50,  45, {235,235,235}, { 60, 36, 30}, { 20, 16, 18}, {200, 30, 40}, {200, 30, 40}, 0 },
    { "OCEANO",     50,  70,  45,  55, {120,230,255}, { 70,150,190}, { 16, 70,130}, { 90,220,230}, { 10, 40, 80}, 3 },
    /* PISTOLEIRO (cowboy): couro e latao, brilho de bala correndo pelo taco e estrela de xerife */
    { "PISTOLEIRO", 70,  35,  60,  55, {205,165, 72}, {122, 74, 36}, { 52, 30, 16}, {255,215, 80}, {150, 98, 48}, 4 },
    /* BRASILEIRO: verde, amarelo e azul da bandeira; faixas e brilho alternando as cores */
    { "BRASILEIRO", 60,  50,  60,  60, {  0,156, 59}, {255,223,  0}, {  0,122, 52}, {  0, 60,170}, {  0, 60,170}, 5 },
    /* tacos de personagem novos */
    { "VAMPIRO",    65,  60,  55,  50, {220, 40, 60}, { 64, 14, 28}, { 20,  8, 18}, {210, 20, 44}, {110, 10, 26}, 6 },
    { "NOEL",       55,  60,  55,  65, {245,245,250}, {200, 32, 42}, { 16,104, 50}, {255,214, 80}, {245,245,250}, 7 },
    { "HACK",       50,  50,  80,  80, {  0,255,200}, { 24, 30, 40}, { 12, 16, 24}, {  0,255,140}, {  0,200,255}, 8 },
    { "MARRETA BIONICA", 60, 40, 55, 55, {255,214, 40}, {214, 36, 40}, {255,214, 40}, {255,214, 40}, {158, 22, 28}, 10 },
    /* MESTRE DO TEMPO: relogio azul e dourado; a habilidade volta no tempo se a tacada perder a partida */
    { "MESTRE DO TEMPO", 55, 50, 65, 65, {120,200,255}, { 28, 40, 84}, { 14, 18, 44}, {255,214, 90}, {120,200,255}, 1 },
    /* tacos de paises (fx 9: faixas com as cores da bandeira) */
    { "MEXICANO",   58,  52,  58,  57, {206, 17, 38}, {  0,104, 71}, {206, 17, 38}, {245,245,245}, {  0,104, 71}, 9 },
    { "ARGENTINO",  56,  56,  58,  58, {246,180, 14}, {117,170,219}, {245,245,245}, {246,180, 14}, {245,245,245}, 9 },
    { "AMERICANO",  70,  38,  50,  50, {255,255,255}, { 10, 49, 97}, {179, 25, 66}, {255,255,255}, {179, 25, 66}, 9 },
    { "ITALIANO",   55,  60,  55,  55, {206, 43, 55}, {  0,146, 70}, {206, 43, 55}, {250,250,250}, {  0,146, 70}, 9 },
    { "JAPONES",    48,  45,  67,  72, {188,  0, 45}, {245,245,245}, {188,  0, 45}, {188,  0, 45}, { 40, 40, 44}, 9 },
    /* GRINCH: pelo verde, ponta brilhante e um coracao vermelho que cresce 3 tamanhos; habilidade sorteada entre 5 poderes */
    { "GRINCH",     58,  62,  55,  60, {150,235, 90}, { 70,150, 50}, {190, 30, 40}, {255,230, 70}, {245,245,245}, 11 },
    /* TRIDENTE (Poseidon): azul profundo com ondas turquesa correndo pelo taco e detalhe dourado */
    { "TRIDENTE",   56,  64,  55,  60, {110,230,255}, { 20, 92,170}, { 10, 48,110}, {255,214, 90}, { 60,200,210}, 12 },
    /* tacos dos personagens que ainda nao tinham habilidade: ALIEN, ROBO, MAGO e GATO */
    { "OVNI",       52,  55,  60,  58, {120,255,150}, { 52, 96, 70}, { 24, 44, 36}, {170,255,120}, {120,255,150}, 13 },
    { "ROBO",       62,  35,  65,  65, {255,200, 60}, {150,158,170}, { 70, 76, 90}, {255,200, 60}, { 40, 44, 54}, 14 },
    { "VARINHA",    48,  70,  55,  55, {255,240,150}, { 80, 40,130}, { 40, 20, 76}, {255,214, 90}, {160, 90,230}, 15 },
    { "GATO",       52,  58,  55,  62, {255,150,190}, {255,184,210}, {120, 70,100}, {255,240,250}, {255,120,170}, 16 },
    /* COVEIRO (lendario): o taco e uma pa (lamina de aco, cabo de madeira e punho em T) com ectoplasma verde */
    { "COVEIRO",    58,  55,  58,  60, {150,255,170}, {128, 88, 50}, { 96, 64, 36}, {176,184,192}, { 60, 70, 64}, 17 },
    /* CASSETETE (Policial, lendario): azul-marinho com detalhe dourado e a sirene vermelha/azul piscando na ponta */
    { "CASSETETE",  60,  50,  60,  62, { 70,140,255}, { 26, 40, 92}, { 16, 22, 52}, {255,205, 50}, {220, 40, 52}, 18 },
};

/* habilidade de cada taco (0 = nenhuma). Taco de personagem: GELO-Pinguim, PISTOLEIRO-Cowboy, VAMPIRO-Vampiro,
   NOEL-Papai Noel, HACK-Hacker, DRAGAO-Viking, SAMURAI-Ninja, OCEANO-Pirata, MARRETA BIONICA-Chapolin */
static const int CUE_AB[NCUES] = { [6] = AB_FOGO, [7] = AB_CONGELAR, [10] = AB_CORTE, [11] = AB_MARE,
                                   [12] = AB_TIRO, [14] = AB_SUGAR, [15] = AB_CHAMINE, [16] = AB_HACK, [17] = AB_MARRETA, [18] = AB_TEMPO, [24] = AB_GRINCH, [25] = AB_MAREALTA,
                                   [26] = AB_RASTREIO, [27] = AB_IMA, [28] = AB_MALDICAO, [29] = AB_VIDAS, [30] = AB_COVEIRO, [31] = AB_POLICIAL };   /* OVNI-Alien, ROBO-Robo, VARINHA-Mago, GATO-Gato */

#define GAME_VERSION "3.1.2"
#define GAME_AUTHOR  "DIEGO REIS"

/* dificuldade do BOT: facil / medio / dificil */
#define NLEVELS 5
static int botLevel = 1, menuT;
static int sndOn = 1, musOn = 1, volLvl = 8, fxOn = 1, tipsOn = 1;   /* opcoes do menu de pausa */
static int sfxMute;                                                    /* > 0 durante simulacoes (HACK): sem som */
#define SFX(id, v) do { if (sndOn && !sfxMute) sfxPlay((id), (v)); } while (0)   /* efeito sonoro (opcao SOM) */
static const char  *LVL_NAME[NLEVELS]  = { "FACIL", "MEDIO", "DIFICIL", "PROFISSIONAL", "MESTRE" };
static const float  BOT_ERR[NLEVELS]   = { 0.075f, 0.02f, 0.004f, 0.0006f, 0.0006f };  /* erro de mira (rad) */
static const float  BOT_POWV[NLEVELS]  = { 0.20f, 0.06f, 0.0f, 0.0f, 0.0f };        /* erro de forca */
static const int    BOT_MISS[NLEVELS]  = { 30, 0, 0, 0, 0 };                      /* % de jogadas ruins */
static const int    BOT_DELAY[NLEVELS] = { 75, 40, 22, 12, 12 };                   /* tempo para pensar */

static unsigned int rs = 2463534242u;
static unsigned int rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

#define COL(r,gg,bb) GS_SETREG_RGBAQ((r),(gg),(bb),0x80,0x00)

/* ---------- desenho ---------- */
static void rect(float x1, float y1, float x2, float y2, u64 c)
{ gsKit_prim_sprite(g, x1+OX, y1+OY, x2+OX, y2+OY, 1, c); }

static void line(float x1, float y1, float x2, float y2, u64 c)
{ gsKit_prim_line(g, x1+OX, y1+OY, x2+OX, y2+OY, 1, c); }

/* chaves para testar no PS2: ponha 0 para voltar ao comportamento antigo (ajuda a achar a causa de um travamento) */
#ifndef USE_NEW_FONT
#define USE_NEW_FONT 1      /* 0 = usa a fonte antiga do gsKit */
#endif
#ifndef FAST_DISC
#define FAST_DISC 1         /* 0 = discos com todos os segmentos de antes (sem o corte para 60 FPS) */
#endif

/* tabela de cos/sin dos circulos (calculada uma vez por numero de lados): o EE nao tem seno/cosseno em hardware,
   e o jogo desenha milhares de segmentos por quadro. t[2*i] = cos, t[2*i+1] = sin, i de 0 a n */
#define CIRC_MAX 64
static float circTab[CIRC_MAX + 1][2 * (CIRC_MAX + 1)];
static unsigned char circOk[CIRC_MAX + 1];
static const float *circ(int n)
{
    if (n < 1 || n > CIRC_MAX) return NULL;
    if (!circOk[n]) {
        int i;
        for (i = 0; i <= n; i++) {
            float a = i * 6.2831853f / n;
            circTab[n][2 * i] = cosf(a); circTab[n][2 * i + 1] = sinf(a);
        }
        circOk[n] = 1;
    }
    return circTab[n];
}

static void discN(float cx, float cy, float r, u64 c, int n)
{
    int i, cap = (int)(3.0f * sqrtf(fabsf(r)) + 3.0f);   /* 60 FPS: segmentos so ate onde o olho ve (erro < 0.4 px); discos de 1 px usavam 14 triangulos */
    const float *t;
#if FAST_DISC
    if (fabsf(r) < 1.3f) {                              /* disco de 1-2 px (botoes, neve, estrelas): um quadradinho basta */
        gsKit_prim_sprite(g, cx+OX-r, cy+OY-r, cx+OX+r, cy+OY+r, 1, c);
        return;
    }
    if (cap < 4) cap = 4;
    if (n > cap) n = cap;
#endif
    t = circ(n);
    for (i = 0; i < n; i++) {
        float c0, s0, c1, s1;
        if (t) { c0 = t[2*i]; s0 = t[2*i+1]; c1 = t[2*i+2]; s1 = t[2*i+3]; }
        else {
            float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n;
            c0 = cosf(a0); s0 = sinf(a0); c1 = cosf(a1); s1 = sinf(a1);
        }
        gsKit_prim_triangle(g, cx+OX, cy+OY,
                            cx+OX+c0*r, cy+OY+s0*r,
                            cx+OX+c1*r, cy+OY+s1*r, 1, c);
    }
}

static void disc(float cx, float cy, float r, u64 c) { discN(cx, cy, r, c, 14); }

static void quad(float x1, float y1, float x2, float y2,
                 float x3, float y3, float x4, float y4, u64 c)
{
    gsKit_prim_triangle(g, x1+OX, y1+OY, x2+OX, y2+OY, x3+OX, y3+OY, 1, c);
    gsKit_prim_triangle(g, x1+OX, y1+OY, x3+OX, y3+OY, x4+OX, y4+OY, 1, c);
}

static void ring(float cx, float cy, float r, u64 c)
{
    int i, n = 16;
    const float *t = circ(n);
    for (i = 0; i < n; i++)
        line(cx+t[2*i]*r, cy+t[2*i+1]*r, cx+t[2*i+2]*r, cy+t[2*i+3]*r, c);
}

/* ---------- fonte nitida: atlas antialiasing (4 bits) + contorno escuro, no lugar da fonte padrao do gsKit ---------- */
/* cada letra: celula 16x24 (avanco 12, altura das maiusculas 13). Gerada de uma fonte monoespacada em negrito.
   O contorno e calculado na inicializacao (dilatacao 3x3). Letras minusculas saem em maiusculas. */
#define FT_CW 16
#define FT_CH 24
#define FT_N  64                              /* caracteres 32..95 */
#define FT_AW 256
#define FT_AH 256
static const unsigned char FNT4[FT_N * FT_CW * FT_CH / 2] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,
0x00,0x00,0x00,0x5f,0xf5,0x00,0x00,0x00,0x00,0x00,0x00,0x5f,0xf5,0x00,0x00,0x00,0x00,0x00,0x00,0x4f,0xf4,0x00,0x00,0x00,
0x00,0x00,0x00,0x3f,0xf4,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0xf3,0x00,0x00,0x00,0x00,0x00,0x00,0x2f,0xf3,0x00,0x00,0x00,
0x00,0x00,0x00,0x2f,0xf2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,
0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x8f,0xf1,0x1f,0xf8,0x00,0x00,
0x00,0x00,0x7f,0xf0,0x0f,0xf7,0x00,0x00,0x00,0x00,0x6f,0xe0,0x0e,0xf6,0x00,0x00,0x00,0x00,0x5f,0xd0,0x0e,0xf5,0x00,0x00,
0x00,0x00,0x4f,0xd0,0x0d,0xf4,0x00,0x00,0x00,0x00,0x4f,0xc0,0x0c,0xf3,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xcb,0x00,0x4f,0x30,0x00,0x00,0x00,0x01,0xf7,0x00,0x8e,0x00,0x00,
0x00,0x00,0x04,0xf3,0x00,0xcb,0x00,0x00,0x00,0x0e,0xff,0xff,0xff,0xff,0xf6,0x00,0x00,0x06,0x7d,0xd7,0x79,0xf9,0x73,0x00,
0x00,0x00,0x0e,0x80,0x06,0xf1,0x00,0x00,0x00,0x00,0x2f,0x60,0x09,0xd0,0x00,0x00,0x00,0x00,0x5f,0x30,0x0c,0xb0,0x00,0x00,
0x00,0x6f,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x37,0xdd,0x77,0x9f,0x97,0x60,0x00,0x00,0x00,0xe8,0x00,0x7f,0x10,0x00,0x00,
0x00,0x02,0xf5,0x00,0xad,0x00,0x00,0x00,0x00,0x06,0xf2,0x00,0xda,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x08,0x80,0x00,0x00,0x00,0x00,0x00,0x3a,0xdf,0xfd,0x92,0x00,0x00,0x00,0x04,0xff,0xfe,0xff,0xfe,0x20,0x00,
0x00,0x0b,0xfe,0x28,0x86,0xff,0x90,0x00,0x00,0x0d,0xfb,0x08,0x80,0xbc,0x90,0x00,0x00,0x0a,0xff,0x68,0x80,0x00,0x00,0x00,
0x00,0x03,0xff,0xff,0xd8,0x30,0x00,0x00,0x00,0x00,0x4c,0xff,0xff,0xfc,0x10,0x00,0x00,0x00,0x00,0x2a,0xdd,0xff,0xc0,0x00,
0x00,0x00,0x00,0x08,0x80,0x9f,0xf2,0x00,0x00,0x2b,0xd5,0x08,0x80,0x5f,0xf3,0x00,0x00,0x0e,0xfd,0x48,0x93,0xcf,0xf1,0x00,
0x00,0x05,0xff,0xff,0xff,0xff,0x70,0x00,0x00,0x00,0x3a,0xdf,0xfd,0xa4,0x00,0x00,0x00,0x00,0x00,0x08,0x80,0x00,0x00,0x00,
0x00,0x00,0x00,0x08,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1a,0xed,0x70,0x00,0x0a,0xe2,0x00,0x00,0x9f,0x7a,0xf3,0x00,0x5f,0x60,0x00,
0x00,0xdc,0x03,0xf7,0x02,0xea,0x00,0x00,0x00,0xfb,0x02,0xf9,0x0b,0xd1,0x00,0x00,0x00,0xdc,0x03,0xf7,0x7f,0x40,0x00,0x00,
0x00,0x9f,0x8b,0xf6,0xf9,0x00,0x00,0x00,0x00,0x1a,0xed,0x6c,0xd7,0xde,0xa1,0x00,0x00,0x00,0x00,0x8f,0x6f,0xb7,0xf9,0x00,
0x00,0x00,0x04,0xf7,0x7f,0x30,0xce,0x00,0x00,0x00,0x1d,0xb0,0x8f,0x20,0xbf,0x00,0x00,0x00,0xae,0x20,0x7f,0x30,0xcd,0x00,
0x00,0x05,0xf5,0x00,0x3f,0xb8,0xf9,0x00,0x00,0x2e,0xa0,0x00,0x06,0xde,0xa1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0xbe,0xeb,0x30,0x00,0x00,0x00,0x00,0x2e,0xfa,0xbf,0xe1,0x00,0x00,
0x00,0x00,0x7f,0xa0,0x0d,0xf4,0x00,0x00,0x00,0x00,0x7f,0x90,0x2f,0xf2,0x00,0x00,0x00,0x00,0x3f,0xe8,0xef,0x70,0x00,0x00,
0x00,0x00,0x2e,0xff,0xc4,0x00,0x00,0x00,0x00,0x05,0xff,0xfe,0x10,0x4e,0x90,0x00,0x00,0x3f,0xf8,0xcf,0x90,0x8f,0x80,0x00,
0x00,0x9f,0xc0,0x2e,0xf6,0xef,0x30,0x00,0x00,0xbf,0xa0,0x05,0xff,0xfc,0x00,0x00,0x00,0x9f,0xe2,0x00,0xbf,0xfa,0x22,0x00,
0x00,0x2e,0xff,0xce,0xff,0xff,0xfe,0x00,0x00,0x02,0xad,0xfe,0xa3,0x2a,0xdb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x5f,0xf4,0x00,0x00,0x00,
0x00,0x00,0x00,0x4f,0xf4,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0xf3,0x00,0x00,0x00,0x00,0x00,0x00,0x2f,0xf2,0x00,0x00,0x00,
0x00,0x00,0x00,0x1f,0xf1,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xef,0xb0,0x00,0x00,
0x00,0x00,0x00,0x09,0xff,0x20,0x00,0x00,0x00,0x00,0x00,0x2f,0xf9,0x00,0x00,0x00,0x00,0x00,0x00,0x8f,0xf2,0x00,0x00,0x00,
0x00,0x00,0x00,0xef,0xc0,0x00,0x00,0x00,0x00,0x00,0x03,0xff,0x80,0x00,0x00,0x00,0x00,0x00,0x06,0xff,0x50,0x00,0x00,0x00,
0x00,0x00,0x08,0xff,0x30,0x00,0x00,0x00,0x00,0x00,0x09,0xff,0x20,0x00,0x00,0x00,0x00,0x00,0x09,0xff,0x10,0x00,0x00,0x00,
0x00,0x00,0x09,0xff,0x20,0x00,0x00,0x00,0x00,0x00,0x08,0xff,0x30,0x00,0x00,0x00,0x00,0x00,0x06,0xff,0x50,0x00,0x00,0x00,
0x00,0x00,0x03,0xff,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x8f,0xf3,0x00,0x00,0x00,
0x00,0x00,0x00,0x2f,0xf9,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0xff,0x20,0x00,0x00,0x00,0x00,0x00,0x01,0xef,0xb0,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0xfe,0x10,0x00,0x00,0x00,
0x00,0x00,0x02,0xff,0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x9f,0xf2,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0xf8,0x00,0x00,0x00,
0x00,0x00,0x00,0x0c,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0xff,0x30,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0x60,0x00,0x00,
0x00,0x00,0x00,0x03,0xff,0x80,0x00,0x00,0x00,0x00,0x00,0x02,0xff,0x90,0x00,0x00,0x00,0x00,0x00,0x01,0xff,0x90,0x00,0x00,
0x00,0x00,0x00,0x02,0xff,0x90,0x00,0x00,0x00,0x00,0x00,0x03,0xff,0x80,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0x60,0x00,0x00,
0x00,0x00,0x00,0x08,0xff,0x30,0x00,0x00,0x00,0x00,0x00,0x0c,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0xf8,0x00,0x00,0x00,
0x00,0x00,0x00,0x9f,0xf2,0x00,0x00,0x00,0x00,0x00,0x02,0xff,0x90,0x00,0x00,0x00,0x00,0x00,0x0b,0xfe,0x10,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xf1,0x00,0x00,0x00,
0x00,0x00,0x00,0x0e,0xe0,0x00,0x00,0x00,0x00,0x00,0x5d,0x9e,0xe9,0xd5,0x00,0x00,0x00,0x00,0xae,0xff,0xfe,0xea,0x00,0x00,
0x00,0x00,0x01,0xdf,0xfd,0x10,0x00,0x00,0x00,0x00,0x1c,0xf7,0x7f,0xc1,0x00,0x00,0x00,0x00,0x05,0xb0,0x0b,0x50,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2f,0xf1,0x00,0x00,0x00,
0x00,0x00,0x00,0x2f,0xf1,0x00,0x00,0x00,0x00,0x00,0x00,0x2f,0xf1,0x00,0x00,0x00,0x00,0x00,0x00,0x2f,0xf1,0x00,0x00,0x00,
0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x03,0x33,0x4f,0xf4,0x33,0x30,0x00,
0x00,0x00,0x00,0x2f,0xf1,0x00,0x00,0x00,0x00,0x00,0x00,0x2f,0xf1,0x00,0x00,0x00,0x00,0x00,0x00,0x2f,0xf1,0x00,0x00,0x00,
0x00,0x00,0x00,0x03,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xe1,0x00,0x00,0x00,
0x00,0x00,0x04,0xff,0x70,0x00,0x00,0x00,0x00,0x00,0x08,0xfe,0x10,0x00,0x00,0x00,0x00,0x00,0x0c,0xf7,0x00,0x00,0x00,0x00,
0x00,0x00,0x1f,0xe1,0x00,0x00,0x00,0x00,0x00,0x00,0x5f,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0xff,0xff,0xc0,0x00,0x00,
0x00,0x00,0x0c,0xff,0xff,0xc0,0x00,0x00,0x00,0x00,0x05,0x66,0x66,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,
0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xc0,0x00,
0x00,0x00,0x00,0x00,0x00,0xdf,0x50,0x00,0x00,0x00,0x00,0x00,0x07,0xfc,0x00,0x00,0x00,0x00,0x00,0x00,0x1e,0xf4,0x00,0x00,
0x00,0x00,0x00,0x00,0x8f,0xb0,0x00,0x00,0x00,0x00,0x00,0x01,0xef,0x30,0x00,0x00,0x00,0x00,0x00,0x09,0xfa,0x00,0x00,0x00,
0x00,0x00,0x00,0x2f,0xf2,0x00,0x00,0x00,0x00,0x00,0x00,0xaf,0x90,0x00,0x00,0x00,0x00,0x00,0x03,0xfe,0x10,0x00,0x00,0x00,
0x00,0x00,0x0b,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x4f,0xe1,0x00,0x00,0x00,0x00,0x00,0x00,0xcf,0x70,0x00,0x00,0x00,0x00,
0x00,0x05,0xfd,0x10,0x00,0x00,0x00,0x00,0x00,0x0d,0xf6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xce,0xec,0x60,0x00,0x00,0x00,0x00,0x8f,0xff,0xff,0xf8,0x00,0x00,
0x00,0x02,0xff,0xc4,0x4c,0xff,0x20,0x00,0x00,0x08,0xff,0x40,0x04,0xff,0x70,0x00,0x00,0x0b,0xff,0x00,0x01,0xff,0xa0,0x00,
0x00,0x0c,0xfe,0x1f,0xf1,0xef,0xc0,0x00,0x00,0x0d,0xfe,0x1f,0xf1,0xef,0xc0,0x00,0x00,0x0c,0xfe,0x00,0x00,0xef,0xc0,0x00,
0x00,0x0b,0xff,0x00,0x01,0xff,0xa0,0x00,0x00,0x07,0xff,0x40,0x04,0xff,0x70,0x00,0x00,0x02,0xff,0xc4,0x4d,0xfe,0x20,0x00,
0x00,0x00,0x7f,0xff,0xff,0xf6,0x00,0x00,0x00,0x00,0x05,0xce,0xec,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2e,0xff,0x00,0x00,0x00,0x00,0x00,0x27,0xef,0xff,0x00,0x00,0x00,
0x00,0x0a,0xff,0xed,0xff,0x00,0x00,0x00,0x00,0x0a,0xea,0x2b,0xff,0x00,0x00,0x00,0x00,0x01,0x00,0x0b,0xff,0x00,0x00,0x00,
0x00,0x00,0x00,0x0b,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0xff,0x00,0x00,0x00,
0x00,0x00,0x00,0x0b,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0xff,0x00,0x00,0x00,0x00,0x00,0x11,0x1b,0xff,0x11,0x10,0x00,
0x00,0x08,0xff,0xff,0xff,0xff,0xf2,0x00,0x00,0x08,0xff,0xff,0xff,0xff,0xf2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0xde,0xec,0x71,0x00,0x00,0x00,0x02,0xdf,0xff,0xff,0xfd,0x10,0x00,
0x00,0x07,0xef,0x83,0x4b,0xff,0x70,0x00,0x00,0x00,0x00,0x00,0x03,0xff,0x90,0x00,0x00,0x00,0x00,0x00,0x08,0xff,0x60,0x00,
0x00,0x00,0x00,0x00,0x6f,0xfd,0x00,0x00,0x00,0x00,0x00,0x09,0xff,0xd2,0x00,0x00,0x00,0x00,0x01,0xbf,0xfb,0x10,0x00,0x00,
0x00,0x00,0x1c,0xff,0x80,0x00,0x00,0x00,0x00,0x00,0xbf,0xf6,0x00,0x00,0x00,0x00,0x00,0x07,0xff,0xb4,0x44,0x44,0x30,0x00,
0x00,0x0c,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x0c,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0xce,0xed,0x81,0x00,0x00,0x00,0x02,0xef,0xff,0xff,0xfd,0x10,0x00,
0x00,0x0a,0xff,0x93,0x4b,0xff,0x80,0x00,0x00,0x04,0x56,0x00,0x03,0xff,0x90,0x00,0x00,0x00,0x00,0x00,0x2a,0xff,0x40,0x00,
0x00,0x00,0x00,0x8f,0xff,0xc4,0x00,0x00,0x00,0x00,0x00,0x8f,0xff,0xb5,0x00,0x00,0x00,0x00,0x00,0x24,0x5b,0xff,0x60,0x00,
0x00,0x00,0x00,0x00,0x00,0xef,0xd0,0x00,0x00,0x0d,0xed,0x00,0x00,0xef,0xe0,0x00,0x00,0x0b,0xff,0xa4,0x5b,0xff,0xb0,0x00,
0x00,0x02,0xef,0xff,0xff,0xfe,0x30,0x00,0x00,0x00,0x18,0xce,0xfd,0x92,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xcf,0xf9,0x00,0x00,0x00,0x00,0x00,0x09,0xff,0xf9,0x00,0x00,
0x00,0x00,0x00,0x5f,0xdf,0xf9,0x00,0x00,0x00,0x00,0x02,0xef,0x5f,0xf9,0x00,0x00,0x00,0x00,0x0b,0xf9,0x1f,0xf9,0x00,0x00,
0x00,0x00,0x8f,0xc1,0x1f,0xf9,0x00,0x00,0x00,0x04,0xfe,0x20,0x1f,0xf9,0x00,0x00,0x00,0x1d,0xf6,0x00,0x1f,0xf9,0x00,0x00,
0x00,0x5f,0xff,0xff,0xff,0xff,0xf6,0x00,0x00,0x5f,0xff,0xff,0xff,0xff,0xf6,0x00,0x00,0x01,0x11,0x11,0x1f,0xf9,0x10,0x00,
0x00,0x00,0x00,0x00,0x1f,0xf9,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0xf9,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0xff,0xff,0xff,0xff,0x30,0x00,0x00,0x03,0xff,0xff,0xff,0xff,0x30,0x00,
0x00,0x04,0xff,0x61,0x11,0x11,0x00,0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x06,0xff,0x7c,0xee,0x92,0x00,0x00,
0x00,0x07,0xff,0xff,0xff,0xfe,0x20,0x00,0x00,0x08,0xff,0xb5,0x5c,0xff,0x90,0x00,0x00,0x00,0x00,0x00,0x01,0xff,0xd0,0x00,
0x00,0x00,0x00,0x00,0x00,0xef,0xe0,0x00,0x00,0x0a,0xdd,0x00,0x01,0xff,0xc0,0x00,0x00,0x08,0xff,0xa4,0x4c,0xff,0x70,0x00,
0x00,0x01,0xdf,0xff,0xff,0xfb,0x00,0x00,0x00,0x00,0x18,0xce,0xec,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0xae,0xec,0x70,0x00,0x00,0x00,0x00,0x5f,0xff,0xff,0xf8,0x00,0x00,
0x00,0x01,0xef,0xe6,0x4c,0xff,0x20,0x00,0x00,0x06,0xff,0x70,0x03,0xeb,0x40,0x00,0x00,0x09,0xff,0x20,0x00,0x00,0x00,0x00,
0x00,0x0b,0xff,0x3b,0xee,0xb3,0x00,0x00,0x00,0x0c,0xff,0xef,0xff,0xfe,0x30,0x00,0x00,0x0b,0xff,0xa2,0x18,0xff,0xa0,0x00,
0x00,0x09,0xff,0x20,0x00,0xef,0xd0,0x00,0x00,0x05,0xff,0x40,0x00,0xef,0xc0,0x00,0x00,0x01,0xdf,0xd4,0x3a,0xff,0x90,0x00,
0x00,0x00,0x4f,0xff,0xff,0xfd,0x10,0x00,0x00,0x00,0x03,0xae,0xfd,0x91,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0xff,0xff,0xff,0xff,0xa0,0x00,0x00,0x0b,0xff,0xff,0xff,0xff,0xa0,0x00,
0x00,0x03,0x44,0x44,0x46,0xff,0x60,0x00,0x00,0x00,0x00,0x00,0x1d,0xfb,0x00,0x00,0x00,0x00,0x00,0x00,0x9f,0xe2,0x00,0x00,
0x00,0x00,0x00,0x04,0xff,0x70,0x00,0x00,0x00,0x00,0x00,0x0d,0xfd,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,
0x00,0x00,0x00,0xcf,0xe1,0x00,0x00,0x00,0x00,0x00,0x02,0xff,0xa0,0x00,0x00,0x00,0x00,0x00,0x06,0xff,0x60,0x00,0x00,0x00,
0x00,0x00,0x09,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0xce,0xfd,0x92,0x00,0x00,0x00,0x01,0xef,0xff,0xff,0xfe,0x20,0x00,
0x00,0x07,0xff,0x80,0x09,0xff,0x70,0x00,0x00,0x08,0xff,0x30,0x04,0xff,0x80,0x00,0x00,0x03,0xff,0xa1,0x1a,0xff,0x30,0x00,
0x00,0x00,0x5c,0xff,0xff,0xc4,0x00,0x00,0x00,0x00,0x7d,0xff,0xff,0xe7,0x00,0x00,0x00,0x07,0xff,0x70,0x07,0xff,0x70,0x00,
0x00,0x0c,0xfe,0x00,0x00,0xef,0xd0,0x00,0x00,0x0e,0xfe,0x00,0x00,0xff,0xe0,0x00,0x00,0x0a,0xff,0x80,0x07,0xff,0xa0,0x00,
0x00,0x02,0xef,0xff,0xff,0xfe,0x20,0x00,0x00,0x00,0x29,0xdf,0xfd,0x92,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0xdf,0xeb,0x50,0x00,0x00,0x00,0x01,0xcf,0xff,0xff,0xf7,0x00,0x00,
0x00,0x07,0xff,0xb3,0x5e,0xff,0x20,0x00,0x00,0x0c,0xff,0x10,0x06,0xff,0x70,0x00,0x00,0x0d,0xfe,0x00,0x02,0xff,0xa0,0x00,
0x00,0x0c,0xff,0x10,0x03,0xff,0xb0,0x00,0x00,0x08,0xff,0xa2,0x3c,0xff,0xc0,0x00,0x00,0x01,0xef,0xff,0xfe,0xef,0xb0,0x00,
0x00,0x00,0x2a,0xee,0xb3,0xff,0x90,0x00,0x00,0x00,0x00,0x00,0x03,0xff,0x50,0x00,0x00,0x05,0xde,0x73,0x5d,0xfd,0x00,0x00,
0x00,0x01,0xef,0xff,0xff,0xf4,0x00,0x00,0x00,0x00,0x29,0xdf,0xea,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,
0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x5f,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x5f,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x5f,0xf8,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x5f,0xfa,0x00,0x00,0x00,
0x00,0x00,0x00,0x9f,0xf2,0x00,0x00,0x00,0x00,0x00,0x00,0xdf,0xa0,0x00,0x00,0x00,0x00,0x00,0x02,0xff,0x20,0x00,0x00,0x00,
0x00,0x00,0x06,0xf9,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0xf2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x27,0xc0,0x00,
0x00,0x00,0x00,0x02,0x7c,0xff,0xf0,0x00,0x00,0x00,0x27,0xcf,0xff,0xfb,0x60,0x00,0x00,0x0b,0xff,0xff,0xb6,0x10,0x00,0x00,
0x00,0x0f,0xfc,0x72,0x00,0x00,0x00,0x00,0x00,0x0f,0xfc,0x72,0x00,0x00,0x00,0x00,0x00,0x0b,0xff,0xff,0xb6,0x10,0x00,0x00,
0x00,0x00,0x16,0xcf,0xff,0xfb,0x60,0x00,0x00,0x00,0x00,0x02,0x7c,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x27,0xc0,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,0x00,
0x00,0x03,0x33,0x33,0x33,0x33,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x22,0x22,0x22,0x22,0x20,0x00,
0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x72,0x00,0x00,0x00,0x00,0x00,
0x00,0x0f,0xff,0xc7,0x20,0x00,0x00,0x00,0x00,0x06,0xbf,0xff,0xfc,0x72,0x00,0x00,0x00,0x00,0x01,0x6c,0xff,0xff,0xb0,0x00,
0x00,0x00,0x00,0x00,0x27,0xcf,0xf0,0x00,0x00,0x00,0x00,0x00,0x27,0xcf,0xf0,0x00,0x00,0x00,0x01,0x6b,0xff,0xff,0xb0,0x00,
0x00,0x06,0xbf,0xff,0xfc,0x61,0x00,0x00,0x00,0x0f,0xff,0xc7,0x20,0x00,0x00,0x00,0x00,0x0c,0x72,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x29,0xdf,0xec,0x71,0x00,0x00,0x00,0x03,0xef,0xff,0xff,0xfe,0x20,0x00,
0x00,0x0d,0xff,0x94,0x4a,0xff,0xa0,0x00,0x00,0x2e,0xea,0x00,0x00,0xef,0xd0,0x00,0x00,0x00,0x00,0x00,0x03,0xff,0xa0,0x00,
0x00,0x00,0x00,0x00,0x4e,0xfe,0x20,0x00,0x00,0x00,0x00,0x07,0xff,0xc2,0x00,0x00,0x00,0x00,0x00,0x5f,0xfa,0x00,0x00,0x00,
0x00,0x00,0x00,0xbf,0xd0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xe0,0x00,0x00,0x00,
0x00,0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x8d,0xfd,0x71,0x00,0x00,
0x00,0x00,0x1c,0xd6,0x46,0xeb,0x00,0x00,0x00,0x00,0xbd,0x10,0x00,0x2e,0x70,0x00,0x00,0x05,0xf3,0x00,0x00,0x07,0xe0,0x00,
0x00,0x0b,0xb0,0x3c,0xe5,0xa7,0xf4,0x00,0x00,0x0f,0x52,0xea,0x4c,0xf5,0xd7,0x00,0x00,0x3f,0x29,0xd0,0x0a,0xf2,0xb8,0x00,
0x00,0x6e,0x0e,0x80,0x0a,0xe0,0xa9,0x00,0x00,0x7d,0x2f,0x50,0x0b,0xb0,0xb9,0x00,0x00,0x8c,0x4f,0x40,0x0e,0x90,0xc7,0x00,
0x00,0x8c,0x4f,0x40,0x2f,0x60,0xe5,0x00,0x00,0x7d,0x3f,0x50,0x8f,0x43,0xf1,0x00,0x00,0x5f,0x1d,0xb6,0xdc,0x7b,0xa0,0x00,
0x00,0x1f,0x44,0xdd,0x36,0xeb,0x10,0x00,0x00,0x0b,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0xf5,0x00,0x00,0x3b,0x00,0x00,
0x00,0x00,0x7f,0x95,0x59,0xf9,0x00,0x00,0x00,0x00,0x05,0xce,0xeb,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xdf,0xfd,0x00,0x00,0x00,0x00,0x00,0x03,0xff,0xff,0x30,0x00,0x00,
0x00,0x00,0x08,0xfd,0xdf,0x80,0x00,0x00,0x00,0x00,0x0d,0xf9,0x9f,0xd0,0x00,0x00,0x00,0x00,0x3f,0xf5,0x5f,0xf3,0x00,0x00,
0x00,0x00,0x8f,0xf1,0x1f,0xf8,0x00,0x00,0x00,0x00,0xdf,0xc0,0x0c,0xfd,0x00,0x00,0x00,0x03,0xff,0x80,0x08,0xff,0x30,0x00,
0x00,0x08,0xff,0xff,0xff,0xff,0x80,0x00,0x00,0x0d,0xff,0xff,0xff,0xff,0xd0,0x00,0x00,0x3f,0xfb,0x33,0x33,0xaf,0xf3,0x00,
0x00,0x8f,0xf5,0x00,0x00,0x5f,0xf8,0x00,0x00,0xdf,0xf1,0x00,0x00,0x1f,0xfd,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0xff,0xfe,0xb4,0x00,0x00,0x00,0x0a,0xff,0xff,0xff,0xff,0x40,0x00,
0x00,0x0a,0xff,0x52,0x39,0xff,0xb0,0x00,0x00,0x0a,0xff,0x30,0x02,0xff,0xb0,0x00,0x00,0x0a,0xff,0x30,0x07,0xff,0x70,0x00,
0x00,0x0a,0xff,0xff,0xff,0xd7,0x00,0x00,0x00,0x0a,0xff,0xff,0xff,0xea,0x20,0x00,0x00,0x0a,0xff,0x52,0x24,0xdf,0xe2,0x00,
0x00,0x0a,0xff,0x30,0x00,0x6f,0xf7,0x00,0x00,0x0a,0xff,0x30,0x00,0x6f,0xf8,0x00,0x00,0x0a,0xff,0x52,0x24,0xdf,0xf5,0x00,
0x00,0x0a,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x0a,0xff,0xff,0xfe,0xc7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xbe,0xfd,0x81,0x00,0x00,0x00,0x00,0xaf,0xff,0xff,0xfd,0x20,0x00,
0x00,0x06,0xff,0xe7,0x5a,0xff,0xb0,0x00,0x00,0x0c,0xff,0x50,0x00,0xbc,0x90,0x00,0x00,0x1f,0xfe,0x00,0x00,0x00,0x00,0x00,
0x00,0x3f,0xfc,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0xfb,0x00,0x00,0x00,0x00,0x00,0x00,0x2f,0xfc,0x00,0x00,0x00,0x00,0x00,
0x00,0x1f,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0xff,0x60,0x00,0x8d,0xb3,0x00,0x00,0x05,0xff,0xe7,0x48,0xff,0xd0,0x00,
0x00,0x00,0x9f,0xff,0xff,0xfe,0x30,0x00,0x00,0x00,0x05,0xbe,0xfd,0x92,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x02,0x33,0x32,0x10,0x00,0x00,0x00,0x00,0x0a,0xff,0xff,0xfc,0x50,0x00,0x00,0x00,0x0a,0xff,0xff,0xff,0xf9,0x00,0x00,
0x00,0x0a,0xff,0x42,0x6e,0xff,0x60,0x00,0x00,0x0a,0xff,0x30,0x04,0xff,0xc0,0x00,0x00,0x0a,0xff,0x30,0x00,0xdf,0xf1,0x00,
0x00,0x0a,0xff,0x30,0x00,0xbf,0xf2,0x00,0x00,0x0a,0xff,0x30,0x00,0xaf,0xf3,0x00,0x00,0x0a,0xff,0x30,0x00,0xbf,0xf2,0x00,
0x00,0x0a,0xff,0x30,0x00,0xef,0xe0,0x00,0x00,0x0a,0xff,0x30,0x06,0xff,0xb0,0x00,0x00,0x0a,0xff,0x64,0x8f,0xff,0x40,0x00,
0x00,0x0a,0xff,0xff,0xff,0xf7,0x00,0x00,0x00,0x0a,0xff,0xff,0xea,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x0a,0xff,0xff,0xff,0xff,0xc0,0x00,
0x00,0x0a,0xff,0x64,0x44,0x44,0x30,0x00,0x00,0x0a,0xff,0x30,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0x30,0x00,0x00,0x00,0x00,
0x00,0x0a,0xff,0xff,0xff,0xff,0x30,0x00,0x00,0x0a,0xff,0xff,0xff,0xff,0x30,0x00,0x00,0x0a,0xff,0x64,0x44,0x44,0x10,0x00,
0x00,0x0a,0xff,0x30,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0x30,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0x64,0x44,0x44,0x41,0x00,
0x00,0x0a,0xff,0xff,0xff,0xff,0xf3,0x00,0x00,0x0a,0xff,0xff,0xff,0xff,0xf3,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x06,0xff,0xff,0xff,0xff,0xc0,0x00,
0x00,0x06,0xff,0x94,0x44,0x44,0x30,0x00,0x00,0x06,0xff,0x80,0x00,0x00,0x00,0x00,0x00,0x06,0xff,0x80,0x00,0x00,0x00,0x00,
0x00,0x06,0xff,0xff,0xff,0xff,0x90,0x00,0x00,0x06,0xff,0xff,0xff,0xff,0x90,0x00,0x00,0x06,0xff,0x94,0x44,0x44,0x20,0x00,
0x00,0x06,0xff,0x80,0x00,0x00,0x00,0x00,0x00,0x06,0xff,0x80,0x00,0x00,0x00,0x00,0x00,0x06,0xff,0x80,0x00,0x00,0x00,0x00,
0x00,0x06,0xff,0x80,0x00,0x00,0x00,0x00,0x00,0x06,0xff,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xce,0xec,0x70,0x00,0x00,0x00,0x00,0xaf,0xff,0xff,0xfa,0x00,0x00,
0x00,0x06,0xff,0xe7,0x5c,0xff,0x50,0x00,0x00,0x0c,0xff,0x30,0x02,0xeb,0x60,0x00,0x00,0x1f,0xfd,0x00,0x00,0x00,0x00,0x00,
0x00,0x3f,0xfb,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0xfb,0x04,0xff,0xff,0xf0,0x00,0x00,0x2f,0xfc,0x04,0xff,0xff,0xf0,0x00,
0x00,0x0f,0xfe,0x01,0x22,0xaf,0xf0,0x00,0x00,0x0b,0xff,0x50,0x00,0x9f,0xf0,0x00,0x00,0x04,0xff,0xe7,0x58,0xef,0xf0,0x00,
0x00,0x00,0x8f,0xff,0xff,0xff,0x70,0x00,0x00,0x00,0x05,0xbe,0xfd,0x82,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0x40,0x03,0xff,0xa0,0x00,0x00,0x0a,0xff,0x40,0x03,0xff,0xa0,0x00,
0x00,0x0a,0xff,0x40,0x03,0xff,0xa0,0x00,0x00,0x0a,0xff,0x40,0x03,0xff,0xa0,0x00,0x00,0x0a,0xff,0x40,0x03,0xff,0xa0,0x00,
0x00,0x0a,0xff,0xff,0xff,0xff,0xa0,0x00,0x00,0x0a,0xff,0xff,0xff,0xff,0xa0,0x00,0x00,0x0a,0xff,0x86,0x68,0xff,0xa0,0x00,
0x00,0x0a,0xff,0x40,0x03,0xff,0xa0,0x00,0x00,0x0a,0xff,0x40,0x03,0xff,0xa0,0x00,0x00,0x0a,0xff,0x40,0x03,0xff,0xa0,0x00,
0x00,0x0a,0xff,0x40,0x03,0xff,0xa0,0x00,0x00,0x0a,0xff,0x40,0x03,0xff,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xff,0xff,0xff,0xff,0x70,0x00,0x00,0x07,0xff,0xff,0xff,0xff,0x70,0x00,
0x00,0x02,0x44,0x8f,0xf9,0x44,0x20,0x00,0x00,0x00,0x00,0x6f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf7,0x00,0x00,0x00,
0x00,0x00,0x00,0x6f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf7,0x00,0x00,0x00,
0x00,0x00,0x00,0x6f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xf7,0x00,0x00,0x00,0x00,0x02,0x44,0x8f,0xf9,0x44,0x20,0x00,
0x00,0x07,0xff,0xff,0xff,0xff,0x70,0x00,0x00,0x07,0xff,0xff,0xff,0xff,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xdf,0xff,0xfe,0x00,0x00,0x00,0x00,0x00,0xdf,0xff,0xfe,0x00,0x00,
0x00,0x00,0x00,0x34,0x4e,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x0e,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x0e,0xfe,0x00,0x00,
0x00,0x00,0x00,0x00,0x0e,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x0e,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x0e,0xfe,0x00,0x00,
0x00,0x09,0xcd,0x00,0x0e,0xfe,0x00,0x00,0x00,0x0c,0xff,0x30,0x1f,0xfd,0x00,0x00,0x00,0x06,0xff,0xc5,0xaf,0xf9,0x00,0x00,
0x00,0x00,0xcf,0xff,0xff,0xe2,0x00,0x00,0x00,0x00,0x18,0xdf,0xe9,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0x30,0x01,0xdf,0xf6,0x00,0x00,0x0a,0xff,0x30,0x0a,0xff,0x90,0x00,
0x00,0x0a,0xff,0x30,0x6f,0xfc,0x00,0x00,0x00,0x0a,0xff,0x32,0xef,0xe1,0x00,0x00,0x00,0x0a,0xff,0x4c,0xff,0x30,0x00,0x00,
0x00,0x0a,0xff,0xcf,0xf8,0x00,0x00,0x00,0x00,0x0a,0xff,0xff,0xfd,0x10,0x00,0x00,0x00,0x0a,0xff,0xfd,0xff,0x80,0x00,0x00,
0x00,0x0a,0xff,0xa1,0xef,0xf3,0x00,0x00,0x00,0x0a,0xff,0x30,0x7f,0xfc,0x00,0x00,0x00,0x0a,0xff,0x30,0x0d,0xff,0x60,0x00,
0x00,0x0a,0xff,0x30,0x04,0xff,0xe2,0x00,0x00,0x0a,0xff,0x30,0x00,0xaf,0xfa,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,
0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,
0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,
0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xe4,0x44,0x44,0x30,0x00,
0x00,0x00,0xef,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0xef,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0x70,0x06,0xff,0xf0,0x00,0x00,0x0f,0xff,0xb0,0x09,0xff,0xf0,0x00,
0x00,0x0f,0xfe,0xf0,0x0d,0xef,0xf0,0x00,0x00,0x0f,0xfb,0xf4,0x2f,0xbf,0xf0,0x00,0x00,0x0f,0xf8,0xf8,0x6f,0x9f,0xf0,0x00,
0x00,0x0f,0xf5,0xfb,0xaf,0x5f,0xf0,0x00,0x00,0x0f,0xf2,0xee,0xce,0x2f,0xf0,0x00,0x00,0x0f,0xf2,0xaf,0xfb,0x2f,0xf0,0x00,
0x00,0x0f,0xf2,0x7f,0xf7,0x2f,0xf0,0x00,0x00,0x0f,0xf2,0x3f,0xf3,0x2f,0xf0,0x00,0x00,0x0f,0xf2,0x00,0x00,0x2f,0xf0,0x00,
0x00,0x0f,0xf2,0x00,0x00,0x2f,0xf0,0x00,0x00,0x0f,0xf2,0x00,0x00,0x2f,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0xc0,0x00,0xcf,0xa0,0x00,0x00,0x0a,0xff,0xf3,0x00,0xcf,0xa0,0x00,
0x00,0x0a,0xff,0xf9,0x00,0xcf,0xa0,0x00,0x00,0x0a,0xfe,0xfe,0x00,0xcf,0xa0,0x00,0x00,0x0a,0xfb,0xef,0x50,0xcf,0xa0,0x00,
0x00,0x0a,0xfc,0x9f,0xb0,0xcf,0xa0,0x00,0x00,0x0a,0xfc,0x3f,0xf2,0xcf,0xa0,0x00,0x00,0x0a,0xfc,0x0c,0xf8,0xcf,0xa0,0x00,
0x00,0x0a,0xfc,0x06,0xfd,0xbf,0xa0,0x00,0x00,0x0a,0xfc,0x01,0xff,0xdf,0xa0,0x00,0x00,0x0a,0xfc,0x00,0xaf,0xff,0xa0,0x00,
0x00,0x0a,0xfc,0x00,0x4f,0xff,0xa0,0x00,0x00,0x0a,0xfc,0x00,0x0d,0xff,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xce,0xec,0x70,0x00,0x00,0x00,0x00,0xbf,0xff,0xff,0xfb,0x00,0x00,
0x00,0x07,0xff,0xd5,0x5d,0xff,0x70,0x00,0x00,0x0d,0xff,0x20,0x02,0xff,0xd0,0x00,0x00,0x2f,0xfc,0x00,0x00,0xcf,0xf2,0x00,
0x00,0x4f,0xfa,0x00,0x00,0xaf,0xf4,0x00,0x00,0x4f,0xfa,0x00,0x00,0xaf,0xf4,0x00,0x00,0x4f,0xfb,0x00,0x00,0xbf,0xf4,0x00,
0x00,0x2f,0xfd,0x00,0x00,0xdf,0xf2,0x00,0x00,0x0d,0xff,0x30,0x03,0xff,0xd0,0x00,0x00,0x07,0xff,0xd5,0x5d,0xff,0x70,0x00,
0x00,0x00,0xbf,0xff,0xff,0xfb,0x00,0x00,0x00,0x00,0x07,0xce,0xec,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0xff,0xfd,0xa3,0x00,0x00,0x00,0x0a,0xff,0xff,0xff,0xff,0x50,0x00,
0x00,0x0a,0xff,0x64,0x5a,0xff,0xe0,0x00,0x00,0x0a,0xff,0x30,0x00,0xdf,0xf2,0x00,0x00,0x0a,0xff,0x30,0x00,0xdf,0xf1,0x00,
0x00,0x0a,0xff,0x63,0x4a,0xff,0xc0,0x00,0x00,0x0a,0xff,0xff,0xff,0xfe,0x30,0x00,0x00,0x0a,0xff,0xff,0xfd,0x92,0x00,0x00,
0x00,0x0a,0xff,0x30,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0x30,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0x30,0x00,0x00,0x00,0x00,
0x00,0x0a,0xff,0x30,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xce,0xec,0x70,0x00,0x00,0x00,0x00,0xbf,0xff,0xff,0xfb,0x00,0x00,
0x00,0x07,0xff,0xd5,0x5d,0xff,0x70,0x00,0x00,0x0d,0xff,0x20,0x03,0xff,0xd0,0x00,0x00,0x2f,0xfc,0x00,0x00,0xcf,0xf2,0x00,
0x00,0x4f,0xfa,0x00,0x00,0xaf,0xf4,0x00,0x00,0x4f,0xfa,0x00,0x00,0xaf,0xf4,0x00,0x00,0x4f,0xfa,0x00,0x00,0xaf,0xf4,0x00,
0x00,0x2f,0xfc,0x00,0x00,0xcf,0xf2,0x00,0x00,0x0e,0xff,0x20,0x02,0xff,0xe0,0x00,0x00,0x08,0xff,0xc3,0x3c,0xff,0x80,0x00,
0x00,0x01,0xcf,0xff,0xff,0xfc,0x10,0x00,0x00,0x00,0x19,0xef,0xfe,0x91,0x00,0x00,0x00,0x00,0x00,0x2f,0xfd,0x00,0x00,0x00,
0x00,0x00,0x00,0x0b,0xff,0xa3,0x21,0x00,0x00,0x00,0x00,0x02,0xef,0xff,0xf6,0x00,0x00,0x00,0x00,0x00,0x2a,0xef,0xd5,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0xff,0xff,0xfe,0xa4,0x00,0x00,0x00,0x0a,0xff,0xff,0xff,0xff,0x70,0x00,
0x00,0x0a,0xff,0x64,0x49,0xff,0xf1,0x00,0x00,0x0a,0xff,0x30,0x00,0xbf,0xf4,0x00,0x00,0x0a,0xff,0x30,0x00,0xaf,0xf3,0x00,
0x00,0x0a,0xff,0x30,0x15,0xff,0xe1,0x00,0x00,0x0a,0xff,0xff,0xff,0xff,0x50,0x00,0x00,0x0a,0xff,0xff,0xff,0xd3,0x00,0x00,
0x00,0x0a,0xff,0x64,0xdf,0xf3,0x00,0x00,0x00,0x0a,0xff,0x30,0x5f,0xfc,0x00,0x00,0x00,0x0a,0xff,0x30,0x0b,0xff,0x60,0x00,
0x00,0x0a,0xff,0x30,0x03,0xff,0xe1,0x00,0x00,0x0a,0xff,0x30,0x00,0x8f,0xfa,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0xce,0xec,0x81,0x00,0x00,0x00,0x03,0xef,0xff,0xff,0xfd,0x10,0x00,
0x00,0x0c,0xff,0x61,0x2a,0xff,0x90,0x00,0x00,0x0e,0xfd,0x00,0x00,0xed,0xa0,0x00,0x00,0x0c,0xff,0x81,0x00,0x00,0x00,0x00,
0x00,0x04,0xff,0xff,0xc8,0x30,0x00,0x00,0x00,0x00,0x4b,0xff,0xff,0xfb,0x20,0x00,0x00,0x00,0x00,0x15,0x9e,0xff,0xd0,0x00,
0x00,0x00,0x00,0x00,0x01,0xcf,0xf4,0x00,0x00,0x5c,0xe6,0x00,0x00,0x7f,0xf5,0x00,0x00,0x3f,0xfe,0x73,0x36,0xef,0xf2,0x00,
0x00,0x07,0xff,0xff,0xff,0xff,0x70,0x00,0x00,0x00,0x39,0xde,0xfd,0xa3,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xff,0xff,0xff,0xff,0xf6,0x00,0x00,0x6f,0xff,0xff,0xff,0xff,0xf6,0x00,
0x00,0x14,0x44,0x9f,0xf9,0x44,0x41,0x00,0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,
0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,
0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,
0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0xff,0x20,0x02,0xff,0xb0,0x00,0x00,0x0b,0xff,0x20,0x02,0xff,0xb0,0x00,
0x00,0x0b,0xff,0x20,0x02,0xff,0xb0,0x00,0x00,0x0b,0xff,0x20,0x02,0xff,0xb0,0x00,0x00,0x0b,0xff,0x20,0x02,0xff,0xb0,0x00,
0x00,0x0b,0xff,0x20,0x02,0xff,0xb0,0x00,0x00,0x0b,0xff,0x20,0x02,0xff,0xb0,0x00,0x00,0x0b,0xff,0x20,0x02,0xff,0xb0,0x00,
0x00,0x0b,0xff,0x30,0x03,0xff,0xa0,0x00,0x00,0x09,0xff,0x50,0x06,0xff,0x80,0x00,0x00,0x05,0xff,0xd5,0x6e,0xff,0x40,0x00,
0x00,0x00,0xcf,0xff,0xff,0xfa,0x00,0x00,0x00,0x00,0x18,0xdf,0xec,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xcf,0xf3,0x00,0x00,0x2f,0xfc,0x00,0x00,0x7f,0xf7,0x00,0x00,0x7f,0xf7,0x00,
0x00,0x2f,0xfc,0x00,0x00,0xbf,0xf2,0x00,0x00,0x0b,0xff,0x20,0x01,0xff,0xc0,0x00,0x00,0x06,0xff,0x60,0x06,0xff,0x60,0x00,
0x00,0x01,0xff,0xb0,0x0a,0xff,0x10,0x00,0x00,0x00,0xbf,0xf1,0x0e,0xfb,0x00,0x00,0x00,0x00,0x5f,0xf5,0x4f,0xf6,0x00,0x00,
0x00,0x00,0x1e,0xf9,0x9f,0xf1,0x00,0x00,0x00,0x00,0x0a,0xfd,0xdf,0xa0,0x00,0x00,0x00,0x00,0x05,0xff,0xff,0x50,0x00,0x00,
0x00,0x00,0x00,0xef,0xfe,0x10,0x00,0x00,0x00,0x00,0x00,0x9f,0xfa,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0x90,0x00,0x00,0x09,0xfe,0x00,0x00,0xbf,0xb0,0x00,0x00,0x0b,0xfb,0x00,
0x00,0x9f,0xd0,0x00,0x00,0x0d,0xf9,0x00,0x00,0x7f,0xf0,0x00,0x00,0x0e,0xf7,0x00,0x00,0x4f,0xf2,0x1f,0xf2,0x1f,0xf4,0x00,
0x00,0x2f,0xf4,0x6f,0xf6,0x3f,0xf2,0x00,0x00,0x0e,0xf5,0xbf,0xfb,0x5f,0xe0,0x00,0x00,0x0c,0xf7,0xfe,0xef,0x7f,0xc0,0x00,
0x00,0x0a,0xfc,0xfa,0xaf,0xbf,0xa0,0x00,0x00,0x07,0xff,0xf6,0x6f,0xff,0x70,0x00,0x00,0x05,0xff,0xf2,0x2f,0xff,0x50,0x00,
0x00,0x03,0xff,0xd0,0x0d,0xff,0x30,0x00,0x00,0x00,0xff,0x90,0x09,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4f,0xfb,0x00,0x00,0xbf,0xf4,0x00,0x00,0x09,0xff,0x60,0x05,0xff,0x90,0x00,
0x00,0x01,0xdf,0xe1,0x1e,0xfd,0x10,0x00,0x00,0x00,0x4f,0xf9,0x9f,0xf3,0x00,0x00,0x00,0x00,0x09,0xff,0xff,0x80,0x00,0x00,
0x00,0x00,0x01,0xdf,0xfc,0x00,0x00,0x00,0x00,0x00,0x00,0xbf,0xfa,0x00,0x00,0x00,0x00,0x00,0x06,0xff,0xff,0x50,0x00,0x00,
0x00,0x00,0x2e,0xfd,0xcf,0xe1,0x00,0x00,0x00,0x00,0xbf,0xf4,0x3f,0xfa,0x00,0x00,0x00,0x06,0xff,0x90,0x09,0xff,0x50,0x00,
0x00,0x1e,0xfe,0x10,0x01,0xef,0xe1,0x00,0x00,0xaf,0xf5,0x00,0x00,0x5f,0xfa,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xaf,0xf4,0x00,0x00,0x5f,0xfa,0x00,0x00,0x2f,0xfc,0x00,0x00,0xcf,0xf2,0x00,
0x00,0x08,0xff,0x50,0x05,0xff,0x80,0x00,0x00,0x01,0xef,0xc0,0x0c,0xfe,0x10,0x00,0x00,0x00,0x6f,0xf5,0x5f,0xf6,0x00,0x00,
0x00,0x00,0x0d,0xfc,0xdf,0xd0,0x00,0x00,0x00,0x00,0x04,0xff,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0xbf,0xfb,0x00,0x00,0x00,
0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,
0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xff,0xff,0xff,0xff,0xb0,0x00,0x00,0x06,0xff,0xff,0xff,0xff,0xb0,0x00,
0x00,0x02,0x44,0x44,0x4d,0xff,0x50,0x00,0x00,0x00,0x00,0x00,0x8f,0xf9,0x00,0x00,0x00,0x00,0x00,0x04,0xff,0xc0,0x00,0x00,
0x00,0x00,0x00,0x2e,0xfe,0x20,0x00,0x00,0x00,0x00,0x00,0xcf,0xf5,0x00,0x00,0x00,0x00,0x00,0x08,0xff,0x90,0x00,0x00,0x00,
0x00,0x00,0x5f,0xfc,0x00,0x00,0x00,0x00,0x00,0x02,0xef,0xe2,0x00,0x00,0x00,0x00,0x00,0x0c,0xff,0x94,0x44,0x44,0x40,0x00,
0x00,0x2f,0xff,0xff,0xff,0xff,0xf2,0x00,0x00,0x2f,0xff,0xff,0xff,0xff,0xf2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0xff,0xf8,0x00,0x00,
0x00,0x00,0x05,0xff,0xdd,0xd7,0x00,0x00,0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,
0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,
0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,
0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,
0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,
0x00,0x00,0x05,0xff,0x40,0x00,0x00,0x00,0x00,0x00,0x05,0xff,0xdd,0xd7,0x00,0x00,0x00,0x00,0x05,0xff,0xff,0xf8,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0d,0xf6,0x00,0x00,0x00,0x00,0x00,
0x00,0x05,0xfd,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xcf,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x4f,0xe1,0x00,0x00,0x00,0x00,
0x00,0x00,0x0b,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0xfe,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0xaf,0x90,0x00,0x00,0x00,
0x00,0x00,0x00,0x2f,0xf2,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0xfa,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xef,0x30,0x00,0x00,
0x00,0x00,0x00,0x00,0x8f,0xb0,0x00,0x00,0x00,0x00,0x00,0x00,0x1e,0xf4,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xfc,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0xdf,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x6f,0xd0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x8f,0xff,0xff,0x50,0x00,0x00,
0x00,0x00,0x7d,0xdd,0xff,0x50,0x00,0x00,0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,
0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,
0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,
0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,
0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,
0x00,0x00,0x00,0x04,0xff,0x50,0x00,0x00,0x00,0x00,0x7d,0xdd,0xff,0x50,0x00,0x00,0x00,0x00,0x8f,0xff,0xff,0x50,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0xf9,0x00,0x00,0x00,0x00,0x00,0x06,0xfd,0xff,0x10,0x00,0x00,
0x00,0x00,0x0c,0xf6,0xbf,0x70,0x00,0x00,0x00,0x00,0x3f,0xe0,0x4f,0xd0,0x00,0x00,0x00,0x00,0xaf,0x80,0x0d,0xf5,0x00,0x00,
0x00,0x01,0xff,0x20,0x07,0xfb,0x00,0x00,0x00,0x08,0xfa,0x00,0x01,0xff,0x30,0x00,0x00,0x0e,0xf4,0x00,0x00,0x9f,0x90,0x00,
0x00,0x5f,0xd0,0x00,0x00,0x3f,0xe1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x01,0xee,0xee,0xee,0xee,0xee,0xee,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static u32 fontAtlas[FT_AW * FT_AH] __attribute__((aligned(64)));
static GSTEXTURE fontTex;
static int useFont = 0;

/* altura de leitura: textos bem pequenos ficavam ilegiveis; a letra fica mais alta (0.26 -> 0.31, 0.30 -> 0.34), mas a LARGURA
   de cada letra continua 24.5*s, igual a fonte antiga, entao nenhum texto/caixa dos menus muda de tamanho. De 0.5 para cima nao muda */
static float legH(float s) { return s < 0.5f ? 0.1f + 0.8f * s : s; }

static void initFontTex(void)
{
    static unsigned char al[FT_N][FT_CH][FT_CW];
    int gi, x, y, dx, dy;
#if !USE_NEW_FONT
    return;
#endif
    for (gi = 0; gi < FT_N; gi++)
        for (y = 0; y < FT_CH; y++)
            for (x = 0; x < FT_CW; x++) {
                int idx = gi * FT_CW * FT_CH + y * FT_CW + x;
                unsigned char by = FNT4[idx >> 1];
                al[gi][y][x] = (idx & 1) ? (by & 15) : (by >> 4);
            }
    for (gi = 0; gi < FT_N; gi++) {
        int gx = (gi % 16) * FT_CW, gy = (gi / 16) * FT_CH;
        for (y = 0; y < FT_CH; y++)
            for (x = 0; x < FT_CW; x++) {
                int v = al[gi][y][x], o = v;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++) {
                        int yy = y + dy, xx = x + dx;
                        if (yy < 0 || yy >= FT_CH || xx < 0 || xx >= FT_CW) continue;
                        if (al[gi][yy][xx] > o) o = al[gi][yy][xx];
                    }
                fontAtlas[(gy + y) * FT_AW + gx + x]      = 0x00ffffffu | ((u32)((v * 128 + 7) / 15) << 24);
                fontAtlas[(gy + 96 + y) * FT_AW + gx + x] = 0x00ffffffu | ((u32)((o * 128 + 7) / 15) << 24);
            }
    }
    memset(&fontTex, 0, sizeof(fontTex));
    fontTex.Width = FT_AW; fontTex.Height = FT_AH;
    fontTex.PSM = GS_PSM_CT32;
    fontTex.Mem = (void *)fontAtlas;
    fontTex.Filter = GS_FILTER_LINEAR;
    fontTex.Vram = gsKit_vram_alloc(g, gsKit_texture_size(FT_AW, FT_AH, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
    if (fontTex.Vram == GSKIT_ALLOC_ERROR) return;      /* sem VRAM: continua com a fonte padrao */
    gsKit_texture_upload(g, &fontTex);
    useFont = 1;
}

/* desenha o texto: avanco de 24.5*s por letra (como a fonte antiga), y = topo das maiusculas */
static void textRaw(float x, float y, float s, u64 c, const char *t)
{
    float A, kx, ky, qx, qy, ox, oy;
    int al, outl, pass;
    u64 olc;
    if (!useFont) { gsKit_fontm_print_scaled(g, fm, x + OX, y + OY, 2, s, c, (char *)t); return; }
    A = 24.5f * s; kx = A / 12.0f; ky = 24.5f * legH(s) / 12.0f;
    qx = FT_CW * kx; qy = FT_CH * ky; ox = 2.0f * kx; oy = 4.0f * ky;
    al = (int)((c >> 24) & 0xff);
    outl = al >= 0x50 && (int)(c & 0xff) + (int)((c >> 8) & 0xff) + (int)((c >> 16) & 0xff) >= 120;   /* sem contorno em texto apagado/escuro */
    olc = GS_SETREG_RGBAQ(0, 0, 0, (al * 4) / 5, 0x00);
    for (pass = outl ? 0 : 1; pass < 2; pass++) {          /* 1o todos os contornos, 2o as letras (contorno de uma letra nao come a vizinha) */
        const char *q = t;
        float cx = x;
        for (; *q; q++, cx += A) {
            int ch = (unsigned char)*q, gi, u, v;
            float px, py;
            if (ch >= 'a' && ch <= 'z') ch -= 32;
            if (ch <= ' ' || ch > 95) continue;
            gi = ch - 32; u = (gi % 16) * FT_CW; v = (gi / 16) * FT_CH;
            px = cx - ox + OX; py = y - oy + OY;
            if (pass == 0) {
                gsKit_prim_sprite_texture(g, &fontTex, px, py, (float)u, (float)(v + 96), px + qx, py + qy, (float)(u + FT_CW), (float)(v + 96 + FT_CH), 2, olc);
            } else {
                gsKit_prim_sprite_texture(g, &fontTex, px, py, (float)u, (float)v, px + qx, py + qy, (float)(u + FT_CW), (float)(v + FT_CH), 2, c);
            }
        }
    }
}

static void text(float x, float y, float s, u64 c, const char *t)
{ textRaw(x, y, s, c, t); }

/* fonte 3x5 para os numeros das bolas */
static const char *DIG[10] = {
    "111101101101111", "010110010010111", "111001111100111", "111001111001111",
    "101101111001001", "111100111001111", "111100111101111", "111001010010010",
    "111101111101111", "111101111001111"
};

static void drawDigit(int d, float x0, float y0, float p)
{
    int k;
    for (k = 0; k < 15; k++)
        if (DIG[d][k] == '1') {
            float px = x0 + (k % 3) * p, py = y0 + (k / 3) * p;
            rect(px, py, px + p, py + p, COL(15,15,15));
        }
}

static void drawNumber(int n, float cx, float cy, float r)
{
    disc(cx, cy, r * 0.66f, COL(250,250,245));
    if (n < 10) {
        float p = r * 0.21f;
        drawDigit(n, cx - 1.5f*p, cy - 2.5f*p, p);
    } else {
        float p = r * 0.14f;
        drawDigit(n / 10, cx - 3.5f*p, cy - 2.5f*p, p);
        drawDigit(n % 10, cx + 0.5f*p,  cy - 2.5f*p, p);
    }
}

static void drawShadow(int i)
{
    float z = b[i].z > 0.0f ? b[i].z : 0.0f;      /* no pulo a sombra fica no chao, menor e mais fraca */
    if (!b[i].in) return;
    discN(b[i].x + 3.6f + z * 0.5f, b[i].y + 4.2f + z * 0.7f, R * (1.10f - z * 0.012f),
          GS_SETREG_RGBAQ(0, 18, 8, z > 0.0f ? 0x0c : 0x14, 0x00), 12);
    discN(b[i].x + 2.4f + z * 0.5f, b[i].y + 3.0f + z * 0.7f, R * (0.96f - z * 0.010f),
          GS_SETREG_RGBAQ(0, 14, 6, z > 0.0f ? 0x16 : 0x24, 0x00), 12);
}

static u64 shadeA(const u8 *c, float f, int add, int a)
{
    int r = (int)(c[0]*f) + add, gg = (int)(c[1]*f) + add, bb = (int)(c[2]*f) + add;
    if (r > 255) r = 255;
    if (gg > 255) gg = 255;
    if (bb > 255) bb = 255;
    if (r < 0) r = 0;
    if (gg < 0) gg = 0;
    if (bb < 0) bb = 0;
    return GS_SETREG_RGBAQ(r, gg, bb, a, 0x00);
}

static u64 shade(const u8 *c, float f, int add) { return shadeA(c, f, add, 0x80); }

static void drawBallVec(int i, float x, float y, float r)
{
    static const u8 WHITE[3] = {250,250,250};
    int id = skinCol[i];
    const u8 *c = i == 0 ? WB_COL[wbSel[optWho()]] : (skinStripe[i] ? WHITE : BC[id]);
    float r2 = r * 0.93f;

    discN(x, y, r, shade(c, 0.55f, 0), 12);          /* borda escura */
    discN(x, y, r2, shade(c, 1.0f, 0), 12);          /* cor base */

    if (skinStripe[i]) {                              /* listra colorida */
        static const float DY[5] = {-0.55f, -0.28f, 0.0f, 0.28f, 0.55f};
        int k;
        for (k = 0; k < 4; k++) {
            float y1 = DY[k] * r2,   y2 = DY[k+1] * r2;
            float w1 = sqrtf(r2*r2 - y1*y1), w2 = sqrtf(r2*r2 - y2*y2);
            quad(x - w1, y + y1, x + w1, y + y1, x + w2, y + y2, x - w2, y + y2,
                 shade(BC[id], 1.0f, 0));
        }
    } else if (i > 0) {
        discN(x - r*0.12f, y - r*0.12f, r*0.62f, shade(c, 1.2f, 25), 8); /* luz */
    }

    if (skinNum[i]) drawNumber(i, x, y, r);

    discN(x - r*0.38f, y - r*0.40f, r*0.2f, COL(255,255,255), 6);       /* brilho */
}

/* ---------- bolas 3D com textura (giram ao rolar) ---------- */
#ifndef USE_TEXTURE_BALLS
#define USE_TEXTURE_BALLS 1   /* mude para 0 para voltar as bolas vetoriais */
#endif

#define SKW 128               /* largura da "pele" de cada bola */
#define SKH 64                /* altura da "pele" de cada bola */
#define ATLAS_W 512           /* 4 colunas x 4 linhas de peles */
#define ATLAS_H 256
#define SS 2                  /* supersampling da textura */
#define NLON 12               /* malha da esfera (16x8 ficava mais redonda mas quase dobrava os triangulos: nao segura 60 FPS) */
#define NLAT 6

static u32 atlas[ATLAS_W * ATLAS_H] __attribute__((aligned(64)));
static GSTEXTURE ballTex;
static int useTex = 0;
static float ballM[16][9];
static float cLon[SKW * SS], sLon[SKW * SS], cLat[SKH * SS], sLat[SKH * SS];
static float mP[NLAT + 1][NLON + 1][3];
static float mUV[NLAT + 1][NLON + 1][2];

static int digitInk(int n, float gu, float gv)
{
    float cs; int ix, iy;
    if (n < 10) {
        cs = 0.32f;
        ix = (int)floorf((gu + 1.5f*cs) / cs); iy = (int)floorf((gv + 2.5f*cs) / cs);
        if (ix < 0 || ix > 2 || iy < 0 || iy > 4) return 0;
        return DIG[n][iy*3 + ix] == '1';
    }
    cs = 0.21f;
    ix = (int)floorf((gu + 3.5f*cs) / cs); iy = (int)floorf((gv + 2.5f*cs) / cs);
    if (iy < 0 || iy > 4 || ix < 0 || ix > 6) return 0;
    if (ix < 3) return DIG[n / 10][iy*3 + ix] == '1';
    if (ix == 3) return 0;
    return DIG[n % 10][iy*3 + (ix - 4)] == '1';
}

/* caveirinha 7x6 para o estilo CAVEIRA: 1 osso, 2 buraco */
static int skullInk(float u, float v)
{
    static const char *SK[6] = { "0111110", "1111111", "1221221", "1111111", "0112110", "0101010" };
    int ix = (int)floorf((u + 1.0f) * 3.5f), iy = (int)floorf((v + 1.0f) * 3.0f);
    if (ix < 0 || ix > 6 || iy < 0 || iy > 5) return 0;
    return SK[iy][ix] - '0';
}

/* pele da bola (equirretangular): cor, listra e 4 numeros ao redor */
static void renderSkin(int id, int ox, int oy)
{
    int px, py, sx, sy, k;
    for (py = 0; py < SKH; py++)
    for (px = 0; px < SKW; px++) {
        float ar = 0, ag = 0, ab = 0, n = (float)(SS * SS);
        for (sy = 0; sy < SS; sy++)
        for (sx = 0; sx < SS; sx++) {
            int ii = px * SS + sx, jj = py * SS + sy;
            float x = cLat[jj] * cLon[ii], y = sLat[jj], z = cLat[jj] * sLon[ii];
            float cr = 245, cg = 245, cb = 238;
            if (id > 0) {
                int col = skinCol[id];
                cr = BC[col][0]; cg = BC[col][1]; cb = BC[col][2];
                if (skinStripe[id] && fabsf(y) > 0.55f) { cr = 245; cg = 245; cb = 238; }
                if (skinStyleEff == 2) {                             /* PLANETAS: faixas e anel */
                    float bd = 0.80f + 0.20f * sinf(y * 14.0f + id * 1.3f) + 0.08f * sinf(y * 33.0f + x * 6.0f);
                    if (fabsf(y) < 0.05f) bd = 1.35f;
                    cr *= bd; cg *= bd; cb *= bd;
                    cr = fminf(cr, 255.0f); cg = fminf(cg, 255.0f); cb = fminf(cb, 255.0f);
                } else if (skinStyleEff == 3 && (!skinStripe[id] || fabsf(y) <= 0.55f)) {   /* FRUTAS: sementes claras */
                    float ln = atan2f(z, x) * 0.95493f, lt = asinf(y) * 1.90986f, sx2, sy2;
                    sy2 = lt - floorf(lt) - 0.5f; sx2 = ln + floorf(lt) * 0.5f; sx2 = sx2 - floorf(sx2) - 0.5f;
                    if (sx2 * sx2 + sy2 * sy2 < 0.08f) { cr = 255; cg = 238; cb = 150; }
                }
                {
                    float d[4]  = { z, x, -z, -x };
                    float gu[4] = { x, -z, -x, z };
                    float gv = -y;
                    for (k = 0; k < 4; k++)
                        if (d[k] > 0 && gu[k]*gu[k] + gv*gv < (skinNum[id] ? 0.4356f : 0.016f)) {
                            cr = cg = cb = 248;
                            if (skinNum[id] && digitInk(id, gu[k] / 0.66f, gv / 0.66f)) cr = cg = cb = 20;
                            break;
                        }
                }
                if (skinStyleEff == 1 && skinNum[id]) {              /* CAVEIRA: caveirinhas entre os numeros */
                    float d2[4] = { z, x, -z, -x }, gu2[4] = { x, -z, -x, z }, gv2 = -y;
                    int k2;
                    for (k2 = 0; k2 < 4; k2++) {
                        float dd = (d2[k2] + d2[(k2 + 1) & 3]) * 0.70711f, uu = (gu2[k2] + gu2[(k2 + 1) & 3]) * 0.70711f;
                        if (dd > 0.0f && uu * uu + gv2 * gv2 < 0.045f) {
                            int sk = skullInk(uu / 0.21f, gv2 / 0.21f);
                            if (sk == 1) cr = cg = cb = 245; else if (sk == 2) cr = cg = cb = 12;
                            break;
                        }
                    }
                }
                if (skinStyleEff == 4) {                             /* BRILHO NO ESCURO: tons neon e branco esverdeado */
                    if (cr > 235 && cg > 235 && cb > 225) { cr = 175; cg = 255; cb = 195; }
                    else if (cr + cg + cb > 150) {
                        cr = cr * 1.2f + 20; cg = cg * 1.2f + 45; cb = cb * 1.2f + 20;
                        cr = fminf(cr, 255.0f); cg = fminf(cg, 255.0f); cb = fminf(cb, 255.0f);
                    }
                }
            }
            ar += cr; ag += cg; ab += cb;
        }
        atlas[(oy + py) * ATLAS_W + ox + px] =
            (u32)(ar / n) | ((u32)(ag / n) << 8) | ((u32)(ab / n) << 16) | (128u << 24);
    }
}

static void initBallTex(void)
{
    int i, a, c;
    skinDefaults();
    for (i = 0; i < SKW * SS; i++) {
        float l = (i + 0.5f) / (SKW * SS) * 6.2831853f;
        cLon[i] = cosf(l); sLon[i] = sinf(l);
    }
    for (i = 0; i < SKH * SS; i++) {
        float p = ((i + 0.5f) / (SKH * SS) - 0.5f) * 3.1415927f;
        cLat[i] = cosf(p); sLat[i] = sinf(p);
    }
    for (i = 0; i < 16; i++) renderSkin(i, (i % 4) * SKW, (i / 4) * SKH);

    for (a = 0; a <= NLAT; a++)
        for (c = 0; c <= NLON; c++) {
            float p = -1.5707963f + 3.1415927f * a / NLAT;
            float l = 6.2831853f * c / NLON;
            float u = (float)SKW * c / NLON, v = (float)SKH * a / NLAT;
            mP[a][c][0] = cosf(p) * cosf(l);
            mP[a][c][1] = sinf(p);
            mP[a][c][2] = cosf(p) * sinf(l);
            if (u < 0.5f) u = 0.5f; if (u > SKW - 0.5f) u = SKW - 0.5f;
            if (v < 0.5f) v = 0.5f; if (v > SKH - 0.5f) v = SKH - 0.5f;
            mUV[a][c][0] = u; mUV[a][c][1] = v;
        }

    memset(&ballTex, 0, sizeof(ballTex));
    ballTex.Width = ATLAS_W; ballTex.Height = ATLAS_H;
    ballTex.PSM = GS_PSM_CT32;
    ballTex.Mem = (void *)atlas;
    ballTex.Filter = GS_FILTER_LINEAR;
    ballTex.Vram = gsKit_vram_alloc(g, gsKit_texture_size(ATLAS_W, ATLAS_H, GS_PSM_CT32),
                                    GSKIT_ALLOC_USERBUFFER);
    if (ballTex.Vram == GSKIT_ALLOC_ERROR) return;
    gsKit_texture_upload(g, &ballTex);
    useTex = 1;
}

/* muda o visual das bolas (modo brasileiro: bolas lisas sem numero, na cor de cada jogador) */
static void setSkins(int br)
{
    int i, k, key = br ? 1 + colSel[0] * NBRC + colSel[1] : (ballTheme ? 5000 + ballTheme : 0) + ballStyle * 100000;
    if (key == skinKey) return;
    skinKey = key; skinStyleEff = br ? 0 : ballStyle;
    skinDefaults();
    for (i = 1; i <= 8; i++) memcpy(BC[i], br ? THEME[0][i - 1] : THEME[ballTheme][i - 1], 3);
    if (br)
        for (k = 0; k < 4; k++) {
            skinCol[1 + k] = BRC[colSel[0]]; skinStripe[1 + k] = 0; skinNum[1 + k] = 0;
            skinCol[9 + k] = BRC[colSel[1]]; skinStripe[9 + k] = 0; skinNum[9 + k] = 0;
        }
    if (useTex) {
        for (i = 1; i < 16; i++) renderSkin(i, (i % 4) * SKW, (i / 4) * SKH);
        gsKit_texture_upload(g, &ballTex);
    }
}

/* orientacao inicial da bola (giro aleatorio ao redor do eixo Z) */
static void resetOrient(int i)
{
    float a = (i == 0) ? 0.0f : (rnd() % 628) / 100.0f;
    float c = cosf(a), s = sinf(a);
    float *M = ballM[i];
    M[0] = c;  M[1] = -s; M[2] = 0;
    M[3] = s;  M[4] = c;  M[5] = 0;
    M[6] = 0;  M[7] = 0;  M[8] = 1;
}

/* rolamento sem deslizar: eixo perpendicular a velocidade */
static void spinBall(int i, float dt)
{
    float wx = b[i].vy / R, wy = b[i].vx / R;
    float w = sqrtf(wx*wx + wy*wy), th, ax, ay, c, s, t;
    float Rm[9], N[9], *M = ballM[i];
    int r, k, j;
    if (w < 0.0001f) return;
    th = w * dt; ax = wx / w; ay = wy / w;
    c = cosf(th); s = sinf(th); t = 1.0f - c;
    Rm[0] = t*ax*ax + c; Rm[1] = t*ax*ay;     Rm[2] = s*ay;
    Rm[3] = t*ax*ay;     Rm[4] = t*ay*ay + c; Rm[5] = -s*ax;
    Rm[6] = -s*ay;       Rm[7] = s*ax;        Rm[8] = c;
    for (r = 0; r < 3; r++)
        for (k = 0; k < 3; k++) {
            float sum = 0;
            for (j = 0; j < 3; j++) sum += Rm[r*3 + j] * M[j*3 + k];
            N[r*3 + k] = sum;
        }
    for (r = 0; r < 3; r++) {            /* mantem a matriz ortonormal */
        float l = sqrtf(N[r*3]*N[r*3] + N[r*3+1]*N[r*3+1] + N[r*3+2]*N[r*3+2]);
        if (l < 0.0001f) l = 1;
        M[r*3] = N[r*3] / l; M[r*3+1] = N[r*3+1] / l; M[r*3+2] = N[r*3+2] / l;
    }
}

static void drawBallMesh(int id, float x, float y, float r)
{
    const float *M = ballM[id];
    float sxp[NLAT + 1][NLON + 1], syp[NLAT + 1][NLON + 1];
    u64 colr[NLAT + 1][NLON + 1];
    float uo = (float)((id % 4) * SKW), vo = (float)((id / 4) * SKH);
    int a, c;
    const u8 *tn = (id == 0) ? WB_COL[wbSel[optWho()]] : NULL;      /* bola branca colorida: tinge pelo vertice */

    discN(x, y, r * 1.03f, GS_SETREG_RGBAQ(10, 40, 25, 0x50, 0x00), 16);  /* borda suave */
    if (skinStyleEff == 4 && id > 0) discN(x, y, r * 1.45f, GS_SETREG_RGBAQ(110, 255, 150, 0x16, 0x00), 14);   /* LOJA: brilho no escuro */

    for (a = 0; a <= NLAT; a++)
        for (c = 0; c <= NLON; c++) {
            const float *p = mP[a][c];
            float vx = M[0]*p[0] + M[1]*p[1] + M[2]*p[2];
            float vy = M[3]*p[0] + M[4]*p[1] + M[5]*p[2];
            float vz = M[6]*p[0] + M[7]*p[1] + M[8]*p[2];
            float dif = vx * -0.42f + vy * 0.52f + vz * 0.74f;
            int cv;
            if (dif < 0) dif = 0;
            cv = (int)((0.38f + 0.68f * dif) * 150.0f * (0.78f + 0.22f * (vz > 0.0f ? vz : 0.0f)));   /* borda da bola mais escura = volume (sem desenhar nada a mais) */
            if (cv > 255) cv = 255;
            sxp[a][c] = x + OX + vx * r;
            syp[a][c] = y + OY - vy * r;
            colr[a][c] = tn ? GS_SETREG_RGBAQ(cv * tn[0] / 255, cv * tn[1] / 255, cv * tn[2] / 255, 0x80, 0x00)
                            : GS_SETREG_RGBAQ(cv, cv, cv, 0x80, 0x00);
        }

    for (a = 0; a < NLAT; a++)
        for (c = 0; c < NLON; c++) {
            float x0 = sxp[a][c],     y0 = syp[a][c];
            float x1 = sxp[a][c+1],   y1 = syp[a][c+1];
            float x2 = sxp[a+1][c+1], y2 = syp[a+1][c+1];
            float x3 = sxp[a+1][c],   y3 = syp[a+1][c];
            if ((x1-x0)*(y2-y0) - (y1-y0)*(x2-x0) > 0)
                gsKit_prim_triangle_goraud_texture(g, &ballTex,
                    x0, y0, uo + mUV[a][c][0],     vo + mUV[a][c][1],
                    x1, y1, uo + mUV[a][c+1][0],   vo + mUV[a][c+1][1],
                    x2, y2, uo + mUV[a+1][c+1][0], vo + mUV[a+1][c+1][1],
                    1, colr[a][c], colr[a][c+1], colr[a+1][c+1]);
            if ((x2-x0)*(y3-y0) - (y2-y0)*(x3-x0) > 0)
                gsKit_prim_triangle_goraud_texture(g, &ballTex,
                    x0, y0, uo + mUV[a][c][0],     vo + mUV[a][c][1],
                    x2, y2, uo + mUV[a+1][c+1][0], vo + mUV[a+1][c+1][1],
                    x3, y3, uo + mUV[a+1][c][0],   vo + mUV[a+1][c][1],
                    1, colr[a][c], colr[a+1][c+1], colr[a+1][c]);
        }

    /* brilho da luz (fixo na tela), em 3 camadas: halo suave, brilho e ponto de luz */
    discN(x - r*0.30f, y - r*0.32f, r*0.46f, GS_SETREG_RGBAQ(255, 255, 255, 0x16, 0x00), 8);
    discN(x - r*0.37f, y - r*0.39f, r*0.24f, GS_SETREG_RGBAQ(255, 255, 255, 0x40, 0x00), 6);
    discN(x - r*0.41f, y - r*0.44f, r*0.11f, GS_SETREG_RGBAQ(255, 255, 255, 0x80, 0x00), 4);
}

static void drawBallAt(int i, float x, float y, float r)
{
    if (!useTex || r < R - 0.5f) { drawBallVec(i, x, y, r); return; }
    drawBallMesh(i, x, y, r);
}

/* bola caindo na cacapa: desliza ate o centro, encolhe e escurece */
static void drawFall(int i)
{
    float t = (float)fallT[i] / FALLN;            /* 1 -> 0 */
    float e = 1.0f - t * t;
    float gx = fallP[i] >= 6 ? TCX : PX[fallP[i]], gy = fallP[i] >= 6 ? TCY : PY[fallP[i]];   /* 6 = cova do coveiro */
    float x = fallX[i] + (gx - fallX[i]) * e;
    float y = fallY[i] + (gy - fallY[i]) * e;
    float r = R * (0.42f + 0.58f * t);
    if (useTex) drawBallMesh(i, x, y, r); else drawBallVec(i, x, y, r);
    discN(x, y, r * 1.02f, GS_SETREG_RGBAQ(0, 0, 0, (int)(0x78 * (1.0f - t)), 0x00), 10);
}

/* ---------- REPLAY da ultima bola encacapada antes da vitoria ----------
   Cada quadro da tacada e gravado (anel dos ultimos REPN quadros). Se a tacada termina a partida, o jogo
   mostra de novo os instantes ate a ultima bola cair (camera lenta perto da cacapa). X ou START pula. */
#define REPN 600                                      /* quadros guardados (10 s) */
#define REP_PRE 150                                   /* quadros mostrados antes da bola cair (2,5 s) */
#define REP_POST 70                                   /* quadros mostrados depois */
#define REP_SLOW 0.45f                                /* velocidade perto da cacapa (1.0 = normal) */
typedef struct { float x[16], y[16], z[16], fX[16], fY[16], M[16][9]; unsigned char in[16], fT[16], fP[16]; } RepFrame;
static RepFrame repBuf[REPN];
#define REPK (REP_PRE + REP_POST + 2)
static RepFrame repKeep[REPK];                        /* recorte da ULTIMA BOLA que caiu (fica guardado para o REPLAY do menu de pausa) */
static int repKeepN, repKeepPot;                      /* repKeepN: quadros guardados (0 = nada para rever); repKeepPot: quadro do recorte em que a bola caiu */
static int repFrame, repPotF = -1, repPotBall, repA, repB, repT, repHold, repSfx, repRet, repSaveMsgT;
static float repPos;
static Ball repSaveB[16];
static int repSaveFT[16], repSaveFP[16], repSavePk[16];
static float repSaveFX[16], repSaveFY[16], repSaveM[16][9];

/* contexto da partida (mesa, cores, personagens...) para rever o replay depois, pela tela de configurar */
typedef struct { int tableSel, tTable, tourney, gmode, mode, colSel[2], charSel[2], ballTheme, wbSel[2], gdSel[2]; } RepCtx;
static RepCtx repCtx, repCtxTmp;
static int repHave, repFromSetup;                     /* repHave: existe replay da partida anterior (so ate a proxima tacada) */
static void repCtxGet(RepCtx *c)
{
    c->tableSel = tableSel; c->tTable = tTable; c->tourney = tourney; c->gmode = gmode; c->mode = mode; c->ballTheme = ballTheme;
    memcpy(c->colSel, colSel, sizeof colSel); memcpy(c->charSel, charSel, sizeof charSel);
    memcpy(c->wbSel, wbSel, sizeof wbSel); memcpy(c->gdSel, gdSel, sizeof gdSel);
}
static void repCtxSet(const RepCtx *c)
{
    tableSel = c->tableSel; tTable = c->tTable; tourney = c->tourney; gmode = c->gmode; mode = c->mode; ballTheme = c->ballTheme;
    memcpy(colSel, c->colSel, sizeof colSel); memcpy(charSel, c->charSel, sizeof charSel);
    memcpy(wbSel, c->wbSel, sizeof wbSel); memcpy(gdSel, c->gdSel, sizeof gdSel);
}

static void repReset(void) { repFrame = 0; repPotF = -1; repHave = 0; }

static void repRecord(void)                           /* chamada 1x por quadro enquanto as bolas rolam */
{
    RepFrame *f = &repBuf[repFrame % REPN];
    int i;
    for (i = 0; i < 16; i++) {
        f->x[i] = b[i].x; f->y[i] = b[i].y; f->z[i] = b[i].z; f->in[i] = (unsigned char)b[i].in;
        f->fT[i] = (unsigned char)fallT[i]; f->fP[i] = (unsigned char)fallP[i]; f->fX[i] = fallX[i]; f->fY[i] = fallY[i];
        memcpy(f->M[i], ballM[i], sizeof f->M[i]);
        if (fallT[i] == FALLN) { repPotF = repFrame; repPotBall = i; }     /* bola acabou de cair: guarda o quadro */
    }
    repFrame++;
}

static void repApply(int fi)
{
    RepFrame *f = &repKeep[fi];
    int i;
    for (i = 0; i < 16; i++) {
        b[i].x = f->x[i]; b[i].y = f->y[i]; b[i].z = f->z[i]; b[i].in = f->in[i]; b[i].vx = b[i].vy = 0.0f; b[i].vz = 0.0f;
        fallT[i] = f->fT[i]; fallP[i] = f->fP[i]; fallX[i] = f->fX[i]; fallY[i] = f->fY[i];
        memcpy(ballM[i], f->M[i], sizeof f->M[i]);
    }
}

static void repLaunch(int ret);
static void repMaybeStart(void)                       /* chamar logo depois de resolve(): guarda a ultima bola que caiu; no fim da partida ja mostra o replay */
{
    int i, a, bb, lo, over = (state == S_OVER);
    if (repFrame < 2 || repPotF < 0) return;             /* sem quadros gravados ou nenhuma bola caiu nesta tacada (o replay anterior continua guardado) */
    lo = repFrame - REPN; if (lo < 0) lo = 0;
    a = repPotF - REP_PRE;  if (a < lo) a = lo;
    bb = repPotF + REP_POST; if (bb > repFrame - 1) bb = repFrame - 1;
    if (bb - a < 20 || bb - a + 1 > REPK) return;
    for (i = 0; i <= bb - a; i++) repKeep[i] = repBuf[(a + i) % REPN];
    repKeepN = bb - a + 1; repKeepPot = repPotF - a;
    if (over) { repHave = 1; repCtxGet(&repCtx); repLaunch(state); }   /* fim de partida: mostra na hora e guarda para rever na tela de configurar */
}

static void repLaunch(int ret)                        /* guarda o estado atual e comeca o replay (ret = estado para onde voltar) */
{
    memcpy(repSaveB, b, sizeof b); memcpy(repSaveFT, fallT, sizeof fallT); memcpy(repSaveFP, fallP, sizeof fallP);
    memcpy(repSavePk, pocketed, sizeof pocketed); memcpy(repSaveFX, fallX, sizeof fallX); memcpy(repSaveFY, fallY, sizeof fallY);
    memcpy(repSaveM, ballM, sizeof ballM);
    repRet = ret; repSaveMsgT = msgTimer;
    repA = 0; repB = repKeepN - 1;
    repPos = (float)repA; repT = 0; repHold = 0; repSfx = 0;
    msgTimer = 0;
    state = S_REPLAY;
}

static void repEnd(void)                              /* volta para o estado de antes do replay (fim de jogo ou mira) */
{
    memcpy(b, repSaveB, sizeof b); memcpy(fallT, repSaveFT, sizeof fallT); memcpy(fallP, repSaveFP, sizeof fallP);
    memcpy(pocketed, repSavePk, sizeof pocketed); memcpy(fallX, repSaveFX, sizeof fallX); memcpy(fallY, repSaveFY, sizeof fallY);
    memcpy(ballM, repSaveM, sizeof ballM);
    if (repRet == S_OVER) overT = 0; else msgTimer = repSaveMsgT;
    if (repFromSetup) { repCtxSet(&repCtxTmp); repFromSetup = 0; }   /* volta as opcoes da tela de configurar */
    state = repRet;
}

static void repWatchPrev(void)                        /* configurar partida: rever o replay da partida anterior */
{
    if (!repHave || repKeepN < 2) return;
    repCtxGet(&repCtxTmp); repCtxSet(&repCtx); repFromSetup = 1;
    repLaunch(S_SETUP);
}

static void repUpdate(int pressed)                    /* 1 passo do replay (estado S_REPLAY): repete ate apertar X */
{
    int fi, near, hasPot = (repKeepPot >= repA && repKeepPot <= repB);
    repT++;
    if (repT > 12 && (pressed & (PAD_CROSS | PAD_START))) { repEnd(); return; }      /* X pula o replay */
    fi = (int)repPos; if (fi > repB) fi = repB;
    repApply(fi);
    if (hasPot && !repSfx && fi >= repKeepPot) { repSfx = 1; SFX(SFX_POCKET, 90); }
    near = hasPot && fi >= repKeepPot - 25 && fi <= repKeepPot + 25;
    if (fi >= repB) { if (++repHold > 30) { repPos = (float)repA; repHold = 0; repSfx = 0; } }   /* chegou no fim: recomeca */
    else repPos += near ? REP_SLOW : 1.0f;
}

/* ---------- logica ---------- */
static int ballGroup(int i) { return i == 0 ? -1 : (i == 8 ? 2 : (i < 8 ? 0 : 1)); }

static int countGroup(int gr)
{
    int i, n = 0;
    for (i = 1; i < 16; i++) if (b[i].in && ballGroup(i) == gr) n++;
    return n;
}

static int plBall(int pl, int k)                      /* k e uma bola do jogador pl? (mesa aberta: todas, menos a 8) */
{
    int gr = grp[pl];
    if (k <= 0) return 0;
    if (gr < 0 || gr > 1) return k != 8;
    return countGroup(gr) > 0 ? ballGroup(k) == gr : k == 8;
}
static int chiqBall(int k) { return chiqOn && chiqOwner >= 0 && plBall(1 - chiqOwner, k); }   /* CHIQUITOLINA: bola do adversario do Chapolin */
static float ballRad(int k) { return chiqBall(k) ? R * CHIQ_R : R; }

static void setmsg(const char *s) { strncpy(msg, s, 63); msg[63] = 0; msgTimer = 150; }
static int isBot(void) { return mode == 0 && cur == 1; }
static const char *pname(int pl) { return (pl == 1 && mode == 0) ? (tourney ? tName[tOpp] : "BOT") : pnames[pl]; }
static int botCue;                                       /* MESTRE: taco que o bot escolheu para esta tacada */
static int botMaster(void) { return mode == 0 && cur == 1 && botLevel == 4 && !(tourney && !tAbil); }   /* dificuldade MESTRE: o bot usa tacos e poderes */
/* MESTRE: na tacada da bola 8 e preciso escolher (cantar) a cacapa; a bola 8 cair em outra cacapa e derrota */
static int callPk = -1, callManual = 0, callAnn = 0, callShotPk = -1, pk8 = -1;   /* cacapa escolhida, se o jogador mexeu, aviso ja dado, cacapa cantada na tacada, cacapa onde a 8 caiu */
static const char *const PK_NAME[6] = { "CIMA ESQUERDA", "CIMA MEIO", "CIMA DIREITA", "BAIXO ESQUERDA", "BAIXO MEIO", "BAIXO DIREITA" };
static int eightCall(void)       /* esta e a tacada da bola 8 numa partida contra o BOT MESTRE? */
{
    return mode == 0 && botLevel == 4 && gmode != GM_BR && grp[cur] >= 0 && b[8].in && countGroup(grp[cur]) == 0;
}
static int eightBestPocket(void) /* cacapa mais alinhada com a linha branca -> 8 (padrao, ate o jogador escolher outra) */
{
    int q, best = 0;
    float ux = b[8].x - b[0].x, uy = b[8].y - b[0].y, ul = sqrtf(ux * ux + uy * uy), bs = -1e9f;
    if (ul < 1.0f) ul = 1.0f;
    ux /= ul; uy /= ul;
    for (q = 0; q < 6; q++) {
        float vx = PX[q] - b[8].x, vy = PY[q] - b[8].y, vl = sqrtf(vx * vx + vy * vy), sc;
        if (vl < 1.0f) vl = 1.0f;
        sc = (ux * vx + uy * vy) / vl - vl * 0.0006f;
        if (sc > bs) { bs = sc; best = q; }
    }
    return best;
}
static int curCue(void) { return botMaster() ? botCue : ((isBot() || (tourney && !tAbil)) ? 0 : cueSel[cur]); }
static int curAb(void) { return (botMaster() || (abilOn && !isBot() && !(tourney && !tAbil))) ? CUE_AB[curCue()] : AB_NONE; }   /* torneio: todos usam o taco CLASSICO */
/* VAMPIRO: barra de sangue. Cada tacada que encacapa uma bola sua enche 1/3; se errar, a barra seca.
   Com 3 bolas seguidas a barra enche e a 4a tacada vira SEDE DE SANGUE (mais forte e a bola alvo curva para a cacapa) */
#define BLOOD_MAX 3
static int blood[2], bloodShot;
/* MORCEGOS (Vampiro): apos uma tacada SEDE DE SANGUE, na jogada do
 * adversario aparecem morcegos na mesa; as bolas batem neles (fisica). */
#define NBAT  6
#define BATR  9.0f
static float batX[NBAT], batY[NBAT], batVX[NBAT], batVY[NBAT];
static int batOn, batWait, batFor;
static int vampOn(int p)                                 /* a barra de sangue e do TACO VAMPIRO (o BOT usa o taco do personagem Vampiro) */
{
    if (tourney && !tAbil) return 0;
    if (mode == 0 && p == 1) return charSel[1] == 6;
    return abilOn && cueSel[p] == 14;
}
static float cuePowMul(int c)     { return 0.60f + 0.0060f * CUES[c].force; }
static float cueSpinK(int c)      { return 0.08f + 0.0055f * CUES[c].effect; }
static float cueGuideScale(int c) { return 0.45f + 0.004f * CUES[c].aim; }
static float cueBarSpeed(int c)   { return 0.026f - 0.00016f * CUES[c].ctl; }
static float cueVSpin(int c)      { return 0.30f + 0.0050f * CUES[c].effect; }   /* forca do rola/puxa */
static float cueJumpMul(int c)    { return 0.80f + 0.0040f * CUES[c].effect; }   /* altura do pulo */

static void rack(void)
{
    static const int order[15] = {1,9,2,10,8,3,11,4,12,5,13,6,14,7,15};
    int i, r, c, k = 0;
    memset(fallT, 0, sizeof(fallT));
    for (i = 0; i < 16; i++) { b[i].vx = b[i].vy = 0; b[i].z = b[i].vz = 0; b[i].in = 1; resetOrient(i); }
    b[0].x = HEADX; b[0].y = TCY;
    for (r = 0; r < 5; r++)
        for (c = 0; c <= r; c++) {
            int id = order[k++];
            b[id].x = FOOTX + r * (R * 1.76f);
            b[id].y = TCY + (c - r * 0.5f) * (2 * R + 0.4f);
        }
}

/* posicoes iniciais da sinuca brasileira (mesa real): cada cor com 2 bolas coladas no meio da sua
   cabeceira e 1 bola em cada tabela comprida; a branca no centro. s: 0..3 jogador 1 | 4..7 jogador 2 */
static void brPos(int s, float *x, float *y)
{
    float ex0 = TX0 + R + 1.5f, ex1 = TX1 - R - 1.5f, ey0 = TY0 + R + 1.5f, ey1 = TY1 - R - 1.5f;
    float sx0 = TX0 + 0.308f * (TX1 - TX0), sx1 = TX0 + 0.692f * (TX1 - TX0), pd = R + 1.2f;
    switch (s) {
    case 0:  *x = ex0; *y = TCY - pd; break;
    case 1:  *x = ex0; *y = TCY + pd; break;
    case 2:  *x = sx0; *y = ey0;      break;
    case 3:  *x = sx0; *y = ey1;      break;
    case 4:  *x = ex1; *y = TCY - pd; break;
    case 5:  *x = ex1; *y = TCY + pd; break;
    case 6:  *x = sx1; *y = ey0;      break;
    default: *x = sx1; *y = ey1;      break;
    }
}

/* modo brasileiro: branca no meio e 4 bolas de cada jogador (disposicao da mesa real) */
static void rackBR(void)
{
    int i, k;
    memset(fallT, 0, sizeof(fallT));
    for (i = 0; i < 16; i++) { b[i].vx = b[i].vy = 0; b[i].z = b[i].vz = 0; b[i].in = 0; resetOrient(i); }
    b[0].in = 1; b[0].x = TCX; b[0].y = TCY;
    for (k = 0; k < 4; k++) {
        b[1 + k].in = 1; brPos(k, &b[1 + k].x, &b[1 + k].y);
        b[9 + k].in = 1; brPos(4 + k, &b[9 + k].x, &b[9 + k].y);
    }
}

/* BOLA 8 MINI: 3 lisas, 3 listradas e a 8 (no meio) num triangulo pequeno */
static void rackMini(void)
{
    static const int ids[7] = { 1, 9, 2, 10, 8, 3, 11 };
    static const int pr[7] = { 0, 1, 1, 2, 2, 2, 3 }, pc[7] = { 0, 0, 1, 0, 1, 2, 1 };
    int i, n = (gmode == GM_DUEL) ? 5 : 7;                  /* duelo: 2 lisas, 2 listradas e a 8 */
    rack();
    for (i = 1; i < 16; i++) b[i].in = 0;
    for (i = 0; i < n; i++) {
        int id = ids[i];
        b[id].in = 1;
        b[id].x = FOOTX + pr[i] * (R * 1.76f);
        b[id].y = TCY + (pc[i] - pr[i] * 0.5f) * (2 * R + 0.4f);
    }
}

static void autoAim(void);
static void newGame(int m)
{
    char buf[40];
    mode = m;
    if (gmode == GM_BR) { rackBR(); grp[0] = 0; grp[1] = 1; }
    else                { if (gmode == GM_MINI || gmode == GM_DUEL) rackMini(); else rack();   grp[0] = grp[1] = -1; }
    cur = 0; ang = 0; pwr = 0;
    charging = 0; breakShot = (gmode != GM_BR); botTimer = 0; botPlanned = 0; state = S_AIM;
    spinSel = spinCur = 0; barDir = 1.0f;
    spinV = spinVCur = 0; jumpArm = 0; overT = 0;
    chalk[0] = chalk[1] = 1.0f; chalkAnim = 0;
    abilArm = 0; abilKind = AB_NONE; abilT = 0; abilFx = 0; fricMul = 1.0f; pockMul = 1.0f; abilForgive = 0; abilExtra = 0; rastT = -1; grinchPend = 0; curse[0] = curse[1] = 0; hackOn = 0; abilCD[0] = abilCD[1] = 0;
    blood[0] = blood[1] = 0; bloodShot = 0; batOn = 0; batWait = 0;
    graveSel = 0; graveOpen = 0; graveOn = 0; buryBall = 0; buryT = 0; potN = 0; memset(graveIn, 0, sizeof(graveIn));
    raceCnt[0] = raceCnt[1] = 0;
    copSel = 0; copOpen = 0; copTgt = 0; lockBall = 0; lockOwner = 0; sirenT[0] = sirenT[1] = 0; ghostBall = 0; chSel = 0; chOpen = 0; cornBall = 0; cornOwner = 0; chiqOn = 0; chiqOwner = -1;
    turnReset = 1; turnCur = -1;
    repKeepN = 0;                                        /* partida nova: nenhum replay guardado */
    sprintf(buf, gmode == GM_BR ? "%s COMECA" : "%s - QUEBRA", pname(0));
    setmsg(buf);
    autoAim();                                           /* ja comeca mirando numa bola sua (no meio), nao no angulo 0 */
}

/* BAGUNCA: sacode as bolas do adversario (cada uma ganha um empurrao em direcao aleatoria) */
static void bagunca(void)
{
    int k, og = grp[1 - cur];
    if (og < 0 && grp[cur] >= 0 && grp[cur] < 2) og = 1 - grp[cur];
    for (k = 1; k < 16; k++) {
        float an, sp;
        if (!b[k].in) continue;
        if (og >= 0 ? ballGroup(k) != og : k == 8) continue;      /* mesa aberta: bagunca todas, menos a 8 */
        an = (rnd() % 6283) / 1000.0f; sp = 90.0f + (rnd() % 1000) / 10.0f;
        b[k].vx += cosf(an) * sp; b[k].vy += sinf(an) * sp;
    }
    fxX = TCX; fxY = TCY;
}

/* POSEIDON (MARE ALTA): uma onda empurra todas as bolas da mesa (menos a branca) para a cacapa mais proxima de cada uma.
   Forca fraca: so as bolas que ja estao perto de uma cacapa chegam la */
static void marePush(void)
{
    int k, p;
    for (k = 1; k < 16; k++) {
        float bd = 1e9f, dx = 0.0f, dy = 0.0f, d;
        if (!b[k].in) continue;
        for (p = 0; p < 6; p++) {
            float ddx = PX[p] - b[k].x, ddy = PY[p] - b[k].y, d2 = ddx * ddx + ddy * ddy;
            if (d2 < bd) { bd = d2; dx = ddx; dy = ddy; }
        }
        d = sqrtf(bd);
        if (d < 1.0f) continue;
        b[k].vx += dx / d * 85.0f; b[k].vy += dy / d * 85.0f;
    }
    fxX = TCX; fxY = TCY;
}

/* ROBO (IMA): puxa as bolas do jogador para a cacapa mais proxima de cada uma (forca fraca).
   Mesa aberta: todas, menos a 8. So sobrou a 8: ela tambem */
static int ownBall(int k)
{
    int gr = grp[cur];
    if (gr < 0) return k != 8;
    return countGroup(gr) > 0 ? ballGroup(k) == gr : k == 8;
}
static void imaPull(void)
{
    int k, p;
    for (k = 1; k < 16; k++) {
        float bd = 1e9f, dx = 0.0f, dy = 0.0f, d;
        if (!b[k].in || !ownBall(k)) continue;
        for (p = 0; p < 6; p++) {
            float ddx = PX[p] - b[k].x, ddy = PY[p] - b[k].y, d2 = ddx * ddx + ddy * ddy;
            if (d2 < bd) { bd = d2; dx = ddx; dy = ddy; }
        }
        d = sqrtf(bd);
        if (d < 1.0f) continue;
        b[k].vx += dx / d * 70.0f; b[k].vy += dy / d * 70.0f;
    }
    fxX = TCX; fxY = TCY;
}

/* GRINCH: ao usar a habilidade, sorteia um dos poderes. O sorteio e "viciado" pela situacao da mesa:
   poucas bolas -> mais PUXAO VERDE | adversario perto de ganhar ou com muitas bolas -> mais BAGUNCA */
static const char *GR_NAME[2] = { "PUXAO VERDE", "BAGUNCA" };
static int grinchPick(void)
{
    static const int KIND[2] = { AB_SUGAR, AB_BAGUNCA };
    int w[2] = { 10, 10 }, i, k, alive = 0, oppN, og = grp[1 - cur];
    if (og < 0 && grp[cur] >= 0 && grp[cur] < 2) og = 1 - grp[cur];
    for (k = 1; k < 16; k++) if (b[k].in) alive++;
    oppN = og >= 0 ? countGroup(og) : alive;
    if (alive <= 6) w[0] += 25; else if (alive <= 9) w[0] += 8;
    if (oppN <= 2) w[1] += 30; else if (oppN >= 5) w[1] += 10;
    i = ((int)(rnd() % (unsigned)(w[0] + w[1])) < w[0]) ? 0 : 1;
    return KIND[i];
}
static void grinchEnsure(void) { if (!grinchPend) grinchPend = grinchPick(); }
static const char *grinchName(void) { grinchEnsure(); return GR_NAME[grinchPend == AB_SUGAR ? 0 : 1]; }

/* COVEIRO: 3 poderes que o jogador escolhe (O na mira arma e troca: ENTERRO > COVA > RESSUSCITAR > cancelar) */
static void respot(int id, float x, float y);
static int graveFindRevive(void)                      /* bola do adversario encacapada mais recentemente (ainda fora da mesa) */
{
    int n, og = grp[1 - cur];
    if (og < 0 || og > 1) return -1;
    for (n = 1; n <= 32 && n <= potN; n++) {
        int id = potLog[(potN - n) & 31];
        if (id > 0 && id != 8 && !b[id].in && ballGroup(id) == og) return id;
    }
    return -1;
}
static const char *graveWhy(int k)                    /* NULL = o poder pode ser usado agora; senao o motivo */
{
    if (k == 0) {
#if ENTERRO_SO_ULTIMA
        if (grp[cur] < 0 || grp[cur] > 1 || countGroup(grp[cur]) != 1)
            return gmode == GM_BR ? "ENTERRO: SO NA SUA ULTIMA BOLA" : "ENTERRO: SO NA SUA ULTIMA BOLA ANTES DA 8";
#endif
    } else if (k == 2 && graveFindRevive() < 0) return "RESSUSCITAR: NENHUMA BOLA DO ADVERSARIO NO CEMITERIO";
    return NULL;
}
/* POLICIAL: bola do adversario que pode ser presa (mesa aberta: qualquer uma, menos a 8; so sobrou a 8: ela tambem) */
static int copCand(int k)
{
    int og = grp[1 - cur];
    if (k < 1 || k > 15 || !b[k].in || k == cornBall) return 0;
    if (og < 0 && grp[cur] >= 0 && grp[cur] < 2) og = 1 - grp[cur];
    if (og < 0) return k != 8;
    return countGroup(og) > 0 ? ballGroup(k) == og : k == 8;
}
static int copNext(int from, int dir)                 /* proxima bola presavel (QUADRADO troca o alvo) */
{
    int n, k = from;
    for (n = 0; n < 16; n++) { k = (k + dir + 16) % 16; if (copCand(k)) return k; }
    return -1;
}
static int copBest(void)                              /* alvo inicial: a bola do adversario mais perto de uma cacapa */
{
    int k, p, best = -1;
    float bd = 1e9f;
    for (k = 1; k < 16; k++) {
        if (!copCand(k)) continue;
        for (p = 0; p < 6; p++) {
            float dx = b[k].x - PX[p], dy = b[k].y - PY[p], d2 = dx * dx + dy * dy;
            if (d2 < bd) { bd = d2; best = k; }
        }
    }
    return best;
}
static const char *copWhy(int k)                      /* NULL = o poder pode ser usado agora; senao o motivo */
{
    if (k == 0 && copBest() < 0) return "ALGEMAS: NENHUMA BOLA DO ADVERSARIO NA MESA";
    return NULL;
}
/* CHAPOLIN (corneta): bola do adversario que pode ser paralisada (nunca a 8; nunca a ultima da cor dele, que ficaria impossivel de encacapar) */
static int cornCand(int k) { return copCand(k) && k != 8 && !(ballGroup(k) < 2 && countGroup(ballGroup(k)) <= 1); }
static int cornNext(int from, int dir)
{
    int n, k = from;
    for (n = 0; n < 16; n++) { k = (k + dir + 16) % 16; if (cornCand(k)) return k; }
    return -1;
}
static int cornBest(void)                             /* alvo inicial: a bola do adversario mais perto de uma cacapa */
{
    int k, p, best = -1;
    float bd = 1e9f;
    for (k = 1; k < 16; k++) {
        if (!cornCand(k)) continue;
        for (p = 0; p < 6; p++) {
            float dx = b[k].x - PX[p], dy = b[k].y - PY[p], d2 = dx * dx + dy * dy;
            if (d2 < bd) { bd = d2; best = k; }
        }
    }
    return best;
}
static const char *chWhy(int k)                       /* NULL = o poder pode ser usado agora; senao o motivo */
{
    if (k == 0) {
        if (cornBall > 0 && b[cornBall].in) return "CORNETA: JA HA UMA BOLA PARALISADA";
        if (cornBest() < 0) return "CORNETA: NENHUMA BOLA DO ADVERSARIO PODE SER PARALISADA";
    }
    return NULL;
}
static const char *abLabel(int ab)                    /* nome mostrado na tela (Grinch: poder sorteado; Coveiro: poder escolhido) */
{
    if (ab == AB_GRINCH) return grinchName();
    if (ab == AB_COVEIRO && graveOpen) return GV_NAME[graveSel];
    if (ab == AB_POLICIAL && copOpen) return CP_NAME[copSel];
    if (ab == AB_MARRETA && chOpen) return CH_NAME[chSel];
    return AB_NAME[ab];
}

#define CURSE_FAIL 35                                 /* % de chance da forca falhar sob a MALDICAO do mago */
static void shoot(float p)
{
    int cursedNow = curse[cur];                       /* MALDICAO do mago: vale so para esta tacada */
    curse[cur] = 0;
    tmSnap = 0; tmArmed = 0;
    if (abilArm && curAb() == AB_TEMPO) {             /* guarda a mesa antes da tacada */
        memcpy(tmB, b, sizeof(b)); tmGrp[0] = grp[0]; tmGrp[1] = grp[1];
        tmSnap = 1; tmArmed = 1;
    }
    float ch = isBot() ? 1.0f : chalk[cur], a = ang, pm = 1.0f;
    int hk = hackOn;                                  /* HACK: tacada certeira, sem desvio de giz */
    memset(pocketed, 0, sizeof(pocketed));
    firstHit = -1;
    gAtStart = grp[cur];
    startOwnLeft = gAtStart >= 0 ? countGroup(gAtStart) : 99;
    hitX = b[0].x; hitY = b[0].y; hitCue = curCue();
    if (!isBot()) {                                   /* giz: ponta sem giz escorrega e a tacada sai torta */
        float miss = hk ? 0.0f : (1.0f - ch) * 0.045f, dev = ((int)(rnd() % 2001) - 1000) / 1000.0f;
        if (hk) { }
        else if (ch < 0.25f && (fabsf(spinSel) > 0.6f || fabsf(spinV) > 0.6f || jumpArm)) { miss *= 2.0f; pm = 0.9f; setmsg("ESCORREGOU! PASSE GIZ NO TACO"); }
        else if (ch < 0.25f) setmsg("POUCO GIZ! PASSE GIZ NO TACO");
        a += dev * miss;
        chalk[cur] -= 0.035f + 0.03f * fabsf(spinSel) + 0.02f * fabsf(spinV) + (jumpArm ? 0.03f : 0.0f);   /* gasta devagar: ~28 tacadas normais */
        if (chalk[cur] < 0.0f) chalk[cur] = 0.0f;
    }
    if (cursedNow && !isBot() && (int)(rnd() % 100) < CURSE_FAIL) {   /* MALDICAO: a forca falha (marcou 100, sai 10) */
        p *= 0.10f; setmsg("MALDICAO! A FORCA FALHOU");
    }
    if (chiqOn && cur != chiqOwner) p *= CHIQ_POW;    /* CHIQUITOLINA: o adversario do Chapolin bate mais fraco */
    b[0].vx = cosf(a) * p * MAXV * cuePowMul(hitCue) * pm;
    b[0].vy = sinf(a) * p * MAXV * cuePowMul(hitCue) * pm;
    SFX(SFX_HIT, 35 + (int)(p * 65.0f));                 /* toc do taco na bola */
    spinCur = spinSel * (0.8f + 0.2f * ch); spinK = cueSpinK(hitCue); spinSel = 0;
    spinVCur = spinV * (0.8f + 0.2f * ch); spinVK = cueVSpin(hitCue); spinV = 0;
    if (abilCD[cur] > 0) abilCD[cur]--;
    abilKind = AB_NONE; abilT = 0; fricMul = 1.0f; pockMul = 1.0f; abilForgive = 0; hackOn = 0; grinchOn = 0; abilExtra = 0; rastT = -1;
    graveOn = 0; buryBall = 0; buryT = 0; ghostBall = 0;
    bloodShot = vampOn(cur) && blood[cur] >= BLOOD_MAX;
    if (bloodShot) {                                     /* VAMPIRO: SEDE DE SANGUE */
        b[0].vx *= 1.5f; b[0].vy *= 1.5f;
        if (abilKind == AB_NONE) abilKind = AB_SUGAR;    /* a bola alvo curva suave para a cacapa */
        abilFx = 36; fxX = hitX; fxY = hitY;
        setmsg("SEDE DE SANGUE! TACADA MAIS FORTE");
    }
    if (hk && abilKind == AB_NONE) abilKind = AB_SUGAR;   /* HACK: depois da batida a bola alvo ainda curva para a cacapa (tacada certeira) */
    if (abilArm && curAb() != AB_NONE) {                 /* habilidade do taco de personagem */
        abilKind = curAb(); abilCD[cur] = AB_COOL; abilFx = 36; fxX = hitX; fxY = hitY;
        if (abilKind == AB_GRINCH) {                      /* vira o poder que ja estava sorteado e mostrado na tela */
            char tb[64];
            grinchEnsure(); abilKind = grinchPend; grinchOn = 1;
            sprintf(tb, "GRINCH USOU: %s!", GR_NAME[abilKind == AB_SUGAR ? 0 : 1]); setmsg(tb);
        }
        if (abilKind == AB_COVEIRO) {                     /* vira o poder escolhido */
            char tb[64];
            abilKind = GV_KIND[graveSel]; graveOn = 1;
            if (graveSel == 0) abilCD[cur] = ENTERRO_RECARGA;      /* NERF: o enterro demora mais para recarregar */
            sprintf(tb, "COVEIRO USOU: %s!", GV_NAME[graveSel]); setmsg(tb);
        }
        if (abilKind == AB_POLICIAL) {                    /* vira o poder escolhido */
            char tb[64];
            abilKind = CP_KIND[copSel];
            sprintf(tb, "POLICIAL USOU: %s!", CP_NAME[copSel]); setmsg(tb);
        }
        if (abilKind == AB_MARRETA) {                     /* CHAPOLIN: vira o poder escolhido */
            char tb[64];
            abilKind = CH_KIND[chSel];
            sprintf(tb, "CHAPOLIN USOU: %s!", CH_NAME[chSel]); setmsg(tb);
        }
        switch (abilKind) {
        case AB_COVA:     { int q; memset(graveIn, 0, sizeof(graveIn)); for (q = 1; q < 16; q++) { graveOX[q] = b[q].x; graveOY[q] = b[q].y; } fxX = TCX; fxY = TCY; } break;
        case AB_RESSUS:   { int id = graveFindRevive(); if (id > 0) { respot(id, TCX, TCY); ghostBall = id; fxX = b[id].x; fxY = b[id].y; } } break;   /* a ressuscitada e um fantasma: sem fisica ate o fim desta jogada */
        case AB_PRENDER:  if (copTgt > 0 && b[copTgt].in) { lockBall = copTgt; lockOwner = cur; lockX = b[copTgt].x; lockY = b[copTgt].y; fxX = lockX; fxY = lockY; } break;   /* POLICIAL: prende a bola */
        case AB_CORNETA:  if (copTgt > 0 && b[copTgt].in && cornCand(copTgt)) { cornBall = copTgt; cornOwner = cur; cornX = b[copTgt].x; cornY = b[copTgt].y; fxX = cornX; fxY = cornY; }
                          else { abilCD[cur] = 0; abilKind = AB_NONE; setmsg("CORNETA: NENHUMA BOLA VALIDA"); } break;   /* CHAPOLIN: paralisa a bola por 1 rodada */
        case AB_CHIQUI:   chiqOn = 1; chiqOwner = cur; fxX = TCX; fxY = TCY; break;                                   /* CHAPOLIN: bolas do adversario menores e lentas */
        case AB_BIONICA:  b[0].vx *= BION_VEL; b[0].vy *= BION_VEL; fricMul = BION_FRIC; break;                        /* CHAPOLIN: tudo mais rapido, demora para parar */
        case AB_SIRENE:   sirenT[1 - cur] = 1; fxX = TCX; fxY = TCY; break;                                                                        /* POLICIAL: o adversario joga sem linha guia */
        case AB_TIRO:     b[0].vx *= 1.5f; b[0].vy *= 1.5f; spinCur = 0.0f; spinVCur = 0.0f; jumpArm = 0; break;
        case AB_CORTE:    b[0].vx *= 1.2f; b[0].vy *= 1.2f; break;
        case AB_MARE:     spinK *= 2.6f; if (fabsf(spinCur) < 0.3f) spinCur = spinCur < 0.0f ? -0.8f : 0.8f; break;
        case AB_CONGELAR: fricMul = 0.22f; break;
        case AB_CHAMINE:  pockMul = 1.45f; break;
        case AB_MAREALTA: marePush(); break;                /* POSEIDON: onda empurra as bolas para as cacapas */
        case AB_IMA:      imaPull(); break;                 /* ROBO: puxa as suas bolas para as cacapas */
        case AB_MALDICAO: curse[1 - cur] = 1; break;        /* MAGO: amaldicoa a proxima tacada do adversario */
        case AB_VIDAS:    abilExtra = 1; abilForgive = 4; break;   /* GATO: nao perde a vez (joga de novo) e a falta / suicidio e perdoado */
        case AB_BAGUNCA:  bagunca(); break;                 /* CHAMINE: cacapas bem maiores nesta tacada */
        default: break;
        }
    }
    abilArm = 0; graveOpen = 0; copOpen = 0; chOpen = 0; grinchPend = 0;                        /* GRINCH: usando ou nao, o proximo poder e sorteado de novo */
    if (jumpArm && !isBot()) {                        /* pulo: a branca sobe e passa por cima das outras bolas */
        b[0].vz = (150.0f + 90.0f * p) * cueJumpMul(hitCue);
        b[0].z = 0.01f; b[0].vx *= 0.9f; b[0].vy *= 0.9f;
    }
    jumpArm = 0;
    breakShot = 0; pwr = 0; charging = 0; follow = FOLLOWN; repReset(); state = S_ROLL;
}

/* menu de tacos (SELECT) */
static void openCueMenu(void)
{
    if (tourney && !tAbil) { setmsg("TORNEIO: TODOS USAM O TACO CLASSICO"); return; }
    cueMenuSel = cueSel[cur]; cueRet = state; charging = 0; state = S_CUES;
}

/* o taco vai para tras com a forca e e disparado para a bola */
static void startStrike(float p)
{
    strikePwr = p; strikeD0 = R + 4 + pwr * 70; strikeK = 0;
    callShotPk = eightCall() ? (callPk >= 0 ? callPk : eightBestPocket()) : -1;   /* MESTRE: cacapa cantada para a bola 8 */
    pk8 = -1;
    callPk = -1; callManual = 0; callAnn = 0;                                     /* a escolha vale so para esta tacada */
    charging = 0; state = S_STRIKE;
}

static float cushionE(float v)
{
    float e = 0.88f - 0.00018f * fabsf(v);      /* mais rapido = perde mais energia */
    return e < 0.62f ? 0.62f : e;
}

/* quina da cacapa: mantem a bola dentro da largura do buraco e rebate (alongX: 1 = trava o x, 0 = trava o y) */
static void jawHit(Ball *q, int alongX, float pc, float rr)
{
    if (alongX) {
        float sg = q->x > pc ? 1.0f : -1.0f;
        q->x = pc + sg * (rr - 0.05f);
        if (q->vx * sg > 0.0f) q->vx = -q->vx * 0.6f;
    } else {
        float sg = q->y > pc ? 1.0f : -1.0f;
        q->y = pc + sg * (rr - 0.05f);
        if (q->vy * sg > 0.0f) q->vy = -q->vy * 0.6f;
    }
}

/* habilidade acionada na primeira bola que a branca toca */
static void abilOnFirstHit(int j)
{
    int k;
    if (abilKind == AB_SUGAR) abilT = 600;
    else if (abilKind == AB_ENTERRO) {                /* COVEIRO: a pa enterra a bola alvo na cacapa mais proxima (NERF: so se estiver perto dela) */
        int ok = 1;
        float bd = 1e9f; int pp, bpp = 0;
#if ENTERRO_SO_ULTIMA
        ok = grp[cur] >= 0 && grp[cur] < 2 && ballGroup(j) == grp[cur] && countGroup(grp[cur]) == 1;
#endif
        {   /* NERF: escolhe a cacapa mais alinhada com o rumo da bola (nao a mais perto); sem nenhuma na frente, o poder falha */
            float vsp = sqrtf(b[j].vx * b[j].vx + b[j].vy * b[j].vy), bcs = -2.0f;
            for (pp = 0; pp < 6; pp++) {
                float ddx = PX[pp] - b[j].x, ddy = PY[pp] - b[j].y, dl2 = sqrtf(ddx * ddx + ddy * ddy), cs = 1.0f;
                if (dl2 < 1.0f) continue;
                if (vsp > 1.0f) cs = (ddx * b[j].vx + ddy * b[j].vy) / (dl2 * vsp);
                if (cs > bcs) { bcs = cs; bpp = pp; bd = dl2 * dl2; }
            }
            if (bcs < ENTERRO_COS) ok = 0;
        }
        if (LOCKED(j)) { ok = 0; setmsg("BOLA PRESA: O ENTERRO NAO FUNCIONA"); }
        else if (j == 8 && !(grp[cur] >= 0 && grp[cur] < 2 && countGroup(grp[cur]) == 0)) { ok = 0; setmsg("ENTERRO NAO FUNCIONA NA BOLA 8 AINDA"); }   /* nao enterra a 8 antes da hora (seria derrota) */
        else if (bd > ENTERRO_ALCANCE * ENTERRO_ALCANCE) { ok = 0; setmsg("ENTERRO: A BOLA ESTA LONGE DA CACAPA"); }
        else if (!ok) setmsg("ENTERRO: SEM CACAPA NA FRENTE DA BOLA");
        if (ok) {
            float sp = sqrtf(b[j].vx * b[j].vx + b[j].vy * b[j].vy), pdx = PX[bpp] - b[j].x, pdy = PY[bpp] - b[j].y, pdl = sqrtf(pdx * pdx + pdy * pdy);
            buryP = bpp; buryBall = j; buryT = ENTERRO_TEMPO; abilFx = 36; fxX = b[j].x; fxY = b[j].y;
            if (sp > 1.0f && pdl > 1.0f) {                /* puxa so uma parte: mistura a direcao da bola com a direcao da cacapa */
                float nx2 = b[j].vx / sp * (1.0f - ENTERRO_PUXAO) + pdx / pdl * ENTERRO_PUXAO, ny2 = b[j].vy / sp * (1.0f - ENTERRO_PUXAO) + pdy / pdl * ENTERRO_PUXAO;
                float nl = sqrtf(nx2 * nx2 + ny2 * ny2);
                if (nl > 0.01f) { b[j].vx = nx2 / nl * sp; b[j].vy = ny2 / nl * sp; }
            }
        } else abilCD[cur] = 0;                       /* nao funcionou: devolve a recarga */
    }
    else if (abilKind == AB_CORTE) { b[0].vx = b[0].vy = 0; abilFx = 36; fxX = b[0].x; fxY = b[0].y; }
    else if (abilKind == AB_FOGO) {
        for (k = 1; k < 16; k++) {
            float dx, dy, d;
            if (k == j || !b[k].in) continue;
            dx = b[k].x - b[j].x; dy = b[k].y - b[j].y; d = sqrtf(dx * dx + dy * dy);
            if (d > 0.5f && d < 100.0f) { float f = 210.0f * (1.0f - d / 120.0f); b[k].vx += dx / d * f; b[k].vy += dy / d * f; }
        }
        abilFx = 36; fxX = b[j].x; fxY = b[j].y;
    }
}

static float normAng(float a);

static int stepPhysics(float dt)
{
    int i, j, p, moving = 0;
    float tpk = tabPock();
    if (cornBall > 0 && b[cornBall].in) {              /* CHAPOLIN: a bola paralisada nao se mexe de jeito nenhum durante a rodada */
        b[cornBall].x = cornX; b[cornBall].y = cornY; b[cornBall].vx = b[cornBall].vy = 0.0f; b[cornBall].vz = 0.0f; b[cornBall].z = 0.0f;
    }
    if (lockBall > 0 && b[lockBall].in) {              /* POLICIAL: a bola presa nao se mexe de jeito nenhum (nenhum poder a move) */
        b[lockBall].x = lockX; b[lockBall].y = lockY; b[lockBall].vx = b[lockBall].vy = 0.0f; b[lockBall].vz = 0.0f; b[lockBall].z = 0.0f;
    }
    if (abilKind == AB_SUGAR && abilT > 0 && firstHit > 0 && b[firstHit].in && b[firstHit].z <= 0.0f) {   /* vampiro/grinch: a bola alvo CURVA suave para a cacapa que ja esta mirando (nao e mais um ima) */
        Ball *q = &b[firstHit];
        float sp = sqrtf(q->vx * q->vx + q->vy * q->vy);
        if (sp > 25.0f) {
            float ux = q->vx / sp, uy = q->vy / sp, bc = bloodShot ? 0.25f : 0.55f;
            int bp = -1;
            for (p = 0; p < 6; p++) {                    /* so ajuda cacapas mais ou menos a frente (ate ~57 graus) */
                float ddx = PX[p] - q->x, ddy = PY[p] - q->y, dl = sqrtf(ddx * ddx + ddy * ddy), cs;
                if (dl < 1.0f) continue;
                cs = (ddx * ux + ddy * uy) / dl;
                if (cs > bc) { bc = cs; bp = p; }
            }
            if (bp >= 0) {                               /* gira a direcao aos poucos, sem dar velocidade extra */
                float da = normAng(atan2f(PY[bp] - q->y, PX[bp] - q->x) - atan2f(uy, ux)), mx = (bloodShot ? 3.6f : 2.0f) * dt, c, sn;
                if (da > mx) da = mx; else if (da < -mx) da = -mx;
                c = cosf(da); sn = sinf(da);
                q->vx = (ux * c - uy * sn) * sp; q->vy = (ux * sn + uy * c) * sp;
            }
        }
        abilT--;
    }
    if (abilKind == AB_ENTERRO && buryT > 0 && buryBall > 0 && b[buryBall].in && b[buryBall].z <= 0.0f) {   /* coveiro: a bola enterrada vai reto para a cacapa */
        Ball *q = &b[buryBall];
        float ddx = PX[buryP] - q->x, ddy = PY[buryP] - q->y, dl = sqrtf(ddx * ddx + ddy * ddy), sp = sqrtf(q->vx * q->vx + q->vy * q->vy);
        if (dl > 1.0f && sp > 12.0f) {                    /* guia a bola para a cacapa: gira rapido e mantem velocidade para chegar la */
            float ux = q->vx / sp, uy = q->vy / sp, da = normAng(atan2f(ddy, ddx) - atan2f(uy, ux)), mx = ENTERRO_GIRO * dt, c, sn, ns = sp;
            if (da > mx) da = mx; else if (da < -mx) da = -mx;
            c = cosf(da); sn = sinf(da);
            q->vx = (ux * c - uy * sn) * ns; q->vy = (ux * sn + uy * c) * ns;
        }
        buryT--;
    }
    if (abilKind == AB_RASTREIO && firstHit < 0 && b[0].in && b[0].z <= 0.0f) {   /* alien: a branca persegue a bola certa mais perto da mira */
        float sp = sqrtf(b[0].vx * b[0].vx + b[0].vy * b[0].vy);
        if (sp > 30.0f) {
            float ux = b[0].vx / sp, uy = b[0].vy / sp, bd = 1e9f;
            int k;
            if (rastT > 0 && !b[rastT].in) rastT = -1;
            if (rastT < 0) {
                for (k = 1; k < 16; k++) {
                    float dx, dy, d;
                    if (!b[k].in || !ownBall(k)) continue;
                    dx = b[k].x - b[0].x; dy = b[k].y - b[0].y; d = sqrtf(dx * dx + dy * dy);
                    if (d < 1.0f || (dx * ux + dy * uy) / d < 0.5f) continue;     /* so as bolas mais ou menos a frente */
                    if (d < bd) { bd = d; rastT = k; }
                }
            }
            if (rastT > 0) {
                float da = normAng(atan2f(b[rastT].y - b[0].y, b[rastT].x - b[0].x) - atan2f(uy, ux)), mx = 2.6f * dt, c, sn;
                if (da > mx) da = mx; else if (da < -mx) da = -mx;
                c = cosf(da); sn = sinf(da);
                b[0].vx = (ux * c - uy * sn) * sp; b[0].vy = (ux * sn + uy * c) * sp;
            }
        }
    }
    for (i = 0; i < 16; i++) {
        int inMouth = 0, air, wl = 0;
        float mdx = 0.0f, mdy = 0.0f, mpr = 0.0f, mpx = 0.0f, mpy = 0.0f;
        if (!b[i].in) continue;
        if (LOCKED(i)) { b[i].vx = b[i].vy = 0.0f; continue; }     /* presa / paralisada: sem movimento, sem cacapa */
        if (b[i].z > 0.0f) {                       /* pulo: sobe e desce sem atrito */
            b[i].vz -= JUMP_G * dt; b[i].z += b[i].vz * dt;
            if (b[i].z <= 0.0f) {
                b[i].z = 0.0f;
                if (b[i].vz < -45.0f) {            /* quica de leve ao cair */
                    b[i].vz = -b[i].vz * 0.38f; b[i].vx *= 0.92f; b[i].vy *= 0.92f; b[i].z = 0.001f;
                } else b[i].vz = 0.0f;
            }
        }
        air = b[i].z > 3.0f;
        if (i == 0 && spinCur != 0.0f && b[0].z <= 0.0f) {          /* efeito: a trajetoria curva */
            float a = spinCur * spinK * dt, c = cosf(a), sn = sinf(a);
            float ovx = b[0].vx, ovy = b[0].vy;
            b[0].vx = ovx * c - ovy * sn; b[0].vy = ovx * sn + ovy * c;
            spinCur *= 1.0f - 1.1f * dt;
        }
        b[i].x += b[i].vx * dt; b[i].y += b[i].vy * dt;
        for (p = 0; p < 6; p++) {
            float dx = b[i].x - PX[p], dy = b[i].y - PY[p], d2 = dx*dx + dy*dy, prc = PRC[p] * pockMul * tpk, mouth = prc + R;
            if (!air && d2 < prc*prc) {
                fallT[i] = FALLN; fallP[i] = p; fallX[i] = b[i].x; fallY[i] = b[i].y;
                b[i].in = 0; b[i].vx = b[i].vy = 0; b[i].z = b[i].vz = 0; pocketed[i] = 1; if (i == 8) pk8 = p; SFX(SFX_POCKET, 90); if (!sfxMute) pfxBurst(shEq[SC_CACAPA], PX[p], PY[p]); break;
            }
            if (d2 < mouth*mouth) {                       /* na boca da cacapa: a tabela abre, a bola pode entrar */
                float d = sqrtf(d2), toward = -(b[i].vx * dx + b[i].vy * dy);
                inMouth = 1; mdx = fabsf(dx); mdy = fabsf(dy); mpr = prc; mpx = PX[p]; mpy = PY[p];
                if (d > 0.001f && toward > 0.0f && POCK_PULL > 0.0f) {   /* so ajuda quem vai em direcao ao buraco */
                    b[i].vx -= dx / d * POCK_PULL * dt;
                    b[i].vy -= dy / d * POCK_PULL * dt;
                }
            }
        }
        if (b[i].in && i > 0 && abilKind == AB_COVA && !air) {       /* coveiro: a cova e uma cacapa extra no meio da mesa; a bola que cai conta como encacapada */
            float gdx = b[i].x - TCX, gdy = b[i].y - TCY;
            if (gdx * gdx + gdy * gdy < GRAVE_R * GRAVE_R) {
                pocketed[i] = 1; fallT[i] = FALLN; fallP[i] = 6; fallX[i] = b[i].x; fallY[i] = b[i].y;
                b[i].in = 0; b[i].vx = b[i].vy = 0; b[i].z = b[i].vz = 0; SFX(SFX_POCKET, 60);
            }
        }
        if (!b[i].in) continue;
        /* a tabela so abre na largura do buraco. Fora dela a bola bate na tabela; se ela ja entrou
           fundo na boca e tenta sair pelo lado, bate na "quina" (jaw) em vez de ser jogada de volta */
        if (b[i].x < TX0+R && !(inMouth && mdy < mpr)) {
            if (inMouth && TX0+R - b[i].x > 2.0f) jawHit(&b[i], 0, mpy, mpr);
            else { b[i].x = TX0+R; if (b[i].vx < 0) { wl = 1; b[i].vx = -b[i].vx * cushionE(b[i].vx); b[i].vy *= 0.985f; } }
        }
        if (b[i].x > TX1-R && !(inMouth && mdy < mpr)) {
            if (inMouth && b[i].x - (TX1-R) > 2.0f) jawHit(&b[i], 0, mpy, mpr);
            else { b[i].x = TX1-R; if (b[i].vx > 0) { wl = 1; b[i].vx = -b[i].vx * cushionE(b[i].vx); b[i].vy *= 0.985f; } }
        }
        if (b[i].y < TY0+R && !(inMouth && mdx < mpr)) {
            if (inMouth && TY0+R - b[i].y > 2.0f) jawHit(&b[i], 1, mpx, mpr);
            else { b[i].y = TY0+R; if (b[i].vy < 0) { wl = 1; b[i].vy = -b[i].vy * cushionE(b[i].vy); b[i].vx *= 0.985f; } }
        }
        if (b[i].y > TY1-R && !(inMouth && mdx < mpr)) {
            if (inMouth && b[i].y - (TY1-R) > 2.0f) jawHit(&b[i], 1, mpx, mpr);
            else { b[i].y = TY1-R; if (b[i].vy > 0) { wl = 1; b[i].vy = -b[i].vy * cushionE(b[i].vy); b[i].vx *= 0.985f; } }
        }
        if (wl) {                                         /* som da tabela (mais forte quanto mais rapida a bola) */
            float cs = sqrtf(b[i].vx * b[i].vx + b[i].vy * b[i].vy);
            if (cs > 25.0f) SFX(SFX_CUSHION, 20 + (int)(cs * 0.14f));
        }
        if (wl && i == 0 && spinCur != 0.0f) {          /* efeito lateral muda o angulo do rebote na tabela */
            float kx = -b[0].vy, ky = b[0].vx;
            b[0].vx += kx * spinCur * 0.12f; b[0].vy += ky * spinCur * 0.12f;
            spinCur *= 0.55f;
        }
    }
    for (i = 0; i < 16; i++) {
        if (!b[i].in) continue;
        for (j = i + 1; j < 16; j++) {
            float dx, dy, d2, d, nx, ny, rv, ov, rs;
            if (!b[j].in) continue;
            if (b[i].z > JUMP_CLEAR || b[j].z > JUMP_CLEAR) continue;      /* pulando (branca ou bola do CHAPULIN): passa por cima das outras */
            if (ghostBall > 0 && (i == ghostBall || j == ghostBall)) continue;   /* RESSUSCITAR: a bola que voltou e um fantasma (sem fisica) */
            dx = b[j].x - b[i].x; dy = b[j].y - b[i].y; d2 = dx*dx + dy*dy;
            rs = ballRad(i) + ballRad(j);                  /* CHIQUITOLINA: bolas menores */
            if (d2 >= rs*rs || d2 < 0.0001f) continue;
            d = sqrtf(d2); nx = dx / d; ny = dy / d;
            if (LOCKED(i) || LOCKED(j)) {          /* POLICIAL / CHAPOLIN: a bola presa ou paralisada e como uma parede - quem bate nela rebate e ela nao cede */
                int mv = LOCKED(i) ? j : i, lk = LOCKED(i) ? i : j;
                float mnx = LOCKED(i) ? nx : -nx, mny = LOCKED(i) ? ny : -ny, vn;
                if (LOCKED(mv)) continue;
                b[mv].x = b[lk].x + mnx * rs; b[mv].y = b[lk].y + mny * rs;
                vn = b[mv].vx * mnx + b[mv].vy * mny;
                if (vn < 0.0f) {
                    if (-vn > 8.0f) SFX(SFX_BALL, 15 + (int)(-vn * 0.16f));
                    b[mv].vx -= 1.85f * vn * mnx; b[mv].vy -= 1.85f * vn * mny;
                    if (mv == 0 && firstHit < 0) { firstHit = lk; abilOnFirstHit(lk); }
                }
                continue;
            }
            ov = (rs - d) * 0.5f;
            b[i].x -= nx*ov; b[i].y -= ny*ov; b[j].x += nx*ov; b[j].y += ny*ov;
            rv = (b[i].vx - b[j].vx) * nx + (b[i].vy - b[j].vy) * ny;
            if (rv > 0) {
                float cvx = b[i].vx, cvy = b[i].vy;
                rv *= 0.98f;                      /* choque quase elastico */
                if (rv > 8.0f) SFX(SFX_BALL, 15 + (int)(rv * 0.16f));   /* clac */
                b[i].vx -= rv*nx; b[i].vy -= rv*ny;
                b[j].vx += rv*nx; b[j].vy += rv*ny;
                if (i == 0 && firstHit < 0) { firstHit = j; abilOnFirstHit(j); }
                if (i == 0 && spinVCur != 0.0f) {              /* rola (segue) ou puxa (volta) depois da batida */
                    float sp0 = sqrtf(cvx*cvx + cvy*cvy);
                    if (sp0 > 1.0f) {
                        float bst = spinVCur * spinVK * sp0;
                        b[0].vx += cvx / sp0 * bst; b[0].vy += cvy / sp0 * bst;
                    }
                    spinVCur = 0.0f;
                }
            }
        }
    }
    if (batOn) {                                         /* MORCEGOS: voam e as bolas batem neles */
        int k;
        for (k = 0; k < NBAT; k++) {
            if (!sfxMute) {                              /* na simulacao (hack) ficam parados e nao mudam o estado */
                batX[k] += batVX[k] * dt; batY[k] += batVY[k] * dt;
                if (batX[k] < TX0 + BATR) { batX[k] = TX0 + BATR; batVX[k] = fabsf(batVX[k]); }
                if (batX[k] > TX1 - BATR) { batX[k] = TX1 - BATR; batVX[k] = -fabsf(batVX[k]); }
                if (batY[k] < TY0 + BATR) { batY[k] = TY0 + BATR; batVY[k] = fabsf(batVY[k]); }
                if (batY[k] > TY1 - BATR) { batY[k] = TY1 - BATR; batVY[k] = -fabsf(batVY[k]); }
            }
            for (i = 0; i < 16; i++) {
                float dx, dy, d2, md, d, nx, ny, vn;
                if (!b[i].in) continue;
                if (i == 0 && b[0].z > 0.0f) continue;   /* a branca no ar passa por cima */
                if (b[i].z > 0.0f) continue;
                dx = b[i].x - batX[k]; dy = b[i].y - batY[k];
                md = R + BATR; d2 = dx * dx + dy * dy;
                if (d2 >= md * md) continue;
                d = sqrtf(d2); if (d < 0.001f) { dx = 1.0f; dy = 0.0f; d = 1.0f; }
                nx = dx / d; ny = dy / d;
                b[i].x += nx * (md - d); b[i].y += ny * (md - d);
                vn = (b[i].vx - batVX[k]) * nx + (b[i].vy - batVY[k]) * ny;
                if (vn < 0.0f) {
                    b[i].vx -= 1.8f * vn * nx; b[i].vy -= 1.8f * vn * ny;
                    if (!sfxMute) {
                        float vx0 = batVX[k], vy0 = batVY[k];
                        batVX[k] = vx0 + 0.5f * vn * nx; batVY[k] = vy0 + 0.5f * vn * ny;
                    }
                    SFX(SFX_BALL, 40);
                }
            }
            if (!sfxMute) {                              /* mantem os morcegos voando */
                float s = sqrtf(batVX[k] * batVX[k] + batVY[k] * batVY[k]);
                if (s < 40.0f) { if (s < 1.0f) { batVX[k] = 60.0f; batVY[k] = 40.0f; } else { batVX[k] *= 40.0f / s; batVY[k] *= 40.0f / s; } }
                else if (s > 220.0f) { batVX[k] *= 220.0f / s; batVY[k] *= 220.0f / s; }
            }
        }
    }
    for (i = 0; i < 16; i++) {
        float sp, s2;
        if (!b[i].in) continue;
        if (b[i].z > 0.0f) { moving = 1; continue; }       /* no ar nao ha atrito */
        sp = sqrtf(b[i].vx*b[i].vx + b[i].vy*b[i].vy);
        if (sp < 9.0f) { b[i].vx = b[i].vy = 0; continue; }
        s2 = sp * (1.0f - FRIC_K * dt * fricMul) - FRIC_C * dt * fricMul;
        if (sp < 40.0f) s2 -= 30.0f * dt * fricMul;     /* freio suave no final */
        if (chiqBall(i)) s2 -= sp * CHIQ_DRAG * dt;      /* CHIQUITOLINA: as bolas do adversario perdem velocidade mais rapido */
        if (s2 < 0) s2 = 0;
        b[i].vx *= s2 / sp; b[i].vy *= s2 / sp;
        if (s2 > 0) moving = 1;
    }
    return moving;
}

static int validPlace(float x, float y)
{
    int i;
    if (x < TX0+R || x > TX1-R || y < TY0+R || y > TY1-R) return 0;
    for (i = 1; i < 16; i++) {
        float dx, dy;
        if (!b[i].in) continue;
        dx = b[i].x - x; dy = b[i].y - y;
        if (dx*dx + dy*dy < (2*R+1)*(2*R+1)) return 0;
    }
    if (batOn) {                                         /* nao posiciona em cima de um morcego */
        int k;
        for (k = 0; k < NBAT; k++) {
            float dx = batX[k] - x, dy = batY[k] - y;
            if (dx*dx + dy*dy < (R+BATR+1)*(R+BATR+1)) return 0;
        }
    }
    return 1;
}

/* MORCEGOS: nascem em lugares livres, longe das bolas */
static void spawnBats(void)
{
    int k, tries;
    for (k = 0; k < NBAT; k++) {
        float x = (TX0 + TX1) * 0.5f, y = (TY0 + TY1) * 0.5f, a;
        for (tries = 0; tries < 200; tries++) {
            int i, ok = 1, j;
            x = TX0 + BATR + (rnd() % (int)(TX1 - TX0 - 2 * BATR));
            y = TY0 + BATR + (rnd() % (int)(TY1 - TY0 - 2 * BATR));
            for (i = 0; i < 16 && ok; i++) {
                float dx, dy;
                if (!b[i].in) continue;
                dx = b[i].x - x; dy = b[i].y - y;
                if (dx*dx + dy*dy < (R+BATR+14)*(R+BATR+14)) ok = 0;
            }
            for (j = 0; j < k && ok; j++) {
                float dx = batX[j] - x, dy = batY[j] - y;
                if (dx*dx + dy*dy < (2*BATR+6)*(2*BATR+6)) ok = 0;
            }
            if (ok) break;
        }
        batX[k] = x; batY[k] = y;
        a = (float)(rnd() % 628) / 100.0f;
        batVX[k] = cosf(a) * 80.0f; batVY[k] = sinf(a) * 80.0f;
    }
}

/* devolve uma bola a mesa num lugar livre (PRESENTE do Noel) */
static void respot(int id, float x, float y)
{
    int tries;
    b[id].in = 0; fallT[id] = 0; b[id].vx = b[id].vy = 0; b[id].z = b[id].vz = 0; pocketed[id] = 0;
    for (tries = 0; tries < 600; tries++) {
        int ok;
        if (tries > 0) {
            x = TX0 + R + (rnd() % (int)(TX1 - TX0 - 2 * R));
            y = TY0 + R + (rnd() % (int)(TY1 - TY0 - 2 * R));
        }
        ok = validPlace(x, y);
        if (ok && id != 0 && b[0].in) {
            float dx = b[0].x - x, dy = b[0].y - y;
            if (dx * dx + dy * dy < (2 * R + 1) * (2 * R + 1)) ok = 0;
        }
        if (ok) break;
    }
    b[id].x = x; b[id].y = y; b[id].in = 1;
}

static void endTurn(int foul, int ownPocket)
{
    char buf[64];
    int forgiven = 0, saved = 0, keep = abilExtra, kept = 0;   /* keep: NOVA VIDA do gato */
    int spawnB = vampOn(cur) && bloodShot, shooter = cur;     /* MORCEGOS depois da SEDE DE SANGUE */
    abilExtra = 0;
    if (vampOn(cur)) {                                   /* VAMPIRO: barra de sangue */
        if (bloodShot) blood[cur] = 0;                   /* gastou a sede de sangue */
        else if (ownPocket && !foul) { if (blood[cur] < BLOOD_MAX) blood[cur]++; }
        /* errou: a barra NAO se perde, mantem os gomos */
    }
    bloodShot = 0;
    botTimer = 0; botPlanned = 0; botCue = 0;
    abilArm = 0; graveOpen = 0; copOpen = 0; chOpen = 0; hackOn = 0;
    if (foul && FORGIVE_ON(abilForgive)) {               /* PRESENTE do Noel / NOVA VIDA do Gato: perdoa a falta */
        foul = 0; forgiven = 1;                          /* se encacapou uma bola sua, ainda joga de novo */
        if (!b[0].in) {
            int tries;
            b[0].in = 1; fallT[0] = 0; b[0].x = (gmode == GM_BR) ? TCX : HEADX; b[0].y = TCY;
            b[0].vx = b[0].vy = 0; b[0].z = b[0].vz = 0;
            for (tries = 0; tries < 500 && !validPlace(b[0].x, b[0].y); tries++) {
                b[0].x = TX0 + R + (rnd() % (int)(TX1 - TX0 - 2 * R));
                b[0].y = TY0 + R + (rnd() % (int)(TY1 - TY0 - 2 * R));
            }
        }
    } else if (abilForgive == 2) forgiven = 1;
    else if (abilForgive == 1) { abilCD[cur] = 0; saved = 1; }   /* PRESENTE armado e sem falta: nao gasta */
    abilForgive = 0;
    if (foul) {
        SFX(SFX_FOUL, 80);
        if (!b[0].in) { b[0].in = 1; fallT[0] = 0; b[0].x = (gmode == GM_BR) ? TCX : HEADX; b[0].y = TCY; }
        b[0].vx = b[0].vy = 0; b[0].z = b[0].vz = 0;
        cur = 1 - cur;
        state = S_PLACE;
        sprintf(buf, "FALTA! BOLA NA MAO - %s", pname(cur));
        setmsg(buf);
    } else {
        if (keep && ownPocket) { abilCD[cur] = 0; kept = 2; }     /* NOVA VIDA nao foi preciso: nao gasta */
        else if (keep) kept = 1;                                   /* NOVA VIDA: joga de novo */
        if (!ownPocket && !kept) cur = 1 - cur;
        state = S_AIM;
        sprintf(buf, kept == 1 ? "NOVA VIDA! %s JOGA DE NOVO" : (kept == 2 ? "NOVA VIDA GUARDADA! VEZ DE %s" :
                forgiven ? "FALTA PERDOADA! VEZ DE %s" : (saved ? "PRESENTE GUARDADO! VEZ DE %s" : "VEZ DE %s")), pname(cur));
        setmsg(buf);
    }
    if (spawnB) { batFor = 1 - shooter; batWait = 1; }   /* os morcegos chegam na proxima jogada do adversario */
}

/* ================= LOJA: moedas, itens e save no memory card ================= */
#define SH_MAXI 6
typedef struct { const char *name; int price; const char *desc; } ShopItem;
static const char *const SH_CATN[SH_CATS] = { "GIZ", "BOLAS", "RASTRO", "CACAPA", "TITULOS", "MOLDURAS" };
static const int SH_N[SH_CATS] = { 6, 5, 5, 4, 6, 6 };
static const ShopItem SH_IT[SH_CATS][SH_MAXI] = {
    { { "AZUL", 0, "O GIZ CLASSICO" }, { "VERDE", 80, "NUVEM DE PO VERDE" }, { "ROSA", 80, "NUVEM DE PO ROSA" },
      { "DOURADO", 200, "PO DOURADO COM BRILHOS" }, { "ROXO MAGICO", 150, "PO MAGICO COM FAISCAS" }, { "ARCO-IRIS", 300, "MUDA DE COR O TEMPO TODO" } },
    { { "CLASSICAS", 0, "AS BOLAS DE SEMPRE" }, { "CAVEIRA", 250, "CAVEIRINHAS NAS BOLAS" }, { "PLANETAS", 250, "FAIXAS E ANEIS DE PLANETA" },
      { "FRUTAS", 200, "SEMENTES DE FRUTA NA CASCA" }, { "BRILHO NO ESCURO", 300, "BOLAS NEON QUE BRILHAM" } },
    { { "NENHUM", 0, "SEM RASTRO" }, { "FOGO", 150, "RASTRO DE FOGO ATRAS DA BOLA" }, { "GELO", 150, "RASTRO DE GELO ATRAS DA BOLA" },
      { "NEON VERDE", 150, "RASTRO NEON ATRAS DA BOLA" }, { "ARCO-IRIS", 300, "RASTRO COLORIDO ATRAS DA BOLA" } },
    { { "PADRAO", 0, "SO A BOLA CAINDO" }, { "CONFETE", 120, "EXPLOSAO DE CONFETE" }, { "FOGO", 200, "EXPLOSAO DE FOGO" }, { "RAIO", 250, "UM RAIO CAI NA CACAPA" } },
    { { "NENHUM", 0, "SEM TITULO NO PLACAR" }, { "NOVATO", 30, "PARA QUEM ESTA COMECANDO" }, { "TUBARAO", 150, "O TERROR DA MESA" },
      { "MESTRE DAS CACAPAS", 300, "NINGUEM ENCACAPA COMO VOCE" }, { "REI DA SINUCA", 500, "O TITULO DOS CAMPEOES" }, { "LENDA", 800, "SO OS MELHORES USAM" } },
    { { "NENHUMA", 0, "PAINEL SEM MOLDURA" }, { "DOURADA", 150, "MOLDURA DE OURO" }, { "NEON AZUL", 200, "MOLDURA NEON PULSANDO" },
      { "FOGO", 300, "MOLDURA COM CHAMAS" }, { "GELO", 300, "MOLDURA COM GELO" }, { "ARCO-IRIS", 500, "MOLDURA QUE MUDA DE COR" } },
};

#if USE_MEMCARD
#define SH_MAGIC 0x53484C38u
#define SH_FILE "mc0:8BALLREIS/loja.sav"
typedef struct { u32 magic; int coins; int own[SH_CATS]; int eq[SH_CATS]; u32 pad; u32 sum; } ShopSave;
static ShopSave shBuf __attribute__((aligned(64)));
static int shLoaded, shSaveFail;
static u32 shSum(const ShopSave *sv)
{
    const u8 *q = (const u8 *)sv; u32 h = 2166136261u; unsigned k;
    for (k = 0; k < sizeof(ShopSave) - sizeof(u32); k++) { h ^= q[k]; h *= 16777619u; }
    return h;
}

/* le o save. merge = 1: soma as moedas ja ganhas nesta sessao (o cartao so ficou pronto depois). Devolve 1 se o arquivo existe */
static int shTryLoad(int merge)
{
    int fd, r, k;
    fd = open(SH_FILE, O_RDONLY);
    if (fd < 0) return 0;
    memset(&shBuf, 0, sizeof(shBuf));
    r = (int)read(fd, &shBuf, sizeof(shBuf));
    close(fd);
    if (r == (int)sizeof(shBuf) && shBuf.magic == SH_MAGIC && shBuf.sum == shSum(&shBuf) && shBuf.coins >= 0) {
        shCoins = (merge ? shCoins : 0) + shBuf.coins; if (shCoins > 999999) shCoins = 999999;
        for (k = 0; k < SH_CATS; k++) {
            shOwn[k] |= shBuf.own[k] | 1;
            if (!merge || shEq[k] == 0)
                shEq[k] = (shBuf.eq[k] >= 0 && shBuf.eq[k] < SH_N[k] && ((shOwn[k] >> shBuf.eq[k]) & 1)) ? shBuf.eq[k] : 0;
        }
    }
    return 1;
}
#endif

static void shopSave(void)
{
#if USE_MEMCARD
    int fd, r, k;
    if (!shLoaded && shTryLoad(1)) shLoaded = 1;           /* o cartao pode ter ficado pronto so agora */
    memset(&shBuf, 0, sizeof(shBuf));
    shBuf.magic = SH_MAGIC; shBuf.coins = shCoins;
    for (k = 0; k < SH_CATS; k++) { shBuf.own[k] = shOwn[k]; shBuf.eq[k] = shEq[k]; }
    shBuf.sum = shSum(&shBuf);
    mkdir("mc0:8BALLREIS", 0777);                              /* se ja existe, so devolve erro (ignorado) */
    fd = open(SH_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) { shSaveFail = 1; return; }
    r = (int)write(fd, &shBuf, sizeof(shBuf));
    close(fd);
    shSaveFail = (r != (int)sizeof(shBuf));
    if (!shSaveFail) shLoaded = 1;
#endif
}

static void shopInit(void)
{
    int k;
    for (k = 0; k < SH_CATS; k++) { shOwn[k] = 1; shEq[k] = 0; }
    shCoins = 0;
#if USE_MEMCARD
    SifLoadModule("rom0:MCMAN", 0, NULL);
    SifLoadModule("rom0:MCSERV", 0, NULL);
    if (shTryLoad(0)) shLoaded = 1;
#endif
    ballStyle = shEq[SC_BOLA];
}

/* moedas que o jogador ganha se vencer a partida (o HUD mostra esse valor no centro, como o pote do 8 Ball Pool) */
static int prizeWin(void)
{
    if (mode == 0) return tourney ? (tRound == 2 ? 200 : 60) : 25 + 15 * botLevel;
    return 15;
}

/* moedas no fim da partida */
static void shopAward(int w)
{
    int e;
    if (mode == 0) {
        if (w == 0) e = prizeWin();
        else e = tourney ? 10 : 8;
    } else e = 15;
    shLastEarn = e;
    shCoins += e; if (shCoins > 999999) shCoins = 999999;
    shopSave();
}

static void endGame(int w)
{
    winner = w; overT = 0;
    if (!tourney) (mode == 0 ? winsBot : winsVs)[w]++;
    shopAward(w);
    state = S_OVER;
}

/* falta na brasileira: o adversario tira UMA DAS PROPRIAS bolas da mesa (nunca a do outro) */
static void takeInit(void)
{
    int i;
    takeN = 0; takeSel = 0;
    for (i = 1; i < 16; i++)                         /* so as bolas de quem vai tirar (cur) */
        if (b[i].in && ballGroup(i) == grp[cur]) takeList[takeN++] = i;
}

static void takeMove(int dx, int dy)
{
    int i, best = -1, wrap = -1;
    float bs = 1e9f, ws = 1e9f, cx, cy;
    if (takeN < 2) return;
    cx = b[takeList[takeSel]].x; cy = b[takeList[takeSel]].y;
    for (i = 0; i < takeN; i++) {
        float ex, ey, along, perp, sc;
        if (i == takeSel) continue;
        ex = b[takeList[i]].x - cx; ey = b[takeList[i]].y - cy;
        along = ex * dx + ey * dy; perp = fabsf(ex * dy - ey * dx);
        sc = along + 2.0f * perp;
        if (along > 2.0f) { if (sc < bs) { bs = sc; best = i; } }
        else if (along < -2.0f) { if (sc < ws) { ws = sc; wrap = i; } }  /* volta pelo outro lado */
    }
    if (best >= 0) takeSel = best; else if (wrap >= 0) takeSel = wrap;
}

static void takeConfirm(void)
{
    char buf[40];
    int mine, theirs;
    b[takeList[takeSel]].in = 0;                    /* a bola sai da mesa */
    mine = countGroup(grp[cur]); theirs = countGroup(grp[1 - cur]);
    if (mine == 0 || theirs == 0) {
        overNote[0] = 0;
        endGame(theirs == 0 ? 1 - cur : cur);
        return;
    }
    state = S_AIM; botTimer = 0; botPlanned = 0; pwr = 0;
    sprintf(buf, "VEZ DE %s", pname(cur));
    setmsg(buf);
}

static void botTakePick(void)                       /* o bot tira a propria bola mais dificil */
{
    int i, p, best = -1, n = 0;
    float bd = -1.0f;
    for (i = 0; i < takeN; i++) if (ballGroup(takeList[i]) == grp[cur]) n++;
    if (n == 0) return;
    if (botLevel == 0) {                            /* facil: escolhe qualquer uma */
        int k = (int)(rnd() % n);
        for (i = 0; i < takeN; i++)
            if (ballGroup(takeList[i]) == grp[cur] && k-- == 0) { takeSel = i; return; }
    }
    for (i = 0; i < takeN; i++) {
        float md = 1e9f;
        int id = takeList[i];
        if (ballGroup(id) != grp[cur]) continue;
        for (p = 0; p < 6; p++) {
            float dx = b[id].x - PX[p], dy = b[id].y - PY[p], d = sqrtf(dx*dx + dy*dy);
            if (d < md) md = d;
        }
        if (md > bd) { bd = md; best = i; }
    }
    if (best >= 0) takeSel = best;
}

/* regras da sinuca brasileira (4 bolas para cada jogador):
   - vence quem encacapar as 4 bolas da sua cor primeiro; encacapou a sua sem falta, joga de novo
   - nao acertar a sua bola primeiro (acertar a do adversario ou nenhuma): o adversario tira
     uma das PROPRIAS bolas da mesa e joga
   - suicidio (branca na cacapa): derrota na hora
   - meteu a ultima bola da sua cor COM falta: perde, mesmo que o adversario ainda tenha as 4 bolas */
/* Mestre do Tempo: depois de QUALQUER tacada com a habilidade, pergunta se quer voltar no tempo e refazer a jogada */
static int tmOffer(const char *note, int eight)
{
    if (!tmNow || !tmSnap) return 0;
    tmNow = 0; tmEight = eight; tmSel = 0;
    strncpy(tmNote, note, 47); tmNote[47] = 0;
    charging = 0; pwr = 0; botTimer = 0; botPlanned = 0;
    state = S_REWIND;
    return 1;
}

static void resolveBR(void)
{
    int i, foul, own = 0, mine, theirs, saved = 0;
    char buf[64];
    overNote[0] = 0;
    if (pocketed[0]) {
        if (FORGIVE_ON(abilForgive)) {                   /* PRESENTE / NOVA VIDA: salva do suicidio e devolve a branca ao centro */
            respot(0, TCX, TCY); pocketed[0] = 0; saved = 1;
        } else {
            strcpy(overNote, "SUICIDIO - BRANCA NA CACAPA");
            endGame(1 - cur);
            return;
        }
    }
    foul = firstHit < 0 || ballGroup(firstHit) != grp[cur];
    if (saved || (foul && FORGIVE_ON(abilForgive))) { foul = 0; abilForgive = 2; }       /* PRESENTE: perdoa a falta */
    for (i = 1; i < 16; i++)
        if (pocketed[i] && ballGroup(i) == grp[cur]) own = 1;
    mine = countGroup(grp[cur]); theirs = countGroup(grp[1 - cur]);
    if (foul && mine == 0) {                         /* meteu a ultima bola, mas com falta: perde mesmo assim */
        strcpy(overNote, firstHit < 0 ? "FALTA: NAO TOCOU NA PROPRIA BOLA" : "FALTA: BATEU NA BOLA DO ADVERSARIO");
        endGame(1 - cur);
        return;
    }
    if (mine == 0 || theirs == 0) { endGame(theirs == 0 ? 1 - cur : cur); return; }
    if (foul) {
        cur = 1 - cur;
        botTimer = 0; botPlanned = 0;
        b[0].vx = b[0].vy = 0;
        takeInit();
        state = S_TAKE;
        sprintf(buf, "FALTA! %s TIRA UMA BOLA", pname(cur));
        setmsg(buf);
        return;
    }
    endTurn(0, own);
}

/* RESSUSCITAR: terminou a rodada fantasma; afasta as bolas que ficaram em cima da ressuscitada */
static void ghostSettle(int g)
{
    int k;
    if (g <= 0 || !b[g].in) return;
    for (k = 0; k < 16; k++) {
        float dx, dy, d;
        if (k == g || !b[k].in) continue;
        dx = b[k].x - b[g].x; dy = b[k].y - b[g].y; d = sqrtf(dx * dx + dy * dy);
        if (d >= 2 * R) continue;
        if (d < 0.01f) { dx = 1.0f; dy = 0.0f; d = 1.0f; }
        b[k].x = b[g].x + dx / d * (2 * R + 0.5f); b[k].y = b[g].y + dy / d * (2 * R + 0.5f);
        if (b[k].x < TX0 + R) b[k].x = TX0 + R; if (b[k].x > TX1 - R) b[k].x = TX1 - R;
        if (b[k].y < TY0 + R) b[k].y = TY0 + R; if (b[k].y > TY1 - R) b[k].y = TY1 - R;
    }
}

/* MODO TREINO: sozinho na mesa, sem adversario, falta ou fim de partida. Bola encacapada sai da mesa; branca na cacapa volta como bola na mao;
   mesa limpa = novas bolas. A vez nunca passa para outro jogador */
static void resolveTrain(void)
{
    int i, left = 0, sc = pocketed[0];
    endTurn(0, 1);                                       /* ownPocket = 1: a vez continua sua */
    cur = 0; msgTimer = 0;
    for (i = 1; i < 16; i++) if (b[i].in) left++;
    if (left == 0) {                                     /* mesa limpa: arruma as bolas de novo (o replay guardado continua) */
        int kn = repKeepN, kp = repKeepPot;
        newGame(mode);
        repKeepN = kn; repKeepPot = kp;
        setmsg("MESA LIMPA! NOVA RODADA");
        return;
    }
    if (sc) {                                            /* branca na cacapa: bola na mao */
        b[0].in = 1; fallT[0] = 0; b[0].x = (gmode == GM_BR) ? TCX : HEADX; b[0].y = TCY;
        b[0].vx = b[0].vy = 0; b[0].z = b[0].vz = 0; pocketed[0] = 0;
        state = S_PLACE;
        setmsg("BRANCA NA CACAPA - BOLA NA MAO");
    }
}

static void resolve(void)
{
    int i, foul, ownPocket = 0;

    overNote[0] = 0;
    if (batOn && cur == batFor) batOn = 0;               /* MORCEGOS: somem quando o adversario termina a tacada */
    for (i = 1; i < 16; i++) {
        if (graveIn[i]) { graveIn[i] = 0; respot(i, graveOX[i], graveOY[i]); }     /* COVA: a bola volta ao lugar de origem */
        if (pocketed[i]) { potLog[potN & 31] = i; potN++; }                          /* cemiterio: lembra das bolas encacapadas */
    }
    tmNow = tmArmed && tmSnap; tmArmed = 0;
    abilKind = AB_NONE; abilT = 0; fricMul = 1.0f; pockMul = 1.0f;
    if (tmNow) {                                      /* MESTRE DO TEMPO: qualquer jogada pode ser desfeita */
        const char *nt = pocketed[0] ? "A BRANCA CAIU NA CACAPA" : (pocketed[8] ? "A BOLA 8 CAIU NA CACAPA" :
                         (firstHit < 0 ? "NAO TOCOU EM NENHUMA BOLA" : "A JOGADA TERMINOU"));
        if (tmOffer(nt, 0)) return;
    }
    if (lockBall > 0 && cur != lockOwner) lockBall = 0;     /* POLICIAL: a prisao dura ate o fim da jogada do adversario */
    sirenT[cur] = 0;                                  /* POLICIAL: a sirene vale so para esta jogada */
    if (chiqOn && cur != chiqOwner) chiqOn = 0;       /* CHAPOLIN: a chiquitolina dura ate o fim da jogada do adversario */
    if (cornBall > 0) {                               /* CHAPOLIN: a corneta dura so 1 rodada (ate o fim da jogada do adversario), depois a bola volta ao normal */
        if (!b[cornBall].in) cornBall = 0;
        else if (cur != cornOwner) { cornBall = 0; setmsg("CORNETA ACABOU: BOLA LIBERADA"); }
    }
    ghostSettle(ghostBall); ghostBall = 0;            /* COVEIRO: a ressuscitada volta ao normal (so na proxima rodada ela tem fisica) */
    if (training) { resolveTrain(); return; }
    if (gmode == GM_BR) { resolveBR(); return; }

    if (pocketed[8]) {
        int ok = gAtStart >= 0 && countGroup(gAtStart) == 0 && !pocketed[0];
        int wrongPk = ok && callShotPk >= 0 && pk8 != callShotPk;      /* MESTRE: a 8 caiu fora da cacapa cantada */
        if (wrongPk) ok = 0;
        if (!ok && FORGIVE_ON(abilForgive)) {              /* PRESENTE / NOVA VIDA: devolve a bola 8 e perdoa */
            respot(8, FOOTX, TCY);
            endTurn(1, 0);
            return;
        }
        if (wrongPk) strcpy(overNote, "BOLA 8 NA CACAPA ERRADA");
        winner = ok ? cur : 1 - cur;
        (mode == 0 ? winsBot : winsVs)[winner]++;
        shopAward(winner);
        state = S_OVER;
        return;
    }
    foul = pocketed[0] || firstHit < 0;
    if (!foul && gAtStart >= 0) {
        int fg = ballGroup(firstHit);
        if (fg != gAtStart && !(fg == 2 && startOwnLeft == 0)) foul = 1;
    }
    if (!foul && gAtStart < 0 && firstHit == 8) foul = 1;

    if (gmode == GM_SUDDEN && !foul) {                /* MORTE SUBITA: a primeira bola encacapada sem falta vence */
        for (i = 1; i < 16; i++)
            if (i != 8 && pocketed[i]) { strcpy(overNote, "MORTE SUBITA"); endGame(cur); return; }
    }
    if (gmode == GM_RACE && !foul) {                  /* CORRIDA DE 3: o primeiro a encacapar 3 bolas (sem falta) vence */
        for (i = 1; i < 16; i++)
            if (i != 8 && pocketed[i]) { raceCnt[cur]++; ownPocket = 1; }
        if (raceCnt[cur] >= 3) { strcpy(overNote, "CORRIDA DE 3"); endGame(cur); return; }
    }
    if (!foul && grp[cur] < 0 && gmode != GM_RACE) {
        for (i = 1; i < 16; i++)
            if (pocketed[i]) { grp[cur] = ballGroup(i); grp[1-cur] = 1 - grp[cur]; break; }
    }
    if (grp[cur] >= 0)
        for (i = 1; i < 16; i++)
            if (pocketed[i] && ballGroup(i) == grp[cur]) ownPocket = 1;

    endTurn(foul, ownPocket);
}

/* ---------- BOT ---------- */
static int pathClear(float x1, float y1, float x2, float y2, int ignore)
{
    int i;
    float dx = x2 - x1, dy = y2 - y1, l2 = dx*dx + dy*dy;
    for (i = 1; i < 16; i++) {
        float t, px, py, ex, ey;
        if (!b[i].in || i == ignore) continue;
        t = l2 > 0 ? ((b[i].x-x1)*dx + (b[i].y-y1)*dy) / l2 : 0;
        if (t < 0) t = 0; if (t > 1) t = 1;
        px = x1 + dx*t; py = y1 + dy*t;
        ex = b[i].x - px; ey = b[i].y - py;
        if (ex*ex + ey*ey < (2*R-1)*(2*R-1)) return 0;
    }
    return 1;
}

static float normAng(float a)
{
    while (a >  3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}

/* inicio da jogada: a mira ja aponta para uma bola sua (a mais reta/no meio da mesa, com caminho livre).
   Se houver bola do adversario (ou qualquer bola) no caminho de todas, a mira fica como estava */
static void autoAim(void)
{
    int t, best = -1;
    float bd = 1e9f;
    if (!b[0].in) return;
    for (t = 1; t < 16; t++) {
        float dx, dy, d;
        if (!b[t].in || LOCKED(t) || !ownBall(t)) continue;
        dx = b[t].x - b[0].x; dy = b[t].y - b[0].y; d = sqrtf(dx * dx + dy * dy);
        if (d < 2 * R) continue;
        d = fabsf(dy) / d + d * 0.00001f;                /* quanto mais reta (no meio), melhor; empate: a mais perto */
        if (d >= bd) continue;
        if (!pathClear(b[0].x, b[0].y, b[t].x, b[t].y, t)) continue;
        bd = d; best = t;
    }
    if (best > 0) ang = atan2f(b[best].y - b[0].y, b[best].x - b[0].x);
}

/* MESTRE: o bot escolhe o taco e o poder conforme a situacao da mesa
   (quebra: POTENCIA | tacada dificil: ENTERRO, PUXAO ou CHAMINE | sem tacada: NOVA VIDA | tacada facil: ALGEMAS, RESSUSCITAR, MALDICAO ou SIRENE) */
static void botKit(int found, int tgt, float dist, float cc)
{
    int ab = AB_NONE, k, p, r = (int)(rnd() % 100), hard;
    float nd = 1e9f;
    botCue = 0; abilArm = 0; graveOpen = 0; copOpen = 0; chOpen = 0;
    if (!botMaster()) return;
    if (breakShot) { botCue = 1; return; }               /* quebra: taco de POTENCIA */
    if (abilCD[cur] > 0) return;
    hard = !found || dist > 380.0f || cc < 0.7f;
    k = copBest();                                       /* bola do jogador mais perto de uma cacapa */
    if (k > 0) for (p = 0; p < 6; p++) { float dx = b[k].x - PX[p], dy = b[k].y - PY[p], d = sqrtf(dx * dx + dy * dy); if (d < nd) nd = d; }
    if (found && hard) {
        ab = r < 45 ? AB_ENTERRO : (r < 75 ? AB_SUGAR : AB_CHAMINE);
        if (tgt <= 0) ab = AB_CHAMINE;
    } else if (!found) {
        ab = r < 40 ? AB_VIDAS : (r < 65 ? AB_ENTERRO : (k > 0 ? AB_PRENDER : AB_VIDAS));
    } else {
        if (graveFindRevive() >= 0 && r < 35) ab = AB_RESSUS;
        else if (k > 0 && nd < 110.0f && r < 75) ab = AB_PRENDER;
        else if (r < 88) ab = AB_MALDICAO;
        else ab = AB_SIRENE;
    }
    switch (ab) {
    case AB_ENTERRO:  botCue = 30; graveSel = 0; graveOpen = 1; break;
    case AB_RESSUS:   botCue = 30; graveSel = 2; graveOpen = 1; break;
    case AB_PRENDER:  botCue = 31; copSel = 0; copOpen = 1; copTgt = k; break;
    case AB_SIRENE:   botCue = 31; copSel = 1; copOpen = 1; break;
    case AB_SUGAR:    botCue = 14; break;
    case AB_CHAMINE:  botCue = 15; break;
    case AB_VIDAS:    botCue = 29; break;
    case AB_MALDICAO: botCue = 28; break;
    default: break;
    }
    if (ab != AB_NONE) {
        abilArm = 1;
        botPow *= cuePowMul(0) / cuePowMul(botCue);      /* o taco muda a forca: compensa para a tacada sair igual */
        if (botPow > 1.0f) botPow = 1.0f;
        setmsg("BOT TROCOU DE TACO E USOU UM PODER");
    }
}

static float speedForDist(float d);              /* definida mais abaixo (usa o atrito da mesa) */
static float botPowFix(float p)                   /* a heuristica do bot foi feita para a fisica antiga: converte para a mesma distancia na nova */
{
    const float h = OLD_FRIC_C / OLD_FRIC_K, vo = p * OLD_MAXV;
    float dOld = (vo - h * logf(1.0f + vo / h)) / OLD_FRIC_K;
    float np = speedForDist(dOld) / MAXV;
    return np > 1.0f ? 1.0f : np;
}

static void botPlan(void)
{
    int t, p, found = 0, gr = grp[cur], bt = 0, bpk = -1;
    float best = 1e9f, cx = b[0].x, cy = b[0].y, bdist = 0.0f, bcc = 1.0f;

    for (t = 1; t < 16; t++) {
        if (!b[t].in) continue;
        if (LOCKED(t)) continue;                  /* bola presa pelo policial: nao da para encacapar */
        if (t == 8) { if (!(gr >= 0 && countGroup(gr) == 0)) continue; }
        else if (gr >= 0 && ballGroup(t) != gr) continue;
        for (p = 0; p < 6; p++) {
            float tx = b[t].x, ty = b[t].y, dx = PX[p]-tx, dy = PY[p]-ty;
            float dl = sqrtf(dx*dx + dy*dy), gx, gy, ax, ay, al, cc, sc;
            dx /= dl; dy /= dl;
            gx = tx - dx*2*R; gy = ty - dy*2*R;
            if (gx < TX0+R || gx > TX1-R || gy < TY0+R || gy > TY1-R) continue;
            ax = gx - cx; ay = gy - cy; al = sqrtf(ax*ax + ay*ay);
            if (al < 1) continue;
            ax /= al; ay /= al;
            cc = ax*dx + ay*dy;
            if (cc < 0.35f) continue;
            if (!pathClear(cx, cy, gx, gy, t)) continue;
            if (!pathClear(tx, ty, PX[p], PY[p], t)) continue;
            sc = al + dl + (1.0f - cc) * (botLevel >= 3 ? 900.0f : 400.0f);   /* profissional prefere tacadas retas */
            if (sc < best) {
                best = sc; found = 1; bt = t; bdist = al + dl; bcc = cc; bpk = p;
                botAng = atan2f(gy - cy, gx - cx);
                botPow = (botLevel >= 3 ? 0.30f : 0.35f) + (al + dl) / (botLevel >= 3 ? 640.0f : 600.0f);
            }
        }
    }
    if ((int)(rnd() % 100) < BOT_MISS[botLevel]) found = 0;   /* jogada ruim */
    if (!found) {
        float bd = 1e9f;
        for (t = 1; t < 16; t++) {
            float dx, dy, d;
            if (!b[t].in) continue;
            if (t == 8 && !(gr >= 0 && countGroup(gr) == 0)) continue;
            if (gr >= 0 && t != 8 && ballGroup(t) != gr) continue;
            dx = b[t].x - cx; dy = b[t].y - cy; d = dx*dx + dy*dy;
            if (d < bd) { bd = d; botAng = atan2f(dy, dx); }
        }
        if (bd > 1e8f) botAng = atan2f(TCY - cy, FOOTX - cx);
        botPow = 0.65f;
    }
    botPow = botPowFix(botPow);
    botPow += (((int)(rnd() % 2001)) - 1000) / 1000.0f * BOT_POWV[botLevel];
    if (botPow < botPowFix(0.25f)) botPow = botPowFix(0.25f);
    if (breakShot) botPow = 1.0f;
    if (botPow > 1.0f) botPow = 1.0f;
    botAng += (((int)(rnd() % 2001)) - 1000) / 1000.0f * (BOT_ERR[botLevel] + (curse[cur] ? 0.03f : 0.0f) + (sirenT[cur] ? 0.05f : 0.0f));   /* maldicao e sirene: o bot erra mais */
    if (eightCall()) {                                       /* MESTRE: o bot canta a cacapa da bola 8 (a da tacada que ele planejou) */
        callPk = (found && bt == 8 && bpk >= 0) ? bpk : eightBestPocket();
        { char cb[64]; sprintf(cb, "BOT CANTOU A CACAPA DA 8: %s", PK_NAME[callPk]); setmsg(cb); }
    }
    botKit(found, bt, bdist, bcc);
}

/* ---------- entrada ---------- */
/* analogico: sticks de -1 a 1 (zona morta radial); stX/stY = esquerdo, rsX/rsY = direito; anX.. = versao suavizada usada no jogo */
static float stX[2], stY[2], rsX[2], rsY[2], anX, anY, anRX, anRY;
static int padAnalogSet[2];

static void stickDead(int hx, int vy, float *ox, float *oy)
{
    float x = (hx - 128) / 128.0f, y = (vy - 128) / 128.0f, m = sqrtf(x * x + y * y);
    if (m < 0.2f) { *ox = 0.0f; *oy = 0.0f; return; }
    m = (m > 1.0f ? 1.0f : m);
    { float k = (m - 0.2f) / 0.8f / sqrtf(x * x + y * y); *ox = x * k; *oy = y * k; }
}

static u32 readpad(int port)
{
    struct padButtonStatus s;
    int st = padGetState(port, 0);
    stX[port] = stY[port] = rsX[port] = rsY[port] = 0.0f;
    if (st != PAD_STATE_STABLE && st != PAD_STATE_FINDCTP1) { padAnalogSet[port] = 0; return 0; }
    if (!padAnalogSet[port] && st == PAD_STATE_STABLE) {          /* liga o modo analogico (o jogador ainda pode trocar no botao ANALOG) */
        padAnalogSet[port] = 1;
        if (padInfoMode(port, 0, PAD_MODETABLE, -1) > 0) padSetMainMode(port, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_UNLOCK);
    }
    if (padRead(port, 0, &s) == 0) return 0;
    if ((s.mode >> 4) == PAD_TYPE_DUALSHOCK) {
        stickDead(s.ljoy_h, s.ljoy_v, &stX[port], &stY[port]);
        stickDead(s.rjoy_h, s.rjoy_v, &rsX[port], &rsY[port]);
    }
    return 0xffff ^ s.btns;
}

/* ---------- desenho do jogo ---------- */
typedef struct { const char *name; u8 felt[3], wood[3], dia[3]; int style; } TableDef;   /* style: 0 normal, 1 preta/ouro, 2 neon animada */
#define NTABLES 28
static const TableDef TABLES[NTABLES] = {
    { "CLASSICA", { 24,114, 68}, {124, 74, 33}, {236,226,190} },
    { "FENIX",    { 14,128, 52}, { 34, 34, 38}, {240,240,244} },
    { "OCEANO",   { 28, 92,170}, {118, 78, 40}, {236,226,190} },
    { "VINHO",    {130, 28, 44}, { 70, 38, 22}, {230,210,170} },
    { "ROYAL",    { 98, 52,150}, { 30, 30, 38}, {230,226,240} },
    { "PRETA OURO", { 26, 26, 34}, { 18, 16, 14}, {212,175, 80}, 1 },
    { "NEON",     {  8, 18, 46}, { 14, 14, 28}, {120,220,255}, 2 },
    { "FOGO",     { 74, 12, 10}, { 34, 14, 10}, {255,170, 60}, 3 },
    { "GELO",     {118,186,224}, {206,228,244}, {255,255,255}, 4 },
    { "GALAXIA",  { 20, 10, 56}, { 20, 16, 38}, {200,170,255}, 5 },
    { "MATRIX",   {  6, 38, 18}, {  6, 22, 10}, { 60,255,100}, 6 },
    { "VEGAS",    {150, 16, 32}, { 58, 20, 16}, {255,215, 90}, 7 },
    { "ARCO-IRIS",{ 44, 44, 70}, {236,236,242}, {255,255,255}, 8 },
    { "ROSA",     {198, 72,132}, { 92, 32, 62}, {255,226,236} },
    { "TURQUESA", { 12,138,150}, { 22, 52, 62}, {230,250,250} },
    { "FAROESTE", {176,130, 74}, { 74, 42, 22}, {244,210,140}, 9 },
    { "NOITE LUA",{ 10, 16, 44}, { 12, 14, 30}, {190,205,255}, 10 },
    { "HALLOWEEN",{ 74, 30, 98}, { 22, 12, 10}, {255,140, 24}, 11 },
    { "VAMPIRO",  {112, 12, 32}, { 22, 10, 16}, {232,232,242}, 12 },
    { "NATAL",    { 16,100, 54}, {158, 26, 34}, {255,255,255}, 13 },
    { "HACK",     {  6, 18, 30}, { 10, 14, 20}, {  0,255,210}, 14 },
    { "CHAPOLIN", {176, 30, 38}, {236,186, 30}, {255,230, 90}, 15 },
    { "TEMPO",    { 18, 30, 78}, { 20, 24, 56}, {255,214, 90}, 16 },
    { "GRINCH",   { 74,158, 48}, { 40, 26, 30}, {255, 70, 60}, 17 },
    { "ABISMO",   {  8, 62,122}, { 10, 30, 56}, {130,235,255}, 18 },
    { "CEMITERIO",{ 30, 52, 44}, { 34, 28, 38}, {180,200,180}, 19 },
    { "DELEGACIA",{ 38, 66,122}, { 20, 26, 48}, {255,215, 80}, 20 },
    { "POOL AZUL",{ 36,136,204}, {142, 26, 38}, {238,232,214}, 21 },   /* feltro azul, madeira vermelha, diamantes claros, cacapas pretas */
};
static u64 tcCush, tcCushHi, tcCushLo, tcDia, tcRail, tcMouth;     /* cores derivadas da mesa escolhida (tcMouth: boca escura das cacapas) */

static void cushH(float xa, float xb, float yo, float yi, float ra, float rb)   /* ra/rb: inclinacao da ponta (canto = 11, meio = 3 quase reto) */
{
    float la = xa - ra, lb = xb + rb;
    if (ra > 8.0f) quad(xa - 20, yo, xa - ra, yo, xa, yi, xa - 20, yi, tcMouth);   /* canto: boca aberta (escura) entre a ponta inclinada da tabela e o buraco */
    if (rb > 8.0f) quad(xb + rb, yo, xb + 20, yo, xb + 20, yi, xb, yi, tcMouth);
    quad(xa - ra, yo, xb + rb, yo, xb, yi, xa, yi, tcCush);
    line(xa, yi, xb, yi, tcCushHi);
    line(la, yo, lb, yo, tcCushLo);
    line(xa - ra, yo, xa, yi, tcCushHi);                                 /* pontas inclinadas (meio e cantos) */
    line(xb + rb, yo, xb, yi, tcCushHi);
}

static void cushV(float ya, float yb, float xo, float xi)
{
    quad(xo, ya - 20, xo, ya - 11, xi, ya, xi, ya - 20, tcMouth);         /* canto: boca aberta (escura) entre a ponta inclinada da tabela e o buraco */
    quad(xo, yb + 11, xo, yb + 20, xi, yb + 20, xi, yb, tcMouth);
    quad(xo, ya - 11, xo, yb + 11, xi, yb, xi, ya, tcCush);
    line(xi, ya, xi, yb, tcCushHi);
    line(xo, ya - 11, xo, yb + 11, tcCushLo);
    line(xo, ya - 11, xi, ya, tcCushHi);                                  /* pontas inclinadas dos cantos */
    line(xo, yb + 11, xi, yb, tcCushHi);
}

static void arcBand(float cx, float cy, float r0, float r1, float a0, float a1, u64 c)   /* faixa de arco (chapa metalica do canto) */
{
    int n = 16, i;
    for (i = 0; i < n; i++) {
        float t0 = a0 + (a1 - a0) * i / n, t1 = a0 + (a1 - a0) * (i + 1) / n;
        quad(cx + cosf(t0) * r0, cy + sinf(t0) * r0, cx + cosf(t1) * r0, cy + sinf(t1) * r0,
             cx + cosf(t1) * r1, cy + sinf(t1) * r1, cx + cosf(t0) * r1, cy + sinf(t0) * r1, c);
    }
}

static void diamond(float cx, float cy)
{
    quad(cx, cy - 3, cx + 3, cy, cx, cy + 3, cx - 3, cy, tcDia);
}


/* ---------- mesas especiais: PRETA OURO (detalhes dourados) e NEON (animada) ---------- */
static u64 CA(int r, int gg, int bb, int a);        /* definida mais abaixo */
static void glowFade(float x1, float y1, float x2, float y2, int a0, int a1, int r, int gg, int bb);   /* idem */
static void tri(float x1, float y1, float x2, float y2, float x3, float y3, u64 c);   /* idem */
static void ringN(float cx, float cy, float r, float w, int n, u64 c);   /* idem */

static void hue3(float h, int *r, int *gg, int *bb)
{
    float x, f;
    int i;
    h -= floorf(h); x = h * 6.0f; i = (int)x; f = x - i;
    switch (i % 6) {
    case 0:  *r = 255; *gg = (int)(255 * f); *bb = 0; break;
    case 1:  *r = (int)(255 * (1.0f - f)); *gg = 255; *bb = 0; break;
    case 2:  *r = 0; *gg = 255; *bb = (int)(255 * f); break;
    case 3:  *r = 0; *gg = (int)(255 * (1.0f - f)); *bb = 255; break;
    case 4:  *r = (int)(255 * f); *gg = 0; *bb = 255; break;
    default: *r = 255; *gg = 0; *bb = (int)(255 * (1.0f - f)); break;
    }
}

static u64 hueCol(float h, int a)
{
    int r, gg, bb;
    hue3(h, &r, &gg, &bb);
    return GS_SETREG_RGBAQ(r, gg, bb, a, 0x00);
}

static void outlineRect(float x1, float y1, float x2, float y2, u64 c)
{
    line(x1, y1, x2, y1, c); line(x1, y2, x2, y2, c);
    line(x1, y1, x1, y2, c); line(x2, y1, x2, y2, c);
}

/* ponto k de N ao longo do retangulo da mesa, a 'g' pixels da borda; (nx,ny) aponta para fora */
static void perim(int k, int N, float g, float *px, float *py, float *nx, float *ny)
{
    float W = TX1 - TX0 + 2 * g, H = TY1 - TY0 + 2 * g, L = 2 * (W + H), q = k * L / N;
    if (q < W)              { *px = TX0 - g + q;              *py = TY0 - g;                 *nx = 0;  *ny = -1; }
    else if (q < W + H)     { *px = TX1 + g;                  *py = TY0 - g + (q - W);       *nx = 1;  *ny = 0; }
    else if (q < 2 * W + H) { *px = TX1 + g - (q - W - H);    *py = TY1 + g;                 *nx = 0;  *ny = 1; }
    else                    { *px = TX0 - g;                  *py = TY1 + g - (q - 2 * W - H); *nx = -1; *ny = 0; }
}

/* mesas animadas novas: 3 FOGO, 4 GELO, 5 GALAXIA, 6 MATRIX, 7 VEGAS, 8 ARCO-IRIS */
static void tableDecoFx(int stage, int style, int mt)
{
    int k, c, j;
    float t = mt * 0.02f, Hh = TY1 - TY0;

    if (style == 3) {                                          /* FOGO */
        if (stage == 0) {
            for (k = 0; k < 72; k++) {                         /* chamas na moldura */
                float px, py, nx, ny, tx, ty, h, sw;
                perim(k, 72, 13.0f, &px, &py, &nx, &ny);
                tx = -ny; ty = nx;
                h = 5.0f + 6.0f * (0.5f + 0.5f * sinf(mt * 0.25f + k * 1.7f));
                sw = 2.0f * sinf(mt * 0.15f + k);
                tri(px - tx * 3.2f, py - ty * 3.2f, px + tx * 3.2f, py + ty * 3.2f,
                    px + nx * h + tx * sw, py + ny * h + ty * sw, CA(255, 80, 20, 0x58));
                tri(px - tx * 1.6f, py - ty * 1.6f, px + tx * 1.6f, py + ty * 1.6f,
                    px + nx * h * 0.55f + tx * sw * 0.5f, py + ny * h * 0.55f + ty * sw * 0.5f, CA(255, 215, 70, 0x68));
            }
        } else if (stage == 1) {
            rect(TX0, TY1 - 52, TX1, TY1, CA(255, 70, 10, 0x0c));
            rect(TX0, TY1 - 24, TX1, TY1, CA(255, 70, 10, 0x0c));
            for (k = 0; k < 26; k++) {                         /* brasas subindo */
                float sp = 0.5f + (k % 5) * 0.18f;
                float ey = TY1 - fmodf(mt * sp + k * 47.0f, Hh), life = (TY1 - ey) / Hh;
                float ex = TX0 + 8 + fmodf(k * 83.7f, 510.0f) + 5.0f * sinf(mt * 0.05f + k);
                rect(ex - 1.2f, ey - 1.2f, ex + 1.2f, ey + 1.2f, CA(255, 210 - (int)(150 * life), 30, 8 + (int)(0x60 * (1.0f - life))));
            }
        } else {
            for (k = 0; k < 6; k++) {
                float pulse = 0.5f + 0.5f * sinf(mt * 0.2f + k * 1.3f);
                discN(PX[k], PY[k], PR[k] + 7.0f, CA(255, 90, 20, 0x10), 10);
                ring(PX[k], PY[k], PR[k] + 4.5f, CA(255, 150 + (int)(60 * pulse), 40, 0x78));
            }
            line(HEADX, TY0, HEADX, TY1, CA(255, 140, 50, 0x30));
        }
    } else if (style == 4) {                                   /* GELO */
        if (stage == 0) {
            outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, CA(220, 240, 255, 0x60));
            for (k = 0; k < 44; k++) {                         /* brilhos de gelo na moldura */
                float px, py, nx, ny, tw2 = sinf(mt * 0.08f + k * 2.3f);
                if (tw2 < 0.2f) continue;
                perim(k, 44, 21.0f, &px, &py, &nx, &ny);
                line(px - 2.5f, py, px + 2.5f, py, CA(255, 255, 255, 0x20 + (int)(0x60 * tw2)));
                line(px, py - 2.5f, px, py + 2.5f, CA(255, 255, 255, 0x20 + (int)(0x60 * tw2)));
            }
            for (k = 0; k < 4; k++) {                          /* flocos nos cantos */
                float cx = (k & 1) ? TX1 + 22 : TX0 - 22, cy = (k & 2) ? TY1 + 22 : TY0 - 22;
                for (j = 0; j < 3; j++) {
                    float a = j * 1.0472f;
                    line(cx - cosf(a) * 6, cy - sinf(a) * 6, cx + cosf(a) * 6, cy + sinf(a) * 6, CA(255, 255, 255, 0x78));
                }
            }
        } else if (stage == 1) {
            float bx = fmodf(mt * 1.1f, 760.0f) - 140.0f, x1 = bx < TX0 ? TX0 : bx, x2 = bx + 34.0f > TX1 ? TX1 : bx + 34.0f;
            outlineRect(TX0 + 3, TY0 + 3, TX1 - 3, TY1 - 3, CA(255, 255, 255, 0x28));
            if (x2 > x1) rect(x1, TY0, x2, TY1, CA(255, 255, 255, 0x0e));
            for (k = 0; k < 30; k++) {                         /* neve caindo */
                float fy = TY0 + fmodf(mt * 0.5f * (0.6f + (k % 4) * 0.25f) + k * 41.0f, Hh);
                float fx = TX0 + 6 + fmodf(k * 113.7f, 516.0f) + 6.0f * sinf(mt * 0.03f + k);
                disc(fx, fy, 1.3f, CA(255, 255, 255, 0x58));
            }
        } else {
            for (k = 0; k < 6; k++) {
                ring(PX[k], PY[k], PR[k] + 4.5f, CA(190, 235, 255, 0x80));
                ring(PX[k], PY[k], PR[k] + 6.5f, CA(190, 235, 255, 0x38));
            }
        }
    } else if (style == 5) {                                   /* GALAXIA */
        if (stage == 0) {
            outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, CA(150, 100, 255, 0x40 + (int)(0x20 * sinf(t))));
            for (k = 0; k < 34; k++) {
                float px, py, nx, ny, tw2 = 0.5f + 0.5f * sinf(mt * 0.07f + k * 1.9f);
                perim(k, 34, 20.0f, &px, &py, &nx, &ny);
                rect(px - 1.0f, py - 1.0f, px + 1.0f, py + 1.0f, CA(210, 180, 255, 0x20 + (int)(0x50 * tw2)));
            }
        } else if (stage == 1) {
            int cyc = mt / 300;
            float ph = fmodf((float)mt, 300.0f) / 300.0f;
            discN(TCX + 120.0f * sinf(t * 0.7f), TCY + 40.0f * cosf(t * 0.5f), 90.0f, CA(110, 50, 200, 0x0a), 20);
            discN(TCX - 140.0f * sinf(t * 0.5f + 1.0f), TCY - 30.0f * cosf(t * 0.6f), 70.0f, CA(30, 110, 220, 0x0a), 20);
            discN(TCX + 60.0f * cosf(t * 0.4f), TCY + 50.0f * sinf(t * 0.9f), 60.0f, CA(220, 60, 160, 0x08), 20);
            for (k = 0; k < 38; k++) {                         /* estrelas */
                float sx = TX0 + 10 + fmodf(k * 151.7f, 508.0f), sy = TY0 + 8 + fmodf(k * 67.3f, 248.0f);
                float tw2 = 0.5f + 0.5f * sinf(mt * 0.06f + k * 1.7f);
                int a = 0x10 + (int)(0x50 * tw2);
                disc(sx, sy, k % 7 == 0 ? 1.7f : 1.1f, (k % 3 == 0) ? CA(255, 235, 170, a) : ((k % 3 == 1) ? CA(190, 210, 255, a) : CA(255, 255, 255, a)));
            }
            if (ph < 0.25f) {                                  /* estrela cadente */
                float u = ph / 0.25f, sx = TX0 + 40 + (cyc % 2) * 140 + u * 320.0f, sy = TY0 + 20 + (cyc % 3) * 60 + u * 110.0f;
                line(sx - 36, sy - 14, sx, sy, CA(255, 255, 255, 0x50));
                line(sx - 18, sy - 7, sx, sy, CA(255, 255, 255, 0x7c));
                disc(sx, sy, 1.7f, CA(255, 255, 255, 0x7c));
            }
        } else {
            for (k = 0; k < 6; k++) {                          /* cacapas viram buracos negros */
                float a = mt * 0.1f + k;
                discN(PX[k], PY[k], PR[k] + 6.0f, CA(90, 40, 170, 0x18), 12);
                ring(PX[k], PY[k], PR[k] + 4.5f, CA(180, 130, 255, 0x80));
                disc(PX[k] + cosf(a) * (PR[k] + 8.0f), PY[k] + sinf(a) * (PR[k] + 8.0f), 1.8f, CA(255, 255, 255, 0x78));
            }
        }
    } else if (style == 6) {                                   /* MATRIX */
        if (stage == 0) {
            int pulse = (int)(0x1c * (1.0f + sinf(mt * 0.1f)));
            outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, CA(60, 255, 100, 0x48 + pulse));
            outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, CA(20, 160, 60, 0x40));
        } else if (stage == 1) {
            for (c = 0; c < 26; c++) {                         /* chuva de codigo */
                float x = TX0 + 10 + c * 20.0f, sp = 1.2f + (c % 5) * 0.4f;
                float hy = fmodf(mt * sp + c * 61.0f, Hh + 90.0f) - 10.0f;
                for (j = 0; j < 7; j++) {
                    float y = hy - j * 11.0f;
                    int a = (j == 0) ? 0x78 : 0x50 - j * 10;
                    if (y < 0 || y > Hh - 8) continue;
                    if (a < 6) a = 6;
                    rect(x, TY0 + y, x + 5, TY0 + y + 8, j == 0 ? CA(200, 255, 210, a) : CA(40, 230, 90, a));
                    if (((mt / 6) + c * 3 + j) % 3 == 0) rect(x + 1.5f, TY0 + y + 2, x + 3.5f, TY0 + y + 6, CA(0, 20, 0, a));
                }
            }
        } else {
            for (k = 0; k < 6; k++) {
                ring(PX[k], PY[k], PR[k] + 4.5f, CA(60, 255, 100, 0x80));
                ring(PX[k], PY[k], PR[k] + 6.5f, CA(60, 255, 100, 0x30));
            }
        }
    } else if (style == 7) {                                   /* VEGAS */
        if (stage == 0) {
            for (k = 0; k < 84; k++) {                         /* lampadas piscando em volta */
                float px, py, nx, ny;
                perim(k, 84, 26.5f, &px, &py, &nx, &ny);
                if (((k + mt / 7) % 3) == 0) {
                    discN(px, py, 5.0f, CA(255, 200, 60, 0x14), 8);
                    rect(px - 2.0f, py - 2.0f, px + 2.0f, py + 2.0f, CA(255, 240, 150, 0x7c));
                } else rect(px - 1.5f, py - 1.5f, px + 1.5f, py + 1.5f, CA(120, 90, 40, 0x60));
            }
        } else if (stage == 1) {
            outlineRect(TX0 + 9, TY0 + 9, TX1 - 9, TY1 - 9, CA(255, 215, 90, 0x30));
            ring(TCX, TCY, 30.0f, CA(255, 215, 90, 0x28)); ring(TCX, TCY, 38.0f, CA(255, 215, 90, 0x28));
            for (k = 0; k < 2; k++) {                          /* losangos (naipe) */
                float dx = k ? TX1 - 60.0f : TX0 + 60.0f;
                quad(dx, TCY - 8, dx + 5, TCY, dx, TCY + 8, dx - 5, TCY, CA(255, 215, 90, 0x30));
            }
            for (k = 0; k < 8; k++) {                          /* brilhos */
                float sx = TX0 + 20 + fmodf(k * 173.3f, 488.0f), sy = TY0 + 14 + fmodf(k * 91.7f, 236.0f);
                float tw2 = sinf(mt * 0.09f + k * 2.2f);
                if (tw2 < 0.1f) continue;
                line(sx - 3, sy, sx + 3, sy, CA(255, 240, 170, 0x20 + (int)(0x60 * tw2)));
                line(sx, sy - 3, sx, sy + 3, CA(255, 240, 170, 0x20 + (int)(0x60 * tw2)));
            }
        } else {
            for (k = 0; k < 6; k++) {
                int on = (((k + mt / 9) & 1) == 0);
                ring(PX[k], PY[k], PR[k] + 4.5f, on ? CA(255, 225, 100, 0x80) : CA(190, 140, 50, 0x50));
            }
        }
    } else if (style == 8) {                                   /* ARCO-IRIS */
        if (stage == 0) {
            outlineRect(TX0 - 14.0f, TY0 - 14.0f, TX1 + 14.0f, TY1 + 14.0f, hueCol(t * 0.4f, 0x70));
            outlineRect(TX0 - 17.5f, TY0 - 17.5f, TX1 + 17.5f, TY1 + 17.5f, hueCol(t * 0.4f + 0.33f, 0x70));
            outlineRect(TX0 - 21.0f, TY0 - 21.0f, TX1 + 21.0f, TY1 + 21.0f, hueCol(t * 0.4f + 0.66f, 0x70));
        } else if (stage == 1) {
            for (k = 0; k < 8; k++)                            /* faixas de cor no feltro */
                rect(TX0, TY0 + k * Hh / 8.0f, TX1, TY0 + (k + 1) * Hh / 8.0f, hueCol(t * 0.15f + k / 8.0f, 0x24));
        } else {
            for (k = 0; k < 6; k++) ring(PX[k], PY[k], PR[k] + 4.5f, hueCol(k / 6.0f + mt * 0.006f, 0x80));
        }
    } else if (style == 9) {                                   /* FAROESTE */
        float W = TX1 - TX0;
        if (stage == 0) {
            outlineRect(TX0 - 12.0f, TY0 - 12.0f, TX1 + 12.0f, TY1 + 12.0f, CA(222, 176, 100, 0x50));   /* corda */
            outlineRect(TX0 - 13.5f, TY0 - 13.5f, TX1 + 13.5f, TY1 + 13.5f, CA(120, 80, 40, 0x50));
            for (k = 0; k < 72; k++) {                         /* pregos nas tabuas */
                float px, py, nx, ny;
                perim(k, 72, 24.0f, &px, &py, &nx, &ny);
                discN(px, py, 2.0f, CA(28, 20, 14, 0x70), 6);
                rect(px - 0.8f, py - 1.2f, px + 0.4f, py - 0.2f, CA(235, 205, 150, 0x70));
            }
        } else if (stage == 1) {
            float tx = TX0 - 20.0f + fmodf(mt * 1.1f, W + 40.0f), ty = TY1 - 30.0f - fabsf(sinf(mt * 0.06f)) * 22.0f, a = mt * 0.12f;
            for (k = 0; k < 10; k++) {                         /* estrela de xerife no centro */
                float a0 = -1.5708f + k * 0.62832f, a1 = a0 + 0.62832f;
                float r0 = (k & 1) ? 14.0f : 34.0f, r1 = (k & 1) ? 34.0f : 14.0f;
                tri(TCX, TCY, TCX + cosf(a0) * r0, TCY + sinf(a0) * r0, TCX + cosf(a1) * r1, TCY + sinf(a1) * r1, CA(96, 54, 22, 0x24));
            }
            ring(TCX, TCY, 42.0f, CA(96, 54, 22, 0x30));
            for (k = 0; k < 2; k++) {                          /* cactos nos cantos */
                float cx0 = k ? TX1 - 30.0f : TX0 + 26.0f, cy0 = k ? TY0 + 44.0f : TY1 - 6.0f;
                u64 cg = CA(46, 96, 44, 0x58);
                rect(cx0 - 3, cy0 - 30, cx0 + 3, cy0, cg);
                rect(cx0 - 11, cy0 - 20, cx0 - 3, cy0 - 15, cg); rect(cx0 - 11, cy0 - 28, cx0 - 7, cy0 - 20, cg);
                rect(cx0 + 3, cy0 - 14, cx0 + 11, cy0 - 9, cg);  rect(cx0 + 7, cy0 - 23, cx0 + 11, cy0 - 14, cg);
            }
            for (k = 0; k < 4; k++) {                          /* poeira soprando */
                float dxp = TX0 - 50.0f + fmodf(mt * (1.5f + k * 0.4f) + k * 160.0f, W + 100.0f), dyp = TY0 + 30.0f + k * 60.0f;
                line(dxp, dyp, dxp + 24.0f, dyp, CA(240, 215, 165, 0x1c));
                line(dxp + 6.0f, dyp + 3.0f, dxp + 20.0f, dyp + 3.0f, CA(240, 215, 165, 0x12));
            }
            ringN(tx, ty, 11.0f, 1.6f, 14, CA(112, 76, 38, 0x70));                  /* bola de mato rolando */
            for (k = 0; k < 3; k++) {
                float ca = a + k * 1.0472f;
                line(tx - cosf(ca) * 10.0f, ty - sinf(ca) * 10.0f, tx + cosf(ca) * 10.0f, ty + sinf(ca) * 10.0f, CA(112, 76, 38, 0x60));
            }
        } else {
            for (k = 0; k < 6; k++) {
                ring(PX[k], PY[k], PR[k] + 4.5f, CA(190, 130, 66, 0x80));
                ring(PX[k], PY[k], PR[k] + 6.0f, CA(110, 70, 34, 0x60));
            }
        }
    }
}

/* ---------- mesa NOITE LUA (style 10): ceu escuro, lua crescente, estrelas, nuvens e estrela cadente ---------- */
static void moonCrescent(float cx, float cy, float r, int a, int cr, int cg, int cb)
{
    float dy, ri = r * 0.84f, off = r * 0.42f, step = r > 12.0f ? 1.5f : 1.0f;
    for (dy = -r + 0.01f; dy < r; dy += step) {
        float wo = sqrtf(r * r - dy * dy), xl = cx - wo, xr = cx + wo, wi2 = ri * ri - dy * dy;
        if (wi2 > 0.0f) {
            float wi = sqrtf(wi2), ia = cx + off - wi, ib = cx + off + wi;
            if (ia > xl) rect(xl, cy + dy, ia < xr ? ia : xr, cy + dy + step, CA(cr, cg, cb, a));
            if (ib < xr) rect(ib, cy + dy, xr, cy + dy + step, CA(cr, cg, cb, a));
        } else {
            rect(xl, cy + dy, xr, cy + dy + step, CA(cr, cg, cb, a));
        }
    }
}

static void tableDecoNight(int stage, int mt)
{
    int k;
    float W = TX1 - TX0;
    if (stage == 0) {
        outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, COL(150,162,200));
        outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, CA(90, 105, 160, 0x60));
        for (k = 0; k < 28; k++) {                          /* estrelinhas na moldura */
            float px, py, nx, ny, tw2 = sinf(mt * 0.06f + k * 2.3f);
            perim(k, 28, 19.0f, &px, &py, &nx, &ny);
            if (tw2 < 0) tw2 = 0;
            rect(px - 1.0f, py - 1.0f, px + 1.0f, py + 1.0f, CA(215, 225, 255, 0x28 + (int)(0x50 * tw2)));
        }
    } else if (stage == 1) {
        float ph = fmodf((float)mt, 480.0f);
        discN(TCX, TCY, 92.0f, CA(150, 175, 255, 0x05), 28);           /* brilho da lua */
        discN(TCX, TCY, 70.0f, CA(170, 190, 255, 0x06), 28);
        discN(TCX, TCY, 50.0f, CA(190, 205, 255, 0x08), 28);
        moonCrescent(TCX, TCY, 36.0f, 0x44, 236, 236, 214);
        moonCrescent(TCX, TCY, 36.0f, 0x14, 255, 255, 240);
        for (k = 0; k < 40; k++) {                                     /* estrelas piscando */
            float sx = TX0 + 10 + fmodf(k * 137.5f, 508.0f), sy = TY0 + 8 + fmodf(k * 71.3f, 248.0f);
            float tw2 = sinf(mt * 0.05f + k * 1.9f);
            if (tw2 < 0) tw2 = 0;
            disc(sx, sy, (k % 5 == 0) ? 1.6f : 1.1f, CA(205, 220, 255, 0x10 + (int)(0x58 * tw2)));
            if (k % 7 == 0 && tw2 > 0.5f) {                            /* brilho em cruz nas maiores */
                line(sx - 3.5f, sy, sx + 3.5f, sy, CA(205, 220, 255, 0x30));
                line(sx, sy - 3.5f, sx, sy + 3.5f, CA(205, 220, 255, 0x30));
            }
        }
        for (k = 0; k < 3; k++) {                                      /* nuvens passando devagar */
            float cx0 = TX0 - 70.0f + fmodf(mt * (0.18f + k * 0.07f) + k * 230.0f, W + 140.0f);
            float cy0 = TY0 + 50.0f + k * 70.0f;
            discN(cx0, cy0, 16.0f, CA(120, 135, 190, 0x0a), 12);
            discN(cx0 + 18.0f, cy0 - 5.0f, 20.0f, CA(120, 135, 190, 0x0a), 12);
            discN(cx0 + 40.0f, cy0, 15.0f, CA(120, 135, 190, 0x0a), 12);
        }
        if (ph < 45.0f) {                                              /* estrela cadente */
            float u = ph / 45.0f, sx = TX0 + 60.0f + u * 220.0f, sy = TY0 + 18.0f + u * 90.0f;
            int a = (int)(0x70 * (1.0f - u));
            glowFade(sx - 46.0f, sy - 20.0f, sx, sy, 0, a, 215, 225, 255);
            disc(sx, sy, 1.8f, CA(255, 255, 255, a));
        }
    } else {
        for (k = 0; k < 6; k++) {
            float pulse = 0.5f + 0.5f * sinf(mt * 0.08f + k * 1.1f);
            discN(PX[k], PY[k], PR[k] + 7.0f, CA(150, 175, 255, 0x0c + (int)(0x08 * pulse)), 12);
            ring(PX[k], PY[k], PR[k] + 4.5f, CA(175, 192, 255, 0x78));
        }
        line(HEADX, TY0, HEADX, TY1, CA(170, 190, 255, 0x2c));
    }
}


/* ---------- mesa HALLOWEEN (style 11): abobora, morcegos, fantasma, aranha, teias e luzes laranja/verde ---------- */
static void batShape(float x, float y, float sc, float fl, int a)
{
    float w = 13.0f * sc, h = (3.0f + 7.0f * fl) * sc;           /* fl: -1..1 (asa batendo) */
    u64 c = CA(12, 5, 20, a);
    tri(x, y - 2.0f * sc, x - w, y - h, x - w * 0.45f, y + 3.0f * sc, c);
    tri(x, y - 2.0f * sc, x + w, y - h, x + w * 0.45f, y + 3.0f * sc, c);
    discN(x, y + 0.5f * sc, 3.2f * sc, c, 8);
    tri(x - 2.4f * sc, y - 2.0f * sc, x - 1.2f * sc, y - 6.0f * sc, x, y - 2.0f * sc, c);
    tri(x + 2.4f * sc, y - 2.0f * sc, x + 1.2f * sc, y - 6.0f * sc, x, y - 2.0f * sc, c);
}

static void pumpkinJ(float x, float y, float r, float flick)
{
    u64 og = COL(246, 128, 18), od = COL(214, 90, 8), ink = COL(46, 14, 4);
    discN(x, y, r * 2.0f, CA(255, 120, 20, (int)(0x14 * flick)), 14);       /* brilho da vela */
    discN(x - r * 0.5f, y, r * 0.85f, od, 12);
    discN(x + r * 0.5f, y, r * 0.85f, od, 12);
    discN(x, y, r * 0.92f, og, 12);
    rect(x - r * 0.13f, y - r * 1.2f, x + r * 0.13f, y - r * 0.8f, COL(44, 96, 24));
    tri(x - r * 0.55f, y - r * 0.05f, x - r * 0.15f, y - r * 0.05f, x - r * 0.35f, y - r * 0.5f, ink);
    tri(x + r * 0.15f, y - r * 0.05f, x + r * 0.55f, y - r * 0.05f, x + r * 0.35f, y - r * 0.5f, ink);
    tri(x - r * 0.55f, y + r * 0.25f, x + r * 0.55f, y + r * 0.25f, x, y + r * 0.62f, ink);
    tri(x - r * 0.2f, y + r * 0.25f, x + r * 0.05f, y + r * 0.25f, x - r * 0.08f, y + r * 0.1f, og);
}

static void webCorner(float x, float y, float dx, float dy, float len, int a)
{
    int k, j;
    for (k = 0; k < 4; k++) {
        float an = k * 0.5236f;
        line(x, y, x + dx * len * cosf(an), y + dy * len * sinf(an), CA(225, 225, 240, a));
    }
    for (j = 1; j <= 3; j++)
        for (k = 0; k < 3; k++) {
            float r = len * j / 3.0f, a0 = k * 0.5236f, a1 = (k + 1) * 0.5236f, m = r * 0.93f, am = (a0 + a1) * 0.5f;
            line(x + dx * r * cosf(a0), y + dy * r * sinf(a0), x + dx * m * cosf(am), y + dy * m * sinf(am), CA(225, 225, 240, a));
            line(x + dx * m * cosf(am), y + dy * m * sinf(am), x + dx * r * cosf(a1), y + dy * r * sinf(a1), CA(225, 225, 240, a));
        }
}

static void tableDecoHalloween(int stage, int mt)
{
    int k;
    float W = TX1 - TX0;
    if (stage == 0) {
        outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, COL(255, 130, 20));
        outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, CA(120, 255, 70, 0x50));
        for (k = 0; k < 30; k++) {                                   /* luzes laranja e verdes piscando */
            float px, py, nx, ny, pu = 0.5f + 0.5f * sinf(mt * 0.1f + k * 0.9f);
            perim(k, 30, 19.0f, &px, &py, &nx, &ny);
            if (k & 1) { discN(px, py, 4.5f, CA(255, 130, 20, 0x14), 8);  rect(px - 1.5f, py - 1.5f, px + 1.5f, py + 1.5f, CA(255, 170, 50, 0x40 + (int)(0x40 * pu))); }
            else       { discN(px, py, 4.5f, CA(110, 255, 70, 0x14), 8);  rect(px - 1.5f, py - 1.5f, px + 1.5f, py + 1.5f, CA(140, 255, 100, 0x40 + (int)(0x40 * pu))); }
        }
        for (k = 0; k < 4; k++)                                      /* aboboras nos cantos */
            pumpkinJ((k & 1) ? TX1 + 22.0f : TX0 - 22.0f, (k & 2) ? TY1 + 22.0f : TY0 - 22.0f, 7.5f, 0.7f + 0.3f * sinf(mt * 0.13f + k * 1.9f));
    } else if (stage == 1) {
        float ph = fmodf((float)mt, 600.0f);
        discN(TCX, TCY, 64.0f, CA(255, 150, 30, 0x06), 28);            /* lua alaranjada enorme */
        discN(TCX, TCY, 44.0f, CA(255, 170, 50, 0x0a), 28);
        discN(TCX, TCY, 30.0f, CA(255, 190, 80, 0x10), 24);
        for (k = 0; k < 5; k++) {                                      /* neblina passando */
            float fx = TX0 - 60.0f + fmodf(mt * (0.25f + k * 0.07f) + k * 170.0f, W + 120.0f), fy = TY0 + 30.0f + k * 52.0f;
            discN(fx, fy, 34.0f, CA(190, 160, 240, 0x07), 14);
            discN(fx + 30.0f, fy + 6.0f, 26.0f, CA(190, 160, 240, 0x07), 14);
        }
        webCorner(TX0, TY0,  1.0f,  1.0f, 52.0f, 0x34);
        webCorner(TX1, TY0, -1.0f,  1.0f, 52.0f, 0x34);
        webCorner(TX0, TY1,  1.0f, -1.0f, 52.0f, 0x34);
        webCorner(TX1, TY1, -1.0f, -1.0f, 52.0f, 0x34);
        {                                                              /* aranha descendo da tabela */
            float sx = TX1 - 120.0f, len = 26.0f + 24.0f * (0.5f + 0.5f * sinf(mt * 0.03f)), sy = TY0 + len;
            line(sx, TY0, sx, sy, CA(235, 235, 245, 0x60));
            for (k = -1; k <= 1; k += 2) {
                line(sx, sy, sx + k * 9.0f, sy - 5.0f, CA(30, 14, 40, 0x90)); line(sx + k * 9.0f, sy - 5.0f, sx + k * 12.0f, sy + 2.0f, CA(30, 14, 40, 0x90));
                line(sx, sy, sx + k * 8.0f, sy + 1.0f, CA(30, 14, 40, 0x90)); line(sx + k * 8.0f, sy + 1.0f, sx + k * 10.0f, sy + 7.0f, CA(30, 14, 40, 0x90));
            }
            discN(sx, sy, 4.6f, COL(24, 10, 32), 10);
            discN(sx - 1.6f, sy - 1.2f, 0.9f, COL(255, 60, 60), 6); discN(sx + 1.6f, sy - 1.2f, 0.9f, COL(255, 60, 60), 6);
        }
        {                                                              /* fantasma flutuando */
            float gx = TX0 + 60.0f + fmodf(mt * 0.22f, W - 120.0f), gy = TCY + 22.0f * sinf(mt * 0.035f), wv = sinf(mt * 0.2f);
            u64 gc = CA(236, 240, 255, 0x1e);
            discN(gx, gy - 8.0f, 12.0f, gc, 14);
            rect(gx - 12.0f, gy - 8.0f, gx + 12.0f, gy + 10.0f, gc);
            for (k = 0; k < 3; k++)
                tri(gx - 12.0f + k * 8.0f, gy + 10.0f, gx - 4.0f + k * 8.0f, gy + 10.0f, gx - 8.0f + k * 8.0f + wv * 2.0f, gy + 16.0f, gc);
            discN(gx - 4.5f, gy - 8.0f, 2.2f, CA(20, 16, 40, 0x70), 8); discN(gx + 4.5f, gy - 8.0f, 2.2f, CA(20, 16, 40, 0x70), 8);
            discN(gx, gy - 2.0f, 2.4f, CA(20, 16, 40, 0x60), 8);
        }
        for (k = 0; k < 3; k++) {                                      /* morcegos */
            float bx = TX0 - 30.0f + fmodf(mt * (0.7f + k * 0.35f) + k * 190.0f, W + 60.0f);
            float by = TY0 + 36.0f + k * 78.0f + 14.0f * sinf(mt * 0.05f + k * 2.0f);
            batShape(bx, by, 1.0f, sinf(mt * 0.4f + k), 0xa0);
        }
        if (ph < 90.0f) {                                              /* morcego grande cruzando a lua */
            float u = ph / 90.0f;
            batShape(TX0 + u * W, TCY - 40.0f + 20.0f * sinf(u * 6.0f), 1.7f, sinf(mt * 0.45f), 0x90);
        }
        for (k = 0; k < 20; k++) {                                     /* pontinhos de luz (vagalumes) */
            float fx = TX0 + 8.0f + fmodf(k * 137.5f, 512.0f), fy = TY0 + 8.0f + fmodf(k * 71.3f, 248.0f) + 4.0f * sinf(mt * 0.05f + k);
            float tw2 = sinf(mt * 0.06f + k * 1.7f);
            if (tw2 < 0) tw2 = 0;
            disc(fx, fy, 1.2f, (k & 1) ? CA(150, 255, 110, 0x10 + (int)(0x50 * tw2)) : CA(255, 190, 80, 0x10 + (int)(0x50 * tw2)));
        }
    } else {
        for (k = 0; k < 6; k++) {                                      /* cacapas com brilho verde de pocao */
            float pulse = 0.5f + 0.5f * sinf(mt * 0.09f + k * 1.2f);
            discN(PX[k], PY[k], PR[k] + 8.0f, CA(110, 255, 70, 0x0e + (int)(0x0a * pulse)), 12);
            ring(PX[k], PY[k], PR[k] + 4.5f, CA(255, 130, 20, 0x90));
            ring(PX[k], PY[k], PR[k] + 6.2f + pulse * 1.6f, CA(130, 255, 90, 0x50));
        }
        line(HEADX, TY0, HEADX, TY1, CA(255, 140, 30, 0x30));
    }
}

/* ---------- mesa VAMPIRO (style 12): velas, espinhos goticos, lua palida, morcegos e sangue escorrendo ---------- */
static void candleV(float x, float y, float fl)
{
    discN(x, y - 6.0f, 13.0f, CA(255, 170, 60, 0x0c + (int)(0x08 * fl)), 12);
    rect(x - 2.6f, y - 2.0f, x + 2.6f, y + 8.0f, COL(240, 232, 210));
    rect(x - 2.6f, y - 2.0f, x - 1.0f, y + 8.0f, COL(255, 250, 235));
    tri(x - 2.2f, y - 2.0f, x + 2.2f, y - 2.0f, x + fl * 0.8f, y - 8.0f - fl * 2.0f, CA(255, 190, 60, 0xb0));
    discN(x, y - 3.5f, 1.6f, CA(255, 250, 200, 0xc0), 6);
}

static void tableDecoVampire(int stage, int mt)
{
    int k;
    float W = TX1 - TX0, H = TY1 - TY0;
    if (stage == 0) {
        outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, COL(150, 16, 36));
        outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, CA(200, 200, 215, 0x50));
        for (k = 0; k < 14; k++) {                                   /* espinhos gotico */
            float x = TX0 + 22.0f + k * ((W - 44.0f) / 13.0f);
            tri(x - 4.0f, TY0 - 22.0f, x + 4.0f, TY0 - 22.0f, x, TY0 - 29.0f, CA(190, 190, 205, 0x90));
            tri(x - 4.0f, TY1 + 22.0f, x + 4.0f, TY1 + 22.0f, x, TY1 + 29.0f, CA(190, 190, 205, 0x90));
        }
        for (k = 0; k < 4; k++)                                      /* velas nos cantos */
            candleV((k & 1) ? TX1 + 21.0f : TX0 - 21.0f, ((k & 2) ? TY1 + 21.0f : TY0 - 21.0f) - 2.0f, sinf(mt * 0.2f + k * 1.7f));
    } else if (stage == 1) {
        discN(TCX, TCY, 74.0f, CA(235, 228, 245, 0x05), 28);         /* lua palida */
        discN(TCX, TCY, 54.0f, CA(235, 228, 245, 0x08), 28);
        discN(TCX, TCY, 38.0f, CA(235, 228, 245, 0x0e), 24);
        ring(TCX, TCY, 38.0f, CA(235, 228, 245, 0x20));
        discN(TCX - 11.0f, TCY - 8.0f, 6.0f, CA(180, 170, 200, 0x10), 10);
        discN(TCX + 12.0f, TCY + 10.0f, 4.0f, CA(180, 170, 200, 0x10), 10);
        for (k = 0; k < 4; k++) {                                    /* nevoa baixa */
            float fx = TX0 - 60.0f + fmodf(mt * (0.22f + k * 0.06f) + k * 190.0f, W + 120.0f), fy = TY1 - 30.0f - k * 14.0f;
            discN(fx, fy, 30.0f, CA(200, 150, 200, 0x07), 14);
            discN(fx + 32.0f, fy + 5.0f, 24.0f, CA(200, 150, 200, 0x07), 14);
        }
        for (k = 0; k < 2; k++)                                      /* morcegos */
            batShape(TX0 - 30.0f + fmodf(mt * (0.8f + k * 0.4f) + k * 260.0f, W + 60.0f), TY0 + 50.0f + k * 110.0f + 12.0f * sinf(mt * 0.05f + k * 2.0f), 1.1f, sinf(mt * 0.4f + k), 0xa0);
        for (k = 0; k < 6; k++) {                                    /* sangue escorrendo da tabela */
            float x = TX0 + 40.0f + k * ((W - 80.0f) / 5.0f), y = TY0 + fmodf(mt * 0.6f + k * 41.0f, H * 0.85f);
            line(x, TY0, x, y - 6.0f, CA(150, 8, 24, 0x50));
            tri(x - 2.0f, y - 1.0f, x + 2.0f, y - 1.0f, x, y - 7.0f, CA(190, 10, 30, 0x90));
            discN(x, y, 2.1f, CA(190, 10, 30, 0x90), 8);
        }
    } else {
        for (k = 0; k < 6; k++) {
            float pulse = 0.5f + 0.5f * sinf(mt * 0.08f + k * 1.3f);
            discN(PX[k], PY[k], PR[k] + 8.0f, CA(230, 20, 50, 0x0e + (int)(0x0a * pulse)), 12);
            ring(PX[k], PY[k], PR[k] + 4.5f, CA(235, 30, 60, 0x90));
            ring(PX[k], PY[k], PR[k] + 6.2f + pulse * 1.4f, CA(200, 200, 215, 0x40));
        }
        line(HEADX, TY0, HEADX, TY1, CA(230, 30, 60, 0x30));
    }
}

/* ---------- mesa NATAL (style 13): pisca-pisca, bolas de Natal, neve, estrelas e o trenó do Papai Noel ---------- */
static void sleighX(float x, float y, int mt)
{
    int r;
    float bn = sinf(mt * 0.3f);
    quad(x, y, x + 26.0f, y, x + 22.0f, y + 10.0f, x + 3.0f, y + 10.0f, CA(200, 30, 40, 0xb0));
    line(x - 2.0f, y + 14.0f, x + 30.0f, y + 14.0f, CA(255, 214, 80, 0xb0));
    line(x + 3.0f, y + 10.0f, x + 3.0f, y + 14.0f, CA(255, 214, 80, 0xb0)); line(x + 22.0f, y + 10.0f, x + 22.0f, y + 14.0f, CA(255, 214, 80, 0xb0));
    discN(x + 10.0f, y - 3.5f, 3.4f, CA(240, 190, 150, 0xc0), 8);
    tri(x + 6.0f, y - 5.5f, x + 14.0f, y - 5.5f, x + 8.0f, y - 12.0f, CA(210, 30, 40, 0xc0));
    for (r = 0; r < 2; r++) {
        float rx = x + 42.0f + r * 26.0f, ry = y + 5.0f + bn * 1.6f * (r ? -1.0f : 1.0f);
        line(x + 26.0f, y + 4.0f, rx - 4.0f, ry, CA(255, 214, 80, 0x70));
        discN(rx + 4.0f, ry, 5.0f, CA(124, 78, 42, 0xc0), 8);
        discN(rx + 11.0f, ry - 4.0f, 3.0f, CA(124, 78, 42, 0xc0), 8);
        line(rx + 11.0f, ry - 6.0f, rx + 8.0f, ry - 12.0f, CA(220, 200, 160, 0xb0)); line(rx + 12.0f, ry - 6.0f, rx + 15.0f, ry - 12.0f, CA(220, 200, 160, 0xb0));
        line(rx, ry + 4.0f, rx - 2.0f, ry + 9.0f + bn, CA(90, 56, 30, 0xb0)); line(rx + 8.0f, ry + 4.0f, rx + 10.0f, ry + 9.0f - bn, CA(90, 56, 30, 0xb0));
        if (r == 1) { discN(rx + 14.0f, ry - 3.0f, 1.6f, CA(255, 40, 40, 0xe0), 6); discN(rx + 14.0f, ry - 3.0f, 5.0f, CA(255, 40, 40, 0x18), 8); }
    }
    for (r = 0; r < 8; r++) disc(x - 4.0f - r * 6.0f, y + 10.0f + sinf(mt * 0.2f + r) * 3.0f, 1.6f - r * 0.15f, CA(255, 240, 180, 0x70 - r * 8));
}

static void tableDecoXmas(int stage, int mt)
{
    int k;
    float W = TX1 - TX0, H = TY1 - TY0;
    if (stage == 0) {
        outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, COL(240, 240, 248));
        outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, CA(255, 214, 80, 0x80));
        for (k = 0; k < 40; k++) {                                   /* pisca-pisca colorido */
            float px, py, nx, ny;
            int ph = (k + mt / 14) % 4, r = 255, gg = 60, bb = 60;
            if (ph == 1) { r = 70; gg = 240; bb = 100; } else if (ph == 2) { r = 255; gg = 225; bb = 80; } else if (ph == 3) { r = 90; gg = 170; bb = 255; }
            perim(k, 40, 19.0f, &px, &py, &nx, &ny);
            discN(px, py, 4.0f, CA(r, gg, bb, 0x14), 8);
            rect(px - 1.5f, py - 1.5f, px + 1.5f, py + 1.5f, CA(r, gg, bb, ((k + mt / 14) & 1) ? 0xb0 : 0x60));
        }
        for (k = 0; k < 4; k++) {                                    /* bolas de Natal nos cantos */
            float x = (k & 1) ? TX1 + 21.0f : TX0 - 21.0f, y = ((k & 2) ? TY1 + 21.0f : TY0 - 21.0f) + 1.0f;
            discN(x, y, 7.2f, (k & 1) ? COL(214, 30, 44) : COL(30, 150, 70), 12);
            discN(x - 2.2f, y - 2.4f, 2.2f, CA(255, 255, 255, 0x90), 6);
            rect(x - 2.0f, y - 9.6f, x + 2.0f, y - 7.0f, COL(255, 214, 80));
        }
    } else if (stage == 1) {
        float ph = fmodf((float)mt, 900.0f);
        for (k = 0; k < 16; k++) {                                   /* estrelas */
            float sx = TX0 + 10.0f + fmodf(k * 131.7f, 508.0f), sy = TY0 + 8.0f + fmodf(k * 67.3f, 248.0f), tw2 = sinf(mt * 0.06f + k * 1.9f);
            if (tw2 < 0) tw2 = 0;
            disc(sx, sy, 1.2f, CA(255, 245, 190, 0x10 + (int)(0x50 * tw2)));
        }
        for (k = 0; k < 3; k++) {                                    /* flocos grandes girando */
            float fx = TX0 + 90.0f + k * 180.0f, fy = TY0 + 60.0f + (k & 1) * 120.0f, a = mt * 0.02f + k, q;
            for (q = 0; q < 3; q++) {
                float aa = a + q * 1.0472f;
                line(fx - cosf(aa) * 9.0f, fy - sinf(aa) * 9.0f, fx + cosf(aa) * 9.0f, fy + sinf(aa) * 9.0f, CA(235, 248, 255, 0x2c));
            }
        }
        for (k = 0; k < 46; k++) {                                   /* neve caindo */
            float sx = TX0 + fmodf(k * 47.3f + 10.0f * sinf(mt * 0.03f + k), W), sy = TY0 + fmodf(mt * (0.35f + (k % 4) * 0.18f) + k * 31.0f, H);
            disc(sx, sy, (k % 5 == 0) ? 1.8f : 1.2f, CA(255, 255, 255, 0x50 + (k % 3) * 0x10));
        }
        if (ph < 280.0f) {                                           /* trenó do Papai Noel cruzando */
            float u = ph / 280.0f;
            sleighX(TX0 - 80.0f + u * (W + 160.0f), TY0 + 48.0f + 18.0f * sinf(u * 8.0f), mt);
        }
    } else {
        for (k = 0; k < 6; k++) {
            float pulse = 0.5f + 0.5f * sinf(mt * 0.09f + k);
            discN(PX[k], PY[k], PR[k] + 8.0f, CA(255, 255, 255, 0x0c + (int)(0x08 * pulse)), 12);
            ring(PX[k], PY[k], PR[k] + 4.5f, (k & 1) ? CA(70, 240, 100, 0x90) : CA(255, 70, 70, 0x90));
            ring(PX[k], PY[k], PR[k] + 6.2f + pulse * 1.4f, CA(255, 255, 255, 0x40));
        }
        line(HEADX, TY0, HEADX, TY1, CA(255, 255, 255, 0x30));
    }
}

/* ---------- mesa HACK (style 14): grade, varredura, pacotes de dados, falhas e mira nas cacapas ---------- */
static void tableDecoHack(int stage, int mt)
{
    int k, q;
    float H = TY1 - TY0;
    if (stage == 0) {
        outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, COL(0, 220, 200));
        outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, CA(255, 40, 190, 0x70));
        for (k = 0; k < 6; k++)                                      /* pacotes de dados correndo em volta */
            for (q = 0; q < 5; q++) {
                float px, py, nx, ny;
                int idx = ((mt / 2 + k * 40 - q * 2) % 240 + 240) % 240;
                perim(idx, 240, 18.5f, &px, &py, &nx, &ny);
                rect(px - 2.0f, py - 2.0f, px + 2.0f, py + 2.0f, (k & 1) ? CA(255, 40, 190, 0xa0 - q * 0x1c) : CA(0, 255, 220, 0xa0 - q * 0x1c));
            }
        for (k = 0; k < 4; k++) {                                    /* cantoneiras [ ] */
            float x = (k & 1) ? TX1 + 25.0f : TX0 - 25.0f, y = (k & 2) ? TY1 + 25.0f : TY0 - 25.0f, dx = (k & 1) ? -1.0f : 1.0f, dy = (k & 2) ? -1.0f : 1.0f;
            line(x, y, x + dx * 12.0f, y, COL(0, 255, 220)); line(x, y, x, y + dy * 12.0f, COL(0, 255, 220));
        }
    } else if (stage == 1) {
        float sy = TY0 + fmodf(mt * 1.6f, H);
        for (k = 1; k < 12; k++) line(TX0 + k * 44.0f, TY0, TX0 + k * 44.0f, TY1, CA(0, 220, 200, 0x0c));
        for (k = 1; k < 6; k++)  line(TX0, TY0 + k * 44.0f, TX1, TY0 + k * 44.0f, CA(0, 220, 200, 0x0c));
        rect(TX0, sy, TX1, sy + 10.0f > TY1 ? TY1 : sy + 10.0f, CA(0, 255, 220, 0x0c));
        line(TX0, sy, TX1, sy, CA(0, 255, 220, 0x28));
        for (k = 0; k < 12; k++)                                     /* chuva de bits */
            for (q = 0; q < 6; q++) {
                float x = TX0 + 20.0f + k * 44.0f, y = TY0 + fmodf(mt * (1.0f + (k % 3) * 0.6f) + k * 37.0f - q * 9.0f + 540.0f, H + 54.0f) - 27.0f;
                if (y < TY0 || y > TY1 - 6.0f) continue;
                rect(x - 1.5f, y, x + 1.5f, y + 6.0f, (k & 1) ? CA(255, 40, 190, 0x58 - q * 0x0c) : CA(0, 255, 220, 0x58 - q * 0x0c));
            }
        if ((mt % 150) < 7)                                          /* falha (glitch) */
            for (k = 0; k < 3; k++) {
                float y = TY0 + fmodf(k * 83.0f + mt * 5.0f, H - 6.0f);
                rect(TX0 + k * 14.0f, y, TX1 - 30.0f + k * 12.0f, y + 4.0f, CA(255, 40, 190, 0x22));
            }
        text(TCX - 44.0f, TCY - 14.0f, 0.9f, CA(0, 255, 220, ((mt / 30) & 1) ? 0x1a : 0x0e), "HACK");
    } else {
        for (k = 0; k < 6; k++) {
            float pulse = 0.5f + 0.5f * sinf(mt * 0.1f + k);
            ring(PX[k], PY[k], PR[k] + 4.5f, CA(0, 255, 220, 0x90));
            ring(PX[k], PY[k], PR[k] + 7.0f + pulse * 2.0f, CA(255, 40, 190, 0x58));
            line(PX[k] - PR[k] - 10.0f, PY[k], PX[k] - PR[k] - 3.0f, PY[k], CA(0, 255, 220, 0x80)); line(PX[k] + PR[k] + 3.0f, PY[k], PX[k] + PR[k] + 10.0f, PY[k], CA(0, 255, 220, 0x80));
            line(PX[k], PY[k] - PR[k] - 10.0f, PX[k], PY[k] - PR[k] - 3.0f, CA(0, 255, 220, 0x80)); line(PX[k], PY[k] + PR[k] + 3.0f, PX[k], PY[k] + PR[k] + 10.0f, CA(0, 255, 220, 0x80));
        }
        line(HEADX, TY0, HEADX, TY1, CA(0, 255, 220, 0x30));
    }
}

/* ---------- mesa CHAPOLIN (style 15): feltro vermelho, moldura amarela, coracoes e estrelas ---------- */
static void heartS(float x, float y, float r, u64 c)
{
    discN(x - r * 0.5f, y - r * 0.25f, r * 0.55f, c, 10);
    discN(x + r * 0.5f, y - r * 0.25f, r * 0.55f, c, 10);
    tri(x - r * 1.02f, y - r * 0.05f, x + r * 1.02f, y - r * 0.05f, x, y + r * 1.05f, c);
}

static void tableDecoChapolin(int stage, int mt)
{
    int k;
    float W = TX1 - TX0, H = TY1 - TY0;
    if (stage == 0) {
        outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, COL(214, 36, 40));
        outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, CA(255, 255, 255, 0x60));
        for (k = 0; k < 44; k++) {                                   /* faixas vermelhas e amarelas na moldura */
            float px, py, nx, ny;
            perim(k, 44, 19.0f, &px, &py, &nx, &ny);
            rect(px - 2.2f, py - 2.2f, px + 2.2f, py + 2.2f, ((k + mt / 20) & 1) ? COL(214, 36, 40) : COL(255, 230, 90));
        }
        for (k = 0; k < 4; k++) {                                    /* coracoes nos cantos */
            float x = (k & 1) ? TX1 + 21.0f : TX0 - 21.0f, y = (k & 2) ? TY1 + 21.0f : TY0 - 21.0f, pu = 1.0f + 0.12f * sinf(mt * 0.15f + k);
            heartS(x, y, 8.5f * pu, COL(120, 14, 20));
            heartS(x, y, 7.0f * pu, COL(255, 214, 40));
        }
    } else if (stage == 1) {
        heartS(TCX, TCY, 62.0f, CA(255, 214, 40, 0x10));             /* coracao grande do peito */
        for (k = 0; k < 18; k++) {                                   /* estrelas */
            float sx = TX0 + 10.0f + fmodf(k * 131.7f, 508.0f), sy = TY0 + 8.0f + fmodf(k * 67.3f, 248.0f), tw2 = sinf(mt * 0.07f + k * 1.9f);
            if (tw2 < 0) tw2 = 0;
            disc(sx, sy, 1.3f, CA(255, 240, 150, 0x10 + (int)(0x50 * tw2)));
        }
        for (k = 0; k < 10; k++) {                                   /* coracoes subindo */
            float hx = TX0 + 30.0f + k * ((W - 60.0f) / 9.0f) + 8.0f * sinf(mt * 0.04f + k * 1.3f);
            float hy = TY1 - fmodf(mt * (0.4f + (k % 3) * 0.15f) + k * 43.0f, H);
            heartS(hx, hy, 4.0f, CA(255, 214, 40, 0x58));
        }
        for (k = 0; k < 2; k++) {                                    /* antenas saltitantes */
            float ax = TX0 + 120.0f + k * 280.0f, bn = 14.0f * fabsf(sinf(mt * 0.07f + k * 1.5f));
            line(ax, TY1, ax, TY1 - 20.0f - bn, CA(40, 20, 20, 0x70));
            discN(ax, TY1 - 24.0f - bn, 5.0f, CA(255, 214, 40, 0xa0), 10);
            discN(ax - 1.4f, TY1 - 25.6f - bn, 1.5f, CA(255, 255, 255, 0xa0), 6);
        }
    } else {
        for (k = 0; k < 6; k++) {
            float pulse = 0.5f + 0.5f * sinf(mt * 0.1f + k);
            discN(PX[k], PY[k], PR[k] + 8.0f, CA(255, 214, 40, 0x0e + (int)(0x0a * pulse)), 12);
            ring(PX[k], PY[k], PR[k] + 4.5f, CA(255, 214, 40, 0x90));
            ring(PX[k], PY[k], PR[k] + 6.2f + pulse * 1.4f, CA(255, 255, 255, 0x40));
        }
        line(HEADX, TY0, HEADX, TY1, CA(255, 214, 40, 0x38));
    }
}

static void tableDecoTime(int stage, int mt)
{
    int k;
    if (stage == 0) {
        outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, COL(255, 214, 90));
        outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, CA(120, 200, 255, 0x60));
        for (k = 0; k < 48; k++) {                                   /* marcas de relogio na moldura; uma luz corre como o ponteiro */
            float px, py, nx, ny;
            u64 c = (k == (mt / 10) % 48) ? COL(255, 255, 255) : (((k & 3) == 0) ? COL(255, 214, 90) : CA(120, 200, 255, 0x90));
            perim(k, 48, 19.0f, &px, &py, &nx, &ny);
            rect(px - 2.0f, py - 2.0f, px + 2.0f, py + 2.0f, c);
        }
        for (k = 0; k < 4; k++) {                                    /* mini relogios nos cantos */
            float x = (k & 1) ? TX1 + 21.0f : TX0 - 21.0f, y = (k & 2) ? TY1 + 21.0f : TY0 - 21.0f, a = mt * 0.05f + k * 1.3f;
            discN(x, y, 8.0f, COL(190, 146, 40), 12);
            discN(x, y, 6.2f, COL(240, 244, 255), 12);
            line(x, y, x + cosf(a) * 5.0f, y + sinf(a) * 5.0f, COL(20, 26, 70));
            line(x, y, x + cosf(a * 0.083f) * 3.2f, y + sinf(a * 0.083f) * 3.2f, COL(20, 26, 70));
        }
    } else if (stage == 1) {
        float am = mt * 0.03f;
        ring(TCX, TCY, 66.0f, CA(255, 214, 90, 0x2c));               /* mostrador gigante no feltro */
        ring(TCX, TCY, 60.0f, CA(120, 200, 255, 0x24));
        for (k = 0; k < 12; k++) {
            float an = k * 0.5236f;
            line(TCX + cosf(an) * 52.0f, TCY + sinf(an) * 52.0f, TCX + cosf(an) * 62.0f, TCY + sinf(an) * 62.0f, CA(255, 214, 90, 0x3c));
        }
        line(TCX, TCY, TCX + cosf(am) * 50.0f, TCY + sinf(am) * 50.0f, CA(120, 200, 255, 0x48));
        line(TCX, TCY, TCX + cosf(am / 12.0f) * 32.0f, TCY + sinf(am / 12.0f) * 32.0f, CA(255, 214, 90, 0x48));
        discN(TCX, TCY, 3.0f, CA(255, 214, 90, 0x58), 8);
        for (k = 0; k < 18; k++) {                                   /* brilhos de poeira do tempo */
            float sx = TX0 + 10.0f + fmodf(k * 131.7f, 508.0f), sy = TY0 + 8.0f + fmodf(k * 67.3f, 248.0f), tw2 = sinf(mt * 0.07f + k * 1.9f);
            if (tw2 < 0) tw2 = 0;
            disc(sx, sy, 1.3f, CA(190, 225, 255, 0x10 + (int)(0x50 * tw2)));
        }
    } else {
        for (k = 0; k < 6; k++) {
            float pulse = 0.5f + 0.5f * sinf(mt * 0.1f + k);
            discN(PX[k], PY[k], PR[k] + 8.0f, CA(120, 200, 255, 0x0e + (int)(0x0a * pulse)), 12);
            ring(PX[k], PY[k], PR[k] + 4.5f, CA(255, 214, 90, 0x90));
            ring(PX[k], PY[k], PR[k] + 6.2f + pulse * 1.4f, CA(120, 200, 255, 0x40));
        }
        line(HEADX, TY0, HEADX, TY1, CA(255, 214, 90, 0x38));
    }
}

/* stage 0: moldura (antes do feltro) | 1: feltro (antes das tabelas) | 2: por cima das cacapas */
/* ---------- mesa GRINCH (style 17): moldura verde e vermelha, coracao que cresce 3 tamanhos, fumaca verde e presentes roubados ---------- */
static void heartPt(float a, float sc, float *x, float *y)
{
    float sn = sinf(a);
    *x = TCX + 16.0f * sn * sn * sn * sc;
    *y = TCY - (13.0f * cosf(a) - 5.0f * cosf(2.0f * a) - 2.0f * cosf(3.0f * a) - cosf(4.0f * a)) * sc;
}

static void tableDecoGrinch(int stage, int mt)
{
    int k;
    float W = TX1 - TX0, H = TY1 - TY0;
    if (stage == 0) {
        outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, COL(214, 36, 48));
        outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, CA(120, 220, 70, 0x90));
        for (k = 0; k < 40; k++) {                                   /* luzes piscando: verde, vermelho e branco */
            float px, py, nx, ny;
            int ph = (k + mt / 12) % 3;
            perim(k, 40, 19.0f, &px, &py, &nx, &ny);
            rect(px - 1.5f, py - 1.5f, px + 1.5f, py + 1.5f,
                 ph == 0 ? CA(120, 230, 70, 0xb0) : (ph == 1 ? CA(230, 40, 50, 0xb0) : CA(255, 255, 255, 0x90)));
        }
        for (k = 0; k < 4; k++) {                                    /* carinha do Grinch nos cantos */
            float x = (k & 1) ? TX1 + 21.0f : TX0 - 21.0f, y = (k & 2) ? TY1 + 21.0f : TY0 - 21.0f;
            discN(x, y, 8.0f, COL(88, 170, 52), 12);
            discN(x - 3.0f, y - 1.5f, 2.4f, COL(255, 230, 60), 8);
            discN(x + 3.0f, y - 1.5f, 2.4f, COL(255, 230, 60), 8);
            rect(x - 3.4f, y - 3.0f, x - 2.6f, y, COL(34, 22, 22));
            rect(x + 2.6f, y - 3.0f, x + 3.4f, y, COL(34, 22, 22));
            line(x - 4.0f, y + 3.4f, x, y + 4.4f, COL(34, 22, 22));
            line(x, y + 4.4f, x + 4.0f, y + 3.4f, COL(34, 22, 22));
        }
    } else if (stage == 1) {
        float hr = 18.0f + 9.0f * fabsf(sinf(mt * 0.025f));          /* o coracao cresce, cresce, cresce... */
        for (k = 0; k < 28; k++) {
            float ax0, ay0, ax1, ay1;
            heartPt(k * 6.2831853f / 28.0f, hr / 16.0f, &ax0, &ay0);
            heartPt((k + 1) * 6.2831853f / 28.0f, hr / 16.0f, &ax1, &ay1);
            line(ax0, ay0, ax1, ay1, CA(214, 36, 48, 0x58));
        }
        ring(TCX, TCY, hr * 1.5f, CA(255, 230, 70, 0x1c));
        for (k = 0; k < 16; k++) {                                   /* fumaca verde subindo */
            float sx = TX0 + fmodf(k * 61.7f + 8.0f * sinf(mt * 0.03f + k), W), sy = TY1 - fmodf(mt * (0.25f + (k % 4) * 0.1f) + k * 29.0f, H);
            discN(sx, sy, 3.0f + (k % 3), CA(110, 220, 70, 0x10), 10);
        }
        for (k = 0; k < 36; k++) {                                   /* neve caindo */
            float sx = TX0 + fmodf(k * 47.3f + 10.0f * sinf(mt * 0.03f + k), W), sy = TY0 + fmodf(mt * (0.35f + (k % 4) * 0.18f) + k * 31.0f, H);
            disc(sx, sy, (k % 5 == 0) ? 1.8f : 1.2f, CA(235, 255, 235, 0x4c + (k % 3) * 0x10));
        }
        for (k = 0; k < 3; k++) {                                    /* presentes roubados flutuando */
            float gx = TX0 + fmodf(mt * (0.18f + k * 0.05f) + k * 190.0f, W), gy = TY0 + 50.0f + k * 80.0f + 6.0f * sinf(mt * 0.04f + k * 2.0f);
            rect(gx - 5.0f, gy - 5.0f, gx + 5.0f, gy + 5.0f, (k & 1) ? CA(230, 40, 50, 0x48) : CA(255, 214, 80, 0x48));
            line(gx, gy - 5.0f, gx, gy + 5.0f, CA(255, 255, 255, 0x60));
            line(gx - 5.0f, gy, gx + 5.0f, gy, CA(255, 255, 255, 0x60));
        }
    } else {
        for (k = 0; k < 6; k++) {
            float pulse = 0.5f + 0.5f * sinf(mt * 0.09f + k);
            discN(PX[k], PY[k], PR[k] + 8.0f, CA(110, 220, 70, 0x0c + (int)(0x08 * pulse)), 12);
            ring(PX[k], PY[k], PR[k] + 4.5f, (k & 1) ? CA(120, 230, 70, 0x90) : CA(230, 40, 50, 0x90));
            ring(PX[k], PY[k], PR[k] + 6.2f + pulse * 1.4f, CA(255, 230, 70, 0x40));
        }
        line(HEADX, TY0, HEADX, TY1, CA(120, 230, 70, 0x30));
    }
}

/* ---------- mesa ABISMO (style 18): fundo do mar, ondas de luz, bolhas, peixes, corais e redemoinhos nas cacapas ---------- */
static void tableDecoAbyss(int stage, int mt)
{
    int k, j;
    float W = TX1 - TX0, H = TY1 - TY0;
    if (stage == 0) {
        outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, COL(40, 170, 205));
        outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, CA(150, 235, 255, 0x70));
        for (k = 0; k < 44; k++) {                                   /* agua correndo na moldura: pontinhos em onda */
            float px, py, nx, ny, wv = sinf(mt * 0.1f - k * 0.7f);
            perim(k, 44, 18.5f + 1.6f * wv, &px, &py, &nx, &ny);
            rect(px - 1.4f, py - 1.4f, px + 1.4f, py + 1.4f, CA(150, 235, 255, 0x50 + (int)(0x40 * (0.5f + 0.5f * wv))));
        }
        for (k = 0; k < 4; k++) {                                    /* corais e conchas nos cantos */
            float x = (k & 1) ? TX1 + 21.0f : TX0 - 21.0f, y = (k & 2) ? TY1 + 21.0f : TY0 - 21.0f, sw = 1.5f * sinf(mt * 0.06f + k);
            for (j = -1; j <= 1; j++)
                tri(x + j * 3.0f - 2.0f, y + 4.0f, x + j * 3.0f + 2.0f, y + 4.0f, x + j * 6.0f + sw, y - 7.0f - (j == 0 ? 3.0f : 0.0f),
                    j == 0 ? COL(255, 120, 110) : COL(230, 84, 96));
            discN(x, y + 4.0f, 4.0f, COL(250, 190, 200), 8);
            discN(x, y + 4.0f, 1.6f, COL(255, 235, 240), 6);
        }
    } else if (stage == 1) {
        rect(TX0, TY0, TX1, TY1, CA(10, 70, 150, 0x10));                           /* tom de fundo do mar */
        for (k = 0; k < 4; k++) {                                                  /* raios de luz atravessando a agua */
            float x0 = TX0 + fmodf(k * 150.0f + mt * 0.25f + 40.0f * sinf(mt * 0.02f + k), W + 40.0f) - 20.0f, x1 = x0 + 26.0f;
            if (x0 < TX0) x0 = TX0;
            if (x1 > TX1) x1 = TX1;
            if (x1 > x0) rect(x0, TY0, x1, TY1, CA(170, 235, 255, 0x0a));
        }
        for (k = 0; k < 5; k++) {                                                  /* ondas de luz ondulando no feltro */
            float yb = TY0 + 30.0f + k * 50.0f;
            for (j = 0; j < 22; j++) {
                float xa = TX0 + j * (W / 22.0f), xb = xa + W / 22.0f;
                float ya = yb + 5.0f * sinf(mt * 0.05f + j * 0.7f + k * 1.3f), yc = yb + 5.0f * sinf(mt * 0.05f + (j + 1) * 0.7f + k * 1.3f);
                line(xa, ya, xb, yc, CA(190, 240, 255, 0x16));
            }
        }
        for (k = 0; k < 24; k++) {                                                 /* bolhas subindo */
            float sx = TX0 + fmodf(k * 53.1f + 8.0f * sinf(mt * 0.04f + k), W), sy = TY1 - fmodf(mt * (0.3f + (k % 4) * 0.12f) + k * 37.0f, H);
            float br = 1.4f + (k % 3) * 0.8f;
            ringN(sx, sy, br, 0.7f, 8, CA(205, 245, 255, 0x58));
            disc(sx - br * 0.35f, sy - br * 0.35f, 0.5f, CA(255, 255, 255, 0x70));
        }
        for (k = 0; k < 3; k++) {                                                  /* peixes passando */
            int dir = (k & 1) ? -1 : 1;
            float u = fmodf(mt * (0.45f + k * 0.17f) + k * 190.0f, W - 40.0f);
            float fx = dir > 0 ? TX0 + 20.0f + u : TX1 - 20.0f - u, fy = TY0 + 42.0f + k * 80.0f + 7.0f * sinf(mt * 0.05f + k * 2.0f);
            u64 fc = k == 0 ? CA(255, 170, 60, 0x50) : (k == 1 ? CA(120, 230, 255, 0x50) : CA(255, 120, 170, 0x50));
            discN(fx, fy, 4.4f, fc, 10);
            discN(fx + dir * 3.0f, fy, 3.2f, fc, 8);
            tri(fx - dir * 3.0f, fy, fx - dir * 10.0f, fy - 4.5f + 1.5f * sinf(mt * 0.3f + k), fx - dir * 10.0f, fy + 4.5f, fc);
            disc(fx + dir * 4.2f, fy - 1.0f, 0.8f, CA(20, 30, 50, 0x80));
        }
    } else {
        for (k = 0; k < 6; k++) {                                                  /* cacapas: redemoinhos e ondas */
            float pulse = 0.5f + 0.5f * sinf(mt * 0.09f + k), a = mt * 0.07f + k;
            discN(PX[k], PY[k], PR[k] + 8.0f, CA(40, 150, 255, 0x0c + (int)(0x08 * pulse)), 12);
            ring(PX[k], PY[k], PR[k] + 4.5f, CA(130, 235, 255, 0x90));
            for (j = 0; j < 2; j++) {
                float rr = fmodf(mt * 0.3f + j * 6.0f + k * 2.0f, 12.0f);
                ringN(PX[k], PY[k], PR[k] + 3.0f + rr, 0.9f, 20, CA(190, 245, 255, 0x50 - (int)(rr * 4.0f)));
            }
            for (j = 0; j < 3; j++) {
                float aa = a + j * 2.094f;
                disc(PX[k] + cosf(aa) * (PR[k] + 6.0f), PY[k] + sinf(aa) * (PR[k] + 6.0f), 1.3f, CA(210, 248, 255, 0xa0));
            }
        }
        line(HEADX, TY0, HEADX, TY1, CA(130, 235, 255, 0x28));
    }
}

/* ---------- mesa CEMITERIO (style 19): moldura de pedra com musgo, grade de ferro, tumulos, corvos, lua e nevoa ---------- */
static int farFromPocket(float x, float y, float d)
{
    int k;
    for (k = 0; k < 6; k++) { float dx = x - PX[k], dy = y - PY[k]; if (dx * dx + dy * dy < d * d) return 0; }
    return 1;
}

/* tumulo: type 0 arredondado com cruzinha, 1 cruz de pedra, 2 quadrado rachado e torto */
static void tombstone(float x, float y, float sc, int al, int type)
{
    u64 st = CA(130, 140, 134, al), sd = CA(84, 92, 88, al), dk = CA(34, 40, 38, al);
    if (type == 1) {
        rect(x - 2.2f * sc, y - 11.0f * sc, x + 2.2f * sc, y + 8.0f * sc, st);
        rect(x - 7.0f * sc, y - 6.0f * sc, x + 7.0f * sc, y - 2.6f * sc, st);
        rect(x + 0.6f * sc, y - 11.0f * sc, x + 2.2f * sc, y + 8.0f * sc, sd);
    } else if (type == 2) {
        quad(x - 6.0f * sc, y - 8.0f * sc, x + 5.0f * sc, y - 9.0f * sc, x + 6.0f * sc, y + 8.0f * sc, x - 6.0f * sc, y + 8.0f * sc, st);
        quad(x + 1.0f * sc, y - 8.5f * sc, x + 5.0f * sc, y - 9.0f * sc, x + 6.0f * sc, y + 8.0f * sc, x + 1.0f * sc, y + 8.0f * sc, sd);
        line(x - 1.0f * sc, y - 8.0f * sc, x + 1.0f * sc, y - 2.0f * sc, dk);
        line(x + 1.0f * sc, y - 2.0f * sc, x - 2.0f * sc, y + 3.0f * sc, dk);
    } else {
        rect(x - 5.5f * sc, y - 4.0f * sc, x + 5.5f * sc, y + 8.0f * sc, st);
        discN(x, y - 4.0f * sc, 5.5f * sc, st, 12);
        rect(x + 1.5f * sc, y - 4.0f * sc, x + 5.5f * sc, y + 8.0f * sc, sd);
        rect(x - 0.7f * sc, y - 7.5f * sc, x + 0.7f * sc, y + 1.0f * sc, dk);
        rect(x - 2.6f * sc, y - 4.8f * sc, x + 2.6f * sc, y - 3.4f * sc, dk);
    }
    rect(x - 8.0f * sc, y + 8.0f * sc, x + 8.0f * sc, y + 10.0f * sc, sd);
    discN(x - 3.0f * sc, y + 7.0f * sc, 2.4f * sc, CA(70, 120, 60, al * 3 / 4), 8);      /* musgo */
}

/* corvo pousado (dir = +1 olha para a direita) */
static void crowPerch(float x, float y, float sc, int dir, int mt, int k)
{
    float bob = (((mt / 40) + k) % 6 == 0) ? 1.2f * sc : 0.0f;                           /* de vez em quando bica */
    u64 bk = COL(16, 16, 22), bl = COL(52, 56, 76);
    tri(x - dir * 5.0f * sc, y - 2.0f * sc, x - dir * 13.0f * sc, y + 3.0f * sc, x - dir * 6.0f * sc, y + 3.5f * sc, bk);
    discN(x, y, 5.5f * sc, bk, 12);
    discN(x - dir * 0.8f * sc, y - 1.5f * sc, 3.2f * sc, bl, 10);
    discN(x + dir * 5.0f * sc, y - 4.0f * sc + bob, 3.0f * sc, bk, 10);
    tri(x + dir * 7.6f * sc, y - 4.8f * sc + bob, x + dir * 7.6f * sc, y - 2.6f * sc + bob, x + dir * 11.5f * sc, y - 3.4f * sc + bob, COL(150, 150, 160));
    discN(x + dir * 5.8f * sc, y - 4.8f * sc + bob, 0.9f * sc, COL(235, 60, 60), 6);
    line(x - 1.5f * sc, y + 5.0f * sc, x - 1.5f * sc, y + 8.0f * sc, bk);
    line(x + 1.5f * sc, y + 5.0f * sc, x + 1.5f * sc, y + 8.0f * sc, bk);
}

/* corvo voando (fl = -1..1 bate as asas) */
static void crowFly(float x, float y, float sc, int dir, float fl, int al)
{
    u64 bk = CA(10, 10, 16, al);
    float wy = 6.0f * sc * fl;
    tri(x, y - 1.5f * sc, x, y + 1.5f * sc, x - 10.0f * sc, y - wy, bk);
    tri(x, y - 1.5f * sc, x, y + 1.5f * sc, x + 10.0f * sc, y - wy, bk);
    discN(x, y, 3.0f * sc, bk, 10);
    discN(x + dir * 3.6f * sc, y - 0.8f * sc, 1.9f * sc, bk, 8);
    tri(x + dir * 5.0f * sc, y - 1.4f * sc, x + dir * 5.0f * sc, y - 0.2f * sc, x + dir * 8.0f * sc, y - 0.6f * sc, CA(150, 150, 160, al));
    tri(x - dir * 2.0f * sc, y - 1.0f * sc, x - dir * 2.0f * sc, y + 1.0f * sc, x - dir * 7.0f * sc, y + 1.0f * sc, bk);
}

/* COVA do Coveiro: buraco no meio da mesa (a = 1 normal, menor = previa na mira) */
static void drawGraveHole(float a)
{
    int al = (int)(0x80 * a), k;
    discN(TCX, TCY, GRAVE_R + 5.0f, CA(70, 48, 28, al * 3 / 4), 24);
    discN(TCX, TCY, GRAVE_R + 1.0f, CA(40, 28, 16, al), 24);
    discN(TCX, TCY, GRAVE_R - 2.0f, CA(2, 2, 4, al), 24);
    ringN(TCX, TCY, GRAVE_R + 3.0f + 1.2f * sinf(menuT * 0.2f), 1.6f, 28, CA(150, 255, 170, al / 2));
    for (k = 0; k < 8; k++)
        disc(TCX + cosf(k * 0.7854f + 0.3f) * (GRAVE_R + 7.0f), TCY + sinf(k * 0.7854f + 0.3f) * (GRAVE_R + 7.0f), 2.4f, CA(96, 66, 38, al));
}

static void tableDecoGrave(int stage, int mt)
{
    int k;
    float W = TX1 - TX0, H = TY1 - TY0, tp = tabPock();
    if (stage == 0) {
        /* moldura de pedra: lajes, rachaduras e musgo */
        rect(TX0 - 25, TY0 - 25, TX1 + 25, TY1 + 25, COL(56, 60, 60));
        for (k = 0; k < 6 + (int)(W / 22.0f); k++) {
            float x = TX0 - 25 + k * 22.0f;
            if (x > TX1 + 25) break;
            line(x, TY0 - 25, x, TY0 - 12, COL(30, 34, 34));
            line(x + 11.0f, TY1 + 12, x + 11.0f, TY1 + 25, COL(30, 34, 34));
        }
        for (k = 0; k < 14; k++) {
            float y = TY0 - 25 + k * 22.0f;
            if (y > TY1 + 25) break;
            line(TX0 - 25, y, TX0 - 12, y, COL(30, 34, 34));
            line(TX1 + 12, y + 11.0f, TX1 + 25, y + 11.0f, COL(30, 34, 34));
        }
        line(TX0 - 25, TY0 - 25, TX1 + 25, TY0 - 25, COL(96, 104, 102));
        line(TX0 - 25, TY0 - 25, TX0 - 25, TY1 + 25, COL(96, 104, 102));
        for (k = 0; k < 30; k++) {                                   /* musgo */
            float px, py, nx, ny;
            perim(k, 30, 14.0f + (float)((k * 7) % 9), &px, &py, &nx, &ny);
            discN(px, py, 2.4f + (k % 3), CA(70, 126, 58, 0x58), 8);
        }
        for (k = 0; k < 8; k++) {                                    /* rachaduras */
            float px, py, nx, ny;
            perim(k * 4 + 1, 32, 18.0f, &px, &py, &nx, &ny);
            line(px, py, px + 5.0f, py + 6.0f, COL(24, 28, 28));
            line(px + 5.0f, py + 6.0f, px + 2.0f, py + 12.0f, COL(24, 28, 28));
        }
        rect(TX0 - 12, TY0 - 12, TX1 + 12, TY1 + 12, COL(16, 20, 18));      /* sulco */
        outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, CA(120, 230, 150, 0x50));
        /* tumulos na moldura, longe das cacapas */
        tombstone(TX0 + 99.0f, TY0 - 19.0f, 0.9f, 0x80, 0);
        tombstone(TX0 + 165.0f, TY0 - 19.0f, 0.9f, 0x80, 1);
        tombstone(TX1 - 165.0f, TY0 - 19.0f, 0.9f, 0x80, 2);
        tombstone(TX1 - 99.0f, TY0 - 19.0f, 0.9f, 0x80, 0);
        tombstone(TX0 + 99.0f, TY1 + 19.0f, 0.9f, 0x80, 2);
        tombstone(TX0 + 165.0f, TY1 + 19.0f, 0.9f, 0x80, 0);
        tombstone(TX1 - 165.0f, TY1 + 19.0f, 0.9f, 0x80, 1);
        tombstone(TX1 - 99.0f, TY1 + 19.0f, 0.9f, 0x80, 2);
        tombstone(TX0 - 19.0f, TY0 + 99.0f, 0.9f, 0x80, 1);
        tombstone(TX1 + 19.0f, TY0 + 165.0f, 0.9f, 0x80, 0);
        /* grade de ferro com pontas de lanca (4 lados, pulando as cacapas) */
        for (k = 0; k < 24; k++) {
            float x = TX0 - 10.0f + k * ((W + 20.0f) / 23.0f);
            if (farFromPocket(x, TY0 - 29.0f, 30.0f)) {
                rect(x - 0.8f, TY0 - 33.0f, x + 0.8f, TY0 - 26.0f, COL(30, 34, 38));
                tri(x - 2.6f, TY0 - 33.0f, x + 2.6f, TY0 - 33.0f, x, TY0 - 39.0f, COL(58, 64, 70));
            }
            if (farFromPocket(x, TY1 + 29.0f, 30.0f)) {
                rect(x - 0.8f, TY1 + 26.0f, x + 0.8f, TY1 + 33.0f, COL(30, 34, 38));
                tri(x - 2.6f, TY1 + 33.0f, x + 2.6f, TY1 + 33.0f, x, TY1 + 39.0f, COL(58, 64, 70));
            }
        }
        for (k = 0; k < 11; k++) {
            float y = TY0 + k * (H / 10.0f);
            if (farFromPocket(TX0 - 29.0f, y, 30.0f)) {
                rect(TX0 - 33.0f, y - 0.8f, TX0 - 26.0f, y + 0.8f, COL(30, 34, 38));
                tri(TX0 - 33.0f, y - 2.6f, TX0 - 33.0f, y + 2.6f, TX0 - 39.0f, y, COL(58, 64, 70));
            }
            if (farFromPocket(TX1 + 29.0f, y, 30.0f)) {
                rect(TX1 + 26.0f, y - 0.8f, TX1 + 33.0f, y + 0.8f, COL(30, 34, 38));
                tri(TX1 + 33.0f, y - 2.6f, TX1 + 33.0f, y + 2.6f, TX1 + 39.0f, y, COL(58, 64, 70));
            }
        }
        line(TX0 + 24.0f, TY0 - 28.0f, TX1 - 24.0f, TY0 - 28.0f, COL(40, 44, 48));      /* travessas */
        line(TX0 + 24.0f, TY1 + 28.0f, TX1 - 24.0f, TY1 + 28.0f, COL(40, 44, 48));
        /* corvos pousados na grade (de vez em quando bicam) */
        crowPerch(TX0 + 70.0f, TY0 - 38.0f, 0.95f, 1, mt, 0);
        crowPerch(TX1 - 120.0f, TY0 - 38.0f, 0.95f, -1, mt, 2);
        crowPerch(TCX + 40.0f, TY0 - 38.0f, 0.85f, 1, mt, 4);
        crowPerch(TX0 + 130.0f, TY1 + 38.0f, 0.95f, -1, mt, 1);
        crowPerch(TX1 - 80.0f, TY1 + 38.0f, 0.95f, 1, mt, 3);
    } else if (stage == 1) {
        discN(TX1 - 74.0f, TY0 + 52.0f, 46.0f, CA(215, 235, 205, 0x05), 30);         /* lua pálida com brilho */
        discN(TX1 - 74.0f, TY0 + 52.0f, 32.0f, CA(215, 235, 205, 0x06), 26);
        moonCrescent(TX1 - 74.0f, TY0 + 52.0f, 24.0f, 0x22, 225, 240, 215);
        tombstone(TX0 + 28.0f, TY1 - 24.0f, 1.5f, 0x1c, 0);                            /* tumulos apagados nos cantos do feltro */
        tombstone(TX0 + 56.0f, TY1 - 20.0f, 1.2f, 0x1a, 1);
        tombstone(TX1 - 30.0f, TY1 - 24.0f, 1.5f, 0x1c, 2);
        tombstone(TX0 + 30.0f, TY0 + 26.0f, 1.2f, 0x18, 2);
        for (k = 0; k < 34; k++) {                                   /* grama morta no chao */
            float gx = TX0 + 6.0f + k * ((W - 12.0f) / 33.0f);
            tri(gx - 1.6f, TY1, gx + 1.6f, TY1, gx + (k & 1 ? 2.5f : -2.5f), TY1 - 6.0f - (float)(k % 4), CA(70, 110, 60, 0x40));
        }
        for (k = 0; k < 4; k++) {                                    /* nevoa baixa */
            float fx = TX0 - 60.0f + fmodf(mt * (0.2f + k * 0.05f) + k * 190.0f, W + 120.0f), fy = TY1 - 26.0f - k * 16.0f;
            discN(fx, fy, 30.0f, CA(170, 210, 190, 0x08), 14);
            discN(fx + 32.0f, fy + 5.0f, 24.0f, CA(170, 210, 190, 0x08), 14);
        }
        for (k = 0; k < 8; k++) {                                    /* fogo-fatuo subindo */
            float wx = TX0 + fmodf(k * 83.1f + 12.0f * sinf(mt * 0.03f + k), W), wy = TY1 - fmodf(mt * (0.25f + (k % 3) * 0.1f) + k * 37.0f, H), tw2 = 0.5f + 0.5f * sinf(mt * 0.1f + k);
            discN(wx, wy, 4.5f, CA(150, 255, 170, 0x08 + (int)(0x08 * tw2)), 10);
            disc(wx, wy, 1.5f, CA(190, 255, 200, 0x40 + (int)(0x30 * tw2)));
        }
        for (k = 0; k < 3; k++) {                                    /* corvos cruzando o ceu */
            float u = fmodf(mt * (0.7f + k * 0.25f) + k * 210.0f, W + 80.0f);
            int dir = (k & 1) ? -1 : 1;
            float cxp = dir > 0 ? TX0 - 40.0f + u : TX1 + 40.0f - u;
            crowFly(cxp, TY0 + 40.0f + k * 70.0f + 10.0f * sinf(mt * 0.03f + k * 2.0f), 0.9f, dir, sinf(mt * 0.28f + k * 1.7f), 0x70);
        }
    } else {
        for (k = 0; k < 6; k++) {
            float pulse = 0.5f + 0.5f * sinf(mt * 0.09f + k * 1.3f);
            discN(PX[k], PY[k], (PR[k] + 8.0f) * tp, CA(120, 255, 160, 0x0c + (int)(0x08 * pulse)), 14);
            ring(PX[k], PY[k], (PR[k] + 4.5f) * tp, CA(150, 255, 170, 0x90));
            ring(PX[k], PY[k], (PR[k] + 6.2f) * tp + pulse * 1.4f, CA(200, 210, 200, 0x40));
        }
        line(HEADX, TY0, HEADX, TY1, CA(150, 255, 170, 0x28));
    }
}

/* ---------- mesa DELEGACIA (style 20): moldura azul-marinho com fita de isolamento, sirenes vermelha/azul, holofotes e luzes nas cacapas ---------- */
static void tableDecoPolice(int stage, int mt)
{
    int k;
    float W = TX1 - TX0, H = TY1 - TY0, tp = tabPock();
    if (stage == 0) {
        rect(TX0 - 25, TY0 - 25, TX1 + 25, TY1 + 25, COL(18, 26, 54));
        line(TX0 - 25, TY0 - 25, TX1 + 25, TY0 - 25, COL(60, 84, 150));
        line(TX0 - 25, TY0 - 25, TX0 - 25, TY1 + 25, COL(60, 84, 150));
        rect(TX0 - 23, TY0 - 23, TX1 + 23, TY0 - 15, COL(14, 14, 18));            /* fita de isolamento: base preta e listras amarelas */
        rect(TX0 - 23, TY1 + 15, TX1 + 23, TY1 + 23, COL(14, 14, 18));
        rect(TX0 - 23, TY0 - 15, TX0 - 15, TY1 + 15, COL(14, 14, 18));
        rect(TX1 + 15, TY0 - 15, TX1 + 23, TY1 + 15, COL(14, 14, 18));
        for (k = 0; k < (int)(W / 14.0f) + 2; k++) {
            float x = TX0 - 19.0f + k * 14.0f;
            if (x + 7.0f > TX1 + 23.0f) break;
            quad(x, TY0 - 23, x + 7.0f, TY0 - 23, x + 3.0f, TY0 - 15, x - 4.0f, TY0 - 15, COL(250, 200, 30));
            quad(x, TY1 + 15, x + 7.0f, TY1 + 15, x + 3.0f, TY1 + 23, x - 4.0f, TY1 + 23, COL(250, 200, 30));
        }
        for (k = 0; k < (int)(H / 14.0f) + 1; k++) {
            float y = TY0 - 11.0f + k * 14.0f;
            if (y + 7.0f > TY1 + 15.0f) break;
            quad(TX0 - 23, y, TX0 - 15, y, TX0 - 19, y + 7.0f, TX0 - 23, y + 7.0f, COL(250, 200, 30));
            quad(TX1 + 15, y, TX1 + 23, y, TX1 + 23, y + 7.0f, TX1 + 19, y + 7.0f, COL(250, 200, 30));
        }
        rect(TX0 - 12, TY0 - 12, TX1 + 12, TY1 + 12, COL(10, 14, 30));              /* sulco */
        outlineRect(TX0 - 13.0f, TY0 - 13.0f, TX1 + 13.0f, TY1 + 13.0f, CA(110, 150, 240, 0x80));
        for (k = 0; k < 4; k++) {                                                    /* sirenes nas tabelas, longe das cacapas */
            float lx = (k & 1) ? TX1 - 70.0f : TX0 + 70.0f, ly = (k & 2) ? TY1 + 19.0f : TY0 - 19.0f;
            int lit = ((mt / 10) + k) & 1;
            discN(lx, ly, 15.0f, lit ? CA(255, 60, 70, 0x14) : CA(80, 140, 255, 0x14), 16);
            discN(lx, ly, 6.5f, CA(10, 12, 24, 0xc0), 12);
            discN(lx, ly, 4.6f, lit ? COL(255, 60, 70) : COL(80, 140, 255), 12);
            discN(lx - 1.3f, ly - 1.5f, 1.5f, COL(255, 255, 255), 6);
        }
    } else if (stage == 1) {
        float s1 = 0.5f + 0.5f * sinf(mt * 0.02f), s2 = 0.5f + 0.5f * cosf(mt * 0.02f);
        float b1 = TX0 + W * (0.15f + 0.30f * s1), b2 = TX1 - W * (0.15f + 0.30f * s2);
        tri(TX0 + 70.0f, TY0, b1 - 50.0f, TY1, b1 + 50.0f, TY1, CA(255, 60, 70, 0x0a));    /* holofotes vermelho e azul cruzando o feltro */
        tri(TX1 - 70.0f, TY0, b2 - 50.0f, TY1, b2 + 50.0f, TY1, CA(80, 140, 255, 0x0a));
        ringN(TCX, TCY, 44.0f, 2.0f, 32, CA(255, 215, 80, 0x14));                    /* distintivo apagado no centro */
        ringN(TCX, TCY, 30.0f, 1.2f, 28, CA(255, 215, 80, 0x10));
        for (k = 0; k < 5; k++) {                                                    /* marcas da tabela de altura na lateral */
            float y = TY0 + 20.0f + k * 22.0f;
            line(TX0 + 4.0f, y, TX0 + 16.0f, y, CA(235, 235, 245, 0x20));
        }
    } else {
        for (k = 0; k < 6; k++) {
            int lit = ((mt / 10) + k) & 1;
            ring(PX[k], PY[k], (PR[k] + 4.5f) * tp, lit ? CA(255, 60, 70, 0x90) : CA(80, 140, 255, 0x90));
            ring(PX[k], PY[k], (PR[k] + 6.2f) * tp, CA(235, 235, 245, 0x40));
        }
    }
}

/* ---------- mesa POOL AZUL (style 21): feltro azul com luz no centro, madeira vermelha ACOLCHOADA (losangos), tabelas azuis com brilho, placas metalicas nos cantos, cacapas pretas com aro prateado ---------- */
static void tableDecoPool(int stage)
{
    int k;
    if (stage == 0) {
        float x, y;
        tcCush = COL(26, 112, 178); tcCushHi = COL(140, 214, 252); tcCushLo = COL(14, 68, 120);      /* tabelas azuis (mais claras que o padrao) */
        outlineRect(TX0 - 27.5f, TY0 - 27.5f, TX1 + 27.5f, TY1 + 27.5f, CA(255, 110, 110, 0x46));     /* brilho do verniz na borda */
        outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, CA(255, 255, 255, 0x14));
        outlineRect(TX0 - 18.0f, TY0 - 18.0f, TX1 + 18.0f, TY1 + 18.0f, CA(40, 4, 10, 0x50));          /* sombra funda perto da borracha */
        /* acolchoado da madeira: losangos cruzados nas 4 faixas da moldura (como na mesa do 8 Ball Pool) */
        for (x = TX0 - 25.0f; x < TX1 + 25.0f; x += 9.0f) {
            float x2 = x + 13.0f;
            line(x, TY0 - 25.0f, x2, TY0 - 12.0f, CA(60, 4, 14, 0x4c));
            line(x2, TY0 - 25.0f, x, TY0 - 12.0f, CA(60, 4, 14, 0x4c));
            line(x, TY1 + 12.0f, x2, TY1 + 25.0f, CA(60, 4, 14, 0x4c));
            line(x2, TY1 + 12.0f, x, TY1 + 25.0f, CA(60, 4, 14, 0x4c));
            disc(x + 6.5f, TY0 - 18.5f, 0.9f, CA(255, 120, 130, 0x40));                                /* botoes do estofado */
            disc(x + 6.5f, TY1 + 18.5f, 0.9f, CA(255, 120, 130, 0x40));
        }
        for (y = TY0 - 25.0f; y < TY1 + 25.0f; y += 9.0f) {
            float y2 = y + 13.0f;
            line(TX0 - 25.0f, y, TX0 - 12.0f, y2, CA(60, 4, 14, 0x4c));
            line(TX0 - 12.0f, y, TX0 - 25.0f, y2, CA(60, 4, 14, 0x4c));
            line(TX1 + 12.0f, y, TX1 + 25.0f, y2, CA(60, 4, 14, 0x4c));
            line(TX1 + 25.0f, y, TX1 + 12.0f, y2, CA(60, 4, 14, 0x4c));
            disc(TX0 - 18.5f, y + 6.5f, 0.9f, CA(255, 120, 130, 0x40));
            disc(TX1 + 18.5f, y + 6.5f, 0.9f, CA(255, 120, 130, 0x40));
        }
        for (k = 0; k < 4; k++) {                                                                      /* placas metalicas escuras nos cantos */
            float cx = (k & 1) ? TX1 + 15.0f : TX0 - 15.0f, cy = (k & 2) ? TY1 + 15.0f : TY0 - 15.0f;
            discN(cx, cy, 15.0f, COL(34, 36, 42), 16);
            discN(cx, cy, 13.2f, COL(78, 82, 92), 16);
            discN(cx - 1.0f, cy - 1.2f, 10.5f, COL(48, 50, 58), 16);
        }
    } else if (stage == 1) {
        for (k = 0; k < 7; k++)                                                                        /* luz de cima: centro mais claro */
            rect(TX0 + 70.0f + k * 24.0f, TY0 + 30.0f + k * 14.0f, TX1 - 70.0f - k * 24.0f, TY1 - 30.0f - k * 14.0f, CA(150, 225, 255, 0x07));
        for (k = 0; k < 4; k++)                                                                        /* borda do feltro mais escura */
            outlineRect(TX0 + k, TY0 + k, TX1 - k, TY1 - k, CA(6, 30, 62, 0x30 - k * 8));
    } else {
        float ce = PRC[0] - POC, cm = PRC[1];                                            /* mesmas aberturas das tabelas */
        u64 hi = CA(170, 230, 255, 0x5a);
        line(TX0 + ce, TY0 + 0.5f, TCX - cm, TY0 + 0.5f, hi);                                          /* brilho azul-claro no gume das tabelas */
        line(TCX + cm, TY0 + 0.5f, TX1 - ce, TY0 + 0.5f, hi);
        line(TX0 + ce, TY1 - 0.5f, TCX - cm, TY1 - 0.5f, hi);
        line(TCX + cm, TY1 - 0.5f, TX1 - ce, TY1 - 0.5f, hi);
        line(TX0 + 0.5f, TY0 + ce, TX0 + 0.5f, TY1 - ce, hi);
        line(TX1 - 0.5f, TY0 + ce, TX1 - 0.5f, TY1 - ce, hi);
        for (k = 0; k < 6; k++) {                                                                      /* aro prateado nas cacapas */
            ring(PX[k], PY[k], PR[k] + 2.4f, COL(168, 174, 188));
            ring(PX[k], PY[k], PR[k] + 3.4f, COL(70, 74, 84));
        }
        line(HEADX, TY0, HEADX, TY1, CA(200, 240, 255, 0x46));                                         /* linha da cabeceira bem visivel */
    }
}

static void tableDeco(int stage, const TableDef *T)
{
    int k;
    int mt = fxOn ? menuT : 0;                        /* efeitos desligados: mesa parada */
    float t = mt * 0.02f;

    if (T->style == 1) {                                   /* PRETA OURO */
        u64 gold = COL(212,175,80), goldD = COL(150,118,48);
        if (stage == 0) {
            outlineRect(TX0 - 22, TY0 - 22, TX1 + 22, TY1 + 22, goldD);
            outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, gold);
            for (k = 0; k < 4; k++) {                      /* ornamentos nos cantos */
                float cx = (k & 1) ? TX1 + 22 : TX0 - 22, cy = (k & 2) ? TY1 + 22 : TY0 - 22;
                discN(cx, cy, 5.0f, goldD, 10);
                discN(cx, cy, 3.6f, COL(18,16,14), 10);
                discN(cx, cy, 1.8f, gold, 8);
            }
            for (k = 0; k < 4; k++) {                      /* perto das cacapas do meio */
                float cx = TCX + ((k & 1) ? 36.0f : -36.0f), cy = (k & 2) ? TY1 + 18.5f : TY0 - 18.5f;
                discN(cx, cy, 2.6f, gold, 8);
            }
        } else if (stage == 1) {
            u64 fl = CA(212,175,80,0x46), fl2 = CA(212,175,80,0x26);
            outlineRect(TX0 + 11, TY0 + 11, TX1 - 11, TY1 - 11, fl);
            ring(TCX, TCY, 30.0f, fl2); ring(TCX, TCY, 38.0f, fl2);
            line(TCX - 48, TCY, TCX - 22, TCY, fl2); line(TCX + 22, TCY, TCX + 48, TCY, fl2);
            line(TCX, TCY - 48, TCX, TCY - 22, fl2); line(TCX, TCY + 22, TCX, TCY + 48, fl2);
            ring(HEADX, TCY, 5.0f, fl); ring(FOOTX, TCY, 5.0f, fl);
        } else {
            for (k = 0; k < 6; k++) ring(PX[k], PY[k], PR[k] + 5.0f, gold);
            line(HEADX, TY0, HEADX, TY1, CA(212,175,80,0x38));
        }
    } else if (T->style == 2) {                            /* NEON animada */
        if (stage == 0) {
            const int N = 120;
            float W = TX1 - TX0 + 52, H = TY1 - TY0 + 52, L = 2 * (W + H);
            for (k = 0; k < N; k++) {                      /* luzes correndo em volta da mesa */
                float s = k * L / N, px, py, pulse;
                int a;
                if (s < W)              { px = TX0 - 26 + s;            py = TY0 - 26; }
                else if (s < W + H)     { px = TX1 + 26;                py = TY0 - 26 + (s - W); }
                else if (s < 2*W + H)   { px = TX1 + 26 - (s - W - H);  py = TY1 + 26; }
                else                    { px = TX0 - 26;                py = TY1 + 26 - (s - 2*W - H); }
                pulse = 0.5f + 0.5f * sinf(mt * 0.12f - k * 0.35f);
                a = 0x30 + (int)(0x50 * pulse);
                discN(px, py, 4.5f, hueCol(k / (float)N - mt * 0.006f, 0x18), 8);
                rect(px - 1.5f, py - 1.5f, px + 1.5f, py + 1.5f, hueCol(k / (float)N - mt * 0.006f, a));
            }
            outlineRect(TX0 - 15.5f, TY0 - 15.5f, TX1 + 15.5f, TY1 + 15.5f, hueCol(t * 0.3f, 0x78));
            outlineRect(TX0 - 21.5f, TY0 - 21.5f, TX1 + 21.5f, TY1 + 21.5f, hueCol(t * 0.3f + 0.5f, 0x40));
        } else if (stage == 1) {
            float rr = fmodf(mt * 0.8f, 125.0f) + 8.0f;
            for (k = 0; k < 3; k++) {                      /* faixas de luz subindo pelo feltro */
                float y0 = fmodf(mt * 0.9f + k * 88.0f, TY1 - TY0), y1 = y0 + 26.0f;
                if (y1 > TY1 - TY0) y1 = TY1 - TY0;
                rect(TX0, TY0 + y0, TX1, TY0 + y1, hueCol(t * 0.3f + k * 0.33f, 0x0e));
            }
            ringN(TCX, TCY, rr, 2.0f, 40, hueCol(t * 0.3f, 0x50 - (int)(0x40 * rr / 133.0f)));
            for (k = 0; k < 30; k++) {                     /* estrelinhas piscando */
                float sx = TX0 + 12 + fmodf(k * 137.5f, 504.0f), sy = TY0 + 10 + fmodf(k * 71.3f, 244.0f);
                float tw2 = sinf(mt * 0.07f + k * 1.9f);
                if (tw2 < 0) tw2 = 0;
                disc(sx, sy, 1.3f, CA(190, 230, 255, 0x10 + (int)(0x50 * tw2)));
            }
        } else {
            for (k = 0; k < 6; k++) {                      /* cacapas brilhando */
                float pulse = 0.5f + 0.5f * sinf(mt * 0.1f + k), h = k / 6.0f + mt * 0.006f;
                discN(PX[k], PY[k], PR[k] + 9.0f, hueCol(h, 0x10), 14);
                ring(PX[k], PY[k], PR[k] + 4.5f, hueCol(h, 0x80));
                ring(PX[k], PY[k], PR[k] + 6.0f + pulse * 2.0f, hueCol(h, 0x50));
            }
            line(HEADX, TY0, HEADX, TY1, hueCol(t * 0.3f, 0x50));
        }
    } else if (T->style == 10) {
        tableDecoNight(stage, mt);
    } else if (T->style == 11) {
        tableDecoHalloween(stage, mt);
    } else if (T->style == 12) {
        tableDecoVampire(stage, mt);
    } else if (T->style == 13) {
        tableDecoXmas(stage, mt);
    } else if (T->style == 14) {
        tableDecoHack(stage, mt);
    } else if (T->style == 15) {
        tableDecoChapolin(stage, mt);
    } else if (T->style == 16) {
        tableDecoTime(stage, mt);
    } else if (T->style == 17) {
        tableDecoGrinch(stage, mt);
    } else if (T->style == 18) {
        tableDecoAbyss(stage, mt);
    } else if (T->style == 19) {
        tableDecoGrave(stage, mt);
    } else if (T->style == 20) {
        tableDecoPolice(stage, mt);
    } else if (T->style == 21) {
        tableDecoPool(stage);
    } else if (T->style >= 3) {
        tableDecoFx(stage, T->style, mt);
    }
}

/* ---------- previa da mesa (tela de configurar partida) ---------- */
static void pvBall(float x, float y, float r, const u8 *c)
{
    discN(x, y, r, shade(c, 0.55f, 0), 8);
    discN(x - r * 0.1f, y - r * 0.1f, r * 0.8f, shade(c, 1.0f, 0), 8);
    rect(x - r * 0.55f, y - r * 0.6f, x - r * 0.2f, y - r * 0.28f, COL(255,255,255));
}

/* efeitos em miniatura das mesas novas (previa da tela de configurar) */
static void previewFx(int style, float x0, float y0, float x1, float y1, float f, int mt)
{
    int i;
    float w = x1 - x0, h = y1 - y0;
    for (i = 0; i < 16; i++) {
        float px = x0 + w * i / 15.0f, s = 0.5f + 0.5f * sinf(mt * 0.2f + i * 1.7f), py;
        switch (style) {
        case 3:                                              /* fogo: chamas em cima e embaixo */
            tri(px - 1.6f, y0 - 2.0f, px + 1.6f, y0 - 2.0f, px, y0 - 4.0f - 3.0f * s, CA(255, 120, 30, 0x78));
            tri(px - 1.6f, y1 + 2.0f, px + 1.6f, y1 + 2.0f, px, y1 + 4.0f + 3.0f * s, CA(255, 120, 30, 0x78));
            break;
        case 4:                                              /* gelo: neve */
            py = y0 + fmodf(mt * 0.35f * (1 + i % 3) + i * 9.0f, h);
            rect(px - 1.0f, py - 1.0f, px + 1.0f, py + 1.0f, CA(255, 255, 255, 0x70));
            break;
        case 5:                                              /* galaxia: estrelas */
            rect(x0 + fmodf(i * 37.3f, w) - 0.7f, y0 + fmodf(i * 23.9f, h) - 0.7f,
                 x0 + fmodf(i * 37.3f, w) + 0.7f, y0 + fmodf(i * 23.9f, h) + 0.7f, CA(255, 255, 255, 0x20 + (int)(0x50 * s)));
            break;
        case 6:                                              /* matrix: gotas de codigo */
            py = y0 + fmodf(mt * 0.8f * (1 + i % 3) + i * 13.0f, h);
            rect(px - 1.0f, py - 4.0f, px + 1.0f, py, CA(60, 255, 100, 0x70));
            break;
        case 7:                                              /* vegas: lampadas */
            rect(px - 1.2f, y0 - f * 0.75f - 1.2f, px + 1.2f, y0 - f * 0.75f + 1.2f,
                 CA(255, 225, 100, (((i + mt / 7) % 3) == 0) ? 0x78 : 0x30));
            rect(px - 1.2f, y1 + f * 0.75f - 1.2f, px + 1.2f, y1 + f * 0.75f + 1.2f,
                 CA(255, 225, 100, (((i + mt / 7) % 3) == 0) ? 0x78 : 0x30));
            break;
        case 10:                                             /* noite lua: lua e estrelas */
            if (i == 0) moonCrescent((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, h * 0.30f, 0x48, 236, 236, 214);
            rect(x0 + fmodf(i * 41.3f, w) - 0.7f, y0 + fmodf(i * 19.9f, h) - 0.7f,
                 x0 + fmodf(i * 41.3f, w) + 0.7f, y0 + fmodf(i * 19.9f, h) + 0.7f, CA(210, 222, 255, 0x20 + (int)(0x50 * s)));
            break;
        case 11:                                             /* halloween: lua, morcegos e luzes */
            if (i == 0) discN((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, h * 0.30f, CA(255, 150, 30, 0x2a), 16);
            if (i < 3) batShape(x0 + fmodf(mt * 0.6f * (1.0f + i * 0.4f) + i * 60.0f, w), y0 + h * (0.22f + 0.26f * i), 0.5f, sinf(mt * 0.4f + i), 0xa0);
            rect(px - 1.2f, y0 - f * 0.75f - 1.2f, px + 1.2f, y0 - f * 0.75f + 1.2f, (i & 1) ? CA(255, 140, 20, 0x78) : CA(120, 255, 60, 0x78));
            rect(px - 1.2f, y1 + f * 0.75f - 1.2f, px + 1.2f, y1 + f * 0.75f + 1.2f, (i & 1) ? CA(120, 255, 60, 0x78) : CA(255, 140, 20, 0x78));
            break;
        case 12:                                             /* vampiro: lua, morcego e sangue */
            if (i == 0) discN((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, h * 0.30f, CA(235, 228, 245, 0x22), 16);
            if (i == 3) batShape(x0 + fmodf(mt * 0.7f, w), y0 + h * 0.3f, 0.5f, sinf(mt * 0.4f), 0xa0);
            if (i % 4 == 1) { float dy2 = y0 + fmodf(mt * 0.4f + i * 7.0f, h); rect(px - 0.8f, y0, px + 0.8f, dy2, CA(190, 10, 30, 0x70)); }
            break;
        case 13:                                             /* natal: neve e luzes coloridas */
            py = y0 + fmodf(mt * 0.35f * (1 + i % 3) + i * 9.0f, h);
            rect(px - 1.0f, py - 1.0f, px + 1.0f, py + 1.0f, CA(255, 255, 255, 0x70));
            { int ph2 = (i + mt / 10) % 3; rect(px - 1.2f, y0 - f * 0.75f - 1.2f, px + 1.2f, y0 - f * 0.75f + 1.2f, ph2 == 0 ? CA(255, 70, 70, 0x80) : (ph2 == 1 ? CA(70, 240, 100, 0x80) : CA(255, 225, 80, 0x80))); }
            break;
        case 15:                                             /* chapolin: coracoes subindo */
            py = y1 - fmodf(mt * 0.4f * (1 + i % 3) + i * 11.0f, h);
            rect(px - 1.2f, py - 1.2f, px + 1.2f, py + 1.2f, CA(255, 214, 40, 0x78));
            if (i == 0) discN((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, h * 0.28f, CA(255, 214, 40, 0x22), 16);
            rect(px - 1.2f, y0 - f * 0.75f - 1.2f, px + 1.2f, y0 - f * 0.75f + 1.2f, (i & 1) ? CA(255, 214, 40, 0x80) : CA(214, 36, 40, 0x80));
            break;
        case 16:                                             /* tempo: relogio e brilhos */
            if (i == 0) { float cx2 = (x0 + x1) * 0.5f, cy2 = (y0 + y1) * 0.5f, a2 = mt * 0.03f; ring(cx2, cy2, h * 0.30f, CA(255, 214, 90, 0x50)); line(cx2, cy2, cx2 + cosf(a2) * h * 0.26f, cy2 + sinf(a2) * h * 0.26f, CA(120, 200, 255, 0x78)); }
            rect(px - 1.2f, y0 - f * 0.75f - 1.2f, px + 1.2f, y0 - f * 0.75f + 1.2f, (i & 1) ? CA(255, 214, 90, 0x80) : CA(120, 200, 255, 0x80));
            break;
        case 17:                                             /* grinch: fumaca verde, luzes verde/vermelho e coracao */
            if (i % 4 == 0) discN(px, y1 - fmodf(mt * 0.4f * (1 + i % 3) + i * 9.0f, h), 2.0f, CA(110, 220, 70, 0x40), 8);
            rect(px - 1.2f, y0 - f * 0.75f - 1.2f, px + 1.2f, y0 - f * 0.75f + 1.2f, (i & 1) ? CA(120, 230, 70, 0x80) : CA(230, 40, 50, 0x80));
            if (i == 0) ring((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, h * (0.12f + 0.05f * s), CA(214, 36, 48, 0x78));
            break;
        case 18:                                             /* abismo: bolhas subindo, ondas e um peixe */
            py = y1 - fmodf(mt * 0.4f * (1 + i % 3) + i * 11.0f, h);
            rect(px - 1.0f, py - 1.0f, px + 1.0f, py + 1.0f, CA(190, 240, 255, 0x70));
            rect(px - 1.2f, y0 - f * 0.75f - 1.2f + 1.5f * sinf(mt * 0.15f + i), px + 1.2f, y0 - f * 0.75f + 1.2f + 1.5f * sinf(mt * 0.15f + i), CA(120, 230, 255, 0x80));
            if (i == 0) { float fx2 = x0 + fmodf(mt * 0.5f, w); discN(fx2, (y0 + y1) * 0.5f, 2.4f, CA(255, 170, 60, 0x80), 8); tri(fx2 - 2.0f, (y0 + y1) * 0.5f, fx2 - 6.0f, (y0 + y1) * 0.5f - 2.4f, fx2 - 6.0f, (y0 + y1) * 0.5f + 2.4f, CA(255, 170, 60, 0x80)); }
            break;
        case 19:                                             /* cemiterio: fogo-fatuo subindo e luzes verdes */
            if (i % 3 == 0) discN(px, y1 - fmodf(mt * 0.3f * (1 + i % 3) + i * 9.0f, h), 2.0f, CA(150, 255, 170, 0x40), 8);
            rect(px - 1.2f, y0 - f * 0.75f - 1.2f, px + 1.2f, y0 - f * 0.75f + 1.2f, (i & 1) ? CA(150, 255, 170, 0x80) : CA(200, 210, 200, 0x60));
            if (i == 0) { float cx2 = (x0 + x1) * 0.5f, cy2 = (y0 + y1) * 0.5f; discN(cx2, cy2, 3.0f, CA(2, 2, 4, 0x90), 10); ring(cx2, cy2, 4.2f, CA(110, 76, 42, 0x90)); }
            break;
        case 20:                                             /* delegacia: luzes vermelhas e azuis piscando */
            rect(px - 1.2f, y0 - f * 0.75f - 1.2f, px + 1.2f, y0 - f * 0.75f + 1.2f, ((i + mt / 10) & 1) ? CA(255, 60, 70, 0x90) : CA(80, 140, 255, 0x90));
            rect(px - 1.2f, y1 + f * 0.75f - 1.2f, px + 1.2f, y1 + f * 0.75f + 1.2f, ((i + mt / 10) & 1) ? CA(80, 140, 255, 0x90) : CA(255, 60, 70, 0x90));
            break;
        case 14:                                             /* hack: bits caindo e varredura */
            py = y0 + fmodf(mt * 0.8f * (1 + i % 3) + i * 13.0f, h);
            rect(px - 0.8f, py - 4.0f, px + 0.8f, py, (i & 1) ? CA(255, 40, 190, 0x70) : CA(0, 255, 220, 0x70));
            if (i == 0) rect(x0, y0 + fmodf(mt * 0.8f, h), x1, y0 + fmodf(mt * 0.8f, h) + 2.0f, CA(0, 255, 220, 0x30));
            break;
        case 9:                                              /* faroeste: bola de mato e poeira */
            if (i == 0) ring(x0 + fmodf(mt * 0.5f, w), y1 - 6.0f - fabsf(sinf(mt * 0.06f)) * 6.0f, 2.6f, CA(112, 76, 38, 0x90));
            if ((i & 3) == 0) { float dx2 = x0 + fmodf(mt * (0.6f + i * 0.05f) + i * 17.0f, w); line(dx2, y0 + h * 0.3f + i * 1.4f, dx2 + 5.0f, y0 + h * 0.3f + i * 1.4f, CA(240, 215, 165, 0x28)); }
            break;
        case 21:                                             /* pool azul: sem efeito animado (antes caia nas faixas de arco-iris) */
            break;
        default:                                             /* arco-iris: faixas */
            if (i < 6) rect(x0, y0 + h * i / 6.0f, x1, y0 + h * (i + 1) / 6.0f, hueCol(mt * 0.003f + i / 6.0f, 0x28));
            break;
        }
    }
}

static void drawTablePreview(float cx, float cy, float w, const TableDef *T)
{
    float k = w / 588.0f, hw = (TX1 - TX0) * 0.5f * k, hh = (TY1 - TY0) * 0.5f * k;
    float x0 = cx - hw, x1 = cx + hw, y0 = cy - hh, y1 = cy + hh;
    float f = 26.0f * k, c = 11.0f * k, pk = 11.0f * k, pr = R * k;
    float qx[6], qy[6], bx, by;
    int i, j, mt = fxOn ? menuT : 0;
    u64 dia = COL(T->dia[0], T->dia[1], T->dia[2]);

    qx[0] = qx[3] = x0; qx[1] = qx[4] = cx; qx[2] = qx[5] = x1;
    qy[0] = qy[1] = qy[2] = y0; qy[3] = qy[4] = qy[5] = y1;

    rect(x0 - f - 2, y0 - f - 2, x1 + f + 2, y1 + f + 2, shade(T->wood, 0.36f, 0));
    rect(x0 - f, y0 - f, x1 + f, y1 + f, shade(T->wood, 0.95f, 0));
    rect(x0 - c - 1, y0 - c - 1, x1 + c + 1, y1 + c + 1, shade(T->wood, 0.45f, 0));
    rect(x0 - c, y0 - c, x1 + c, y1 + c, shade(T->felt, 0.5f, 0));
    rect(x0, y0, x1, y1, shade(T->felt, 1.0f, 0));
    rect(x0 + hw * 0.35f, y0 + hh * 0.35f, x1 - hw * 0.35f, y1 - hh * 0.35f, shade(T->felt, 1.0f, 10));

    for (j = 1; j <= 3; j++) {                         /* diamantes de mira */
        float dxl = (cx - x0) * j / 4.0f, dxr = (x1 - cx) * j / 4.0f, dy = (y1 - y0) * j / 4.0f, o = (c + f) * 0.5f;
        rect(x0 + dxl - 1, y0 - o - 1, x0 + dxl + 1, y0 - o + 1, dia);
        rect(x0 + dxl - 1, y1 + o - 1, x0 + dxl + 1, y1 + o + 1, dia);
        rect(cx + dxr - 1, y0 - o - 1, cx + dxr + 1, y0 - o + 1, dia);
        rect(cx + dxr - 1, y1 + o - 1, cx + dxr + 1, y1 + o + 1, dia);
        rect(x0 - o - 1, y0 + dy - 1, x0 - o + 1, y0 + dy + 1, dia);
        rect(x1 + o - 1, y0 + dy - 1, x1 + o + 1, y0 + dy + 1, dia);
    }
    for (i = 0; i < 6; i++) {                          /* cacapas */
        discN(qx[i], qy[i], pk + 1.8f, COL(110,114,122), 12);
        discN(qx[i], qy[i], pk + 0.9f, COL(36,38,44), 12);
        discN(qx[i], qy[i], pk, COL(2,2,4), 12);
    }

    if (T->style == 1) {                               /* preta ouro */
        u64 gl = COL(212,175,80);
        outlineRect(x0 - f * 0.62f, y0 - f * 0.62f, x1 + f * 0.62f, y1 + f * 0.62f, gl);
        outlineRect(x0 + 4, y0 + 4, x1 - 4, y1 - 4, CA(212,175,80,0x50));
        for (i = 0; i < 6; i++) ring(qx[i], qy[i], pk + 2.2f, gl);
    } else if (T->style == 2) {                        /* neon animada */
        const int N = 40;
        float g = f * 0.78f, W = (x1 - x0) + 2 * g, H = (y1 - y0) + 2 * g, L = 2 * (W + H);
        for (i = 0; i < N; i++) {
            float s2 = i * L / N, px, py;
            if (s2 < W)             { px = x0 - g + s2;             py = y0 - g; }
            else if (s2 < W + H)    { px = x1 + g;                  py = y0 - g + (s2 - W); }
            else if (s2 < 2*W + H)  { px = x1 + g - (s2 - W - H);   py = y1 + g; }
            else                    { px = x0 - g;                  py = y1 + g - (s2 - 2*W - H); }
            rect(px - 1.2f, py - 1.2f, px + 1.2f, py + 1.2f,
                 hueCol(i / (float)N - mt * 0.006f, 0x50 + (int)(0x2c * (1.0f + sinf(mt * 0.12f - i * 0.4f)))));
        }
        for (i = 0; i < 6; i++) ring(qx[i], qy[i], pk + 2.2f, hueCol(i / 6.0f + mt * 0.006f, 0x80));
        ringN(cx, cy, fmodf(mt * 0.25f, hh), 1.0f, 24, hueCol(mt * 0.006f, 0x50));
    } else if (T->style >= 3) {
        previewFx(T->style, x0, y0, x1, y1, f, mt);
    }

    /* bolas na posicao inicial do modo escolhido */
    if (gmode == GM_BR) {
        const u8 *c0 = BC0[BRC[colSel[0]]], *c1 = BC0[BRC[colSel[1]]];
        for (j = 0; j < 8; j++) {
            brPos(j, &bx, &by);
            pvBall(x0 + (bx - TX0) * k, y0 + (by - TY0) * k, pr, j < 4 ? c0 : c1);
        }
        pvBall(cx, cy, pr, WB_COL[wbSel[0]]);
    } else {
        int n = 0, r2, cc;
        for (r2 = 0; r2 < 5; r2++)
            for (cc = 0; cc <= r2; cc++) {
                bx = FOOTX + r2 * (R * 1.76f); by = TCY + (cc - r2 * 0.5f) * (2 * R + 0.4f);
                pvBall(x0 + (bx - TX0) * k, y0 + (by - TY0) * k, pr, THEME[ballTheme][n++ % 8]);
            }
        pvBall(x0 + (HEADX - TX0) * k, cy, pr, WB_COL[wbSel[0]]);
    }
}

static void drawTable(void)
{
    const TableDef *T = &TABLES[(tourney && tTable >= 0) ? tTable : tableSel];
    u64 w0 = shade(T->wood, 0.36f, 0), w1 = shade(T->wood, 0.80f, 0), w2 = shade(T->wood, 1.0f, 0);
    u64 whi = shade(T->wood, 1.28f, 14), wlo = shade(T->wood, 0.48f, 0), wgr = shade(T->wood, 0.42f, 0);
    u64 ga = shade(T->wood, 0.90f, 0), gb = shade(T->wood, 1.10f, 5);
    int p, k;
    float x, y;

    tcRail = w2;
    tcCush = shade(T->felt, 1.06f, 8); tcCushHi = shade(T->felt, 1.55f, 36); tcCushLo = shade(T->felt, 0.62f, 0);   /* borracha mais clara que o feltro */
    tcMouth = shade(T->wood, 0.10f, 0);                                                                              /* quase preto: continua o fundo do buraco ate a ponta da tabela */
    tcDia = COL(T->dia[0], T->dia[1], T->dia[2]);

    /* sombra da mesa no chao */
    rect(TX0 - 22, TY0 - 20, TX1 + 40, TY1 + 42, GS_SETREG_RGBAQ(0, 0, 0, 0x30, 0x00));

    /* moldura de madeira em camadas (bisel) */
    rect(TX0 - 30, TY0 - 30, TX1 + 30, TY1 + 30, w0);
    rect(TX0 - 28, TY0 - 28, TX1 + 28, TY1 + 28, w1);
    rect(TX0 - 25, TY0 - 25, TX1 + 25, TY1 + 25, w2);
    line(TX0 - 28, TY0 - 28, TX1 + 28, TY0 - 28, whi);
    line(TX0 - 28, TY0 - 28, TX0 - 28, TY1 + 28, whi);
    line(TX0 - 28, TY1 + 28, TX1 + 28, TY1 + 28, wlo);
    line(TX1 + 28, TY0 - 28, TX1 + 28, TY1 + 28, wlo);

    /* veios da madeira */
    for (k = 0; k < 4; k++) {
        u64 gc = (k & 1) ? ga : gb;
        line(TX0 - 24, TY0 - 23 + k*3.4f, TX1 + 24, TY0 - 23 + k*3.4f, gc);
        line(TX0 - 24, TY1 + 13 + k*3.4f, TX1 + 24, TY1 + 13 + k*3.4f, gc);
        line(TX0 - 23 + k*3.4f, TY0 - 24, TX0 - 23 + k*3.4f, TY1 + 24, gc);
        line(TX1 + 13 + k*3.4f, TY0 - 24, TX1 + 13 + k*3.4f, TY1 + 24, gc);
    }

    /* brilho e sombra na madeira */
    rect(TX0 - 25, TY0 - 25, TX1 + 25, TY0 - 22, CA(255, 255, 255, 0x0c));
    rect(TX0 - 25, TY0 - 25, TX0 - 22, TY1 + 25, CA(255, 255, 255, 0x08));
    rect(TX0 - 25, TY1 + 22, TX1 + 25, TY1 + 25, CA(0, 0, 0, 0x16));
    rect(TX1 + 22, TY0 - 25, TX1 + 25, TY1 + 25, CA(0, 0, 0, 0x12));

    /* sulco entre a madeira e a borracha */
    rect(TX0 - 12, TY0 - 12, TX1 + 12, TY1 + 12, wgr);
    tableDeco(0, T);

    /* feltro com leve brilho no centro (luz de cima) */
    rect(TX0, TY0, TX1, TY1, shade(T->felt, 1.0f, 0));
    for (k = 1; k < 15; k++)
        rect(TX0 + k*11, TY0 + k*7, TX1 - k*11, TY1 - k*7, shade(T->felt, 1.0f, (k * 3) / 2));
    for (k = 0; k < 6; k++) {                           /* sombra do feltro junto das tabelas (profundidade) */
        int a = 0x24 - k * 4;
        float d = k * 3.0f;
        rect(TX0, TY0 + d, TX1, TY0 + d + 3, CA(0, 0, 0, a));
        rect(TX0, TY1 - d - 3, TX1, TY1 - d, CA(0, 0, 0, a));
        rect(TX0 + d, TY0 + d + 3, TX0 + d + 3, TY1 - d - 3, CA(0, 0, 0, a));
        rect(TX1 - d - 3, TY0 + d + 3, TX1 - d, TY1 - d - 3, CA(0, 0, 0, a));
    }
    tableDeco(1, T);

    /* tabelas (borrachas) com as pontas inclinadas nas cacapas */
    {   /* a abertura da tabela e a mesma da fisica (PRC), senao a bola bate numa parede invisivel */
        float ce = PRC[0] * tabPock() - POC, cm = PRC[1] * tabPock();
        u64 scoop = shade(T->felt, 0.42f, 0);                            /* vao das cacapas do meio: escuro, continua o buraco ate o feltro */
        rect(TCX - cm, TY0 - 11, TCX + cm, TY0, scoop);                  /* meio: o vao entre as pontas mostra o feltro (sombra do buraco) */
        rect(TCX - cm, TY1, TCX + cm, TY1 + 11, scoop);
        cushH(TX0 + ce, TCX - cm, TY0 - 11, TY0, 11.0f, 3.0f);
        cushH(TCX + cm, TX1 - ce, TY0 - 11, TY0, 3.0f, 11.0f);
        cushH(TX0 + ce, TCX - cm, TY1 + 11, TY1, 11.0f, 3.0f);
        cushH(TCX + cm, TX1 - ce, TY1 + 11, TY1, 3.0f, 11.0f);
        cushV(TY0 + ce, TY1 - ce, TX0 - 11, TX0);
        cushV(TY0 + ce, TY1 - ce, TX1 + 11, TX1);
    }

    /* diamantes de mira na madeira */
    for (k = 1; k <= 3; k++) {
        x = TX0 + (TCX - TX0) * k / 4.0f;
        diamond(x, TY0 - 18.5f);  diamond(x, TY1 + 18.5f);
        x = TCX + (TX1 - TCX) * k / 4.0f;
        diamond(x, TY0 - 18.5f);  diamond(x, TY1 + 18.5f);
        y = TY0 + (TY1 - TY0) * k / 4.0f;
        diamond(TX0 - 18.5f, y);  diamond(TX1 + 18.5f, y);
    }

    /* cacapas iguais ao 8 Ball Pool: buraco redondo com degrade vinho->preto e chapa prateada so no lado de fora dos cantos */
    for (p = 0; p < 6; p++) {
        float r0 = PR[p] * tabPock(), px = PX[p], py = PY[p];
        int mid = (p % 3 == 1);
        static const float IA[6] = { 0.7854f, 1.5708f, 2.3562f, 5.4978f, 4.7124f, 3.9270f };
        float ia = IA[p];                                                            /* direcao do buraco para dentro da mesa */
        if (mid) { float cmv = PRC[1] * tabPock() - 1.0f; if (r0 < cmv) r0 = cmv; }   /* so visual: o buraco do meio preenche o vao entre as tabelas (abertura para a bola entrar) */
        if (!mid) {                                                                  /* chapa prateada do canto (nao entra no feltro) */
            float a0 = ia + 0.75f, a1 = ia + 6.283f - 0.75f;
            arcBand(px, py, r0 + 0.2f, r0 + 4.2f, a0, a1, COL(150,154,166));
            arcBand(px, py, r0 + 0.2f, r0 + 1.4f, a0, a1, COL(226,228,238));         /* brilho interno */
            arcBand(px, py, r0 + 3.4f, r0 + 4.2f, a0, a1, COL(78,80,92));            /* borda externa */
        }
        discN(px, py, r0 + 1.0f, CA(0, 0, 0, 0x30), 28);                             /* contorno escuro */
        discN(px, py, r0, shade(T->wood, 0.34f, 0), 28);                             /* boca: vinho nas bordas */
        discN(px + 0.9f, py + 1.1f, r0 * 0.86f, shade(T->wood, 0.20f, 0), 28);       /* degrade para o fundo */
        discN(px + 1.8f, py + 2.2f, r0 * 0.68f, COL(8,3,4), 28);
        discN(px + 2.6f, py + 3.1f, r0 * 0.48f, COL(0,0,0), 24);                     /* fundo preto */
    }

    /* linha da cabeceira e marcas do feltro */
    line(HEADX, TY0, HEADX, TY1, shade(T->felt, 1.0f, 34));
    disc(HEADX, TCY, 2.2f, shade(T->felt, 0.77f, 0));
    disc(FOOTX, TCY, 2.2f, shade(T->felt, 0.77f, 0));
    tableDeco(2, T);
}

static void thick(float x1, float y1, float x2, float y2, u64 c)
{
    float dx = x2 - x1, dy = y2 - y1, l = sqrtf(dx*dx + dy*dy), nx, ny;
    if (l < 0.01f) return;
    nx = -dy / l * 0.6f; ny = dx / l * 0.6f;
    line(x1 + nx, y1 + ny, x2 + nx, y2 + ny, c);
    line(x1 - nx, y1 - ny, x2 - nx, y2 - ny, c);
}

/* ---- linha de mira: feixes com brilho (3 camadas), sempre ate a bola ---- */
static u64 CA(int r, int gg, int bb, int a)
{
    if (a < 0) a = 0;
    if (a > 0x80) a = 0x80;
    return GS_SETREG_RGBAQ(r, gg, bb, a, 0x00);
}

static void beam(float x1, float y1, float x2, float y2, float w, u64 c)
{
    float dx = x2 - x1, dy = y2 - y1, l = sqrtf(dx*dx + dy*dy), nx, ny;
    if (l < 0.01f) return;
    nx = -dy / l * w * 0.5f; ny = dx / l * w * 0.5f;
    quad(x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny, c);
}

static void glow(float x1, float y1, float x2, float y2, int a, int r, int gg, int bb)
{
    if (fxOn) {
        beam(x1, y1, x2, y2, 6.0f, CA(r, gg, bb, a / 8));
        beam(x1, y1, x2, y2, 3.6f, CA(r, gg, bb, a / 4));
    }
    beam(x1, y1, x2, y2, 1.8f, CA(r, gg, bb, a));
}

/* linha que vai sumindo (a0 -> a1) */
static void glowFade(float x1, float y1, float x2, float y2, int a0, int a1, int r, int gg, int bb)
{
    int k;
    for (k = 0; k < 8; k++) {
        float u0 = k / 8.0f, u1 = (k + 1) / 8.0f;
        glow(x1 + (x2-x1)*u0, y1 + (y2-y1)*u0, x1 + (x2-x1)*u1, y1 + (y2-y1)*u1,
             a0 + (int)((a1 - a0) * (u0 + u1) * 0.5f), r, gg, bb);
    }
}

static void ringN(float cx, float cy, float r, float w, int n, u64 c)
{
    int i;
    const float *t = circ(n);
    for (i = 0; i < n; i++) {
        float c0, s0, c1, s1;
        if (t) { c0 = t[2*i]; s0 = t[2*i+1]; c1 = t[2*i+2]; s1 = t[2*i+3]; }
        else {
            float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n;
            c0 = cosf(a0); s0 = sinf(a0); c1 = cosf(a1); s1 = sinf(a1);
        }
        beam(cx + c0*r, cy + s0*r, cx + c1*r, cy + s1*r, w, c);
    }
}

/* primeiro obstaculo no caminho do centro da bola: bola (hit>0) ou tabela (wall) */
static float castRay(float cx, float cy, float dx, float dy, int ignore, int *hit, int *wall)
{
    float tmin = 1e9f, t;
    int i;
    *hit = -1; *wall = 0;
    for (i = 1; i < 16; i++) {
        float fx, fy, proj, perp2;
        if (!b[i].in || i == ignore) continue;
        fx = b[i].x - cx; fy = b[i].y - cy;
        proj = fx*dx + fy*dy;
        if (proj < 0) continue;
        perp2 = fx*fx + fy*fy - proj*proj;
        if (perp2 > 4*R*R) continue;
        t = proj - sqrtf(4*R*R - perp2);
        if (t < tmin) { tmin = t; *hit = i; *wall = 0; }
    }
    if (dx >  0.0001f) { t = (TX1-R-cx)/dx; if (t < tmin) { tmin = t; *hit = -1; *wall = 1; } }
    if (dx < -0.0001f) { t = (TX0+R-cx)/dx; if (t < tmin) { tmin = t; *hit = -1; *wall = 1; } }
    if (dy >  0.0001f) { t = (TY1-R-cy)/dy; if (t < tmin) { tmin = t; *hit = -1; *wall = 2; } }
    if (dy < -0.0001f) { t = (TY0+R-cy)/dy; if (t < tmin) { tmin = t; *hit = -1; *wall = 2; } }
    if (tmin < 0) tmin = 0;
    return tmin;
}

static int nearPocket(float x, float y)
{
    int k;
    for (k = 0; k < 6; k++) {
        float ex = x - PX[k], ey = y - PY[k];
        if (ex*ex + ey*ey < PRC[k] * PRC[k] * tabPock() * tabPock()) return k;
    }
    return -1;
}

/* a bola alvo, saindo de (x,y) na direcao (nx,ny), chega numa cacapa sem nada no meio?
   devolve o indice da cacapa (ou -1) e a distancia ate o centro dela em *len */
static int pocketPath(float x, float y, float nx, float ny, int ignore, float *len)
{
    int k, i, best = -1;
    float bt = 1e9f;
    for (k = 0; k < 6; k++) {
        float fx = PX[k] - x, fy = PY[k] - y;
        float proj = fx*nx + fy*ny, perp = fx*ny - fy*nx, lim = PRC[k] * tabPock() - 1.0f;
        int blocked = 0;
        if (perp < 0) perp = -perp;
        if (proj < R || perp > lim || proj >= bt) continue;
        for (i = 1; i < 16 && !blocked; i++) {            /* outra bola no caminho? */
            float gx, gy, pp, qq;
            if (!b[i].in || i == ignore) continue;
            gx = b[i].x - x; gy = b[i].y - y;
            pp = gx*nx + gy*ny; qq = gx*ny - gy*nx;
            if (qq < 0) qq = -qq;
            if (pp > 0 && pp < proj && qq < 2.0f * R) blocked = 1;
        }
        if (!blocked) { bt = proj; best = k; }
    }
    if (best >= 0) *len = bt;
    return best;
}

/* pulo armado: arco e marca de onde a branca deve cair (estimativa sem atrito) */
static void drawJumpMark(void)
{
    float p = charging ? pwr : 0.5f, vz = (150.0f + 90.0f * p) * cueJumpMul(curCue());
    float sp = p * MAXV * cuePowMul(curCue()) * 0.9f * 0.9f, T = 2.0f * vz / JUMP_G, dist = sp * T;
    float apex = vz * vz / (2.0f * JUMP_G), dx = cosf(ang), dy = sinf(ang);
    float lx = b[0].x + dx * dist, ly = b[0].y + dy * dist;
    int k;
    if (lx < TX0 + R) lx = TX0 + R;
    if (lx > TX1 - R) lx = TX1 - R;
    if (ly < TY0 + R) ly = TY0 + R;
    if (ly > TY1 - R) ly = TY1 - R;
    for (k = 1; k < 8; k++) {
        float u = k / 8.0f;
        discN(b[0].x + (lx - b[0].x) * u, b[0].y + (ly - b[0].y) * u - 4.0f * apex * u * (1.0f - u) * 0.55f,
              1.5f, CA(130, 255, 160, 0x60), 6);
    }
    ringN(lx, ly, R + 2.0f + 1.5f * sinf(menuT * 0.2f), 1.8f, 20, CA(120, 255, 150, 0x70));
    ringN(lx, ly, R * 0.45f, 1.2f, 12, CA(120, 255, 150, 0x50));
}

/* mira estilo 8 Ball Pool: a linha SEMPRE chega ate a bola fantasma (ou ate a
   tabela, sem quicar). Depois do impacto mostra um trecho curto da bola alvo e do
   desvio da branca. O taco (stat "mira") aumenta esses trechos; o MIRA LONGA
   ainda mostra um rebote na tabela. */
static void drawGuide(void)
{
    float dx = cosf(ang), dy = sinf(ang), cx = b[0].x, cy = b[0].y;
    float gs = cueGuideScale(curCue());
    int sn = (curCue() == CUE_SNIPER || hackOn);
    int lg = sn ? 70 : 255;                                /* sniper: laser vermelho */
    int seg, maxSeg = sn ? 4 : (CUES[curCue()].aim >= 100 ? 2 : 1);   /* MIRA LONGA: 1 rebote | SNIPER: 3 */
    int gr = 255, gg = lg, gb = lg, gsel = gdSel[optWho()];           /* cor escolhida da linha guia */
    if (gsel > 0) {
        if (gsel == NGD - 1) hue3(menuT * 0.008f, &gr, &gg, &gb);
        else { gr = GD_COL[gsel][0]; gg = GD_COL[gsel][1]; gb = GD_COL[gsel][2]; }
    }

    for (seg = 0; seg < maxSeg; seg++) {
        int hit, wall, al = seg == 0 ? 0x7c : (sn ? 0x64 - seg * 0x14 : 0x50);
        float t = castRay(cx, cy, dx, dy, -1, &hit, &wall);
        float gx = cx + dx*t, gy = cy + dy*t;
        float sx = cx + (seg == 0 ? dx*R : 0), sy = cy + (seg == 0 ? dy*R : 0);
        float ex = gx - dx*R, ey = gy - dy*R;

        if ((ex - sx)*dx + (ey - sy)*dy > 0.5f) glow(sx, sy, ex, ey, al, gr, gg, gb);
        discN(gx, gy, R, CA(gr, gg, gb, 0x12), 20);
        ringN(gx, gy, R,        1.6f, 28, CA(gr, gg, gb, al));
        ringN(gx, gy, R - 1.6f, 1.0f, 28, CA(gr, gg, gb, al / 2));

        if (hit > 0 && !tipsOn) return;                  /* dicas desligadas: so a linha ate a bola */
        if (hit > 0) {
            float nx = b[hit].x - gx, ny = b[hit].y - gy, nl = sqrtf(nx*nx + ny*ny);
            float dot, tx, ty, tl, L, tmax;
            int h2, w2, pk, pkp;
            float pl = 0.0f;
            if (nl < 0.01f) return;
            nx /= nl; ny /= nl;
            dot = dx*nx + dy*ny; if (dot < 0) dot = 0;

            /* caminho da bola alvo: ate a tabela/outra bola, destacando cacapa */
            L = (22.0f + 70.0f * dot) * gs;
            if (sn) L = 1.0e6f;                           /* sniper: caminho completo */
            tmax = castRay(b[hit].x, b[hit].y, nx, ny, hit, &h2, &w2);
            pkp = -1; (void)pl;                           /* sem destaque verde da cacapa */
            if (pkp >= 0) {                               /* vai para a cacapa: desenha o caminho ate o buraco */
                float le = pl - PR[pkp] * 0.4f;
                glowFade(b[hit].x + nx*R, b[hit].y + ny*R, b[hit].x + nx*le, b[hit].y + ny*le,
                         0x7c, 0x68, 90, 255, 120);
                ringN(PX[pkp], PY[pkp], PR[pkp] + 3.0f, 2.0f, 28, CA(90, 255, 120, 0x78));
                ringN(PX[pkp], PY[pkp], PR[pkp] + 6.0f, 1.2f, 28, CA(90, 255, 120, 0x40));
                L = 0.0f;                                 /* ja desenhado: pula o trecho curto */
            } else if (L >= tmax - 0.5f) {
                L = tmax;
            }
            if (L > R) glowFade(b[hit].x + nx*R, b[hit].y + ny*R,
                                b[hit].x + nx*L,  b[hit].y + ny*L, 0x7c, L >= tmax - 0.5f ? 0x50 : 0x10,
                                gsel > 0 ? gr : 255, gsel > 0 ? gg : 255, gsel > 0 ? gb : 255);

            /* desvio da bola branca */
            tx = dx - dot*nx; ty = dy - dot*ny; tl = sqrtf(tx*tx + ty*ty);
            if (tl > 0.05f) {
                tx /= tl; ty /= tl;
                L = (12.0f + 40.0f * tl) * gs;
                if (sn) L = 1.0e6f;
                tmax = castRay(gx, gy, tx, ty, hit, &h2, &w2);
                if (L > tmax) L = tmax;
                if (L > R) glowFade(gx + tx*R, gy + ty*R, gx + tx*L, gy + ty*L, 0x60, 0x10,
                                gsel > 0 ? gr : 190, gsel > 0 ? gg : 215, gsel > 0 ? gb : 255);
            }
            return;
        }

        /* tabela: cacapa encerra; senao quica e continua */
        if (nearPocket(gx, gy) >= 0) {
            int pk = nearPocket(gx, gy);
            ringN(PX[pk], PY[pk], PR[pk] + 2.0f, 2.0f, 24, CA(255, 90, 80, 0x78));
            return;
        }
        if (wall == 1) dx = -dx; else dy = -dy;
        cx = gx; cy = gy;
    }
}

/* taco realista: afunila da ponta (azul) ate o cabo, com virola,
   emenda, faixa, incrustacao, borracha, sombra e brilho */
static float cueDx, cueDy, cueD0, cueBx, cueBy;
static int cueAl;

static float cueW(float u)
{
    if (u < 13.0f)  return 2.2f + 0.3f * u / 13.0f;
    if (u < 166.0f) return 2.6f + 1.4f * (u - 13.0f) / 153.0f;
    return 4.2f + 2.0f * (u - 166.0f) / 124.0f;
}

/* ponto do taco: u = distancia ao longo do taco, s = lado */
static void cuePt(float u, float s, float *ox, float *oy)
{
    float d = cueD0 + u;
    *ox = cueBx - cueDx * d - cueDy * s;
    *oy = cueBy - cueDy * d + cueDx * s;
}

static void cueSeg(float u1, float u2, const u8 *c)
{
    float w1 = cueW(u1), w2 = cueW(u2);
    float ax, ay, bx, by, cx, cy, dx, dy, ex, ey, fx, fy;
    cuePt(u1, -w1, &ax, &ay); cuePt(u2, -w2, &bx, &by);
    cuePt(u2, 0, &cx, &cy);   cuePt(u1, 0, &dx, &dy);
    cuePt(u2, w2, &ex, &ey);  cuePt(u1, w1, &fx, &fy);
    quad(ax, ay, bx, by, cx, cy, dx, dy, shadeA(c, 1.2f, 12, cueAl));   /* lado claro */
    quad(dx, dy, cx, cy, ex, ey, fx, fy, shadeA(c, 0.66f, 0, cueAl));   /* lado escuro */
}

static void shovelTri(float u1, float s1, float u2, float s2, float u3, float s3, u64 c)
{
    float ax, ay, bx, by, cx, cy;
    cuePt(u1, s1, &ax, &ay); cuePt(u2, s2, &bx, &by); cuePt(u3, s3, &cx, &cy);
    tri(ax, ay, bx, by, cx, cy, c);
}

static void drawCue(float px, float py, float d0, int al, float a, int ci)
{
    static const u8 FER[3]   = { 238, 238, 228 };
    static const u8 JOINT[3] = { 188, 188, 196 };
    static const u8 RUB[3]   = { 22, 22, 22 };
    static const u8 CHK[3] = { 70, 130, 230 };
    const CueDef *cd = &CUES[ci];
    float ax, ay, bx, by, cx, cy, ex, ey, cm = cueChalk * 0.75f;
    u8 tipc[3];
    int q;

    for (q = 0; q < 3; q++) tipc[q] = (u8)(cd->tip[q] * (1.0f - cm) + CHK[q] * cm);
    cueDx = cosf(a); cueDy = sinf(a);
    cueBx = px; cueBy = py; cueD0 = d0; cueAl = al;

    /* sombra no feltro */
    cuePt(0, -2.2f, &ax, &ay);   cuePt(290, -6.2f, &bx, &by);
    cuePt(290, 6.2f, &cx, &cy);  cuePt(0, 2.2f, &ex, &ey);
    quad(ax + 4, ay + 5, bx + 6, by + 7, cx + 6, cy + 7, ex + 4, ey + 5,
         GS_SETREG_RGBAQ(0, 12, 5, (al * 30) / 128, 0x00));

    if (cd->fx == 17) {                                       /* COVEIRO: o taco e uma pa (lamina na ponta, cabo de madeira e punho em T) */
        static const u8 STL[3] = { 176, 184, 192 };
        u64 lt = shadeA(STL, 1.15f, 10, al), dk = shadeA(STL, 0.62f, 0, al);
        static const float L[5][2] = { {0.0f, 0.0f}, {4.0f, -7.0f}, {16.0f, -9.0f}, {40.0f, -3.0f}, {40.0f, 0.0f} };
        int k2;
        for (k2 = 0; k2 < 3; k2++) {                          /* lamina: metade clara e metade escura */
            shovelTri(L[0][0], L[0][1], L[k2 + 1][0], L[k2 + 1][1], L[k2 + 2][0], L[k2 + 2][1], lt);
            shovelTri(L[0][0], -L[0][1], L[k2 + 1][0], -L[k2 + 1][1], L[k2 + 2][0], -L[k2 + 2][1], dk);
        }
        cuePt(6.0f, 0.0f, &ax, &ay); cuePt(38.0f, 0.0f, &bx, &by);
        line(ax, ay, bx, by, shadeA(STL, 0.8f, 0, al));       /* nervura da lamina */
        cueSeg(40.0f, 50.0f, JOINT);                          /* encaixe */
        cueSeg(50.0f, 262.0f, cd->wood);                      /* cabo */
        cueSeg(262.0f, 284.0f, cd->butt);
        cuePt(280.0f, -11.0f, &ax, &ay); cuePt(290.0f, -11.0f, &bx, &by);
        cuePt(290.0f, 11.0f, &cx, &cy);  cuePt(280.0f, 11.0f, &ex, &ey);
        quad(ax, ay, bx, by, cx, cy, ex, ey, shadeA(cd->butt, 1.0f, 0, al));   /* punho em T */
        cuePt(50.0f, -1.2f, &ax, &ay); cuePt(258.0f, -3.0f, &bx, &by);
        line(ax, ay, bx, by, shadeA(cd->wood, 1.3f, 40, al));
    } else {
    cueSeg(0.0f,   3.5f,   tipc);
    cueSeg(3.5f,   13.0f,  FER);
    cueSeg(13.0f,  166.0f, cd->wood);
    cueSeg(166.0f, 172.0f, JOINT);
    cueSeg(172.0f, 200.0f, cd->butt);
    cueSeg(200.0f, 214.0f, cd->wrap);
    cueSeg(214.0f, 232.0f, cd->butt);
    cueSeg(232.0f, 237.0f, cd->inlay);
    cueSeg(237.0f, 284.0f, cd->butt);
    cueSeg(284.0f, 290.0f, RUB);

    /* brilho ao longo do taco */
    cuePt(14.0f, -1.2f, &ax, &ay);  cuePt(160.0f, -2.3f, &bx, &by);
    line(ax, ay, bx, by, shadeA(cd->wood, 1.3f, 40, al));
    cuePt(174.0f, -2.4f, &ax, &ay); cuePt(284.0f, -3.9f, &bx, &by);
    line(ax, ay, bx, by, shadeA(cd->butt, 1.7f, 30, al));
    }

    if (cd->fx) {
        int mt = fxOn ? menuT : 0, k;
        if (cd->fx == 1) {                                        /* brilho pulsando na ponta e no detalhe */
            float p = 0.5f + 0.5f * sinf(mt * 0.12f);
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 5.0f + 2.5f * p, CA(cd->tip[0], cd->tip[1], cd->tip[2], (al * (0x18 + (int)(0x18 * p))) / 128), 10);
            cuePt(234.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 4.0f + 2.0f * p, CA(cd->inlay[0], cd->inlay[1], cd->inlay[2], (al * 0x20) / 128), 10);
        } else if (cd->fx == 2) {                                 /* arco-iris: fita e detalhe mudam de cor */
            int r, gg, bb;
            u8 c1[3], c2[3];
            hue3(mt * 0.006f, &r, &gg, &bb);          c1[0] = r; c1[1] = gg; c1[2] = bb;
            hue3(mt * 0.006f + 0.5f, &r, &gg, &bb);   c2[0] = r; c2[1] = gg; c2[2] = bb;
            cueSeg(200.0f, 214.0f, c1);
            cueSeg(232.0f, 237.0f, c2);
        } else if (cd->fx == 9) {                                 /* paises: faixas nas cores da bandeira */
            const u8 *fc[3];
            fc[0] = cd->wood; fc[1] = cd->butt; fc[2] = cd->inlay;
            for (k = 0; k < 6; k++) cueSeg(26.0f + k * 24.0f, 38.0f + k * 24.0f, fc[(k + mt / 45) % 3]);
            for (k = 0; k < 3; k++) {
                float u = fmodf(mt * 1.1f + k * 97.0f, 290.0f), tw2 = 0.5f + 0.5f * sinf(mt * 0.18f + k * 2.1f);
                cuePt(u, (k & 1) ? 1.5f : -1.5f, &ax, &ay);
                disc(ax, ay, 1.0f + tw2, CA(255, 255, 255, (al * (0x28 + (int)(0x40 * tw2))) / 128));
            }
        } else if (cd->fx == 11) {                                /* grinch: ponta verde brilhando e o coracao que cresce */
            float p = 0.5f + 0.5f * sinf(mt * 0.1f), hs = 3.0f + 3.0f * fabsf(sinf(mt * 0.05f));
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 4.0f + 3.0f * p, CA(150, 255, 90, (al * (0x18 + (int)(0x20 * p))) / 128), 10);
            cuePt(234.0f, 0.0f, &ax, &ay);
            discN(ax, ay, hs, CA(230, 40, 50, (al * 0x70) / 128), 10);
            discN(ax - 0.9f, ay - 1.0f, hs * 0.3f, CA(255, 255, 255, (al * 0x60) / 128), 6);
            for (k = 0; k < 3; k++) {
                float u = fmodf(mt * 1.1f + k * 97.0f, 290.0f), tw2 = 0.5f + 0.5f * sinf(mt * 0.18f + k * 2.1f);
                cuePt(u, (k & 1) ? 1.4f : -1.4f, &ax, &ay);
                disc(ax, ay, 1.0f + tw2, (k & 1) ? CA(255, 230, 70, (al * (0x30 + (int)(0x40 * tw2))) / 128) : CA(150, 255, 90, (al * (0x30 + (int)(0x40 * tw2))) / 128));
            }
        } else if (cd->fx == 12) {                                /* tridente: ponta turquesa, ondas correndo e detalhe dourado */
            float p = 0.5f + 0.5f * sinf(mt * 0.1f);
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 4.0f + 3.0f * p, CA(120, 240, 255, (al * (0x18 + (int)(0x20 * p))) / 128), 10);
            for (k = 0; k < 4; k++) {
                float u = fmodf(mt * 1.2f + k * 72.0f, 290.0f), wv = sinf(u * 0.09f - mt * 0.15f);
                cuePt(u, 2.6f * wv, &ax, &ay);
                disc(ax, ay, 1.4f, CA(170, 240, 255, (al * 0x70) / 128));
            }
            cuePt(234.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 3.6f, CA(255, 214, 90, (al * 0x70) / 128), 10);
        } else if (cd->fx == 13) {                                /* ovni: ponta verde brilhando e luzes de disco voador */
            float p = 0.5f + 0.5f * sinf(mt * 0.11f);
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 4.0f + 3.0f * p, CA(120, 255, 150, (al * (0x18 + (int)(0x20 * p))) / 128), 10);
            for (k = 0; k < 4; k++) {
                float u = fmodf(mt * 1.4f + k * 72.0f, 290.0f), tw2 = 0.5f + 0.5f * sinf(mt * 0.2f + k * 1.9f);
                cuePt(u, (k & 1) ? 1.6f : -1.6f, &ax, &ay);
                disc(ax, ay, 1.0f + tw2, (k & 1) ? CA(200, 130, 255, (al * (0x30 + (int)(0x40 * tw2))) / 128) : CA(120, 255, 150, (al * (0x30 + (int)(0x40 * tw2))) / 128));
            }
            cuePt(234.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 3.4f, CA(170, 255, 120, (al * 0x70) / 128), 10);
        } else if (cd->fx == 14) {                                /* robo: luzinhas de LED piscando e ponta amarela */
            float p = 0.5f + 0.5f * sinf(mt * 0.14f);
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 4.0f + 2.0f * p, CA(255, 200, 60, (al * (0x18 + (int)(0x20 * p))) / 128), 10);
            for (k = 0; k < 4; k++) {
                int on = (((mt / 18) + k) & 3) == 0;
                cuePt(40.0f + k * 52.0f, 0.0f, &ax, &ay);
                discN(ax, ay, 2.0f, on ? CA(255, 70, 60, (al * 0x78) / 128) : CA(70, 20, 20, (al * 0x60) / 128), 8);
            }
            cuePt(234.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 3.0f, ((mt / 24) & 1) ? CA(110, 255, 140, (al * 0x78) / 128) : CA(255, 200, 60, (al * 0x78) / 128), 8);
        } else if (cd->fx == 15) {                                /* varinha: estrelinhas roxas e douradas e ponta brilhante */
            float p = 0.5f + 0.5f * sinf(mt * 0.13f);
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 4.0f + 3.0f * p, CA(255, 240, 150, (al * (0x18 + (int)(0x28 * p))) / 128), 10);
            for (k = 0; k < 4; k++) {
                float u = fmodf(mt * 1.0f + k * 72.0f, 290.0f), tw2 = 0.5f + 0.5f * sinf(mt * 0.22f + k * 1.7f), sz = 1.5f + 2.0f * tw2;
                u64 sc = (k & 1) ? CA(255, 214, 90, (al * (0x38 + (int)(0x48 * tw2))) / 128) : CA(200, 140, 255, (al * (0x38 + (int)(0x48 * tw2))) / 128);
                cuePt(u, (k & 1) ? 3.0f : -3.0f, &ax, &ay);
                line(ax - sz, ay, ax + sz, ay, sc); line(ax, ay - sz, ax, ay + sz, sc);
            }
        } else if (cd->fx == 16) {                                /* gato: ponta rosa pulsando e bolinhas pulando */
            float p = 0.5f + 0.5f * sinf(mt * 0.16f);
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 4.0f + 2.5f * p, CA(255, 150, 190, (al * (0x18 + (int)(0x20 * p))) / 128), 10);
            for (k = 0; k < 3; k++) {
                float u = fmodf(mt * 0.9f + k * 100.0f, 290.0f), bn = fabsf(sinf(mt * 0.12f + k * 1.7f));
                cuePt(u, 2.0f + 3.0f * bn, &ax, &ay);
                disc(ax, ay, 1.6f, CA(255, 190, 215, (al * 0x70) / 128));
            }
        } else if (cd->fx == 18) {                                /* policial: sirene vermelha e azul piscando na ponta e luzes correndo */
            int rb2 = (mt / 8) & 1;
            float p = 0.5f + 0.5f * sinf(mt * 0.3f);
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 4.0f + 3.0f * p, rb2 ? CA(255, 50, 60, (al * 0x50) / 128) : CA(70, 130, 255, (al * 0x50) / 128), 10);
            for (k = 0; k < 3; k++) {
                float u = fmodf(mt * 0.9f + k * 97.0f, 290.0f);
                cuePt(u, (k & 1) ? 2.0f : -2.0f, &ax, &ay);
                disc(ax, ay, 1.4f, ((k + mt / 8) & 1) ? CA(255, 80, 90, (al * 0x70) / 128) : CA(110, 160, 255, (al * 0x70) / 128));
            }
        } else if (cd->fx == 17) {                                /* coveiro: ectoplasma verde, ponta de osso e cruzinha */
            float p = 0.5f + 0.5f * sinf(mt * 0.12f);
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 4.0f + 3.0f * p, CA(150, 255, 170, (al * (0x18 + (int)(0x20 * p))) / 128), 10);
            for (k = 0; k < 3; k++) {
                float u = fmodf(mt * 0.8f + k * 97.0f, 290.0f), tw2 = 0.5f + 0.5f * sinf(mt * 0.15f + k * 2.0f);
                cuePt(u, -2.0f - 2.0f * tw2, &ax, &ay);
                disc(ax, ay, 1.2f + tw2, CA(170, 255, 190, (al * (0x28 + (int)(0x40 * tw2))) / 128));
            }
        } else if (cd->fx == 10) {                                /* chipote: bola amarela pulsando e coracoes */
            float p = 0.5f + 0.5f * sinf(mt * 0.16f);
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 5.0f + 2.0f * p, CA(255, 214, 40, (al * 0x60) / 128), 12);
            discN(ax - 1.5f, ay - 1.7f, 1.6f, CA(255, 255, 255, (al * 0x70) / 128), 6);
            for (k = 0; k < 3; k++) {
                float u = fmodf(mt * 0.9f + k * 100.0f, 290.0f), bn = fabsf(sinf(mt * 0.12f + k * 1.7f));
                cuePt(u, 2.0f + 3.0f * bn, &ax, &ay);
                disc(ax, ay, 1.6f, CA(255, 214, 40, (al * 0x70) / 128));
            }
        } else if (cd->fx == 6) {                                 /* vampiro: ponta pulsando e gotas de sangue */
            float p = 0.5f + 0.5f * sinf(mt * 0.1f);
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 4.0f + 3.0f * p, CA(255, 30, 60, (al * (0x18 + (int)(0x20 * p))) / 128), 10);
            for (k = 0; k < 3; k++) {
                float dd = fmodf(mt * 0.5f + k * 9.0f, 14.0f);
                cuePt(24.0f + k * 62.0f, 2.8f + dd * 0.5f, &ax, &ay);
                disc(ax, ay, 1.7f - dd * 0.06f, CA(200, 10, 30, (al * 0x70) / 128));
            }
        } else if (cd->fx == 7) {                                 /* noel: listras de bengala e brilhos de neve */
            static const u8 WHT[3] = { 245, 245, 250 };
            for (k = 0; k < 7; k++) cueSeg(24.0f + k * 20.0f, 34.0f + k * 20.0f, WHT);
            for (k = 0; k < 4; k++) {
                float u = fmodf(mt * 1.2f + k * 75.0f, 290.0f), tw2 = 0.5f + 0.5f * sinf(mt * 0.2f + k * 1.9f);
                cuePt(u, (k & 1) ? 1.4f : -1.4f, &ax, &ay);
                disc(ax, ay, 1.0f + tw2, (k & 1) ? CA(255, 214, 80, (al * (0x30 + (int)(0x40 * tw2))) / 128) : CA(255, 255, 255, (al * (0x30 + (int)(0x40 * tw2))) / 128));
            }
        } else if (cd->fx == 8) {                                 /* hack: pacotes de dados ciano/magenta e falha */
            static const u8 HCY[3] = { 0, 255, 210 }, HMG[3] = { 255, 40, 190 };
            cuePt(0.0f, 0.0f, &ax, &ay);
            discN(ax, ay, 5.0f, CA(0, 255, 210, (al * 0x24) / 128), 10);
            for (k = 0; k < 5; k++) {
                float u = fmodf(mt * (2.0f + k * 0.5f) + k * 61.0f, 280.0f);
                cueSeg(u, u + 7.0f, (k & 1) ? HMG : HCY);
            }
            if ((mt / 40) % 5 == 0) cueSeg(60.0f, 72.0f, HMG);
        } else if (cd->fx == 5) {                                 /* brasileiro: faixas verde/amarelo/azul e estrelinhas */
            static const u8 BAZ[3] = {  0,  60, 170 }, BAM[3] = { 255, 223, 0 }, BVE[3] = { 0, 156, 59 }, BBR[3] = { 255, 255, 255 };
            int ph = (mt / 18) % 3;
            cueSeg(200.0f, 214.0f, ph == 0 ? BAM : (ph == 1 ? BVE : BAZ));
            cueSeg(232.0f, 237.0f, ph == 0 ? BBR : (ph == 1 ? BAM : BBR));
            for (k = 0; k < 4; k++) {
                float u = fmodf(mt * 1.5f + k * 72.0f, 290.0f), tw2 = 0.5f + 0.5f * sinf(mt * 0.2f + k * 1.7f);
                cuePt(u, 0.0f, &ax, &ay);
                if (k & 1) disc(ax, ay, 1.2f + tw2, CA(255, 223, 0, (al * (0x30 + (int)(0x40 * tw2))) / 128));
                else       disc(ax, ay, 1.2f + tw2, CA(60, 230, 110, (al * (0x30 + (int)(0x40 * tw2))) / 128));
            }
        } else if (cd->fx == 4) {                                 /* pistoleiro: brilho de bala + estrela de xerife */
            float u = fmodf(mt * 2.2f, 300.0f), p = 0.5f + 0.5f * sinf(mt * 0.15f);
            cuePt(u, 0.0f, &ax, &ay);
            discN(ax, ay, 2.4f, CA(255, 240, 180, (al * 0x60) / 128), 8);
            cuePt(u - 12.0f, 0.0f, &bx, &by);
            line(ax, ay, bx, by, CA(255, 225, 140, (al * 0x40) / 128));
            cuePt(234.0f, 0.0f, &ax, &ay);
            line(ax - 3.0f - 2.0f * p, ay, ax + 3.0f + 2.0f * p, ay, CA(255, 235, 150, (al * 0x70) / 128));
            line(ax, ay - 3.0f - 2.0f * p, ax, ay + 3.0f + 2.0f * p, CA(255, 235, 150, (al * 0x70) / 128));
        } else {                                                  /* faiscas correndo pelo taco */
            for (k = 0; k < 5; k++) {
                float u = fmodf(mt * 1.6f + k * 58.0f, 290.0f), tw2 = sinf(mt * 0.2f + k * 2.1f);
                float side = (k & 1) ? 1.2f : -1.2f;
                if (tw2 < 0) tw2 = -tw2;
                cuePt(u, side, &ax, &ay);
                disc(ax, ay, 1.0f + tw2, CA(255, 255, 255, (al * (0x30 + (int)(0x40 * tw2))) / 128));
            }
        }
    }
}

/* largura aproximada do texto da fonte (medida no emulador): ~24.5 px por letra em escala 1 */
#define FADV 24.5f
static float tw(float s, const char *t) { return strlen(t) * FADV * s; }

/* moldura fina (4 retangulos): nao tinge o interior da caixa */
static void frameR(float x1, float y1, float x2, float y2, float t, u64 c)
{
    rect(x1, y1, x2, y1 + t, c); rect(x1, y2 - t, x2, y2, c);
    rect(x1, y1 + t, x1 + t, y2 - t, c); rect(x2 - t, y1 + t, x2, y2 - t, c);
}

/* retangulo com os cantos cortados (3 faixas sem sobreposicao, para o alfa nao somar) */
static void cham(float x1, float y1, float x2, float y2, float c, u64 col)
{
    rect(x1 + c, y1, x2 - c, y1 + c, col);
    rect(x1, y1 + c, x2, y2 - c, col);
    rect(x1 + c, y2 - c, x2 - c, y2, col);
}

/* caixa do HUD: sombra, borda, vidro escuro em degrade, brilho no topo e cantos chanfrados */
static void hbox(float x1, float y1, float x2, float y2, u64 border, u64 fill)
{
    float c = 4.0f, h = y2 - y1;
    int k;
    cham(x1 - 1.0f, y1 - 1.0f, x2 + 3.0f, y2 + 3.0f, c + 1.0f, GS_SETREG_RGBAQ(0, 0, 0, 0x14, 0x00));
    cham(x1 - 1.5f, y1 - 1.5f, x2 + 1.5f, y2 + 1.5f, c + 1.5f, border);
    cham(x1, y1, x2, y2, c, fill);
    for (k = 0; k < 4; k++) {                            /* degrade: mais claro em cima, escuro embaixo */
        float ya = y1 + c + (h - 2 * c) * k / 4.0f, yb = y1 + c + (h - 2 * c) * (k + 1) / 4.0f;
        rect(x1, ya, x2, yb, k < 2 ? CA(255, 255, 255, 0x08 - k * 3) : CA(0, 0, 0, 0x06 + (k - 2) * 8));
    }
    cham(x1, y1, x2, y2, c, CA(84, 40, 160, 0x1c));       /* verniz roxo (combina com os menus) */
    rect(x1 + c, y1, x2 - c, y1 + 1.0f, CA(255, 255, 255, 0x2c));
    rect(x1 + c, y2 - 1.0f, x2 - c, y2, CA(0, 0, 0, 0x30));
}

/* tecla + descricao; devolve a largura usada */
static float keycap(float x, float y, const char *k, const char *d, u64 kc, u64 dc)
{
    const float s = 0.36f;
    float kw = tw(s, k) + 12.0f;
    hbox(x, y, x + kw, y + 17, CA(120, 128, 146, 0x70), CA(38, 42, 56, 0x7c));
    text(x + 6, y + 4, s, kc, k);
    text(x + kw + 6, y + 4, s, dc, d);
    return kw + 6 + tw(s, d);
}

/* barra de forca vertical no lado esquerdo: ao apertar X ela sobe e desce
   sozinha; X de novo dispara, O cancela. Depois da tacada ela volta a zero. */
static void drawPowerBar(void)
{
    const float bx = 14.0f, by0 = TY0, by1 = TY1, bw = 14.0f;
    const int   nseg = 24;
    float sh = (by1 - by0) / nseg, fillTop = by1 - barLevel * (by1 - by0);
    char buf[16];
    int k;

    cham(bx - 5.5f, by0 - 5.5f, bx + bw + 5.5f, by1 + 5.5f, 5.0f, COL(226,172,44));      /* moldura dourada */
    cham(bx - 4.0f, by0 - 4.0f, bx + bw + 4.0f, by1 + 4.0f, 4.0f, COL(30,16,56));
    rect(bx - 2.0f, by0 - 2.0f, bx - 1.0f, by1 + 2.0f, CA(255, 255, 255, 0x1c));
    for (k = 0; k < nseg; k++) {
        float t = (k + 0.5f) / nseg;
        int r = (int)(255 * (t * 2 > 1 ? 1 : t * 2));
        int g = (int)(255 * ((1 - t) * 2 > 1 ? 1 : (1 - t) * 2));
        float sb = by1 - k * sh, st = sb - sh;
        rect(bx, st + 1, bx + bw, sb - 0.5f, COL(r / 6, g / 6, 8));            /* apagado */
        if (fillTop < sb) {
            if (st < fillTop) st = fillTop;
            rect(bx, st + 1 > sb - 0.5f ? st : st + 1, bx + bw, sb - 0.5f, COL(r, g, 40));  /* aceso */
        }
    }
    for (k = 1; k < 4; k++)
        line(bx + bw + 4, by1 - k * (by1 - by0) / 4, bx + bw + 8, by1 - k * (by1 - by0) / 4,
             COL(190,190,190));
    if (hackOn && state == S_AIM) {                                   /* HACK: marca a forca ideal */
        float hy = by1 - hackPow * (by1 - by0);
        rect(bx - 8, hy - 1.5f, bx + bw + 8, hy + 1.5f, COL(0, 255, 220));
        text(bx + bw + 10, hy - 6, 0.26f, COL(0, 255, 220), "IDEAL");
    }
    hbox(2, 44, 40, 62, CA(226, 172, 44, 0x90), CA(8, 12, 22, 0x78));
    text(21.0f - tw(0.3f, "FORCA") * 0.5f, 49, 0.3f, COL(255,235,150), "FORCA");
    sprintf(buf, "%d%%", (int)(barLevel * 100 + 0.5f));
    hbox(4, by1 + 8, 38, by1 + 24, CA(226, 172, 44, 0x70), CA(8, 12, 22, 0x78));
    text(21.0f - tw(0.34f, buf) * 0.5f, by1 + 12, 0.34f, COL(255,230,60), buf);
}

/* indicador de efeito (estilo 8 Ball Pool): bola branca com aro metalico; o ponto vermelho mostra o lado (L2/R2)
   e o rola/puxa (CIMA/BAIXO); PULO acende com TRIANGULO */
static void drawSpinIndicator(void)
{
    const float cx = 36.0f, cy = 406.0f, r = 13.0f;
    float px = cx + spinSel * (r - 5.0f), py = cy - spinV * (r - 5.0f);
    hbox(6, 387, 64, 445, CA(70, 78, 96, 0x60), CA(8, 12, 22, 0x70));          /* painel do efeito / pulo */
    discN(cx, cy, r + 2.8f, COL(22,26,38), 20);                       /* aro: escuro, filete claro, escuro */
    discN(cx, cy, r + 1.8f, COL(150,158,174), 20);
    discN(cx, cy, r + 0.9f, COL(30,34,46), 20);
    discN(cx, cy, r, COL(176,178,184), 20);                           /* bola: base, corpo e brilho */
    discN(cx - r * 0.10f, cy - r * 0.10f, r * 0.90f, COL(228,228,226), 20);
    discN(cx - r * 0.18f, cy - r * 0.20f, r * 0.62f, COL(248,248,246), 16);
    discN(cx - r * 0.38f, cy - r * 0.42f, r * 0.20f, COL(255,255,255), 8);
    line(cx, cy - r + 1.5f, cx, cy + r - 1.5f, CA(120,124,134,0x40));  /* mira fina */
    line(cx - r + 1.5f, cy, cx + r - 1.5f, cy, CA(120,124,134,0x40));
    discN(px, py, 4.0f, COL(255,255,255), 10);                        /* ponto: contorno branco + vermelho */
    discN(px, py, 3.0f, COL(214,34,40), 10);
    discN(px - 0.8f, py - 0.9f, 1.0f, COL(255,150,150), 6);
    text(8, 424, 0.3f, COL(200,200,205), "EFEITO");
    text(8, 435, 0.3f, jumpArm ? (((menuT / 10) & 1) ? COL(120,255,150) : COL(60,200,100)) : COL(95,100,112), "PULO");
}

/* giz do jogador da vez (canto inferior direito) */
static void drawChalkIndicator(void)
{
    const float x = 604.0f, y = 399.0f;
    float lv = chalk[cur];
    u64 fc;
    if (isBot()) return;
    hbox(601, 391, 636, 445, CA(70, 78, 96, 0x60), CA(8, 12, 22, 0x70));       /* painel do giz */
    fc = lv > 0.5f ? COL(110,230,130) : (lv > 0.25f ? COL(245,215,70) : COL(240,80,70));
    rect(x + 2, y + 2, x + 26, y + 18, COL(20,40,90));                /* cubo de giz */
    rect(x, y, x + 24, y + 16, COL(60,120,225));
    rect(x + 3, y + 3, x + 21, y + 8, COL(120,175,250));
    rect(x + 8, y + 10, x + 16, y + 14, COL(30,70,160));
    rect(x, y + 22, x + 24, y + 27, COL(40,42,52));
    rect(x, y + 22, x + 24 * lv, y + 27, fc);
    if (lv >= 0.25f || ((menuT / 15) & 1)) text(x + 1, y + 29, 0.3f, COL(200,200,205), "GIZ");
}

static float tipKey(float x, float y, const char *k, const char *l, u64 c1, u64 c2)
{ return tipsOn ? keycap(x, y, k, l, c1, c2) : 0.0f; }

static void drawChar(int id, float cx, float cy, float s, int mood, int t, int full);

/* HUD do topo (layout novo): faixa baixa de vidro com 3 blocos: [retrato | nome, grupo e bolas] [placar] [nome, grupo e bolas | retrato]
   quem joga tem borda dourada, brilho e filete na cor do jogador; o outro fica apagado */
static void hudText(float x, float y, float sc, u64 c, const char *t, float maxw);
static void drawPlayerPanel(int pl)
{
    float x1 = pl == 0 ? 70.0f : 354.0f, x2 = x1 + 216.0f, y1 = 4.0f, y2 = 48.0f;
    float ax1 = pl == 0 ? x1 + 4.0f : x2 - 44.0f, ax2 = ax1 + 40.0f, ay1 = y1 + 2.0f, ay2 = y2 - 2.0f, acx = (ax1 + ax2) * 0.5f;
    float c0 = pl == 0 ? ax2 + 8.0f : x1 + 8.0f, c1 = pl == 0 ? x2 - 8.0f : ax1 - 8.0f;
    int act = (pl == cur), k, slots = gmode == GM_BR ? 4 : 8;
    const char *nm = pname(pl);
    const char *gn = gmode == GM_BR ? BRC_NAME[colSel[pl]] :
                     (grp[pl] < 0 ? (gmode == GM_RACE ? raceTxt(pl) : "MESA ABERTA") : (grp[pl] == 0 ? "LISAS" : "LISTRADAS"));
    u64 side = pl == 0 ? COL(80,140,225) : COL(225,90,92);
    float sp = (c1 - c0 - 14.0f) / (float)(slots - 1), bx0 = c0 + 7.0f, tx = c0;
    if (sp > 20.0f) sp = 20.0f;

    hbox(x1, y1, x2, y2, act ? CA(255, 205, 70, 0x78) : CA(70, 78, 96, 0x38), act ? CA(38, 32, 12, 0x74) : CA(8, 12, 22, 0x4c));
    if (pl == 0) shopFrameDraw(shEq[SC_MOLDURA], x1, y1, x2, y2);          /* LOJA: moldura */
    if (act) rect(x1 + 4, y1 + 1, x2 - 4, y1 + 3, CA(255, 225, 120, 0x50));        /* brilho de quem joga */

    /* retrato quadrado, chanfrado, com luz e filete embaixo */
    {   /* moldura dourada (quem joga brilha mais; o outro fica com o ouro mais fosco) */
        u64 g0 = act ? COL(112,72,8) : COL(58,44,18), g1 = act ? COL(228,174,46) : COL(148,116,44), g2 = act ? COL(255,234,150) : COL(190,164,100);
        cham(ax1 - 2.5f, ay1 - 2.5f, ax2 + 2.5f, ay2 + 2.5f, 4.5f, g0);
        cham(ax1 - 2.0f, ay1 - 2.0f, ax2 + 2.0f, ay2 + 2.0f, 4.0f, g1);
        rect(ax1 + 1.0f, ay1 - 2.0f, ax2 - 1.0f, ay1 - 1.0f, g2);                 /* brilho no topo */
        rect(ax1 - 2.0f, ay1 + 3.0f, ax1 - 1.0f, ay2 - 3.0f, g2);                 /* e no lado esquerdo */
        cham(ax1 - 0.8f, ay1 - 0.8f, ax2 + 0.8f, ay2 + 0.8f, 3.0f, g0);          /* filete escuro por dentro */
        cham(ax1, ay1, ax2, ay2, 3.0f, pl == 0 ? COL(40,78,128) : COL(128,52,54));
        discN(ax1 - 1.6f, ay1 - 1.6f, 1.5f, g2, 6); discN(ax2 + 1.6f, ay1 - 1.6f, 1.5f, g2, 6);   /* pinos nos cantos */
        discN(ax1 - 1.6f, ay2 + 1.6f, 1.5f, g2, 6); discN(ax2 + 1.6f, ay2 + 1.6f, 1.5f, g2, 6);
    }
    discN(acx, ay1 + 16.0f, 16.0f, CA(255, 255, 255, 0x0c), 14);
    drawChar(charSel[pl], acx, ay1 + 26.0f, 0.36f, 0, menuT, 0);
    rect(ax1 + 3.0f, ay2 - 2.0f, ax2 - 3.0f, ay2, side);
    if (curse[pl]) frameR(ax1 - 3.0f, ay1 - 3.0f, ax2 + 3.0f, ay2 + 3.0f, 2.0f, ((menuT / 10) & 1) ? CA(215, 150, 255, 0x90) : CA(150, 70, 230, 0x90));   /* MALDICAO */

    /* nome e grupo */
    hudText(c0, y1 + 5, 0.4f, act ? COL(255,235,110) : COL(170,178,194), nm, c1 - c0 - 40.0f);
    if (gmode == GM_BR) {
        const u8 *bc = BC0[BRC[colSel[pl]]];
        discN(c1 - 4.0f, y1 + 10.0f, 4.0f, shade(bc, 0.55f, 0), 10);
        discN(c1 - 4.0f, y1 + 10.0f, 3.2f, shade(bc, 1.0f, 0), 10);
    }
    text(tx, y1 + 18, 0.28f, act ? COL(235,238,245) : COL(120,128,144), gn);
    if (pl == 0 && shEq[SC_TITULO] > 0) {                /* LOJA: titulo do jogador */
        const char *tt = SH_IT[SC_TITULO][shEq[SC_TITULO]].name;
        float room = c1 - c0 - tw(0.28f, gn) - 8.0f, w = tw(0.24f, tt);
        if (room > 30.0f) { if (w > room) w = room; hudText(c1 - w, y1 + 19, 0.24f, COL(255,215,70), tt, room); }
    }

    if (act && timeSel && (mode == 1 || tourney) && !isBot() && turnT >= 0 && state != S_ROLL && state != S_STRIKE) {
        int sec = (turnT + 59) / 60;
        float fr = (float)turnT / (float)(TIME_SEC[timeSel] * 60);
        char tb[8];
        u64 tcol = sec <= 5 ? ((menuT / 8) & 1 ? COL(255,90,80) : COL(255,160,140)) : COL(255,255,255);
        if (fr < 0) fr = 0; if (fr > 1) fr = 1;
        sprintf(tb, "%dS", sec);
        text(c1 - tw(0.4f, tb), y1 + 5, 0.4f, tcol, tb);
        rect(x1 + 4, y2 - 4, x2 - 4, y2 - 1, CA(40, 44, 56, 0x90));
        rect(x1 + 4, y2 - 4, x1 + 4 + (x2 - x1 - 8) * fr, y2 - 1, sec <= 5 ? COL(255,90,80) : (fr < 0.5f ? COL(255,200,60) : COL(110,230,130)));
    } else if (act) rect(x1 + 4, y2 - 3, x2 - 4, y2 - 1, side);                 /* filete na cor do jogador */

    if (vampOn(pl)) {                                    /* VAMPIRO: barra de sangue (3 gomos) logo abaixo do bloco */
        int full = blood[pl] >= BLOOD_MAX, q;
        float gw = (x2 - x1 - 8.0f) / 3.0f, pul = full ? (0.5f + 0.5f * sinf(menuT * 0.25f)) : 0.0f;
        for (q = 0; q < 3; q++) {
            float gx = x1 + 4.0f + q * gw;
            rect(gx, y2 + 4.0f, gx + gw - 2.0f, y2 + 9.0f, CA(30, 8, 14, 0xa0));
            if (q < blood[pl]) rect(gx, y2 + 4.0f, gx + gw - 2.0f, y2 + 9.0f, full ? COL(255, (int)(40 + 90 * pul), (int)(50 + 60 * pul)) : COL(200, 20, 44));
            if (q < blood[pl]) rect(gx, y2 + 4.0f, gx + gw - 2.0f, y2 + 5.5f, CA(255, 160, 170, 0x50));
        }
        if (full) text(x1 + 4.0f, y2 + 11.0f, 0.26f, COL(255, 110, 120), "SEDE DE SANGUE!");
    }

    /* bolas: casinhas pequenas numa fileira */
    if (grp[pl] < 0) {                                   /* mesa aberta: casinhas vazias */
        for (k = 0; k < slots - 1; k++) {
            float bx = bx0 + k * sp, by = y1 + 34.0f;
            discN(bx, by, 6.6f, CA(14, 18, 28, 0x60), 12);
            ring(bx, by, 6.6f, CA(58, 66, 86, 0x60));
        }
        return;
    }
    for (k = 0; k < slots - (gmode == GM_BR ? 0 : 1); k++) {
        int id = grp[pl] == 0 ? k + 1 : k + 9;
        float bx = bx0 + k * sp, by = y1 + 34.0f;
        if ((gmode == GM_MINI || gmode == GM_DUEL) && (id < 8 ? id : id - 8) > (gmode == GM_DUEL ? 2 : 3)) continue;
        discN(bx, by, 6.8f, COL(10,13,22), 12);
        ring(bx, by, 6.8f, COL(44,52,70));
        if (b[id].in) drawBallAt(id, bx, by, 5.4f);
        else discN(bx, by, 1.8f, CA(90, 100, 124, 0x50), 8);
    }
    if (gmode != GM_BR && countGroup(grp[pl]) == 0) {   /* so falta a bola 8 */
        float bx = bx0 + 7 * sp, by = y1 + 34.0f;
        discN(bx, by, 6.8f, COL(10,13,22), 12);
        ring(bx, by, 6.8f, act ? COL(255,215,60) : COL(44,52,70));
        drawBallAt(8, bx, by, 5.4f);
    }
}

/* pilha de moedas douradas (como o pote do 8 Ball Pool) */
static void coinStack(float cx, float cy)
{
    static const float ox[3] = { -3.5f, 3.5f, 0.0f }, oy[3] = { 3.0f, 3.0f, -2.0f };
    int k;
    for (k = 0; k < 3; k++) {
        float x = cx + ox[k], y = cy + oy[k];
        cham(x - 4.5f, y - 2.5f, x + 4.5f, y + 3.5f, 1.8f, COL(116,74,8));        /* lateral */
        cham(x - 4.5f, y - 3.5f, x + 4.5f, y + 1.5f, 1.8f, COL(236,184,48));      /* face */
        rect(x - 2.5f, y - 2.5f, x + 2.5f, y - 1.5f, COL(255,238,160));           /* brilho */
    }
}

/* placar de vitorias (centro do topo): caixa dourada, o numero de cada jogador na cor dele e o premio em moedas embaixo */
static void drawScoreBoard(void)
{
    int *w = mode == 0 ? winsBot : winsVs;
    char buf[16];
    float s;
    hbox(290, 4, 350, 48, tourney ? CA(255, 215, 60, 0x90) : CA(226, 172, 44, 0x90), CA(8, 12, 22, 0x78));
    frameR(293, 7, 347, 45, 1.0f, CA(226, 172, 44, 0x38));
    rect(300, 5, 340, 7, tourney ? CA(255, 215, 60, 0x50) : CA(255, 235, 150, 0x30));
    if (tourney) {                                       /* torneio: mostra a fase em vez do placar */
        const char *rn = tRound == 0 ? "QUARTAS" : (tRound == 1 ? "SEMI" : "FINAL");
        text(320 - tw(0.26f, "TORNEIO") * 0.5f, 11, 0.26f, COL(255,215,70), "TORNEIO");
        text(320 - tw(0.34f, rn) * 0.5f, 27, 0.34f, COL(255,235,110), rn);
        return;
    }
    if (training) {                                      /* treino: mostra quantas bolas faltam na mesa */
        int i, n = 0;
        for (i = 1; i < 16; i++) if (b[i].in) n++;
        sprintf(buf, "%d", n);
        text(320 - tw(0.26f, "TREINO") * 0.5f, 11, 0.26f, COL(255,215,70), "TREINO");
        text(320 - tw(0.46f, buf) * 0.5f, 22, 0.46f, COL(255,235,110), buf);
        text(320 - tw(0.22f, "BOLAS") * 0.5f, 38, 0.22f, COL(175,185,195), "BOLAS");
        return;
    }
    s = (w[0] > 9 || w[1] > 9) ? 0.36f : 0.46f;
    sprintf(buf, "%d", w[0] > 99 ? 99 : w[0]);
    text(320 - 8 - tw(s, buf), 10, s, COL(120,175,255), buf);
    text(320 - tw(s, ":") * 0.5f, 9, s, COL(150,158,175), ":");
    sprintf(buf, "%d", w[1] > 99 ? 99 : w[1]);
    text(320 + 8, 10, s, COL(255,125,120), buf);
    rect(298, 28, 342, 29, CA(226, 172, 44, 0x40));                 /* divisoria */
    coinStack(303.0f, 37.0f);
    sprintf(buf, "+%d", prizeWin());
    text(314, 33, 0.28f, COL(255,215,70), buf);                    /* premio de quem vencer */
}

/* texto que encolhe para caber em maxw pixels */
static void hudText(float x, float y, float sc, u64 c, const char *t, float maxw)
{
    float w = tw(sc, t);
    if (w > maxw && w > 0.0f) sc *= maxw / w;
    text(x, y, sc, c, t);
}

static void drawHUD(void)
{
    float x;
    int pl;
    for (pl = 0; pl < (training ? 1 : 2); pl++) drawPlayerPanel(pl);
    drawScoreBoard();

    drawPowerBar();
    drawSpinIndicator();
    drawChalkIndicator();

    /* barra inferior: mensagem + teclas + taco equipado */
    hbox(70, 396, 598, 443, CA(70, 78, 96, 0x60), CA(8, 12, 22, 0x70));
    rect(70, 396, 74, 443, cur == 0 ? COL(80,140,225) : COL(225,90,92));          /* faixa na cor de quem joga */
    rect(74, 396, 598, 398, CA(255, 215, 60, 0x60));
    rect(74, 398, 448, 410, cur == 0 ? CA(80, 140, 225, 0x14) : CA(225, 90, 92, 0x14));
    rect(448, 401, 449, 438, CA(120, 130, 150, 0x40));                           /* divisoria: dicas | taco e poder */
    if (msgTimer > 0) {                                                          /* linha de cima: mensagem */
        float msc = 0.42f, mw = tw(msc, msg);
        msgTimer--;
        if (mw > 360.0f) msc *= 360.0f / mw;
        text(259.0f - tw(msc, msg) * 0.5f, 401, msc, COL(255,235,110), msg);
    } else if (state == S_AIM && curse[cur]) {                                   /* MALDICAO sobre quem esta mirando */
        const char *ct = "AMALDICOADO: MIRA INVERTIDA!";
        text(259.0f - tw(0.34f, ct) * 0.5f, 402, 0.34f, ((menuT / 10) & 1) ? COL(215,150,255) : COL(150,70,230), ct);
    } else {
        char vt[40];
        if (training) strcpy(vt, "TREINO LIVRE"); else sprintf(vt, "VEZ DE %s", pname(cur));
        text(259.0f - tw(0.34f, vt) * 0.5f, 402, 0.34f, COL(150,158,175), vt);
    }

    x = 80.0f;
    if (state == S_AIM && !isBot()) {
        if (charging) {
            x += tipKey(x, 421, "X", "DISPARAR", COL(255,230,60), COL(235,235,240)) + 16;
            tipKey(x, 421, "O", "CANCELAR", COL(255,230,60), COL(235,235,240));
        } else {
            if (tipsOn && ((menuT / 360) & 1)) {                    /* 2a pagina de dicas */
                x += tipKey(x, 421, "CIMA/BAIXO", "ROLA/PUXA", COL(255,230,60), COL(235,235,240)) + 14;
                x += tipKey(x, 421, "TRI", jumpArm ? "PULO LIGADO" : "PULO", COL(255,230,60), COL(235,235,240)) + 14;
                if (curAb() != AB_NONE) tipKey(x, 421, "O", "HABILIDADE", COL(0,255,200), COL(235,235,240));
            } else {
            x += tipKey(x, 421, "X", "FORCA", COL(255,230,60), COL(235,235,240)) + 14;
            if (chalk[cur] < 0.5f) {
                x += tipKey(x, 421, "QUAD", "GIZ", COL(110,200,255), COL(235,235,240)) + 14;
                tipKey(x, 421, "L2/R2", "EFEITO", COL(255,230,60), COL(235,235,240));
            } else {
                x += tipKey(x, 421, "L2/R2", "EFEITO", COL(255,230,60), COL(235,235,240)) + 14;
                tipKey(x, 421, "SELECT", "TACOS", COL(255,230,60), COL(235,235,240));
            }
            }
        }
    } else if (state == S_PLACE && !isBot()) {
        x += tipKey(x, 421, "SETAS", "MOVER", COL(255,230,60), COL(235,235,240)) + 14;
        tipKey(x, 421, "X", "CONFIRMAR", COL(255,230,60), COL(235,235,240));
    } else if (state == S_TAKE && !isBot()) {
        x += tipKey(x, 421, "SETAS", "ESCOLHER BOLA", COL(255,230,60), COL(235,235,240)) + 14;
        tipKey(x, 421, "X", "TIRAR DA MESA", COL(255,230,60), COL(235,235,240));
    } else if (state == S_TAKE) {
        text(x, 425, 0.4f, COL(170,176,190), "BOT ESCOLHENDO UMA BOLA...");
    } else if (state == S_AIM || state == S_PLACE) {
        text(x, 425, 0.4f, COL(170,176,190), "VEZ DO BOT...");
    } else if (state == S_ROLL || state == S_STRIKE) {
        text(x, 425, 0.4f, COL(170,176,190), "AGUARDE AS BOLAS PARAREM...");
    }

    /* coluna da direita: poder (em cima) e taco equipado (embaixo) */
    if (state == S_AIM && !isBot() && curAb() == AB_COVEIRO && abilCD[cur] == 0) {   /* mini seletor do Coveiro */
        static const char *SH[3] = { "ENTERRO", "COVA", "RESSUSC." };
        int k;
        for (k = 0; k < 3; k++) {
            float x1 = 454.0f + k * 47.0f, y1 = 400.0f;
            int ok = graveWhy(k) == NULL, sel = graveOpen && graveSel == k;
            rect(x1, y1, x1 + 45.0f, y1 + 15.0f, sel ? CA(20, 50, 32, 0x80) : CA(14, 20, 34, 0x70));
            outlineRect(x1, y1, x1 + 45.0f, y1 + 15.0f, sel ? (ok ? COL(110,255,140) : COL(255,110,100)) : COL(80,88,100));
            text(x1 + 22.5f - tw(0.2f, SH[k]) * 0.5f, y1 + 4.0f, 0.2f, ok ? (sel ? COL(255,255,255) : COL(200,210,220)) : COL(100,106,118), SH[k]);
        }
    } else if (state == S_AIM && !isBot() && curAb() == AB_MARRETA && abilCD[cur] == 0) {   /* mini seletor do Chapolin */
        static const char *SH3[3] = { "CORNETA", "CHIQUI.", "BIONICA" };
        int k;
        for (k = 0; k < 3; k++) {
            float x1 = 454.0f + k * 47.0f, y1 = 400.0f;
            int ok = chWhy(k) == NULL, sel = chOpen && chSel == k;
            rect(x1, y1, x1 + 45.0f, y1 + 15.0f, sel ? CA(60, 44, 10, 0x80) : CA(14, 20, 34, 0x70));
            outlineRect(x1, y1, x1 + 45.0f, y1 + 15.0f, sel ? (ok ? COL(255,214,70) : COL(255,110,100)) : COL(80,88,100));
            text(x1 + 22.5f - tw(0.2f, SH3[k]) * 0.5f, y1 + 4.0f, 0.2f, ok ? (sel ? COL(255,255,255) : COL(200,210,220)) : COL(100,106,118), SH3[k]);
        }
        if (chOpen && chSel == 0 && abilArm) {
            char tb3[48];
            sprintf(tb3, "BOLA %d - QUADRADO TROCA O ALVO", copTgt);
            hudText(454.0f, 388.0f, 0.24f, COL(255,230,60), tb3, 150.0f);
        }
    } else if (state == S_AIM && !isBot() && curAb() == AB_POLICIAL && abilCD[cur] == 0) {   /* mini seletor do Policial */
        static const char *SH2[2] = { "ALGEMAS", "SIRENE" };
        int k;
        for (k = 0; k < 2; k++) {
            float x1 = 454.0f + k * 71.0f, y1 = 400.0f;
            int ok = copWhy(k) == NULL, sel = copOpen && copSel == k;
            rect(x1, y1, x1 + 69.0f, y1 + 15.0f, sel ? CA(20, 34, 70, 0x80) : CA(14, 20, 34, 0x70));
            outlineRect(x1, y1, x1 + 69.0f, y1 + 15.0f, sel ? (ok ? COL(110,160,255) : COL(255,110,100)) : COL(80,88,100));
            text(x1 + 34.5f - tw(0.2f, SH2[k]) * 0.5f, y1 + 4.0f, 0.2f, ok ? (sel ? COL(255,255,255) : COL(200,210,220)) : COL(100,106,118), SH2[k]);
        }
        if (copOpen && copSel == 0 && abilArm) {
            char tb3[48];
            sprintf(tb3, "BOLA %d - QUADRADO TROCA O ALVO", copTgt);
            hudText(454.0f, 388.0f, 0.24f, COL(255,230,60), tb3, 150.0f);
        }
    } else if (state == S_AIM && !isBot() && curAb() != AB_NONE) {                  /* indicador do poder */
        char ab[40];
        u64 ac;
        if (hackOn)                 { sprintf(ab, "HACK %d/%d  O: OUTRA", hCandI + 1, hCandN); ac = COL(0,255,220); }
        else if (curAb() == AB_COVEIRO && graveOpen && !abilArm) { sprintf(ab, "%s INDISPONIVEL", GV_NAME[graveSel]); ac = COL(255,120,110); }
        else if (abilArm)           { sprintf(ab, "%s ARMADO!", abLabel(curAb())); ac = COL(110,255,140); }
        else if (abilCD[cur] > 0)   { sprintf(ab, "%s: %d", AB_NAME[curAb()], abilCD[cur]); ac = COL(150,155,170); }
        else                        { sprintf(ab, "O: %s", abLabel(curAb())); ac = COL(255,230,60); }
        rect(454, 400, 594, 415, CA(14, 20, 34, 0x70));
        outlineRect(454, 400, 594, 415, ac);
        hudText(458.0f, 403.0f, 0.28f, ac, ab, 132.0f);
    }
    if (state == S_AIM || state == S_CUES) {                                      /* taco equipado */
        const CueDef *cd = &CUES[curCue()];
        rect(454, 420, 594, 438, CA(14, 20, 34, 0x70));
        outlineRect(454, 420, 594, 438, COL(80,88,100));
        rect(458, 423, 464, 435, COL(cd->tip[0], cd->tip[1], cd->tip[2]));
        hudText(470.0f, 425.0f, 0.34f, COL(215,220,230), cd->name, 120.0f);
    }
}

static void tri(float x1, float y1, float x2, float y2, float x3, float y3, u64 c)
{ gsKit_prim_triangle(g, x1+OX, y1+OY, x2+OX, y2+OY, x3+OX, y3+OY, 1, c); }

/* ---------- caricaturas (10 personagens, desenhadas com formas) ---------- */
#define NCHARS 18
#define CPI 3.14159265f
static const char *CHAR_NAME[NCHARS] = { "PINGUIM", "ROBO", "PIRATA", "NINJA", "ALIEN",
                                         "COWBOY", "VAMPIRO", "MAGO", "VIKING", "GATO", "CHAPOLIN", "HACKER", "PAPAI NOEL", "MESTRE DO TEMPO", "GRINCH", "POSEIDON", "COVEIRO", "POLICIAL" };

/* kit de cada personagem (para o modo historia e para o catalogo): taco e mesa. -1 = nao tem */
static const int CHAR_CUE[NCHARS] = {  7, 27, 11, 10, 26, 12, 14, 28,  6, 29, 17, 16, 15, 18, 24, 25, 30, 31 };
static const int CHAR_TAB[NCHARS] = {  8,  6,  2,  5,  9, 15, 18, 16,  7, 13, 21, 20, 19, 22, 23, 24, 25, 26 };
static int cueChar(int c) { int i; for (i = 0; i < NCHARS; i++) if (CHAR_CUE[i] == c) return i; return -1; }
static int effTable(void) { if (tableAuto && CHAR_TAB[charSel[0]] >= 0) return CHAR_TAB[charSel[0]]; return tableRandom ? (menuT / 150) % NTABLES : tableSel; }

static void ell(float cx, float cy, float rx, float ry, u64 c)
{
    int i, n = 16;
    const float *t = circ(n);
    for (i = 0; i < n; i++)
        tri(cx, cy, cx + t[2*i] * rx, cy + t[2*i+1] * ry, cx + t[2*i+2] * rx, cy + t[2*i+3] * ry, c);
}

/* fatia de elipse (a0..a1 em rad; y cresce para baixo: 0..PI = metade de baixo, PI..2PI = metade de cima) */
static void slice(float cx, float cy, float rx, float ry, float a0, float a1, u64 c)
{
    int i, n = 10;
    for (i = 0; i < n; i++) {
        float p0 = a0 + (a1 - a0) * i / n, p1 = a0 + (a1 - a0) * (i + 1) / n;
        tri(cx, cy, cx + cosf(p0) * rx, cy + sinf(p0) * ry, cx + cosf(p1) * rx, cy + sinf(p1) * ry, c);
    }
}

static void arcL(float cx, float cy, float rx, float ry, float a0, float a1, float w, u64 c)
{
    int i, n = 8;
    for (i = 0; i < n; i++) {
        float p0 = a0 + (a1 - a0) * i / n, p1 = a0 + (a1 - a0) * (i + 1) / n;
        beam(cx + cosf(p0) * rx, cy + sinf(p0) * ry, cx + cosf(p1) * rx, cy + sinf(p1) * ry, w, c);
    }
}

static void star4(float x, float y, float r, u64 c)
{
    quad(x, y - r, x + r * 0.32f, y, x, y + r, x - r * 0.32f, y, c);
    quad(x - r, y, x, y - r * 0.32f, x + r, y, x, y + r * 0.32f, c);
}

/* lagrimas escorrendo dos olhos (derrota) */
static void tears(float cx, float cy, float s, int t, float dx, float ey)
{
    int k, q;
    for (k = -1; k <= 1; k += 2) {
        float ex = cx + k * dx * s, y0 = cy + (ey + 4.0f) * s;
        beam(ex + k * 1.5f * s, y0, ex + k * 2.5f * s, y0 + 15.0f * s, 3.0f * s, CA(120, 190, 255, 0x40));
        for (q = 0; q < 2; q++) {
            float age = fmodf(t * 0.9f + q * 8.0f + (k > 0 ? 4.0f : 0.0f), 16.0f);
            discN(ex + k * (2.0f + age * 0.15f) * s, y0 + age * 1.6f * s, (2.4f - age * 0.06f) * s,
                  CA(140, 205, 255, 0x78 - (int)(age * 3.0f)), 8);
        }
    }
}

/* rosto padrao: olhos, sobrancelhas e boca de acordo com o humor (0 normal, 1 vitoria, 2 derrota).
   parts: 1 = olhos/sobrancelhas, 2 = boca. brow = 0 nao desenha sobrancelhas */
static void faceStd(float cx, float cy, float s, int mood, int t, float dx, float ey, float my, u64 brow, int parts)
{
    int k;
    u64 ink = COL(34, 22, 22), wh = COL(255, 255, 255);
    if (parts & 1) {
        for (k = -1; k <= 1; k += 2) {
            float ex = cx + k * dx * s, yy = cy + ey * s;
            if (mood == 1) {
                beam(ex - 4.2f * s, yy + 2.0f * s, ex, yy - 2.2f * s, 2.4f * s, ink);
                beam(ex, yy - 2.2f * s, ex + 4.2f * s, yy + 2.0f * s, 2.4f * s, ink);
            } else {
                float lk = (mood == 2) ? 1.6f : 0.0f;
                discN(ex, yy, 4.6f * s, wh, 10);
                discN(ex, yy + lk * s, 2.5f * s, ink, 8);
                discN(ex - 0.9f * s, yy + (lk - 0.9f) * s, 0.9f * s, wh, 6);
            }
            if (brow) {
                if (mood == 0)      beam(ex - 4.5f * s, yy - 7.5f * s, ex + 4.5f * s, yy - 7.5f * s, 2.0f * s, brow);
                else if (mood == 1) arcL(ex, yy - 4.0f * s, 5.0f * s, 3.2f * s, 1.15f * CPI, 1.85f * CPI, 2.0f * s, brow);
                else                beam(ex - k * 4.5f * s, yy - 10.0f * s, ex + k * 4.5f * s, yy - 6.0f * s, 2.0f * s, brow);
            }
        }
        if (mood == 1) {
            discN(cx - (dx + 3.0f) * s, cy + (ey + 8.0f) * s, 3.6f * s, CA(255, 110, 130, 0x40), 8);
            discN(cx + (dx + 3.0f) * s, cy + (ey + 8.0f) * s, 3.6f * s, CA(255, 110, 130, 0x40), 8);
        }
        if (mood == 2) tears(cx, cy, s, t, dx, ey);
    }
    if (parts & 2) {
        if (mood == 0) arcL(cx, cy + (my - 3.0f) * s, 6.0f * s, 4.0f * s, 0.15f * CPI, 0.85f * CPI, 1.8f * s, ink);
        else if (mood == 1) {
            slice(cx, cy + (my - 4.0f) * s, 9.0f * s, 9.0f * s, 0.0f, CPI, COL(140, 20, 30));
            rect(cx - 8.0f * s, cy + (my - 4.0f) * s, cx + 8.0f * s, cy + (my - 1.2f) * s, wh);
            ell(cx, cy + (my + 3.5f) * s, 4.5f * s, 2.8f * s, COL(235, 100, 120));
        } else arcL(cx, cy + (my + 4.0f) * s, 6.5f * s, 4.5f * s, 1.15f * CPI, 1.85f * CPI, 1.8f * s, ink);
    }
}

/* bracos: baixos (normal), levantados acenando (vitoria) ou caidos (derrota) */
static void armsC(float cx, float cy, float s, int mood, int t, u64 sleeve, u64 hand, float w)
{
    int k;
    for (k = -1; k <= 1; k += 2) {
        float sx = cx + k * 26.0f * s, sy = cy + 44.0f * s, hx, hy;
        if (mood == 1) {
            hx = cx + k * (46.0f + 3.0f * sinf(t * 0.3f + k)) * s;
            hy = cy + (2.0f + 7.0f * sinf(t * 0.35f + (k > 0 ? 3.1f : 0.0f))) * s;
        } else if (mood == 2) { hx = cx + k * 31.0f * s; hy = cy + 86.0f * s; }
        else                  { hx = cx + k * 37.0f * s; hy = cy + 74.0f * s; }
        beam(sx, sy, hx, hy, w * s, sleeve);
        discN(sx, sy, w * 0.5f * s, sleeve, 8);
        discN(hx, hy, w * 0.62f * s, hand, 10);
    }
}

static void legsC(float cx, float cy, float s, u64 pants, u64 shoe)
{
    int k;
    for (k = -1; k <= 1; k += 2) {
        rect(cx + (k > 0 ? 3.0f : -15.0f) * s, cy + 82.0f * s, cx + (k > 0 ? 15.0f : -3.0f) * s, cy + 99.0f * s, pants);
        ell(cx + k * 10.0f * s, cy + 100.0f * s, 10.0f * s, 4.6f * s, shoe);
    }
}

static void torsoC(float cx, float cy, float s, u64 c, u64 dk)
{
    quad(cx - 29.0f * s, cy + 44.0f * s, cx + 29.0f * s, cy + 44.0f * s, cx + 27.0f * s, cy + 86.0f * s, cx - 27.0f * s, cy + 86.0f * s, c);
    ell(cx, cy + 40.0f * s, 29.0f * s, 17.0f * s, c);
    rect(cx - 27.0f * s, cy + 82.0f * s, cx + 27.0f * s, cy + 86.0f * s, dk);
}

/* desenha o personagem id. (cx, cy) = centro da cabeca; s = escala (1 = cabeca com raio 25);
   mood: 0 normal, 1 vitoria, 2 derrota; full: 0 so a cabeca (avatar), 1 corpo inteiro */
static void drawChar(int id, float cx, float cy, float s, int mood, int t, int full)
{
    int k;
    u64 skin = COL(241, 192, 152), ink = COL(34, 22, 22), wh = COL(255, 255, 255), gold = COL(255, 205, 50);
    #define XX(v) (cx + (v) * s)
    #define YY(v) (cy + (v) * s)
    #define ZZ(v) ((v) * s)

    switch (((id % NCHARS) + NCHARS) % NCHARS) {
    case 0: {   /* PINGUIM */
        u64 blk = COL(34, 38, 52), org = COL(255, 150, 30), red = COL(220, 50, 62);
        if (full) {
            ell(XX(-11), YY(101), ZZ(11), ZZ(4.6f), org); ell(XX(11), YY(101), ZZ(11), ZZ(4.6f), org);
            ell(cx, YY(62), ZZ(33), ZZ(42), blk);
            ell(cx, YY(66), ZZ(22), ZZ(34), wh);
            for (k = -1; k <= 1; k += 2) {
                float hx, hy;
                if (mood == 1)      { hx = XX(k * (52 + 3 * sinf(t * 0.3f + k))); hy = YY(12 + 7 * sinf(t * 0.35f + (k > 0 ? 3.1f : 0.0f))); }
                else if (mood == 2) { hx = XX(k * 38); hy = YY(84); }
                else                { hx = XX(k * 42); hy = YY(66); }
                beam(XX(k * 27), YY(42), hx, hy, ZZ(11), blk);
                discN(hx, hy, ZZ(5.8f), blk, 8);
            }
        }
        disc(cx, cy, ZZ(26), blk);
        ell(cx, YY(5), ZZ(19), ZZ(16), wh);
        for (k = -1; k <= 1; k += 2) {
            if (mood == 1) {
                beam(XX(k * 8 - 3.6f), YY(0.8f), XX(k * 8), YY(-2.4f), ZZ(2.2f), ink);
                beam(XX(k * 8), YY(-2.4f), XX(k * 8 + 3.6f), YY(0.8f), ZZ(2.2f), ink);
            } else {
                discN(XX(k * 8), YY(mood == 2 ? 0 : -1), ZZ(mood == 2 ? 4.6f : 3.8f), ink, 8);
                discN(XX(k * 8 - 1.2f), YY(mood == 2 ? -1.2f : -2.2f), ZZ(1.3f), wh, 6);
            }
            if (mood == 2) beam(XX(k * 8 - k * 4.5f), YY(-9), XX(k * 8 + k * 4.5f), YY(-5.5f), ZZ(2.0f), ink);
        }
        if (mood == 1) {
            tri(XX(-6), YY(6.5f), XX(6), YY(6.5f), cx, YY(18), COL(140, 20, 30));
            tri(XX(-7), YY(3), XX(7), YY(3), cx, YY(9), org);
            tri(XX(-4), YY(16), XX(4), YY(16), cx, YY(12.5f), org);
        } else if (mood == 2) {
            tri(XX(-7), YY(5), XX(7), YY(5), cx, YY(12), org);
            tears(cx, cy, s, t, 8, -1);
        } else tri(XX(-7), YY(4), XX(7), YY(4), cx, YY(13), org);
        if (full) {
            beam(XX(-21), YY(27), XX(21), YY(27), ZZ(8), red);
            beam(XX(13), YY(30), XX(18), YY(50), ZZ(6.5f), COL(180, 30, 45));
        }
        break; }

    case 1: {   /* ROBO */
        u64 st = COL(172, 184, 200), sd = COL(104, 114, 132), dk = COL(22, 28, 42), cn = COL(80, 232, 255), gr = COL(100, 255, 150);
        if (full) {
            legsC(cx, cy, s, sd, COL(60, 66, 80));
            rect(XX(-30), YY(34), XX(30), YY(87), sd);
            rect(XX(-28), YY(36), XX(28), YY(85), st);
            rect(XX(-18), YY(46), XX(18), YY(72), dk);
            discN(XX(-9), YY(54), ZZ(3.2f), COL(255, 80, 80), 8);
            discN(cx, YY(54), ZZ(3.2f), ((t / 12) & 1) ? gr : COL(40, 120, 70), 8);
            discN(XX(9), YY(54), ZZ(3.2f), COL(255, 220, 70), 8);
            rect(XX(-14), YY(62), XX(14), YY(65), cn);
            armsC(cx, cy, s, mood, t, sd, COL(210, 218, 230), 8.5f);
        }
        rect(XX(-27), YY(-24), XX(27), YY(25), sd);
        rect(XX(-25), YY(-22), XX(25), YY(23), st);
        rect(XX(-25), YY(-22), XX(25), YY(-17), COL(208, 218, 232));
        rect(XX(-31), YY(-6), XX(-27), YY(8), sd); rect(XX(27), YY(-6), XX(31), YY(8), sd);
        beam(cx, YY(-24), cx, YY(-38), ZZ(3), sd);
        discN(cx, YY(-41), ZZ(4.6f), ((t / 12) & 1) ? COL(255, 70, 70) : COL(150, 30, 30), 8);
        rect(XX(-21), YY(-11), XX(21), YY(5), dk);
        if (mood == 1) {
            for (k = -1; k <= 1; k += 2) arcL(XX(k * 11), YY(3), ZZ(6), ZZ(5.5f), 1.1f * CPI, 1.9f * CPI, ZZ(2.8f), cn);
        } else if (mood == 2) {
            for (k = -1; k <= 1; k += 2) beam(XX(k * 5), YY(-5), XX(k * 17), YY(1), ZZ(4), cn);
            tears(cx, cy, s, t, 11, 1);
        } else {
            rect(XX(-17), YY(-8), XX(-6), YY(2), cn); rect(XX(6), YY(-8), XX(17), YY(2), cn);
        }
        rect(XX(-14), YY(11), XX(14), YY(20), dk);
        if (mood == 1) {
            rect(XX(-12), YY(13), XX(12), YY(18), gr);
            for (k = -2; k <= 2; k++) rect(XX(k * 5 - 0.6f), YY(13), XX(k * 5 + 0.6f), YY(18), dk);
        } else if (mood == 2) arcL(cx, YY(22), ZZ(9), ZZ(5), 1.15f * CPI, 1.85f * CPI, ZZ(1.8f), st);
        else for (k = -2; k <= 2; k++) rect(XX(k * 5.5f - 0.8f), YY(12), XX(k * 5.5f + 0.8f), YY(19), st);
        break; }

    case 2: {   /* PIRATA */
        u64 red = COL(205, 40, 52), blk = COL(26, 24, 30);
        if (full) {
            legsC(cx, cy, s, COL(44, 54, 112), blk);
            torsoC(cx, cy, s, COL(238, 238, 244), COL(200, 200, 210));
            for (k = 0; k < 4; k++) rect(XX(-28), YY(50 + k * 9), XX(28), YY(54 + k * 9), red);
            rect(XX(-28), YY(77), XX(28), YY(83), blk); rect(XX(-4.5f), YY(77), XX(4.5f), YY(83), gold);
            armsC(cx, cy, s, mood, t, COL(238, 238, 244), skin, 8.5f);
        }
        disc(cx, cy, ZZ(25), skin);
        ringN(XX(-25.5f), YY(8), ZZ(3.4f), ZZ(1.6f), 10, gold);
        faceStd(cx, cy, s, mood, t, 9, -3, 15, COL(46, 28, 22), 3);
        ell(XX(-7.5f), YY(8.5f), ZZ(8.5f), ZZ(3.6f), COL(62, 40, 30)); ell(XX(7.5f), YY(8.5f), ZZ(8.5f), ZZ(3.6f), COL(62, 40, 30));
        beam(XX(-22), YY(-9), XX(24), YY(-13), ZZ(2.0f), blk);
        discN(XX(-9), YY(-3), ZZ(6.0f), blk, 10);
        beam(XX(-9), YY(-3), XX(-25), YY(1), ZZ(1.8f), blk);
        ell(cx, YY(-20), ZZ(34), ZZ(7.5f), blk);
        quad(XX(-22), YY(-20), XX(22), YY(-20), XX(16), YY(-42), XX(-16), YY(-42), blk);
        ell(cx, YY(-42), ZZ(16), ZZ(5), blk);
        beam(XX(-24), YY(-20), XX(24), YY(-20), ZZ(2.2f), gold);
        beam(XX(-10), YY(-26), XX(10), YY(-37), ZZ(2.6f), wh); beam(XX(-10), YY(-37), XX(10), YY(-26), ZZ(2.6f), wh);
        discN(cx, YY(-31.5f), ZZ(5.6f), wh, 10); rect(XX(-3), YY(-29), XX(3), YY(-25.5f), wh);
        discN(XX(-2.2f), YY(-32), ZZ(1.4f), blk, 6); discN(XX(2.2f), YY(-32), ZZ(1.4f), blk, 6);
        break; }

    case 3: {   /* NINJA */
        u64 nv = COL(54, 60, 98), nd = COL(34, 38, 64), red = COL(215, 40, 52), mk = COL(122, 128, 172);
        if (full) {
            beam(XX(22), YY(26), XX(36), YY(4), ZZ(4.5f), COL(70, 50, 34));
            beam(XX(28), YY(14), XX(35), YY(18), ZZ(3), gold);
            legsC(cx, cy, s, nd, COL(14, 14, 24));
            torsoC(cx, cy, s, nv, nd);
            rect(XX(-28), YY(70), XX(28), YY(77), red);
            armsC(cx, cy, s, mood, t, nv, COL(70, 76, 112), 8.5f);
        }
        disc(cx, cy, ZZ(25), nv);
        ell(cx, YY(-2), ZZ(20), ZZ(8.5f), skin);
        faceStd(cx, cy, s, mood, t, 8.5f, -2, 15, 0, 1);
        if (mood == 1)      arcL(cx, YY(9), ZZ(10), ZZ(6), 0.15f * CPI, 0.85f * CPI, ZZ(2.0f), mk);
        else if (mood == 2) arcL(cx, YY(17), ZZ(8), ZZ(4.5f), 1.15f * CPI, 1.85f * CPI, ZZ(2.0f), mk);
        else                beam(XX(-6), YY(13), XX(6), YY(13), ZZ(1.6f), mk);
        beam(XX(-26), YY(-12), XX(26), YY(-12), ZZ(7), red);
        rect(XX(-9), YY(-15.5f), XX(9), YY(-8.5f), COL(190, 196, 208)); rect(XX(-2), YY(-14), XX(2), YY(-10), nd);
        beam(XX(25), YY(-12), XX(43), YY(-6 + 3 * sinf(t * 0.2f)), ZZ(5), red);
        beam(XX(25), YY(-12), XX(41), YY(-19 + 3 * sinf(t * 0.2f + 1.5f)), ZZ(5), COL(165, 30, 42));
        break; }

    case 4: {   /* ALIEN */
        u64 gr = COL(122, 216, 96), gd = COL(70, 150, 62), su = COL(112, 82, 202), sd = COL(70, 50, 150);
        if (full) {
            legsC(cx, cy, s, su, sd);
            torsoC(cx, cy, s, su, sd);
            beam(XX(-14), YY(32), XX(14), YY(32), ZZ(6), COL(205, 210, 225));
            discN(cx, YY(60), ZZ(5), COL(255, 230, 80), 8);
            armsC(cx, cy, s, mood, t, su, gr, 8.5f);
        }
        for (k = -1; k <= 1; k += 2) {
            beam(XX(k * 10), YY(-26), XX(k * 18), YY(-45 + 2 * sinf(t * 0.2f + k)), ZZ(3), gd);
            discN(XX(k * 18), YY(-47 + 2 * sinf(t * 0.2f + k)), ZZ(5), (k < 0) ? COL(255, 230, 80) : COL(255, 120, 190), 10);
        }
        ell(cx, YY(0), ZZ(28), ZZ(31), gr);
        for (k = -1; k <= 1; k += 2) {
            float ry = mood == 1 ? 6.5f : (mood == 2 ? 13.0f : 12.0f);
            ell(XX(k * 11), YY(mood == 1 ? 3 : 2), ZZ(9), ZZ(ry), COL(14, 18, 26));
            discN(XX(k * 13.5f), YY(mood == 1 ? 1 : -3), ZZ(mood == 2 ? 3.4f : 2.6f), wh, 8);
        }
        discN(XX(-2), YY(13), ZZ(0.9f), gd, 6); discN(XX(2), YY(13), ZZ(0.9f), gd, 6);
        if (mood == 1) slice(cx, YY(18), ZZ(8.5f), ZZ(8.5f), 0, CPI, COL(124, 22, 44));
        else if (mood == 2) { arcL(cx, YY(25), ZZ(5.5f), ZZ(3.5f), 1.15f * CPI, 1.85f * CPI, ZZ(1.8f), COL(40, 100, 40)); tears(cx, cy, s, t, 11, 8); }
        else beam(XX(-5), YY(22), XX(5), YY(22), ZZ(1.8f), COL(40, 100, 40));
        break; }

    case 5: {   /* COWBOY */
        u64 hat = COL(156, 100, 50), hd = COL(112, 70, 36), vest = COL(124, 80, 42), shirt = COL(238, 228, 204), red = COL(212, 44, 52);
        if (full) {
            legsC(cx, cy, s, COL(62, 92, 162), COL(92, 56, 30));
            torsoC(cx, cy, s, shirt, COL(205, 195, 170));
            quad(XX(-29), YY(44), XX(-8), YY(44), XX(-9), YY(86), XX(-27), YY(86), vest);
            quad(XX(29), YY(44), XX(8), YY(44), XX(9), YY(86), XX(27), YY(86), vest);
            tri(XX(-24), YY(50), XX(-12), YY(50), XX(-18), YY(62), gold);
            rect(XX(-27), YY(79), XX(27), YY(84), COL(72, 42, 22)); rect(XX(-5), YY(78), XX(5), YY(85), gold);
            tri(XX(-17), YY(21), XX(17), YY(21), cx, YY(46), red);
            armsC(cx, cy, s, mood, t, shirt, skin, 8.5f);
        }
        disc(cx, cy, ZZ(25), skin);
        discN(cx, YY(4), ZZ(3.6f), COL(218, 152, 112), 8);
        faceStd(cx, cy, s, mood, t, 9, -3, 16, COL(80, 50, 25), 3);
        ell(XX(-8.5f), YY(9.5f), ZZ(9.5f), ZZ(3.8f), COL(72, 46, 26)); ell(XX(8.5f), YY(9.5f), ZZ(9.5f), ZZ(3.8f), COL(72, 46, 26));
        discN(XX(-18), YY(8), ZZ(3), COL(72, 46, 26), 6); discN(XX(18), YY(8), ZZ(3), COL(72, 46, 26), 6);
        ell(cx, YY(-20), ZZ(40), ZZ(8.5f), hat);
        tri(XX(-40), YY(-20), XX(-33), YY(-19), XX(-43), YY(-28), hat); tri(XX(40), YY(-20), XX(33), YY(-19), XX(43), YY(-28), hat);
        quad(XX(-19), YY(-20), XX(19), YY(-20), XX(15), YY(-40), XX(-15), YY(-40), hat);
        ell(cx, YY(-40), ZZ(15), ZZ(4.5f), COL(176, 120, 64));
        beam(XX(-19), YY(-24), XX(19), YY(-24), ZZ(4.2f), hd);
        discN(cx, YY(-24), ZZ(3), gold, 8);
        break; }

    case 6: {   /* VAMPIRO */
        u64 pale = COL(232, 232, 244), blk = COL(26, 22, 32), red = COL(176, 26, 50);
        if (full) {
            quad(XX(-38), YY(30), XX(38), YY(30), XX(52), YY(98), XX(-52), YY(98), COL(124, 18, 38));
            tri(XX(-40), YY(-6), XX(-17), YY(34), XX(-42), YY(36), blk); tri(XX(40), YY(-6), XX(17), YY(34), XX(42), YY(36), blk);
            tri(XX(-37), YY(2), XX(-20), YY(31), XX(-40), YY(32), red); tri(XX(37), YY(2), XX(20), YY(31), XX(40), YY(32), red);
            legsC(cx, cy, s, blk, COL(10, 10, 16));
            torsoC(cx, cy, s, blk, COL(10, 10, 16));
            tri(XX(-11), YY(30), XX(11), YY(30), cx, YY(68), COL(244, 244, 250));
            tri(cx, YY(36), XX(-11), YY(30), XX(-11), YY(42), red); tri(cx, YY(36), XX(11), YY(30), XX(11), YY(42), red);
            discN(cx, YY(36), ZZ(3), COL(150, 20, 40), 8);
            armsC(cx, cy, s, mood, t, blk, pale, 8.5f);
        }
        tri(XX(-24), YY(-4), XX(-23), YY(9), XX(-36), YY(-11), pale); tri(XX(24), YY(-4), XX(23), YY(9), XX(36), YY(-11), pale);
        disc(cx, cy, ZZ(25), pale);
        slice(cx, YY(-8), ZZ(26.5f), ZZ(18), CPI, 2 * CPI, blk);
        tri(XX(-12), YY(-8), XX(12), YY(-8), cx, YY(-1), blk);
        faceStd(cx, cy, s, mood, t, 9, 2, 16, blk, 3);
        if (mood != 1) for (k = -1; k <= 1; k += 2) discN(XX(k * 9), YY(2 + (mood == 2 ? 1.6f : 0)), ZZ(2.6f), COL(224, 30, 52), 8);
        {
            float fy = (mood == 1) ? 11.5f : 12.8f, fl = (mood == 1) ? 7.5f : (mood == 2 ? 3.5f : 5.0f);
            tri(XX(-5.5f), YY(fy), XX(-2), YY(fy), XX(-3.8f), YY(fy + fl), wh);
            tri(XX(5.5f), YY(fy), XX(2), YY(fy), XX(3.8f), YY(fy + fl), wh);
        }
        break; }

    case 7: {   /* MAGO */
        u64 robe = COL(56, 76, 188), rd = COL(36, 50, 132), hat = COL(72, 60, 176), wb = COL(247, 247, 252);
        if (full) {
            ell(XX(-12), YY(101), ZZ(10), ZZ(4.6f), COL(100, 64, 34)); ell(XX(12), YY(101), ZZ(10), ZZ(4.6f), COL(100, 64, 34));
            quad(XX(-29), YY(44), XX(29), YY(44), XX(38), YY(100), XX(-38), YY(100), robe);
            ell(cx, YY(40), ZZ(29), ZZ(17), robe);
            rect(XX(-38), YY(95), XX(38), YY(100), rd);
            beam(XX(-29), YY(66), XX(29), YY(66), ZZ(5), gold); rect(XX(-4), YY(63), XX(4), YY(69), COL(255, 235, 140));
            star4(XX(-15), YY(80), ZZ(5), COL(255, 230, 100)); star4(XX(14), YY(54), ZZ(4), COL(255, 230, 100));
            armsC(cx, cy, s, mood, t, robe, skin, 10.5f);
        }
        disc(cx, cy, ZZ(25), skin);
        discN(cx, YY(3), ZZ(4), COL(224, 160, 124), 8);
        slice(cx, YY(6), ZZ(22), ZZ(20), 0, CPI, wb);
        tri(XX(-20), YY(7), XX(20), YY(7), cx, YY(64), wb);
        faceStd(cx, cy, s, mood, t, 9, -4, 14, wb, 3);
        ell(cx, YY(-19), ZZ(34), ZZ(7.5f), hat);
        tri(XX(-24), YY(-19), XX(24), YY(-19), XX(6), YY(-68), hat);
        beam(XX(-25), YY(-20), XX(25), YY(-20), ZZ(5), gold);
        discN(XX(6), YY(-68), ZZ(4), gold, 8);
        star4(XX(-7), YY(-38), ZZ(4.5f), COL(255, 230, 100)); star4(XX(5), YY(-52), ZZ(3.5f), COL(255, 230, 100));
        if (mood == 1) for (k = 0; k < 4; k++) {
            float a = t * 0.09f + k * 1.571f;
            star4(cx + cosf(a) * ZZ(46), cy - ZZ(10) + sinf(a) * ZZ(22), ZZ(3.5f + 1.5f * sinf(t * 0.3f + k)), COL(255, 245, 160));
        }
        break; }

    case 8: {   /* VIKING */
        u64 steel = COL(152, 158, 172), horn = COL(246, 238, 208), bd = COL(224, 130, 50), fur = COL(124, 86, 52);
        if (full) {
            legsC(cx, cy, s, COL(100, 106, 122), COL(80, 52, 30));
            torsoC(cx, cy, s, fur, COL(72, 46, 26));
            for (k = -4; k <= 4; k++) discN(XX(k * 6.5f), YY(33 + (k & 1) * 2), ZZ(6), COL(196, 166, 124), 8);
            rect(XX(-27), YY(78), XX(27), YY(84), COL(70, 44, 24)); rect(XX(-5), YY(77), XX(5), YY(85), gold);
            armsC(cx, cy, s, mood, t, skin, skin, 9.5f);
        }
        for (k = -1; k <= 1; k += 2) {
            beam(XX(k * 22), YY(-15), XX(k * 34), YY(-19), ZZ(8), horn);
            beam(XX(k * 34), YY(-19), XX(k * 39), YY(-38), ZZ(6), horn);
            discN(XX(k * 39), YY(-39), ZZ(3), horn, 6);
        }
        disc(cx, cy, ZZ(25), skin);
        slice(cx, YY(3), ZZ(26), ZZ(30), 0, CPI, bd);
        for (k = -1; k <= 1; k += 2) { beam(XX(k * 14), YY(24), XX(k * 14), YY(42), ZZ(5.5f), bd); discN(XX(k * 14), YY(43), ZZ(3), COL(200, 40, 40), 6); }
        ell(XX(-9.5f), YY(7), ZZ(10), ZZ(4), COL(244, 170, 80)); ell(XX(9.5f), YY(7), ZZ(10), ZZ(4), COL(244, 170, 80));
        faceStd(cx, cy, s, mood, t, 9, 1, 18, COL(120, 70, 30), 3);
        slice(cx, YY(-8), ZZ(26.5f), ZZ(18), CPI, 2 * CPI, steel);
        rect(XX(-26), YY(-13), XX(26), YY(-8), COL(112, 118, 134));
        rect(XX(-2), YY(-8), XX(2), YY(5), COL(112, 118, 134));
        for (k = -2; k <= 2; k++) discN(XX(k * 9), YY(-10.5f), ZZ(1.5f), gold, 6);
        break; }

    case 10: {  /* CHAPOLIN COLORADO: capuz vermelho com duas antenas, coracao amarelo no peito */
        u64 rd = COL(214, 36, 40), rdD = COL(158, 22, 28), yl = COL(255, 214, 40), yd = COL(206, 154, 18), nose = COL(224, 160, 120);
        float wob = 2.5f * sinf(t * 0.2f) + (mood == 1 ? 3.0f * sinf(t * 0.5f) : 0.0f), drop = (mood == 2) ? 5.0f : 0.0f;
        if (full) {
            legsC(cx, cy, s, rd, yl);
            torsoC(cx, cy, s, rd, yd);
            rect(XX(-27), YY(76), XX(27), YY(88), yl);                                  /* calcao amarelo */
            rect(XX(-27), YY(76), XX(27), YY(79), yd);
            discN(XX(-6.5f), YY(55), ZZ(7.4f), yl, 10); discN(XX(6.5f), YY(55), ZZ(7.4f), yl, 10);   /* coracao */
            tri(XX(-13.5f), YY(58), XX(13.5f), YY(58), cx, YY(73), yl);
            discN(XX(-9), YY(53), ZZ(2), COL(255, 245, 190), 6);
            armsC(cx, cy, s, mood, t, rd, yl, 8.5f);
        }
        for (k = -1; k <= 1; k += 2) {                                                  /* antenas */
            float bx = XX(k * 21 + k * wob), by = YY(-47 + drop);
            beam(XX(k * 10), YY(-20), bx, by, ZZ(2.4f), ink);
            discN(bx, by, ZZ(5.2f), yl, 10);
            discN(bx - ZZ(1.4f), by - ZZ(1.6f), ZZ(1.6f), COL(255, 245, 190), 6);
        }
        disc(cx, cy, ZZ(25), rd);                                                       /* capuz */
        arcL(cx, cy, ZZ(24), ZZ(24), 1.1f * CPI, 1.9f * CPI, ZZ(2.2f), rdD);
        ell(cx, YY(5), ZZ(17.5f), ZZ(19), skin);                                        /* rosto */
        discN(cx, YY(5), ZZ(3.4f), nose, 8);
        faceStd(cx, cy, s, mood, t, 8.5f, -3, 14, COL(60, 30, 20), 3);
        break; }

    case 11: {  /* HACKER: capuz escuro, oculos ciano e moletom com 01 */
        u64 hd = COL(30, 34, 46), hdL = COL(56, 62, 80), cyn = COL(0, 240, 220), jeans = COL(36, 46, 84), shoe = COL(60, 255, 140);
        if (full) {
            legsC(cx, cy, s, jeans, shoe);
            torsoC(cx, cy, s, hd, hdL);
            beam(XX(-6), YY(44), XX(-6), YY(60), ZZ(1.6f), hdL); beam(XX(6), YY(44), XX(6), YY(60), ZZ(1.6f), hdL);
            text(XX(-13), YY(60), 0.42f * s, cyn, "01");
            armsC(cx, cy, s, mood, t, hd, skin, 8.5f);
        }
        disc(cx, cy, ZZ(26), hd);
        arcL(cx, cy, ZZ(25), ZZ(25), 1.05f * CPI, 1.95f * CPI, ZZ(2.0f), hdL);
        ell(cx, YY(5), ZZ(17.5f), ZZ(19), skin);
        discN(cx, YY(5.5f), ZZ(3.2f), COL(218, 152, 112), 8);
        faceStd(cx, cy, s, mood, t, 8.5f, -3, 14, 0, 3);
        rect(XX(-16), YY(-8), XX(16), YY(2), CA(0, 240, 220, 0x50));                        /* oculos */
        beam(XX(-16), YY(-8), XX(16), YY(-8), ZZ(1.4f), cyn); beam(XX(-16), YY(2), XX(16), YY(2), ZZ(1.4f), cyn);
        beam(XX(-16), YY(-8), XX(-16), YY(2), ZZ(1.4f), cyn); beam(XX(16), YY(-8), XX(16), YY(2), ZZ(1.4f), cyn);
        if (((t / 12) & 3) == 0) beam(XX(-11), YY(-6), XX(-6), YY(0), ZZ(1.2f), COL(255, 255, 255));
        ell(XX(-27), YY(2), ZZ(4), ZZ(8), cyn); ell(XX(27), YY(2), ZZ(4), ZZ(8), cyn);     /* fone */
        break; }

    case 12: {  /* PAPAI NOEL */
        u64 rd = COL(208, 32, 40), rdD = COL(150, 18, 28), wt = COL(248, 248, 252), wtD = COL(214, 220, 232), blk = COL(30, 24, 26);
        if (full) {
            legsC(cx, cy, s, rd, blk);
            torsoC(cx, cy, s, rd, rdD);
            rect(XX(-4), YY(44), XX(4), YY(78), wt);
            rect(XX(-28), YY(77), XX(28), YY(85), blk); rect(XX(-6.5f), YY(75), XX(6.5f), YY(87), gold); rect(XX(-3.5f), YY(78), XX(3.5f), YY(84), blk);
            for (k = 0; k < 3; k++) discN(cx, YY(52 + k * 8), ZZ(1.6f), blk, 6);
            armsC(cx, cy, s, mood, t, rd, wt, 9.0f);
        }
        disc(cx, cy, ZZ(25), skin);
        faceStd(cx, cy, s, mood, t, 9, -4, 17, wtD, 1);
        discN(XX(-17), YY(6), ZZ(3.6f), CA(255, 110, 120, 0x50), 8); discN(XX(17), YY(6), ZZ(3.6f), CA(255, 110, 120, 0x50), 8);
        ell(cx, YY(16), ZZ(23), ZZ(15), wt);                                              /* barba */
        ell(cx, YY(19), ZZ(20), ZZ(11), wtD);
        ell(cx, YY(15), ZZ(21), ZZ(12), wt);
        ell(XX(-8), YY(9), ZZ(9), ZZ(4), wt); ell(XX(8), YY(9), ZZ(9), ZZ(4), wt);        /* bigode */
        discN(cx, YY(5), ZZ(4.4f), COL(236, 120, 110), 8);                                /* nariz */
        if (mood == 1) slice(cx, YY(16), ZZ(6), ZZ(6.5f), 0.0f, CPI, COL(140, 20, 30));
        else if (mood == 2) arcL(cx, YY(21), ZZ(5), ZZ(3), 1.15f * CPI, 1.85f * CPI, ZZ(1.6f), ink);
        quad(XX(-24), YY(-12), XX(24), YY(-12), XX(26), YY(-30), XX(-8), YY(-50), rd);   /* gorro */
        ell(cx, YY(-12), ZZ(28), ZZ(7), wt);
        discN(XX(-8), YY(-50), ZZ(5.6f), wt, 10);
        break; }

    case 13: {  /* MESTRE DO TEMPO: capuz azul com detalhes dourados, monoculo e relogio no peito */
        u64 bl = COL(34, 48, 110), blD = COL(18, 26, 70), gd2 = COL(255, 214, 90), gdD = COL(190, 146, 40), cy2 = COL(120, 200, 255), sv = COL(214, 222, 236);
        float ha = t * 0.05f;
        if (full) {
            legsC(cx, cy, s, blD, gdD);
            torsoC(cx, cy, s, bl, gdD);
            rect(XX(-27), YY(76), XX(27), YY(80), gd2);                                   /* cinto */
            discN(cx, YY(62), ZZ(13), gdD, 16);                                           /* relogio */
            discN(cx, YY(62), ZZ(11), COL(240, 244, 255), 16);
            for (k = 0; k < 4; k++) discN(XX(cosf(k * CPI * 0.5f) * 8.6f), YY(62 + sinf(k * CPI * 0.5f) * 8.6f), ZZ(0.9f), blD, 6);
            beam(cx, YY(62), XX(cosf(ha) * 8.0f), YY(62 + sinf(ha) * 8.0f), ZZ(1.4f), blD);
            beam(cx, YY(62), XX(cosf(ha * 0.083f) * 5.0f), YY(62 + sinf(ha * 0.083f) * 5.0f), ZZ(1.8f), blD);
            armsC(cx, cy, s, mood, t, bl, skin, 9.0f);
        }
        disc(cx, cy, ZZ(26), bl);                                                         /* capuz */
        arcL(cx, cy, ZZ(25), ZZ(25), 1.05f * CPI, 1.95f * CPI, ZZ(2.2f), gd2);
        discN(cx, YY(-17), ZZ(4.5f), gd2, 10); discN(cx, YY(-17), ZZ(3.0f), cy2, 8);      /* relogio na testa do capuz */
        ell(cx, YY(5), ZZ(17.5f), ZZ(19), skin);
        ell(cx, YY(22), ZZ(8.5f), ZZ(5.5f), sv);                                          /* barbicha */
        faceStd(cx, cy, s, mood, t, 8.5f, -3, 14, sv, 3);
        ring(XX(8.5f), YY(-3), ZZ(6.8f), gd2);                                            /* monoculo */
        beam(XX(14), YY(1), XX(22), YY(18), ZZ(1.0f), gd2);
        break; }

    case 14: {  /* GRINCH: pelo verde, olhos amarelos, sobrancelha malvada, sorriso torto e gorro roubado do Noel */
        u64 gr = COL(100, 184, 58), grD = COL(60, 126, 34), grL = COL(168, 216, 112), rd = COL(208, 32, 40), ye = COL(255, 226, 70);
        if (full) {
            legsC(cx, cy, s, gr, grD);
            torsoC(cx, cy, s, gr, grD);
            ell(cx, YY(66), ZZ(15), ZZ(18), grL);                                         /* barriga clara */
            beam(XX(-19), YY(47), XX(19), YY(47), ZZ(8), rd);                             /* cachecol roubado */
            beam(XX(10), YY(50), XX(14), YY(72), ZZ(6), rd);
            armsC(cx, cy, s, mood, t, gr, gr, 9.0f);
        }
        tri(XX(-21), YY(-2), XX(-38), YY(-20), XX(-24), YY(12), gr); tri(XX(21), YY(-2), XX(38), YY(-20), XX(24), YY(12), gr);   /* orelhas pontudas */
        ell(cx, YY(3), ZZ(24), ZZ(27), gr);                                               /* cabeca comprida */
        ell(cx, YY(14), ZZ(14), ZZ(8), grL);                                              /* focinho claro */
        for (k = -1; k <= 1; k += 2) {
            float ex = XX(k * 9), yy = YY(-2);
            if (mood == 1) {
                beam(ex - ZZ(4.2f), yy + ZZ(2.0f), ex, yy - ZZ(2.2f), ZZ(2.4f), ink);
                beam(ex, yy - ZZ(2.2f), ex + ZZ(4.2f), yy + ZZ(2.0f), ZZ(2.4f), ink);
            } else {
                discN(ex, yy, ZZ(4.8f), ye, 10);
                rect(ex - ZZ(0.8f), yy - ZZ(3.2f) + (mood == 2 ? ZZ(1.6f) : 0.0f), ex + ZZ(0.8f), yy + ZZ(3.2f) + (mood == 2 ? ZZ(1.6f) : 0.0f), ink);
            }
            if (mood == 2) beam(XX(k * 14), YY(-6), XX(k * 4), YY(-12), ZZ(2.4f), grD);   /* triste: inclinada para cima */
            else           beam(XX(k * 14), YY(-12), XX(k * 4), YY(-6), ZZ(2.4f), grD);   /* malvada: desce para o centro */
        }
        discN(cx, YY(6), ZZ(3.2f), grD, 8);                                               /* nariz */
        faceStd(cx, cy, s, mood, t, 9, -2, 15, 0, 2);                                     /* so a boca */
        quad(XX(-24), YY(-12), XX(24), YY(-12), XX(26), YY(-30), XX(-8), YY(-50), rd);   /* gorro roubado */
        ell(cx, YY(-12), ZZ(28), ZZ(7), COL(248, 248, 252));
        discN(XX(-8), YY(-50), ZZ(5.6f), COL(248, 248, 252), 10);
        break; }

    case 15: {  /* POSEIDON: cabelo e barba azuis, coroa de coral, tunica turquesa e tridente dourado */
        u64 sk = COL(238, 190, 150), bd = COL(44, 132, 218), bdD = COL(24, 88, 170), bdL = COL(130, 205, 255);
        u64 cr = COL(255, 118, 104), crD = COL(214, 72, 76), tn = COL(28, 152, 196), tnD = COL(16, 100, 148), gd2 = COL(255, 206, 60);
        if (full) {
            float hx, hy;
            legsC(cx, cy, s, sk, gd2);
            torsoC(cx, cy, s, tn, tnD);
            beam(XX(-27), YY(79), XX(27), YY(79), ZZ(5), gd2);                            /* cinto dourado */
            discN(cx, YY(79), ZZ(5.5f), cr, 8);                                           /* concha no cinto */
            armsC(cx, cy, s, mood, t, tn, sk, 9.0f);
            if (mood == 1)      { hx = XX(-(46 + 3 * sinf(t * 0.3f - 1.0f))); hy = YY(2 + 7 * sinf(t * 0.35f)); }   /* segue a mao esquerda */
            else if (mood == 2) { hx = XX(-31); hy = YY(86); }
            else                { hx = XX(-37); hy = YY(74); }
            beam(hx, hy + ZZ(36), hx, hy - ZZ(46), ZZ(3.2f), gd2);                        /* tridente */
            beam(hx - ZZ(9), hy - ZZ(52), hx - ZZ(9), hy - ZZ(38), ZZ(2.4f), gd2);
            beam(hx + ZZ(9), hy - ZZ(52), hx + ZZ(9), hy - ZZ(38), ZZ(2.4f), gd2);
            beam(hx - ZZ(9), hy - ZZ(38), hx, hy - ZZ(31), ZZ(2.4f), gd2);
            beam(hx + ZZ(9), hy - ZZ(38), hx, hy - ZZ(31), ZZ(2.4f), gd2);
            tri(hx - ZZ(3.6f), hy - ZZ(46), hx + ZZ(3.6f), hy - ZZ(46), hx, hy - ZZ(62), gd2);
            tri(hx - ZZ(12.6f), hy - ZZ(52), hx - ZZ(5.4f), hy - ZZ(52), hx - ZZ(9), hy - ZZ(63), gd2);
            tri(hx + ZZ(5.4f), hy - ZZ(52), hx + ZZ(12.6f), hy - ZZ(52), hx + ZZ(9), hy - ZZ(63), gd2);
        }
        ell(cx, YY(-2), ZZ(30), ZZ(28), bd);                                              /* cabelo */
        ell(XX(-25), YY(20), ZZ(8), ZZ(21), bd); ell(XX(25), YY(20), ZZ(8), ZZ(21), bd);  /* mechas dos lados */
        ell(cx, YY(4), ZZ(23), ZZ(24), sk);                                               /* rosto */
        discN(cx, YY(5), ZZ(3.0f), COL(222, 160, 124), 8);                                /* nariz */
        ell(cx, YY(20), ZZ(22), ZZ(15), bd);                                              /* barba */
        tri(XX(-18), YY(24), XX(18), YY(24), cx, YY(46), bd);                             /* barba pontuda */
        beam(cx, YY(28), cx, YY(42), ZZ(2.0f), bdL);
        ell(XX(-8), YY(10), ZZ(8.5f), ZZ(3.4f), bdL); ell(XX(8), YY(10), ZZ(8.5f), ZZ(3.4f), bdL);   /* bigode */
        faceStd(cx, cy, s, mood, t, 9, -3, 17, bdD, 3);                                   /* olhos, sobrancelhas e boca */
        rect(XX(-20), YY(-31), XX(20), YY(-25), crD);                                     /* coroa de coral */
        for (k = -2; k <= 2; k++) {
            int ak = k < 0 ? -k : k;
            float bx = k * 9.0f, ty = -31.0f - (ak == 0 ? 16.0f : (ak == 1 ? 12.0f : 9.0f)), tx = bx + (k < 0 ? -2.0f : (k > 0 ? 2.0f : 0.0f));
            tri(XX(bx - 4.5f), YY(-31), XX(bx + 4.5f), YY(-31), XX(tx), YY(ty), ak == 1 ? crD : cr);
            discN(XX(tx), YY(ty), ZZ(1.8f), COL(250, 250, 255), 6);                       /* perola na ponta */
        }
        break; }

    case 16: {  /* COVEIRO: chapeu preto, rosto palido, barba suja, casaco escuro e uma pa */
        u64 pl = COL(206, 214, 196), pl2 = COL(150, 164, 146), coat = COL(46, 44, 56), coatD = COL(26, 24, 34), hatc = COL(30, 28, 36);
        u64 band = COL(120, 96, 60), wd = COL(120, 82, 46), steel = COL(176, 184, 192), gl = COL(150, 255, 170);
        if (full) {
            float hx, hy;
            legsC(cx, cy, s, coatD, COL(20, 18, 22));
            torsoC(cx, cy, s, coat, coatD);
            beam(cx, YY(46), cx, YY(84), ZZ(2.0f), coatD);                                /* abotoadura */
            discN(cx, YY(58), ZZ(1.8f), band, 6); discN(cx, YY(70), ZZ(1.8f), band, 6);
            armsC(cx, cy, s, mood, t, coat, pl, 9.0f);
            if (mood == 1)      { hx = XX(-(46 + 3 * sinf(t * 0.3f - 1.0f))); hy = YY(2 + 7 * sinf(t * 0.35f)); }   /* segue a mao esquerda */
            else if (mood == 2) { hx = XX(-31); hy = YY(86); }
            else                { hx = XX(-37); hy = YY(74); }
            beam(hx, hy + ZZ(34), hx, hy - ZZ(40), ZZ(3.0f), wd);                         /* cabo da pa */
            quad(hx - ZZ(6), hy - ZZ(60), hx + ZZ(6), hy - ZZ(60), hx + ZZ(8), hy - ZZ(42), hx - ZZ(8), hy - ZZ(42), steel);   /* lamina */
            beam(hx, hy - ZZ(58), hx, hy - ZZ(44), ZZ(1.0f), COL(120, 128, 138));
        }
        ell(cx, YY(4), ZZ(23), ZZ(25), pl);                                               /* rosto palido */
        ell(cx, YY(23), ZZ(16), ZZ(8), pl2);                                              /* barba suja */
        discN(cx, YY(8), ZZ(2.8f), pl2, 8);                                               /* nariz */
        faceStd(cx, cy, s, mood, t, 9, 0, 16, coatD, 3);
        ell(XX(-9), YY(6), ZZ(5.5f), ZZ(2.2f), CA(70, 80, 96, 0x40)); ell(XX(9), YY(6), ZZ(5.5f), ZZ(2.2f), CA(70, 80, 96, 0x40));   /* olheiras */
        ell(cx, YY(-14), ZZ(36), ZZ(7), hatc);                                            /* aba do chapeu */
        quad(XX(-20), YY(-14), XX(20), YY(-14), XX(17), YY(-42), XX(-17), YY(-42), hatc); /* copa */
        beam(XX(-20), YY(-18), XX(20), YY(-18), ZZ(4), band);
        discN(XX(31), YY(-8 + 3 * sinf(t * 0.2f)), ZZ(3.6f), CA(150, 255, 170, 0x70), 8); /* fogo-fatuo */
        discN(XX(31), YY(-8 + 3 * sinf(t * 0.2f)), ZZ(1.4f), gl, 6);
        break; }

    case 17: {  /* POLICIAL: bone azul-marinho com distintivo dourado, farda azul, bigode e algemas no cinto */
        u64 bl = COL(36, 62, 128), blD = COL(20, 36, 84), cap = COL(22, 36, 88), vis = COL(12, 16, 30), bad = COL(255, 205, 50), mus = COL(70, 46, 30);
        if (full) {
            legsC(cx, cy, s, blD, COL(16, 16, 22));
            torsoC(cx, cy, s, bl, blD);
            rect(XX(-28), YY(77), XX(28), YY(85), COL(22, 22, 28));                       /* cinto */
            rect(XX(-6), YY(76), XX(6), YY(86), bad);                                     /* fivela */
            beam(cx, YY(46), cx, YY(76), ZZ(2.0f), blD);                                  /* abotoadura */
            discN(XX(-14), YY(58), ZZ(5.0f), bad, 10); discN(XX(-14), YY(58), ZZ(2.4f), bl, 8);   /* distintivo no peito */
            ringN(XX(18), YY(80), ZZ(3.6f), ZZ(1.4f), 12, COL(230, 232, 240));          /* algemas no cinto */
            ringN(XX(24), YY(80), ZZ(3.6f), ZZ(1.4f), 12, COL(230, 232, 240));
            armsC(cx, cy, s, mood, t, bl, skin, 9.0f);
        }
        disc(cx, cy, ZZ(25), skin);
        faceStd(cx, cy, s, mood, t, 9, 0, 17, COL(40, 28, 22), 3);
        ell(XX(-7), YY(10), ZZ(8), ZZ(3.2f), mus); ell(XX(7), YY(10), ZZ(8), ZZ(3.2f), mus);   /* bigode */
        ell(cx, YY(-13), ZZ(33), ZZ(7), vis);                                         /* aba do bone */
        quad(XX(-22), YY(-12), XX(22), YY(-12), XX(19), YY(-38), XX(-19), YY(-38), cap);   /* copa */
        beam(XX(-22), YY(-17), XX(22), YY(-17), ZZ(4), vis);
        discN(cx, YY(-27), ZZ(5.4f), bad, 10); discN(cx, YY(-27), ZZ(2.6f), COL(220, 40, 52), 8);   /* distintivo do bone */
        break; }

    default: {  /* GATO */
        u64 og = COL(244, 152, 64), od = COL(206, 106, 34), pk = COL(250, 150, 172), cr = COL(255, 250, 244);
        if (full) {
            float wob = 4.0f * sinf(t * 0.15f);
            beam(XX(26), YY(82), XX(44), YY(78), ZZ(8), og);
            beam(XX(44), YY(78), XX(53), YY(62 + wob), ZZ(8), og);
            beam(XX(53), YY(62 + wob), XX(47), YY(46 + wob), ZZ(8), og);
            discN(XX(47), YY(45 + wob), ZZ(4.6f), cr, 8);
            legsC(cx, cy, s, og, cr);
            torsoC(cx, cy, s, og, od);
            ell(cx, YY(64), ZZ(15), ZZ(19), cr);
            armsC(cx, cy, s, mood, t, og, cr, 9);
        }
        tri(XX(-28), YY(-2), XX(-25), YY(-34), XX(-6), YY(-20), og); tri(XX(28), YY(-2), XX(25), YY(-34), XX(6), YY(-20), og);
        tri(XX(-24), YY(-8), XX(-22.5f), YY(-27), XX(-11), YY(-19), pk); tri(XX(24), YY(-8), XX(22.5f), YY(-27), XX(11), YY(-19), pk);
        ell(cx, YY(2), ZZ(29), ZZ(24), og);
        beam(cx, YY(-21), cx, YY(-11), ZZ(2.6f), od); beam(XX(-7), YY(-20), XX(-5), YY(-12), ZZ(2.6f), od); beam(XX(7), YY(-20), XX(5), YY(-12), ZZ(2.6f), od);
        for (k = -1; k <= 1; k += 2) {
            if (mood == 1) {
                beam(XX(k * 11 - 4.5f), YY(-0.5f), XX(k * 11), YY(-4.5f), ZZ(2.4f), ink);
                beam(XX(k * 11), YY(-4.5f), XX(k * 11 + 4.5f), YY(-0.5f), ZZ(2.4f), ink);
            } else {
                ell(XX(k * 11), YY(-2), ZZ(mood == 2 ? 7.4f : 6.4f), ZZ(mood == 2 ? 8 : 7), COL(124, 222, 112));
                ell(XX(k * 11), YY(-2), ZZ(mood == 2 ? 2.4f : 1.8f), ZZ(mood == 2 ? 6.5f : 5.6f), ink);
                discN(XX(k * 11 - 2), YY(-5), ZZ(1.4f), wh, 6);
            }
        }
        if (mood == 2) tears(cx, cy, s, t, 11, 1);
        ell(cx, YY(11), ZZ(13), ZZ(9), cr);
        tri(XX(-3.6f), YY(5), XX(3.6f), YY(5), cx, YY(9.5f), pk);
        if (mood == 1) {
            slice(cx, YY(10), ZZ(6), ZZ(7), 0, CPI, COL(130, 22, 40));
            ell(cx, YY(15), ZZ(3), ZZ(2), pk);
        } else if (mood == 2) {
            arcL(XX(-3.2f), YY(14), ZZ(3.2f), ZZ(2.6f), 1.1f * CPI, 1.9f * CPI, ZZ(1.4f), ink);
            arcL(XX(3.2f), YY(14), ZZ(3.2f), ZZ(2.6f), 1.1f * CPI, 1.9f * CPI, ZZ(1.4f), ink);
        } else {
            arcL(XX(-3.2f), YY(10.5f), ZZ(3.2f), ZZ(2.6f), 0.1f * CPI, 0.9f * CPI, ZZ(1.4f), ink);
            arcL(XX(3.2f), YY(10.5f), ZZ(3.2f), ZZ(2.6f), 0.1f * CPI, 0.9f * CPI, ZZ(1.4f), ink);
        }
        for (k = -1; k <= 1; k += 2) {
            int q;
            for (q = -1; q <= 1; q++)
                beam(XX(k * 12), YY(9.5f + q * 2.2f), XX(k * 33), YY(8 + q * 5.5f), ZZ(1.2f), CA(255, 255, 255, 0x70));
        }
        if (full) {
            beam(XX(-19), YY(30), XX(19), YY(30), ZZ(6), COL(210, 40, 50));
            discN(cx, YY(37), ZZ(4.8f), gold, 8);
        }
        break; }
    }
    #undef XX
    #undef YY
    #undef ZZ
}

/* menu de tacos: foto do taco, nome e estatisticas */
/* catalogo: ordem das fichas (padrao primeiro, depois os tacos de personagem) */
/* raridades dos personagens (e dos tacos deles): 0 RARO, 1 SUPER RARO, 2 EPICO, 3 MISTICO, 4 LENDARIO, 5 ULTRA LENDARIO */
static const char *RAR_NAME[6] = { "RARO", "SUPER RARO", "EPICO", "MISTICO", "LENDARIO", "ULTRA LENDARIO" };
static u64 rarCol(int r)
{
    switch (r) {
    case 0:  return COL(120,190,255);
    case 1:  return COL(110,230,150);
    case 2:  return COL(200,120,255);
    case 3:  return COL(255,110,190);
    case 4:  return COL(255,215,70);
    case 5: {                                             /* ULTRA LENDARIO: cor que corre pelo arco-iris (tons vivos, sempre brilhantes) */
        float t = menuT * 0.06f;
        return COL(155 + (int)(100.0f * sinf(t)), 155 + (int)(100.0f * sinf(t + 2.094f)), 155 + (int)(100.0f * sinf(t + 4.189f)));
    }
    default: return COL(255,215,70);
    }
}
/* ULTRA LENDARIO: Coveiro e Chapolin | LENDARIO: Grinch e Policial | MISTICO: Vampiro, Mago, Papai Noel e Mestre do Tempo | EPICO: Hacker, Poseidon e Gato |
   SUPER RARO: Viking, Ninja e Pirata | RARO: Pinguim, Robo, Alien e Cowboy */
static const int CHAR_RAR[NCHARS] = { 0, 0, 1, 1, 0, 0, 3, 3, 1, 2, 5, 2, 3, 3, 4, 2, 5, 4 };

/* catalogo: ordem das fichas, por categoria (padrao, paises e depois os tacos de personagem por raridade) */
#define NCAT_SEC 8
static const int CATH[NCAT_SEC] = { 0, 8, 14, 18, 21, 25, 28, 30 };     /* 1a ficha de cada categoria */
static const char *CATHN[NCAT_SEC] = { "PADRAO", "PAISES", "RARO", "SUPER RARO", "EPICO", "MISTICO", "LENDARIO", "ULTRA LENDARIO" };
static u64 catHCol(int h) { return h == 0 ? COL(150,220,255) : (h == 1 ? COL(120,235,150) : rarCol(h - 2)); }
static const int CAT[NCUES] = { 0, 1, 2, 3, 4, 5, 8, 9,   13, 19, 20, 21, 22, 23,   7, 12, 26, 27,   11, 10, 6,   16, 18, 25, 29,   14, 15, 28,   24, 31,   30, 17 };
static int catPos(int c) { int i; for (i = 0; i < NCUES; i++) if (CAT[i] == c) return i; return 0; }
static int catSec(int pos) { int h, r = 0; for (h = 0; h < NCAT_SEC; h++) if (CATH[h] <= pos) r = h; return r; }
static int catStep(int d) { return CAT[(catPos(cueMenuSel) + d + NCUES) % NCUES]; }


/* texto que encolhe para caber em maxw pixels (evita sair da caixa / encostar em outro texto) */
static void textFit(float x, float y, float sc, u64 c, const char *t, float maxw)
{
    float w = tw(sc, t);
    if (w > maxw && w > 0.0f) sc *= maxw / w;
    text(x, y, sc, c, t);
}

static void drawCueMenu(void)
{
    static const char *SN[4] = { "FORCA", "EFEITO", "MIRA", "CONTROLE" };
    static const u8 SC[4][3] = { {235,95,55}, {90,155,255}, {95,215,115}, {240,210,70} };
    static const char *AD[NAB][2] = {
        { "", "" },
        { "A BRANCA SAI COMO UMA BALA: +50% DE VELOCIDADE", "E SEM EFEITO. O TACO DISPARA COM UM CLARAO." },
        { "O GELO COBRE A MESA: AS BOLAS DESLIZAM MUITO", "MAIS LONGE (QUASE SEM ATRITO) NESTA TACADA." },
        { "DEPOIS DA BATIDA, A BOLA ALVO CURVA SUAVEMENTE EM", "DIRECAO A CACAPA QUE JA ESTA MIRANDO (SEM IMA)." },
        { "TODAS AS CACAPAS FICAM 45% MAIORES NESTA TACADA,", "COM ARO DOURADO. A CHAMINE DO PAPAI NOEL!" },
        { "TRAVA A MIRA NA MELHOR TACADA (O DE NOVO TROCA", "A OPCAO), MOSTRA O CAMINHO E A FORCA IDEAL." },
        { "A BOLA ALVO EXPLODE E EMPURRA PARA LONGE", "AS BOLAS QUE ESTAO PERTO DELA." },
        { "+20% DE FORCA E A BRANCA PARA NA HORA AO", "BATER NA BOLA (TACADA CORTADA)." },
        { "A BRANCA FAZ UMA CURVA FORTE (EFEITO X2,5)", "MESMO SEM USAR L2/R2." },
        { "CHAPULIN! A MARRETADA FAZ A BOLA ALVO DAR UM PULO DE", "GAFANHOTO E VOAR POR CIMA DAS OUTRAS BOLAS." },
        { "DEPOIS DE QUALQUER TACADA, VOCE PODE VOLTAR NO", "TEMPO E REFAZER A JOGADA - SE QUISER." },
        { "PODER SORTEADO CONFORME A MESA (PUXAO VERDE OU BAGUNCA) E", "MOSTRADO ANTES. NAO USOU NA TACADA? SORTEIA OUTRO." },
        { "", "" },
        { "UMA ONDA GIGANTE EMPURRA TODAS AS BOLAS DA MESA", "NA DIRECAO DA CACAPA MAIS PROXIMA (FORCA FRACA)." },
        { "A BRANCA CURVA SOZINHA E PERSEGUE A BOLA CERTA MAIS", "PERTO DA SUA MIRA, ATE A PRIMEIRA BATIDA." },
        { "AS SUAS BOLAS SAO PUXADAS PARA A CACAPA MAIS PROXIMA", "(FORCA FRACA). MESA ABERTA: TODAS, MENOS A 8." },
        { "AMALDICOA O ADVERSARIO: NA PROXIMA TACADA DELE", "OS BOTOES DE MIRAR FICAM INVERTIDOS E A FORCA FALHA AS VEZES." },
        { "NAO PERDE A VEZ: SE NAO ENCACAPAR, VOCE JOGA DE NOVO.", "PERDOA FALTA E SUICIDIO. SE ENCACAPAR, NAO GASTA." },
        { "", "" }, { "", "" }, { "", "" }, { "", "" }, { "", "" }, { "", "" }, { "", "" }, { "", "" }, { "", "" }, { "", "" } };
    const CueDef *cd = &CUES[cueMenuSel];
    int val[4], k, ch = cueChar(cueMenuSel), ab = CUE_AB[cueMenuSel], tb = ch >= 0 ? CHAR_TAB[ch] : -1;
    char buf[64];
    float y;

    val[0] = cd->force; val[1] = cd->effect; val[2] = cd->aim; val[3] = cd->ctl;

    rect(0, 0, 640, 448, GS_SETREG_RGBAQ(0, 0, 0, 0x68, 0x00));
    hbox(10, 8, 630, 436, CA(255, 215, 60, 0x7c), CA(8, 12, 24, 0x7c));
    sprintf(buf, "CATALOGO DE TACOS - %s", pname(cur));
    text(24, 14, 0.5f, COL(255,215,70), buf);
    rect(22, 40, 616, 41, CA(255, 215, 60, 0x40));

    /* lista (3 categorias, com rolagem) */
    {
        int tot = NCUES + NCAT_SEC, sel0 = catPos(cueMenuSel) + catSec(catPos(cueMenuSel)) + 1, hs = 0;
        int vis = 20, top = sel0 - vis / 2, e, kk = 0;
        if (top > tot - vis) top = tot - vis;
        if (top < 0) top = 0;
        y = 46.0f;
        for (e = 0; e < tot; e++) {
            int isH = (hs < NCAT_SEC && kk == CATH[hs]), hi = hs;
            if (isH) hs++;
            if (e < top || e >= top + vis) { if (!isH) kk++; continue; }
            if (isH) {
                text(24, y + 2, 0.3f, catHCol(hi), CATHN[hi]);
                rect(24, y + 13, 206, y + 14, CA(150, 160, 180, 0x40));
            } else {
                int c = CAT[kk], sel = (c == cueMenuSel), cc = cueChar(c);
                if (sel) rect(20, y - 2, 210, y + 16, CA(255, 215, 60, 0x30 + (int)(0x10 * sinf(menuT * 0.15f))));
                if (c == cueSel[cur]) discN(30, y + 7, 3.4f, COL(110,230,130), 8);
                text(40, y, 0.34f, sel ? COL(255,240,150) : COL(175,185,195), CUES[c].name);
                if (cc >= 0) drawChar(cc, 196, y + 8, 0.2f, 0, menuT, 0);
                kk++;
            }
            y += 18.0f;
        }
        if (top > 0) text(196, 40, 0.26f, COL(255,215,70), "^");
        if (top < tot - vis) text(196, 410, 0.26f, COL(255,215,70), "v");
    }

    /* foto do taco (palco com o feltro da mesa do personagem) */
    {
        const u8 *fe = tb >= 0 ? TABLES[tb].felt : TABLES[0].felt, *wo = tb >= 0 ? TABLES[tb].wood : TABLES[0].wood;
        rect(226, 48, 616, 114, shade(wo, 0.9f, 0));
        rect(228, 50, 614, 112, shade(fe, 1.0f, 0));
        drawCue(252.0f, 82.0f, 0.0f, 0x80, 3.14159265f, cueMenuSel);
    }

    text(228, 118, 0.6f, COL(255,255,255), cd->name);
    if (ch >= 0 && CHAR_RAR[ch] == 5) sprintf(buf, "TACO ULTRA LENDARIO DO %s", CHAR_NAME[ch]); else if (ch >= 0 && CHAR_RAR[ch] == 4) sprintf(buf, "TACO LENDARIO DO %s", CHAR_NAME[ch]); else if (ch >= 0) sprintf(buf, "TACO DO %s", CHAR_NAME[ch]); else strcpy(buf, cueMenuSel == 13 || (cueMenuSel >= 19 && cueMenuSel <= 23) ? "TACO DE PAIS" : "TACO PADRAO");
    text(228, 142, 0.32f, ch >= 0 ? COL(255,190,110) : (strcmp(buf, "TACO DE PAIS") == 0 ? COL(120,235,150) : COL(150,220,255)), buf);

    for (k = 0; k < 4; k++) {
        float yy = 164.0f + k * 22.0f;
        text(228, yy, 0.34f, COL(210,210,220), SN[k]);
        rect(330, yy + 2, 470, yy + 14, COL(40,42,52));
        rect(330, yy + 2, 330 + 1.4f * val[k], yy + 14, COL(SC[k][0], SC[k][1], SC[k][2]));
    }

    if (ch >= 0) {                                              /* caricatura do personagem */
        float bob = 3.0f * fabsf(sinf(menuT * 0.1f));
        drawChar(ch, 566.0f, 196.0f - bob, 0.72f, 1, menuT, 1);
        text(566.0f - tw(0.3f, CHAR_NAME[ch]) * 0.5f, 262.0f, 0.3f, COL(255,235,150), CHAR_NAME[ch]);
        text(566.0f - tw(0.28f, RAR_NAME[CHAR_RAR[ch]]) * 0.5f, 275.0f, 0.28f, rarCol(CHAR_RAR[ch]), RAR_NAME[CHAR_RAR[ch]]);
    }

    /* habilidade */
    hbox(226, 290, 616, 362, ab ? CA(255, 215, 60, 0x70) : CA(90, 96, 112, 0x60), CA(14, 18, 32, 0x70));
    if (ab) {
        sprintf(buf, "HABILIDADE: %s", AB_NAME[ab]);
        textFit(236, 295, 0.42f, COL(255,215,70), buf, 345.0f);
        if (ab == AB_COVEIRO) {                                  /* 3 poderes, voce escolhe */
            textFit(236, 311, 0.25f, COL(215,220,232), "ENTERRO: CURVA UM POUCO A BOLA ALVO PARA A CACAPA A FRENTE", 375.0f);
            textFit(236, 322, 0.25f, COL(215,220,232), "COVA: CACAPA EXTRA NO MEIO DA MESA; A BOLA QUE CAI CONTA", 375.0f);
            textFit(236, 333, 0.25f, COL(215,220,232), "RESSUSCITAR: VOLTA A ULTIMA BOLA DO ADVERSARIO (FANTASMA)", 375.0f);
        } else if (ab == AB_MARRETA) {                           /* 3 poderes, voce escolhe */
            textFit(236, 311, 0.25f, COL(215,220,232), "CORNETA: PARALISA UMA BOLA DO ADVERSARIO POR 1 RODADA", 375.0f);
            textFit(236, 322, 0.25f, COL(215,220,232), "CHIQUITOLINA: BOLAS DO ADVERSARIO MENORES, LENTAS E -25% DE FORCA", 375.0f);
            textFit(236, 333, 0.25f, COL(215,220,232), "MARRETA BIONICA: BOLAS MAIS RAPIDAS E DEMORAM PARA PARAR", 375.0f);
        } else if (ab == AB_POLICIAL) {                          /* 2 poderes, voce escolhe */
            textFit(236, 311, 0.25f, COL(215,220,232), "ALGEMAS: PRENDE UMA BOLA DO ADVERSARIO POR 1 JOGADA. NADA A MOVE", 375.0f);
            textFit(236, 322, 0.25f, COL(215,220,232), "SIRENE: O ADVERSARIO JOGA SEM LINHA GUIA NA PROXIMA JOGADA", 375.0f);
            textFit(236, 333, 0.25f, COL(215,220,232), "QUADRADO TROCA A BOLA QUE VAI SER PRESA", 375.0f);
        } else {
            textFit(236, 316, 0.28f, COL(215,220,232), AD[ab][0], 345.0f);
            textFit(236, 329, 0.28f, COL(215,220,232), AD[ab][1], 345.0f);
        }
        sprintf(buf, (ab == AB_COVEIRO || ab == AB_POLICIAL || ab == AB_MARRETA) ? "O NA MIRA ARMA E TROCA O PODER - RECARGA %d TACADAS" : "APERTE O NA MIRA - RECARREGA EM %d TACADAS", AB_COOL);
        textFit(236, 344, 0.26f, COL(0,255,200), buf, 345.0f);
    } else {
        text(236, 303, 0.4f, COL(130,138,155), "SEM HABILIDADE ESPECIAL");
        text(236, 328, 0.28f, COL(120,128,145), "TACO DE ATRIBUTOS (FORCA, EFEITO, MIRA, CONTROLE)");
    }

    /* mesa do personagem */
    if (tb >= 0) {
        rect(228, 372, 252, 386, shade(TABLES[tb].wood, 0.95f, 0));
        rect(230, 374, 250, 384, shade(TABLES[tb].felt, 1.0f, 0));
        sprintf(buf, "MESA DO PERSONAGEM: %s", TABLES[tb].name);
        textFit(260, 373, 0.32f, COL(190,200,215), buf, 195.0f);
    } else text(228, 373, 0.32f, COL(120,128,145), "MESA: LIVRE (ESCOLHA NA CONFIGURACAO)");
    if (cueMenuSel == 13 || (cueMenuSel >= 19 && cueMenuSel <= 23)) text(228, 392, 0.3f, COL(120,235,150), "TACO COM AS CORES DA BANDEIRA - SO ATRIBUTOS");

    if (ab && !abilOn) text(228, 392, 0.3f, COL(255,120,110), "HABILIDADES DESLIGADAS NESTA PARTIDA");
    if (cueSel[cur] == cueMenuSel) text(616.0f - tw(0.4f, "EM USO"), 372, 0.4f, COL(110,230,130), "EM USO");
    else text(616.0f - tw(0.4f, "X: EQUIPAR"), 372, 0.4f, COL(255,230,60), "X: EQUIPAR");
    text(24, 418, 0.3f, COL(170,170,180), "CIMA/BAIXO: ESCOLHER   X: EQUIPAR   O/SELECT: VOLTAR");
}

/* ---------- menu inicial animado ---------- */
#define NMB 10
#define MBR 13.0f
static const int MB_ID[NMB] = { 0, 1, 3, 5, 8, 9, 11, 13, 14, 2 };

static void menuInit(void)
{
    int i;
    for (i = 0; i < 16; i++) b[i].in = 0;
    for (i = 0; i < NMB; i++) {
        int id = MB_ID[i];
        float a = (rnd() % 628) / 100.0f, sp = 32.0f + (float)(rnd() % 40);
        b[id].in = 1; resetOrient(id);
        b[id].x = 50.0f + (float)(rnd() % 540); b[id].y = 40.0f + (float)(rnd() % 370);
        b[id].vx = cosf(a) * sp; b[id].vy = sinf(a) * sp;
    }
}

static void menuUpdate(void)
{
    int i, j;
    const float dt = 1.0f / 60.0f;
    const float ymin = 292.0f;                                 /* as bolas rolam so na faixa da mesa */
    for (i = 0; i < NMB; i++) {
        Ball *p = &b[MB_ID[i]];
        p->x += p->vx * dt; p->y += p->vy * dt;
        if (ymin > 0.0f && p->y < ymin - 1.0f) p->y = ymin + MBR + fmodf(p->x * 1.7f, 120.0f);   /* veio de outra tela: espalha */
        if (p->x < MBR)          { p->x = MBR;          p->vx =  fabsf(p->vx); }
        if (p->x > 640.0f - MBR) { p->x = 640.0f - MBR; p->vx = -fabsf(p->vx); }
        if (p->y < ymin + MBR)   { p->y = ymin + MBR;   p->vy =  fabsf(p->vy); }
        if (p->y > 448.0f - MBR) { p->y = 448.0f - MBR; p->vy = -fabsf(p->vy); }
        spinBall(MB_ID[i], dt);
    }
    for (i = 0; i < NMB; i++)
        for (j = i + 1; j < NMB; j++) {
            Ball *p = &b[MB_ID[i]], *q = &b[MB_ID[j]];
            float dx = q->x - p->x, dy = q->y - p->y, d2 = dx*dx + dy*dy, d, nx, ny, ov, rv;
            if (d2 >= 4*MBR*MBR || d2 < 0.01f) continue;
            d = sqrtf(d2); nx = dx / d; ny = dy / d; ov = (2*MBR - d) * 0.5f;
            p->x -= nx*ov; p->y -= ny*ov; q->x += nx*ov; q->y += ny*ov;
            rv = (p->vx - q->vx) * nx + (p->vy - q->vy) * ny;
            if (rv > 0) { p->vx -= rv*nx; p->vy -= rv*ny; q->vx += rv*nx; q->vy += rv*ny; }
        }
}

/* texto centralizado (largura estimada) */
static void textC(float y, float sc, u64 c, const char *t)
{ text(320.0f - tw(sc, t) * 0.5f, y, sc, c, t); }

/* fundo (estilo 8 Ball Pool): por do sol, predios, palmeiras e mesa com bolas rolando */
static void palmTree(float x, float h, float lean)         /* silhueta de palmeira */
{
    static const float FX[7] = { -50, -38, -20, 0, 20, 38, 50 }, FY[7] = { 10, -4, -15, -20, -15, -4, 10 };
    float tx = x + lean, ty = 270.0f - h, sw = 3.0f * sinf(menuT * 0.03f + x);
    int k;
    u64 c = COL(24, 10, 44);
    quad(x - 3.5f, 270, x + 3.5f, 270, tx + 2.0f, ty, tx - 2.0f, ty, c);
    for (k = 0; k < 7; k++)
        tri(tx, ty, tx + FX[k] * 0.55f + sw, ty + FY[k] * 0.55f - 9.0f, tx + FX[k] + sw, ty + FY[k] + 9.0f, c);
}

static void drawBackdropEx(int withBalls)
{
    int k, i, layer;
    /* ceu do por do sol: roxo -> rosa -> laranja */
    for (k = 0; k < 17; k++) {
        float f = k / 16.0f;
        int r, gg, bl;
        if (f < 0.55f) { float u = f / 0.55f; r = 54 + (int)(150 * u); gg = 20 + (int)(30 * u); bl = 100 + (int)(10 * u); }
        else           { float u = (f - 0.55f) / 0.45f; r = 204 + (int)(51 * u); gg = 50 + (int)(100 * u); bl = 110 - (int)(40 * u); }
        rect(0, k * 16.0f, 640, k * 16.0f + 17.0f, COL(r, gg, bl));
    }
    for (k = 0; k < 4; k++) discN(380.0f, 266.0f, 240.0f - k * 55.0f, CA(255, 190, 100, 0x0a), 28);   /* brilho do horizonte */
    /* predios: camada de tras (clara, sem janelas) e da frente (escura, com janelas acesas) */
    for (layer = 0; layer < 2; layer++) {
        float x = layer ? -10.0f : -30.0f;
        int n = 0;
        while (x < 650.0f) {
            float w = 20.0f + ((n * 13 + layer * 7) % 5) * 7.0f;
            float h = (layer ? 46.0f : 76.0f) + ((n * 37 + layer * 19) % 11) * (layer ? 7.0f : 9.0f);
            rect(x, 270.0f - h, x + w, 271.0f, layer ? COL(40, 18, 72) : COL(108, 48, 124));
            if (layer) {
                int wy, wx;
                for (wy = (int)(270.0f - h) + 6; wy < 262; wy += 9)
                    for (wx = (int)x + 3; wx + 3 < (int)(x + w) - 2; wx += 7)
                        if ((n * 7 + wy * 3 + wx) % 5 == 0) rect((float)wx, (float)wy, (float)wx + 3.0f, (float)wy + 4.0f, CA(255, 200, 110, 0x60));
            }
            x += w + (layer ? 2.0f : 1.0f); n++;
        }
    }
    rect(0, 244, 640, 270, CA(255, 150, 90, 0x14));                  /* neblina quente na base dos predios */
    palmTree(32.0f, 150.0f, 14.0f); palmTree(84.0f, 96.0f, 9.0f); palmTree(610.0f, 132.0f, -12.0f);
    /* mesa: borda de madeira com losangos e feltro azul-petroleo */
    rect(0, 268, 640, 281, COL(88, 44, 28));
    rect(0, 268, 640, 271, COL(168, 104, 56));
    rect(0, 279, 640, 281, CA(0, 0, 0, 0x50));
    for (i = 0; i < 8; i++) { float cx = 40.0f + i * 80.0f; quad(cx, 271, cx + 4, 274, cx, 277, cx - 4, 274, COL(235, 200, 110)); }
    for (k = 0; k < 11; k++) {
        float u = k / 10.0f, y = 281.0f + k * 16.0f;
        rect(0, y, 640, y + 17.0f, COL(24 - (int)(18 * u), 160 - (int)(90 * u), 178 - (int)(86 * u)));
    }
    for (k = 0; k < 2; k++) discN(320.0f, 370.0f, 300.0f - k * 90.0f, CA(120, 235, 240, 0x08), 30);   /* luz sobre o feltro */
    for (k = 0; withBalls && k < NMB; k++) {                         /* bolas rolando na mesa */
        int id = MB_ID[k];
        discN(b[id].x + 5, b[id].y + 7, MBR * 1.05f, GS_SETREG_RGBAQ(0, 0, 0, 0x28, 0x00), 14);
        drawBallAt(id, b[id].x, b[id].y, MBR);
    }
    for (k = 0; k < 5; k++) {                                        /* vinheta lateral e de baixo */
        int a = 0x26 - k * 6;
        float d = k * 12.0f;
        rect(0, 448 - d - 12, 640, 448 - d, CA(0, 0, 8, a));
        rect(d, 0, d + 12, 448, CA(0, 0, 8, a - 6));
        rect(640 - d - 12, 0, 640 - d, 448, CA(0, 0, 8, a - 6));
    }
    if (state != S_MENU) rect(0, 0, 640, 448, CA(0, 0, 20, withBalls ? 0x26 : 0x4a));   /* sub-telas e partida: cenario mais escuro */
}

static void drawBackdrop(void) { drawBackdropEx(1); }

static void panel(float x1, float y1, float x2, float y2)
{
    int k, n = 8;
    float c = 5.0f, mid = y2 - y1 - 2.0f * c;
    u64 gold = CA(255, 215, 80, 0x7c);
    cham(x1 - 4, y1 - 2, x2 + 6, y2 + 7, 6.0f, GS_SETREG_RGBAQ(0, 0, 0, 0x28, 0x00));          /* sombra */
    cham(x1 - 2.5f, y1 - 2.5f, x2 + 2.5f, y2 + 2.5f, 6.5f, CA(255, 205, 60, 0x88));            /* borda dourada */
    cham(x1 - 1.0f, y1 - 1.0f, x2 + 1.0f, y2 + 1.0f, 5.5f, COL(22, 12, 46));                   /* aro escuro */
    rect(x1 + c, y1, x2 - c, y1 + c, CA(52, 28, 100, 0x6c));                                   /* topo (arredondado) */
    for (k = 0; k < n; k++)                                                                    /* miolo em degrade roxo -> azul-noite */
        rect(x1, y1 + c + mid * k / n, x2, y1 + c + mid * (k + 1) / n,
             CA(48 - k * 4, 26 - k, 96 - k * 6, 0x6c));
    rect(x1 + c, y2 - c, x2 - c, y2, CA(18, 14, 46, 0x6c));                                    /* base (arredondada) */
    rect(x1 + c, y1 + 1, x2 - c, y1 + 2, CA(255, 255, 255, 0x28));                             /* brilho */
    rect(x1 + 2, y1 + c, x2 - 2, y1 + c + mid * 0.18f, CA(200, 150, 255, 0x0e));
    frameR(x1 + 3, y1 + 3, x2 - 3, y2 - 3, 1.0f, CA(255, 215, 80, 0x22));                      /* filete interno */
    discN(x1 + 1, y1 + 1, 2.2f, gold, 6); discN(x2 - 1, y1 + 1, 2.2f, gold, 6);                /* pinos dourados */
    discN(x1 + 1, y2 - 1, 2.2f, gold, 6); discN(x2 - 1, y2 - 1, 2.2f, gold, 6);
}


/* ===== visual dos menus: titulo, rodape com botoes, pilulas e barras ===== */
static void uiTitle(const char *t)                    /* faixa do topo (roxo + dourado) com titulo */
{
    float sc = 0.62f, w = tw(sc, t) * 1.08f, x0 = 320.0f - w * 0.5f, sh = fmodf(menuT * 3.0f, 560.0f);
    u64 gd = COL(255, 205 + (int)(40 * sinf(menuT * 0.05f)), 60);
    int e;
    rect(0, 0, 640, 17, CA(74, 34, 130, 0x84));
    rect(0, 17, 640, 34, CA(34, 14, 70, 0x84));
    rect(0, 0, 640, 2, CA(255, 255, 255, 0x18));
    rect(0, 34, 640, 36, CA(255, 205, 60, 0x70));
    rect(40.0f + sh, 34, 90.0f + sh, 36, CA(255, 255, 220, 0x70));        /* brilho que corre */
    text(x0 + 3, 10, sc, CA(0, 0, 0, 0x70), t);
    text(x0, 7, sc, gd, t);
    rect(x0 - 96, 15, x0 - 34, 17, CA(255, 205, 60, 0x60));
    rect(x0 + w + 34, 15, x0 + w + 96, 17, CA(255, 205, 60, 0x60));
    for (e = 0; e < 2; e++) {                                             /* bolas 8 nos lados do titulo */
        float ex = e ? x0 + w + 20.0f : x0 - 20.0f;
        discN(ex, 17, 9.0f, COL(12, 12, 18), 14);
        discN(ex - 2.5f, 13.5f, 2.2f, CA(255, 255, 255, 0x50), 6);
        discN(ex, 17, 4.4f, COL(245, 245, 245), 10);
    }
}

/* botoes do controle: 0 X, 1 O, 2 TRIANGULO, 3 QUADRADO, 4 START, 5 SELECT, 6 CIMA/BAIXO, 7 ESQ/DIR, 8 L1/R1 */
static const float UIKW[9] = { 20, 20, 20, 20, 46, 52, 24, 24, 36 };
static void uiKey(float x, float y, int k)
{
    float cx = x + 10.0f, cy = y + 9.0f;
    u64 wh = COL(255,255,255), pill = COL(74,80,98);
    switch (k) {
    case 0: discN(cx, cy, 9.0f, COL(70,120,235), 14); text(cx - tw(0.34f, "X") * 0.5f, y + 2.0f, 0.34f, wh, "X"); break;
    case 1: discN(cx, cy, 9.0f, COL(220,64,64), 14);  text(cx - tw(0.34f, "O") * 0.5f, y + 2.0f, 0.34f, wh, "O"); break;
    case 2: discN(cx, cy, 9.0f, COL(40,170,110), 14); tri(cx, cy - 5, cx - 5, cy + 4, cx + 5, cy + 4, wh); break;
    case 3: discN(cx, cy, 9.0f, COL(200,90,170), 14); rect(cx - 4, cy - 4, cx + 4, cy + 4, wh); break;
    case 4: rect(x, y + 1, x + 46, y + 17, pill); text(x + 23 - tw(0.24f, "START") * 0.5f, y + 5, 0.24f, wh, "START"); break;
    case 5: rect(x, y + 1, x + 52, y + 17, pill); text(x + 26 - tw(0.22f, "SELECT") * 0.5f, y + 5, 0.22f, wh, "SELECT"); break;
    case 6: rect(x, y, x + 24, y + 18, pill);
            tri(x + 12, y + 2, x + 7, y + 8, x + 17, y + 8, wh); tri(x + 12, y + 16, x + 7, y + 10, x + 17, y + 10, wh); break;
    case 7: rect(x, y, x + 24, y + 18, pill);
            tri(x + 3, y + 9, x + 9, y + 4, x + 9, y + 14, wh); tri(x + 21, y + 9, x + 15, y + 4, x + 15, y + 14, wh); break;
    default: rect(x, y + 1, x + 36, y + 17, pill); text(x + 18 - tw(0.22f, "L1/R1") * 0.5f, y + 5, 0.22f, wh, "L1/R1"); break;
    }
}

static void uiFoot(int n, const int *ks, const char *const *ls)     /* rodape: botao + o que ele faz */
{
    float tot = 0.0f, x;
    int i;
    for (i = 0; i < n; i++) tot += UIKW[ks[i]] + 6.0f + tw(0.3f, ls[i]) + (i + 1 < n ? 18.0f : 0.0f);
    x = 320.0f - tot * 0.5f;
    rect(0, 424, 640, 448, CA(0, 0, 12, 0x58));
    rect(0, 424, 640, 425, CA(255, 205, 60, 0x44));
    for (i = 0; i < n; i++) {
        uiKey(x, 427, ks[i]);
        x += UIKW[ks[i]] + 6.0f;
        text(x, 430, 0.3f, COL(185,195,210), ls[i]);
        x += tw(0.3f, ls[i]) + 18.0f;
    }
}

static void uiToggle(float xr, float y, int on, const char *lOn, const char *lOff)   /* chave ligado/desligado */
{
    float x = xr - 90.0f;
    cham(x - 1, y - 1, xr + 1, y + 21, 6.0f, on ? COL(70,210,110) : COL(150,76,84));
    cham(x, y, xr, y + 20, 5.0f, on ? COL(24,110,58) : COL(66,30,40));
    rect(x + 5, y + 1, xr - 5, y + 8, CA(255, 255, 255, 0x14));
    if (on) { discN(xr - 11, y + 10, 8.0f, COL(235,255,240), 12); discN(xr - 11, y + 10, 5.5f, COL(110,235,150), 10); text(x + 8, y + 5, 0.26f, COL(200,255,220), lOn); }
    else    { discN(x + 11, y + 10, 8.0f, COL(235,215,218), 12); discN(x + 11, y + 10, 5.5f, COL(170,120,126), 10); text(x + 25, y + 5, 0.26f, COL(235,170,176), lOff); }
}

static void uiSection(float x1, float x2, float y, u64 c, const char *t)       /* titulo de grupo de opcoes */
{
    text(x1, y, 0.28f, c, t);
    rect(x1 + tw(0.28f, t) + 8, y + 6, x2, y + 7, CA(120, 140, 160, 0x50));
}

/* amostra de cor: kind 0 = bola branca, 1 = linha guia */
static void drawSwatch(int kind, int idx, float x, float y)
{
    if (kind == 0) {
        const u8 *c = WB_COL[idx];
        discN(x, y, 7.6f, shade(c, 0.55f, 0), 12);
        discN(x, y, 6.8f, shade(c, 1.0f, 0), 12);
        discN(x - 2.2f, y - 2.4f, 1.8f, COL(255,255,255), 6);
    } else {
        int r, gg, bb;
        if (idx == NGD - 1) hue3(menuT * 0.01f, &r, &gg, &bb);
        else { r = GD_COL[idx][0]; gg = GD_COL[idx][1]; bb = GD_COL[idx][2]; }
        rect(x - 10, y - 2.0f, x + 10, y + 2.0f, CA(r, gg, bb, 0x30));
        rect(x - 10, y - 0.7f, x + 10, y + 0.7f, COL(r, gg, bb));
    }
}

/* ---------- tela de fim de jogo: animacao de vitoria e de derrota ---------- */
static void drawTrophy(float cx, float cy, float s, int t)
{
    u64 gd = COL(255, 208, 52), dk = COL(206, 142, 22), lt = COL(255, 240, 160);
    int k;
    rect(cx - 24 * s, cy + 46 * s, cx + 24 * s, cy + 55 * s, dk);
    rect(cx - 18 * s, cy + 40 * s, cx + 18 * s, cy + 46 * s, gd);
    rect(cx - 5 * s, cy + 22 * s, cx + 5 * s, cy + 40 * s, gd);
    for (k = -1; k <= 1; k += 2) ringN(cx + k * 30 * s, cy - 14 * s, 11 * s, 3.4f * s, 16, gd);
    rect(cx - 26 * s, cy - 34 * s, cx + 26 * s, cy + 2 * s, gd);
    slice(cx, cy + 2 * s, 26 * s, 22 * s, 0.0f, CPI, gd);
    rect(cx - 29 * s, cy - 38 * s, cx + 29 * s, cy - 32 * s, lt);
    rect(cx - 20 * s, cy - 28 * s, cx - 14 * s, cy + 4 * s, lt);
    star4(cx, cy - 12 * s, 11 * s, lt);
    {   /* brilho que passa */
        float sh = fmodf(t * 0.04f, 2.0f);
        if (sh < 1.0f) beam(cx - 28 * s + sh * 56 * s, cy - 34 * s, cx - 38 * s + sh * 56 * s, cy + 4 * s, 4 * s, CA(255, 255, 255, 0x50));
    }
}


/* icones do menu principal: 0 treino, 1 jogar, 2 torneio, 3 musica, 4 opcoes */
static void menuIcon(int k, float cx, float cy, float s, int hot)
{
    u64 gd = hot ? COL(255,215,70) : COL(160,168,182);
    int q;
    switch (k) {
    case 0:                                              /* treino: alvo */
        discN(cx, cy, 14*s, hot ? COL(240,240,245) : COL(150,152,160), 22);
        discN(cx, cy, 10.5f*s, hot ? COL(225,50,50) : COL(110,112,120), 20);
        discN(cx, cy, 7*s, hot ? COL(240,240,245) : COL(150,152,160), 18);
        discN(cx, cy, 3.5f*s, hot ? COL(225,50,50) : COL(110,112,120), 14);
        break;
    case 1: {
        float sc = 0.36f * s;
        discN(cx + 1.5f*s, cy + 2*s, 13*s, CA(0, 0, 0, 0x40), 16);
        discN(cx, cy, 13*s, hot ? COL(30,90,220) : COL(70,84,120), 18);
        discN(cx, cy, 6.5f*s, COL(245,245,245), 14);
        text(cx - tw(sc, "8") * 0.5f + 1.0f, cy - 16*sc, sc, COL(10,10,14), "8");
        discN(cx - 5*s, cy - 6*s, 2.4f*s, CA(255, 255, 255, 0x70), 8);
        break; }
    case 2:
        drawTrophy(cx, cy - 2.5f*s, 0.3f*s, hot ? menuT : 0);
        break;
    case 3:
        rect(cx + 4*s, cy - 12*s, cx + 7*s, cy + 8*s, gd);
        discN(cx, cy + 9*s, 5.5f*s, gd, 12);
        tri(cx + 7*s, cy - 12*s, cx + 16*s, cy - 6*s, cx + 7*s, cy - 3*s, gd);
        break;
    case 4: {                                            /* loja: moeda */
        float sc = 0.5f * s;
        discN(cx + 1.5f*s, cy + 2*s, 13*s, CA(0, 0, 0, 0x40), 16);
        discN(cx, cy, 13*s, hot ? COL(255,205,60) : COL(120,126,140), 18);
        discN(cx, cy, 9.5f*s, hot ? COL(215,150,20) : COL(90,96,110), 16);
        text(cx - tw(sc, "$") * 0.5f, cy - 10*sc, sc, hot ? COL(255,240,150) : COL(170,176,190), "$");
        break; }
    default: {
        float rot = hot ? menuT * 0.02f : 0.0f;
        ringN(cx, cy, 8*s, 4*s, 16, gd);
        for (q = 0; q < 8; q++) {
            float a = rot + q * 0.7854f;
            discN(cx + cosf(a) * 12*s, cy + sinf(a) * 12*s, 3.2f*s, gd, 8);
        }
        break; }
    }
}

static void drawOverScreen(void)
{
    int w = winner, l = 1 - winner, k;
    int youLost = (mode == 0 && w == 1);                 /* contra o bot e o bot venceu */
    float tt = (float)overT, ent = tt < 36.0f ? tt / 36.0f : 1.0f;
    float sc = 1.2f, base = 222.0f;
    float wx = w == 0 ? 150.0f : 490.0f, lx = w == 0 ? 490.0f : 150.0f;
    float up = (1.0f - ent) * 170.0f, jump = fabsf(sinf(tt * 0.115f)) * 20.0f * ent;
    float ts;
    char buf[64];
    int gv = (charSel[w] == 16);                         /* coveiro venceu: animacao especial */
    float bp = gv ? fminf(fmaxf((tt - 70.0f) / 330.0f, 0.0f), 1.0f) : 0.0f;
    int *ws = mode == 0 ? winsBot : winsVs;
    const char *hint = tourney ? "X: CONTINUAR" : "X: REVANCHE   START: MENU";

    /* fundo */
    for (k = 0; k < 14; k++)
        rect(0, k * 32.0f, 640, k * 32.0f + 32.0f, youLost ? COL(18 + k, 20 + k, 34 + k * 2) : COL(30 + k * 2, 14 + k, 70 + k * 3));
    for (k = 0; k < 12; k += 2) {                        /* raios girando atras de quem venceu */
        float a0 = tt * 0.012f + k * CPI / 6.0f, a1 = a0 + CPI / 6.0f, cyr = base + 30.0f;
        tri(wx, cyr, wx + cosf(a0) * 380.0f, cyr + sinf(a0) * 380.0f, wx + cosf(a1) * 380.0f, cyr + sinf(a1) * 380.0f,
            youLost ? CA(255, 120, 100, 0x12) : CA(255, 225, 100, 0x1c));
    }

    if (!youLost) {                                      /* fogos de artificio */
        for (k = 0; k < 3; k++) {
            float cyc = fmodf(tt + k * 37.0f, 110.0f);
            int bi = (int)((tt + k * 37.0f) / 110.0f), q;
            float fx = 80.0f + fmodf(bi * 173.0f + k * 211.0f, 480.0f), fy = 60.0f + fmodf(bi * 89.0f + k * 53.0f, 110.0f);
            int al = 0x70 - (int)(cyc * 0.62f);
            for (q = 0; q < 14; q++) {
                float a = q * 6.2831853f / 14.0f, r = cyc * 1.7f, g = cyc * cyc * 0.004f;
                float px = fx + cosf(a) * r, py = fy + sinf(a) * r + g;
                int r8, g8, b8;
                hue3(k * 0.3f + bi * 0.17f, &r8, &g8, &b8);
                beam(fx + cosf(a) * (r - 9.0f), fy + sinf(a) * (r - 9.0f) + g * 0.8f, px, py, 1.6f, CA(r8, g8, b8, al));
                discN(px, py, 2.2f, CA(255, 255, 255, al), 6);
            }
        }
        for (k = 0; k < 64; k++) {                       /* confete */
            float px = fmodf(k * 61.0f + 14.0f * sinf(tt * 0.04f + k), 660.0f) - 10.0f;
            float py = fmodf(tt * (1.3f + (k % 5) * 0.45f) + k * 47.0f, 470.0f) - 14.0f;
            float a = tt * 0.15f + k, ux = cosf(a) * 4.0f, uy = sinf(a) * 4.0f, vx = -sinf(a) * 2.0f, vy = cosf(a) * 2.0f;
            quad(px - ux - vx, py - uy - vy, px + ux - vx, py + uy - vy, px + ux + vx, py + uy + vy, px - ux + vx, py - uy + vy,
                 hueCol(k * 0.071f, 0x78));
        }
    }

    /* titulo e legendas */
    if (youLost) strcpy(buf, "DERROTA");
    else if (tourney && tRound == 2) strcpy(buf, "CAMPEAO!");
    else if (mode == 0) strcpy(buf, "VITORIA!");
    else sprintf(buf, "%s VENCEU!", pname(w));
    ts = (mode == 1 ? 0.7f : 0.95f) + 0.03f * sinf(tt * 0.15f);
    {
        float tx = 320.0f - tw(ts, buf) * 0.5f, ty = 22.0f - (1.0f - ent) * 60.0f;
        u64 gl = youLost ? CA(90, 20, 20, 0x50) : CA(150, 80, 0, 0x50);
        text(tx - 2, ty - 2, ts, gl, buf); text(tx + 2, ty - 2, ts, gl, buf);
        text(tx - 2, ty + 2, ts, gl, buf); text(tx + 2, ty + 3, ts, gl, buf);
        text(tx, ty, ts, youLost ? COL(255,110,100) : COL(255,230,70), buf);
    }
    if (tourney) { if (youLost) sprintf(buf, "%s VENCEU... VOCE ESTA ELIMINADO!", tName[tOpp]); else sprintf(buf, "VOCE VENCEU %s!", tName[tOpp]); }
    else if (mode == 0) strcpy(buf, youLost ? "O BOT VENCEU... TENTE DE NOVO!" : "VOCE VENCEU O BOT!");
    else sprintf(buf, "PARABENS, %s!", pname(w));
    textC(62, 0.4f, youLost ? COL(255,190,180) : COL(255,245,190), buf);
    if (overNote[0]) textC(82, 0.32f, COL(255,150,130), overNote);

    /* sombras no chao */
    ell(wx, base + 108 * sc, 36 * sc * (1.0f - jump * 0.012f), 6 * sc, CA(0, 0, 0, 0x38));
    ell(lx, base + 108 * sc, 36 * sc, 6 * sc, CA(0, 0, 0, 0x38));

    /* quem perdeu: cabisbaixo, chorando, com nuvem de chuva */
    drawChar(charSel[l], lx, base + 8.0f + up * 0.6f + sinf(tt * 0.35f) * 1.5f + (gv ? bp * 70.0f : 0.0f), sc, 2, overT, 1);
    {
        float cyl = 112.0f + 3.0f * sinf(tt * 0.05f);
        for (k = 0; k < 5; k++) {
            float ox = (k - 2) * 20.0f, oy = (k == 1 || k == 3) ? -8.0f : 0.0f;
            discN(lx + ox, cyl + oy, 20.0f + (k == 2 ? 4.0f : 0.0f), COL(110, 116, 136), 12);
        }
        rect(lx - 46, cyl, lx + 46, cyl + 12, COL(110, 116, 136));
        for (k = 0; k < 5; k++) discN(lx + (k - 2) * 20.0f, cyl + 12, 10.0f, COL(110, 116, 136), 8);
        for (k = 0; k < 32; k++) {
            float rx = lx - 46.0f + fmodf(k * 23.0f, 92.0f), ry = cyl + 24.0f + fmodf(tt * 4.5f + k * 31.0f, 220.0f);
            line(rx, ry, rx - 2.0f, ry + 9.0f, CA(150, 190, 255, 0x78));
        }
    }

    /* quem venceu: pula, braços pro alto */
    if (gv) {                                            /* COVEIRO: joga areia e enterra o adversario */
        float g = base + 108.0f * sc, dir = lx > wx ? 1.0f : -1.0f, hh = bp * 190.0f * sc, rm = hh > 0.0f ? 1.0f : 0.0f;
        float sx = wx + dir * 34.0f * sc, sy = base - 6.0f * sc + sinf(tt * 0.5f) * 8.0f;
        int i, n = 36;
        drawChar(charSel[w], wx, base - jump * 0.25f + up + sinf(tt * 0.5f) * 3.0f, sc, 1, overT, 1);
        /* monte de terra crescendo sobre o perdedor */
        for (i = 0; i < 60 && rm > 0.0f; i++) {
            float f = (float)i / 60.0f, y1 = g - hh * f, y0 = g - hh * (f + 1.0f / 60.0f);
            float hw = (34.0f + 40.0f * (1.0f - f * f)) * sc * (0.35f + 0.65f * bp + 0.0f);
            u64 cc = (i & 3) == 0 ? COL(120, 84, 48) : COL(150, 108, 62);
            rect(lx - hw, y0, lx + hw, y1 + 1.0f, cc);
            rect(lx - hw, y0, lx - hw + 4.0f, y1 + 1.0f, COL(104, 72, 40));
        }
        if (rm > 0.0f) { discN(lx, g - hh, (34.0f * sc) * (0.35f + 0.65f * bp), COL(150, 108, 62), 14); }
        for (i = 0; i < 12; i++) rect(lx - 70 * sc + i * 12.0f * sc, g - 2.0f - (i % 3) * 2.0f, lx - 62 * sc + i * 12.0f * sc, g + 2.0f, COL(130, 92, 54));
        /* areia voando em arcos */
        if (tt > 40.0f && bp < 1.0f) {
            for (i = 0; i < n; i++) {
                float p = fmodf(tt * 0.035f + (float)i / n, 1.0f);
                float ex = lx + sinf(i * 2.3f) * 26.0f, ey = g - 40.0f - fmodf(i * 17.0f, 80.0f);
                float px = sx + (ex - sx) * p, py = sy + (ey - sy) * p - sinf(p * 3.14159f) * 95.0f;
                discN(px, py, 2.0f + (i % 3), (i & 1) ? COL(224, 188, 108) : COL(196, 158, 88), 6);
            }
            for (i = 0; i < 8; i++) {                    /* poeira no impacto */
                float p = fmodf(tt * 0.05f + i * 0.125f, 1.0f);
                discN(lx + sinf(i * 1.7f) * 40.0f, g - 20.0f - p * 40.0f, 3.0f + p * 7.0f, CA(200, 170, 110, (int)(0x50 * (1.0f - p))), 8);
            }
        }
        /* cruz no tumulo quando termina */
        if (bp >= 1.0f) {
            float cy2 = g - hh - 4.0f, bob = sinf(tt * 0.1f) * 1.5f;
            rect(lx - 3.0f * sc, cy2 - 46.0f * sc, lx + 3.0f * sc, cy2 + 4.0f, COL(110, 76, 40));
            rect(lx - 16.0f * sc, cy2 - 34.0f * sc, lx + 16.0f * sc, cy2 - 28.0f * sc, COL(110, 76, 40));
            for (i = 0; i < 3; i++) {                    /* fiapos verdes do alem */
                float ph = fmodf(tt * 0.02f + i * 0.33f, 1.0f);
                discN(lx + sinf(tt * 0.06f + i * 2.0f) * 14.0f, cy2 - 50.0f * sc - ph * 50.0f + bob, 4.0f * (1.0f - ph) + 1.0f, CA(150, 255, 170, (int)(0x70 * (1.0f - ph))), 8);
            }
            textC(150, 0.5f, COL(190, 255, 190), "DESCANSE EM PAZ");
        }
    } else
    drawChar(charSel[w], wx, base - jump + up, sc, 1, overT, 1);

    /* trofeu no centro com brilhos */
    drawTrophy(320.0f, 206.0f + 4.0f * sinf(tt * 0.08f) + up * 0.5f, 1.0f, overT);
    for (k = 0; k < 6; k++) {
        float a = tt * 0.05f + k * 1.047f;
        star4(320.0f + cosf(a) * 54.0f, 206.0f + sinf(a) * 46.0f, 3.0f + 2.0f * sinf(tt * 0.2f + k),
              youLost ? CA(200, 210, 230, 0x50) : COL(255, 245, 170));
    }

    /* nomes, placar e dica */
    sprintf(buf, "%s", pname(w));
    text(wx - tw(0.4f, buf) * 0.5f, 358, 0.4f, COL(255,235,110), buf);
    sprintf(buf, "%s", pname(l));
    text(lx - tw(0.4f, buf) * 0.5f, 358, 0.4f, COL(150,158,175), buf);
    if (tourney) sprintf(buf, "TORNEIO - %s", tRoundName(tRound));
    else sprintf(buf, "%s %d X %d %s", pname(0), ws[0], ws[1], pname(1));
    textC(380, 0.5f, COL(255,255,255), buf);
    if (shLastEarn > 0) {                                /* LOJA: moedas ganhas */
        char eb[32];
        sprintf(eb, "+%d MOEDAS", shLastEarn);
        textC(388, 0.38f, COL(255,225,100), eb);
    }
    textC(410, 0.36f, ((overT / 25) & 1) ? COL(255,255,255) : COL(200,205,215), hint);

    if (tt < 36.0f) rect(0, 0, 640, 448, CA(255, 255, 255, (int)(0x50 * (1.0f - ent))));   /* flash de entrada */
}

/* ---------- configuracao da partida e nome do jogador 2 ---------- */
enum { SR_OPP, SR_LVL, SR_TIME, SR_MODE, SR_TABLE, SR_BALLS, SR_C0, SR_C1, SR_CH0, SR_CH1, SR_WB, SR_GD, SR_GO, SR_NEXT, SR_ABIL, SR_TAUTO, SR_RPL, NSR };
#define SOLO (tourney || training)          /* so um jogador escolhe personagem (torneio e treino) */
#define NSTEPS 4                          /* configurar partida em etapas: 0 partida, 1 personagens, 2 mesa e bolas, 3 bola branca e guia */
static int setStep;
static int oppSel, setSel;                 /* adversario: 0 = BOT, 1 = outro jogador */
static char nameBuf[12];
static int nameLen, nameCur, nameWho;      /* nameWho: 0 = jogador 1, 1 = jogador 2 */
static const char KBCH[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static int setRows(int *v)                 /* linhas visiveis da etapa atual */
{
    int n = 0;
    switch (setStep) {
    case 0:
        if (!tourney && !training) v[n++] = SR_OPP;
        if (oppSel == 0 && !training) v[n++] = SR_LVL;
        if (oppSel == 1 || tourney) { v[n++] = SR_TIME; v[n++] = SR_ABIL; }   /* tempo e habilidades: versus e torneio */
        v[n++] = SR_MODE;
        if (repHave) v[n++] = SR_RPL;                            /* so aparece quando ha replay da partida anterior */
        v[n++] = SR_NEXT;
        break;
    case 1:
        v[n++] = SR_CH0; if (!SOLO) v[n++] = SR_CH1; v[n++] = SR_NEXT;
        break;
    case 2:
        if (!tourney) { v[n++] = SR_TABLE; v[n++] = SR_TAUTO; }      /* torneio: a mesa muda a cada partida */
        if (gmode == GM_BR) { v[n++] = SR_C0; v[n++] = SR_C1; }
        else                { v[n++] = SR_BALLS; }
        v[n++] = SR_NEXT;
        break;
    default:
        v[n++] = SR_WB; v[n++] = SR_GD; v[n++] = SR_GO;
        break;
    }
    return n;
}

static void cycleColor(int pl, int d)      /* cada jogador usa uma cor diferente */
{
    int o = colSel[1 - pl];
    do { colSel[pl] = (colSel[pl] + d + NBRC) % NBRC; } while (colSel[pl] == o);
}

/* ordem de exibicao: personagens por raridade (do mais comum ao mais raro; empate: ordem alfabetica) e mesas em ordem alfabetica.
   so muda a ordem em que aparecem/rodam nos menus: os numeros internos (CHAR_*, TABLES) continuam os mesmos */
static int csOrd[NCHARS], csInv[NCHARS], csOrdOk;        /* casa da grade -> personagem, e o inverso */
static int tabOrd[NTABLES], tabInv[NTABLES], tabOrdOk;   /* posicao na lista -> mesa, e o inverso */
static void csOrdInit(void)
{
    int i, j;
    if (csOrdOk) return;
    for (i = 0; i < NCHARS; i++) csOrd[i] = i;
    for (i = 1; i < NCHARS; i++) {                        /* insercao: estavel e simples */
        int c = csOrd[i];
        for (j = i - 1; j >= 0; j--) {
            int o = csOrd[j];
            if (CHAR_RAR[o] < CHAR_RAR[c] || (CHAR_RAR[o] == CHAR_RAR[c] && strcmp(CHAR_NAME[o], CHAR_NAME[c]) <= 0)) break;
            csOrd[j + 1] = o;
        }
        csOrd[j + 1] = c;
    }
    for (i = 0; i < NCHARS; i++) csInv[csOrd[i]] = i;
    csOrdOk = 1;
}
static void tabOrdInit(void)
{
    int i, j;
    if (tabOrdOk) return;
    for (i = 0; i < NTABLES; i++) tabOrd[i] = i;
    for (i = 1; i < NTABLES; i++) {
        int t = tabOrd[i];
        for (j = i - 1; j >= 0 && strcmp(TABLES[tabOrd[j]].name, TABLES[t].name) > 0; j--) tabOrd[j + 1] = tabOrd[j];
        tabOrd[j + 1] = t;
    }
    for (i = 0; i < NTABLES; i++) tabInv[tabOrd[i]] = i;
    tabOrdOk = 1;
}

static void cycleChar(int pl, int d)       /* cada jogador usa um personagem diferente */
{
    int o = charSel[1 - pl], v;
    csOrdInit();
    v = charRand[pl] ? NCHARS : csInv[charSel[pl]];                     /* posicao na lista por raridade; NCHARS = ALEATORIO */
    do { v = (v + d + NCHARS + 1) % (NCHARS + 1); } while (v < NCHARS && csOrd[v] == o && !charRand[1 - pl]);
    charRand[pl] = (v == NCHARS);
    if (!charRand[pl]) charSel[pl] = csOrd[v];
    if (pl == 0 && !charRand[0] && (charSel[0] == 16 || charSel[0] == 17)) tableAuto = 1;      /* COVEIRO: usa a mesa Cemiterio (pode desligar em MESA DO PERSONAGEM) */
}

/* ---------- torneio mata-mata ---------- */
static const char *TBOT[12] = { "BRUNO", "LUCAO", "TONHO", "DUDU", "MARCOS", "PEDRAO", "ZECA", "CARLAO", "NANDO", "TIAGO", "JUNIOR", "RAFA" };

static void tourneyPrepare(void)                 /* simula as outras partidas da rodada e acha o rival do jogador */
{
    int n = 8 >> tRound, m, t;
    do { t = (int)(rnd() % NTABLES); } while (t == tTable);   /* a mesa muda a cada partida do torneio */
    tTable = t;
    for (m = 0; m < n / 2; m++) {
        int a = tSlot[tRound][2 * m], c = tSlot[tRound][2 * m + 1];
        if (a == 0 || c == 0) { tOpp = (a == 0) ? c : a; tPM = m; }
        else tSlot[tRound + 1][m] = (rnd() & 1) ? a : c;
    }
}

static void tourneyBegin(void)                   /* sorteia rivais, personagens e chaves */
{
    int i, j, t, n = 0, ids[NCHARS], nm[12], ord[8];
    for (i = 0; i < 4; i++) for (j = 0; j < 8; j++) tSlot[i][j] = -1;
    for (i = 0; i < NCHARS; i++) if (i != charSel[0]) ids[n++] = i;
    for (i = n - 1; i > 0; i--) { j = (int)(rnd() % (unsigned)(i + 1)); t = ids[i]; ids[i] = ids[j]; ids[j] = t; }
    for (i = 0; i < 12; i++) nm[i] = i;
    for (i = 11; i > 0; i--) { j = (int)(rnd() % (unsigned)(i + 1)); t = nm[i]; nm[i] = nm[j]; nm[j] = t; }
    tChar[0] = charSel[0]; strcpy(tName[0], pnames[0]);
    for (i = 1; i < 8; i++) { tChar[i] = ids[i - 1]; strcpy(tName[i], TBOT[nm[i - 1]]); }
    for (i = 0; i < 8; i++) ord[i] = i;
    for (i = 7; i > 0; i--) { j = (int)(rnd() % (unsigned)(i + 1)); t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    for (i = 0; i < 8; i++) tSlot[0][i] = ord[i];
    tRound = 0; tRes = 0; tChamp = -1; mode = 0; tTable = -1;
    cueSel[0] = (tAbil && CHAR_CUE[charSel[0]] >= 0) ? CHAR_CUE[charSel[0]] : 0;   /* regra: so o taco CLASSICO (ou, com habilidades, o taco do seu personagem) */
    cueSel[1] = 0;
    tourneyPrepare();
    state = S_BRACKET;
}

static void tourneyPlay(void)                    /* comeca a partida da rodada atual */
{
    charSel[1] = tChar[tOpp];
    if (!tAbil) cueSel[0] = 0;
    cueSel[1] = 0;
    rs += 7919 + menuT;
    newGame(0);
}

static void tourneyAfter(void)                   /* chamado ao sair da tela de fim de partida */
{
    if (winner == 0) {
        tSlot[tRound + 1][tPM] = 0;
        if (tRound == 2) { tChamp = 0; tRes = 3; }
        else { tRound++; tourneyPrepare(); tRes = 1; }
    } else {
        int r, m;
        tSlot[tRound + 1][tPM] = tOpp;
        for (r = tRound + 1; r < 3; r++)         /* eliminado: o resto do torneio e simulado */
            for (m = 0; m < (8 >> r) / 2; m++)
                tSlot[r + 1][m] = (rnd() & 1) ? tSlot[r][2 * m] : tSlot[r][2 * m + 1];
        tChamp = tSlot[3][0]; tRes = 2;
    }
    state = S_BRACKET;
}

static void bracketInput(u32 pr)
{
    if (pr & (PAD_CROSS | PAD_START)) {
        if (tRes <= 1) tourneyPlay();
        else { tourney = 0; menuInit(); state = S_MENU; }
    } else if (pr & PAD_SELECT) { tourney = 0; menuInit(); state = S_MENU; }
}

/* ---------- SELETOR DE PERSONAGEM (etapa 1): grade de retratos, cursor por jogador, pedestais e PRONTO ---------- */
#define CS_COLS 7
#define CS_ROWS 3
#define CS_RND  NCHARS                                /* casa do ALEATORIO (depois dos personagens; as ultimas casas ficam vazias) */
static int csStage, csReady[2], csPos, csInit;        /* csStage: 0 jogador 1 escolhe, 1 jogador 2 (ou BOT) escolhe, 2 os dois prontos */
static const u8 CSC[2][3] = { {255,215,60}, {90,220,255} };   /* cor de cada jogador: dourado e ciano */

static int csSlot(int pl) { csOrdInit(); return charRand[pl] ? CS_RND : csInv[charSel[pl]]; }
static int csChooser(void) { return csStage < 2 ? csStage : (SOLO ? 0 : 1); }
static int csTaken(int pl, int slot)                  /* o outro jogador ja travou este personagem? */
{
    int o = 1 - pl;
    csOrdInit();
    return !SOLO && slot < NCHARS && csReady[o] && !charRand[o] && charSel[o] == csOrd[slot];
}
static void csEnsure(void)
{
    if (csInit) return;
    csInit = 1; csStage = 0; csReady[0] = csReady[1] = 0; csPos = csSlot(0);
}
static int csMove(int pos, int dx, int dy)            /* anda pela grade pulando as casas vazias */
{
    int r = pos / CS_COLS, c = pos % CS_COLS, n;
    for (n = 0; n < 24; n++) {
        c = (c + dx + CS_COLS) % CS_COLS; r = (r + dy + CS_ROWS) % CS_ROWS;
        if (r * CS_COLS + c <= CS_RND) return r * CS_COLS + c;
    }
    return pos;
}
static void csApply(int pl)                           /* aplica a casa do cursor ao jogador pl */
{
    if (csPos == CS_RND) charRand[pl] = 1;
    else if (!csTaken(pl, csPos)) { csOrdInit(); charRand[pl] = 0; charSel[pl] = csOrd[csPos]; }
    if (pl == 0 && !charRand[0] && (charSel[0] == 16 || charSel[0] == 17)) tableAuto = 1;      /* COVEIRO: usa a mesa Cemiterio */
    if (pl == 0 && !SOLO && !charRand[0] && !charRand[1] && charSel[0] == charSel[1]) {     /* o 2o jogador ainda nao escolheu: sai da frente */
        int c = charSel[1];
        do { c = (c + 1) % NCHARS; } while (c == charSel[0]);
        charSel[1] = c;
    }
}
static void csInput(u32 pr)
{
    int pl, dx = ((pr & PAD_RIGHT) ? 1 : 0) - ((pr & PAD_LEFT) ? 1 : 0), dy = ((pr & PAD_DOWN) ? 1 : 0) - ((pr & PAD_UP) ? 1 : 0);
    csEnsure();
    pl = csChooser();
    if (pr & PAD_L1) { setStep = 0; setSel = 0; return; }
    if (pr & (PAD_R1 | PAD_START)) { setStep = 2; setSel = 0; return; }
    if (pr & PAD_CIRCLE) {                                      /* O: destrava o ultimo jogador (no comeco, volta uma etapa) */
        if (csStage == 0) { setStep = 0; setSel = 0; return; }
        csStage = (csStage == 2) ? (SOLO ? 0 : 1) : 0;
        csReady[csStage] = 0; csPos = csSlot(csStage);
        return;
    }
    if (csStage == 2) { if (pr & PAD_CROSS) { setStep = 2; setSel = 0; } return; }
    if (dx || dy) { csPos = csMove(csPos, dx, dy); csApply(pl); }
    if (pr & PAD_CROSS) {
        if (csTaken(pl, csPos)) return;
        csApply(pl);
        csReady[pl] = 1;
        if (pl == 0 && !SOLO) { csStage = 1; csPos = csSlot(1); } else csStage = 2;
    }
}

static void resolveRandoms(void)                /* ALEATORIO: sorteia personagens (diferentes entre si) e a mesa */
{
    int pl;
    for (pl = 0; pl < 2; pl++) {
        int o = charSel[1 - pl], c, tries = 0;
        if (!charRand[pl]) continue;
        do { c = (int)(rnd() % NCHARS); } while (c == o && ++tries < 60);
        charSel[pl] = c;
    }
    if (tableRandom && !tableAuto) tableSel = (int)(rnd() % NTABLES);
}

static void startMatch(void)
{
    rs += 7919 + menuT;
    resolveRandoms();
    wbSel[1] = wbSel[0];                         /* a cor da bola branca vale para a partida toda */
    setSkins(gmode == GM_BR);
    abilOn = (oppSel == 0) ? 1 : !abilSel;
    if (tableAuto && CHAR_TAB[charSel[0]] >= 0) tableSel = CHAR_TAB[charSel[0]];
    if (tourney) { tourneyBegin(); return; }
    newGame(oppSel);
}

static void nameOpen(int who)              /* abre o teclado na tela para o nome de um jogador */
{
    nameWho = who; nameCur = 0; state = S_NAME;
    if (strcmp(pnames[who], who ? "JOGADOR 2" : "JOGADOR 1") != 0) {      /* mantem o nome ja digitado */
        strncpy(nameBuf, pnames[who], 10); nameBuf[10] = 0; nameLen = (int)strlen(nameBuf);
    } else { nameBuf[0] = 0; nameLen = 0; }
}

static void confirmName(void)
{
    int n = nameLen;
    while (n > 0 && nameBuf[n - 1] == ' ') n--;
    nameBuf[n] = 0; nameLen = n;
    strcpy(pnames[nameWho], n > 0 ? nameBuf : (nameWho ? "JOGADOR 2" : "JOGADOR 1"));
    if (nameWho == 0 && oppSel == 1) { nameOpen(1); return; }       /* versus: agora o nome do jogador 2 */
    startMatch();
}

static void setupInput(u32 pr)
{
    int v[NSR], n, d = 0, row;
    if (setStep != 1) csInit = 0;
    if (setStep == 1) { csInput(pr); return; }          /* etapa 1: seletor de personagem em grade */
    n = setRows(v);
    if (pr & PAD_UP)    setSel = (setSel + n - 1) % n;
    if (pr & PAD_DOWN)  setSel = (setSel + 1) % n;
    if (pr & PAD_LEFT)  d = -1;
    if (pr & PAD_RIGHT) d = 1;
    if (pr & PAD_CROSS) d = 1;
    if (pr & PAD_CIRCLE) {                                       /* O: etapa anterior (na primeira, volta ao menu) */
        if (setStep > 0) { setStep--; setSel = 0; } else state = S_MENU;
        return;
    }
    if (pr & PAD_R1) { if (setStep < NSTEPS - 1) { setStep++; setSel = 0; } return; }
    if (pr & PAD_L1) { if (setStep > 0) { setStep--; setSel = 0; } return; }
    row = v[setSel];
    if (row == SR_RPL && (pr & PAD_CROSS)) { repWatchPrev(); return; }                       /* X: assiste o replay da partida anterior */
    if (row == SR_NEXT && (pr & (PAD_CROSS | PAD_START))) { setStep++; setSel = 0; return; }
    if (row == SR_GO && (pr & (PAD_CROSS | PAD_START))) { nameOpen(0); return; }   /* abre a tela para digitar o nome */
    if (pr & PAD_START) {
        if (setStep < NSTEPS - 1) { setStep++; setSel = 0; } else nameOpen(0);
        return;
    }
    if (d == 0) return;
    switch (row) {
    case SR_OPP:   oppSel = 1 - oppSel; break;
    case SR_LVL:   botLevel = (botLevel + d + NLEVELS) % NLEVELS; break;
    case SR_TIME:  timeSel = (timeSel + d + NTIMES) % NTIMES; break;
    case SR_MODE:  gmode = (gmode + d + NGM) % NGM; break;
    case SR_TABLE: { int v; tabOrdInit(); v = tableRandom ? NTABLES : tabInv[tableSel]; v = (v + d + NTABLES + 1) % (NTABLES + 1); tableRandom = (v == NTABLES); if (!tableRandom) tableSel = tabOrd[v]; tableAuto = 0; } break;   /* ordem alfabetica; ultima opcao: ALEATORIA */
    case SR_ABIL:  if (tourney) tAbil = !tAbil; else abilSel = !abilSel; break;
    case SR_TAUTO: tableAuto = !tableAuto; break;
    case SR_BALLS: ballTheme = (ballTheme + d + NTHEMES) % NTHEMES; break;
    case SR_C0:    cycleColor(0, d); break;
    case SR_C1:    cycleColor(1, d); break;
    case SR_CH0:   cycleChar(0, d); break;
    case SR_CH1:   cycleChar(1, d); break;
    case SR_WB:    wbSel[0] = wbSel[1] = (wbSel[0] + d + NWB) % NWB; break;
    case SR_GD:    gdSel[0] = gdSel[1] = (gdSel[0] + d + NGD) % NGD; break;
    default: break;
    }
    n = setRows(v);
    if (setSel >= n) setSel = n - 1;
}

static void nameInput(u32 pr)
{
    if (pr & PAD_LEFT)  nameCur = (nameCur % 10 == 0) ? nameCur + 9 : nameCur - 1;
    if (pr & PAD_RIGHT) nameCur = (nameCur % 10 == 9) ? nameCur - 9 : nameCur + 1;
    if (pr & PAD_UP)    nameCur = (nameCur + 30) % 40;
    if (pr & PAD_DOWN)  nameCur = (nameCur + 10) % 40;
    if (pr & PAD_SELECT) { if (nameWho == 1) nameOpen(0); else state = S_SETUP; return; }
    if (pr & PAD_START)  { confirmName(); return; }
    if (pr & PAD_CIRCLE) { if (nameLen > 0) nameBuf[--nameLen] = 0; }
    if (pr & PAD_CROSS) {
        if (nameCur >= 38) { confirmName(); return; }
        if (nameCur == 37) { if (nameLen > 0) nameBuf[--nameLen] = 0; }
        else if (nameLen < 10) {
            nameBuf[nameLen++] = (nameCur == 36) ? ' ' : KBCH[nameCur];
            nameBuf[nameLen] = 0;
        }
    }
}

static void selBar(float x1, float x2, float y, float h);

static void rarRGB(int r, int *R3, int *G3, int *B3)                /* mesma cor do rarCol, em numeros */
{
    static const unsigned char RC[5][3] = { {120,190,255}, {110,230,150}, {200,120,255}, {255,110,190}, {255,215,70} };
    if (r >= 0 && r < 5) { *R3 = RC[r][0]; *G3 = RC[r][1]; *B3 = RC[r][2]; }
    else hue3(menuT * 0.01f, R3, G3, B3);
}

static void csTile(int slot, float x, float y)
{
    const float w = 62.0f, h = 50.0f;
    int i, ch = csChooser(), isCur = (slot == csPos && csStage < 2), own = -1, col;
    float pu = 0.5f + 0.5f * sinf(menuT * 0.2f);
    u64 bc;
    int rr = 96, rg = 104, rb = 122;                              /* cor da carta: a da raridade (ou cinza se vazia) */
    for (i = 0; i < (SOLO ? 1 : 2); i++) if (csReady[i] && slot == csSlot(i)) own = i;
    col = isCur ? ch : own;
    if (isCur) { y -= 3.0f; }                                     /* a carta escolhida sobe um pouco */
    csOrdInit();
    if (slot < NCHARS) rarRGB(CHAR_RAR[csOrd[slot]], &rr, &rg, &rb);
    if (col >= 0) {                                               /* brilho em volta da carta */
        int a = isCur ? 0x28 + (int)(0x28 * pu) : 0x30;
        outlineRect(x - 4.0f, y - 4.0f, x + w + 4.0f, y + h + 4.0f, CA(CSC[col][0], CSC[col][1], CSC[col][2], a / 2));
        outlineRect(x - 3.0f, y - 3.0f, x + w + 3.0f, y + h + 3.0f, CA(CSC[col][0], CSC[col][1], CSC[col][2], a));
        outlineRect(x - 2.0f, y - 2.0f, x + w + 2.0f, y + h + 2.0f, CA(CSC[col][0], CSC[col][1], CSC[col][2], a + 0x10));
    }
    bc = col >= 0 ? COL(CSC[col][0], CSC[col][1], CSC[col][2]) : COL(rr / 2 + 30, rg / 2 + 30, rb / 2 + 30);
    rect(x + 2, y + 4, x + w + 2, y + h + 4, CA(0, 0, 0, 0x34));                              /* sombra */
    cham(x - 1.5f, y - 1.5f, x + w + 1.5f, y + h + 1.5f, 5.0f, bc);                          /* borda */
    cham(x, y, x + w, y + h, 4.0f, COL(rr / 7 + 6, rg / 7 + 6, rb / 7 + 12));                /* fundo escuro com tom da raridade */
    rect(x + 3, y + h * 0.55f, x + w - 3, y + h - 3, CA(rr, rg, rb, 0x1c));                  /* degrade de baixo na cor da raridade */
    rect(x + 4, y + 1, x + w - 4, y + 3, CA(255, 255, 255, 0x22));                           /* brilho no topo */
    if (slot < NCHARS) {
        discN(x + w * 0.5f, y + 22.0f, 17.0f, CA(rr, rg, rb, 0x20), 14);                     /* aura atras do rosto */
        drawChar(csOrd[slot], x + w * 0.5f, y + 30.0f, 0.5f, isCur ? 1 : 0, menuT, 0);
        rect(x + 6.0f, y + h - 6.0f, x + w - 6.0f, y + h - 4.0f, rarCol(CHAR_RAR[csOrd[slot]]));      /* faixa da raridade */
    } else if (slot == CS_RND) {
        text(x + w * 0.5f - tw(0.9f, "?") * 0.5f, y + 6.0f, 0.9f, COL(255,230,120), "?");
        text(x + w * 0.5f - tw(0.22f, "ALEATORIO") * 0.5f, y + h - 12.0f, 0.22f, COL(215,220,232), "ALEATORIO");
    } else {
        text(x + w * 0.5f - tw(0.9f, "?") * 0.5f, y + 6.0f, 0.9f, COL(60,56,86), "?");
    }
    if (isCur && slot < NCHARS && csTaken(ch, slot)) {
        rect(x + 1.0f, y + 1.0f, x + w - 1.0f, y + h - 1.0f, CA(0, 0, 0, 0x50));
        text(x + w * 0.5f - tw(0.24f, "EM USO") * 0.5f, y + 18.0f, 0.24f, COL(255,120,110), "EM USO");
    }
    if (col >= 0) {                                               /* etiqueta: de quem e a carta */
        const char *tg = (col == 1 && oppSel == 0) ? "BOT" : (col == 1 ? "P2" : "P1");
        float tgw = tw(0.24f, tg) + 8.0f;
        cham(x + 1, y + 1, x + 1 + tgw, y + 13, 3.0f, COL(CSC[col][0], CSC[col][1], CSC[col][2]));
        text(x + 5.0f, y + 3.0f, 0.24f, COL(10, 12, 24), tg);
    }
}

static void csPed(int pl, float px)                               /* pedestal: o personagem escolhido, nome e situacao */
{
    int ci = charRand[pl] ? (menuT / 90) % NCHARS : charSel[pl];
    int hot = (csStage < 2 && csChooser() == pl), rdy = csReady[pl];
    float bob = hot ? -3.0f * fabsf(sinf(menuT * 0.15f)) : 0.0f;
    int on = hot || rdy;
    u64 ac = on ? COL(CSC[pl][0], CSC[pl][1], CSC[pl][2]) : COL(70,80,104);
    const char *nm = charRand[pl] ? "ALEATORIO" : CHAR_NAME[ci], *st;
    const char *who = (pl == 1 && oppSel == 0) ? "BOT" : pnames[pl];
    u64 sc;
    int k;
    if (on) {                                                    /* holofote: cone de luz + aura no chao */
        float pu = 0.5f + 0.5f * sinf(menuT * 0.1f);
        quad(px - 12.0f, 268.0f, px + 12.0f, 268.0f, px + 62.0f, 392.0f, px - 62.0f, 392.0f, CA(CSC[pl][0], CSC[pl][1], CSC[pl][2], 0x0c + (int)(6 * pu)));
        for (k = 0; k < 3; k++) discN(px, 386.0f, 70.0f - k * 18.0f, CA(CSC[pl][0], CSC[pl][1], CSC[pl][2], 0x0a), 20);
    }
    ell(px, 396.0f, 50.0f, 9.0f, CA(0, 0, 0, 0x50));             /* sombra no chao */
    ell(px, 393.0f, 46.0f, 8.0f, COL(26,30,48));                 /* plataforma */
    ell(px, 391.0f, 42.0f, 6.0f, ac);
    ell(px, 390.0f, 36.0f, 4.0f, COL(18,22,38));
    drawChar(ci, px, 306.0f + bob, 0.74f, on ? 1 : 0, menuT, 1);
    {                                                            /* placa do jogador */
        float pw = tw(0.3f, who) + 22.0f;
        hbox(px - pw * 0.5f, 264.0f, px + pw * 0.5f, 280.0f, on ? CA(CSC[pl][0], CSC[pl][1], CSC[pl][2], 0xa0) : CA(70, 78, 96, 0x60), CA(8, 12, 22, 0x78));
        text(px - tw(0.3f, who) * 0.5f, 267.0f, 0.3f, ac, who);
    }
    text(px - tw(0.34f, nm) * 0.5f + 1.5f, 400.0f, 0.34f, CA(0, 0, 0, 0x70), nm);
    text(px - tw(0.34f, nm) * 0.5f, 399.0f, 0.34f, on ? COL(255,240,150) : COL(175,185,195), nm);
    if (rdy)      { st = "PRONTO!";            sc = COL(120,255,150); }
    else if (hot) { st = "ESCOLHENDO...";      sc = ((menuT / 20) & 1) ? COL(255,230,90) : COL(190,160,50); }
    else          { st = pl == 1 && oppSel == 0 ? "BOT" : "AGUARDANDO"; sc = COL(120,130,148); }
    hbox(px - 48.0f, 409.0f, px + 48.0f, 421.0f, rdy ? COL(90,200,120) : (hot ? COL(255,215,60) : COL(70,78,96)), CA(8, 14, 28, 0x80));
    text(px - tw(0.24f, st) * 0.5f, 412.0f, 0.24f, sc, st);
}

static void drawCharSelect(void)
{
    const float tw0 = 62.0f, th0 = 50.0f, gx = 12.0f, gy = 8.0f;
    float x0 = 320.0f - (CS_COLS * tw0 + (CS_COLS - 1) * gx) * 0.5f, y0 = 68.0f;
    char buf[96];
    int i, ch = csChooser();
    csEnsure();
    for (i = 0; i < CS_COLS * CS_ROWS; i++)
        csTile(i, x0 + (i % CS_COLS) * (tw0 + gx), y0 + (i / CS_COLS) * (th0 + gy));
    /* faixa de informacao (estilo letreiro) */
    rect(0, 244, 640, 262, CA(74, 34, 130, 0x50));
    rect(0, 244, 640, 246, CA(255, 205, 60, 0x70));
    rect(0, 260, 640, 262, CA(255, 205, 60, 0x70));
    if (csStage == 2) {
        if ((menuT / 25) & 1) textC(248, 0.34f, COL(120,255,150), "PRONTO! X OU START: PROXIMA ETAPA   O: VOLTAR");
        else                  textC(248, 0.34f, COL(255,240,150), "OS DOIS JOGADORES ESTAO PRONTOS");
    } else {
        int sl = csPos, id;
        csOrdInit();
        id = sl < NCHARS ? csOrd[sl] : 0;
        if (sl == CS_RND)        sprintf(buf, "J%d: ALEATORIO - SORTEIA UM PERSONAGEM NA HORA DE JOGAR", ch + 1);
        else if (sl > CS_RND)    sprintf(buf, "EM BREVE");
        else if (csTaken(ch, sl)) sprintf(buf, "%s ESTA EM USO PELO JOGADOR %d", CHAR_NAME[id], 2 - ch);
        else sprintf(buf, "%s - %s - TACO: %s - MESA: %s", CHAR_NAME[id], RAR_NAME[CHAR_RAR[id]],
                     CHAR_CUE[id] >= 0 ? CUES[CHAR_CUE[id]].name : "PADRAO", CHAR_TAB[id] >= 0 ? TABLES[CHAR_TAB[id]].name : "LIVRE");
        textFit(320.0f - 300.0f, 248.0f, 0.32f, (sl < CS_RND && !csTaken(ch, sl)) ? rarCol(CHAR_RAR[id]) : COL(255,150,130), buf, 600.0f);
    }
    if (SOLO) csPed(0, 320.0f);
    else {
        csPed(0, 170.0f); csPed(1, 470.0f);
        textC(318.0f, 0.9f, COL(255, 215 + (int)(30 * sinf(menuT * 0.07f)), 70), "VS");        /* VS entre os pedestais */
    }
}

static void drawSetupScreen(void)
{
    static const char *LBL[NSR] = { "ADVERSARIO", "DIFICULDADE", "TEMPO POR JOGADA", "MODO", "MESA",
                                    "COR DAS BOLAS", "COR JOGADOR 1", "COR JOGADOR 2", "PERSONAGEM", "PERSONAGEM", "BOLA BRANCA", "LINHA GUIA", "INICIAR PARTIDA", "PROXIMA ETAPA", "HABILIDADES", "MESA DO PERSONAGEM", "VER REPLAY ANTERIOR" };
    static const char *STEPN[NSTEPS] = { "PARTIDA", "PERSONAGENS", "MESA E BOLAS", "BRANCA E GUIA" };
    static const u8 LC[NLEVELS][3] = { {90,230,110}, {245,215,70}, {240,80,70}, {200,110,255}, {80,220,255} };
    int v[NSR], n = setRows(v), i;
    const float rh = 36.0f;
    float y2 = 86.0f + n * rh - 4.0f;
    const char *hint;
    char buf[40];

    uiTitle(tourney ? "CONFIGURAR TORNEIO" : (training ? "MODO TREINO" : "CONFIGURAR PARTIDA"));
    for (i = 0; i < NSTEPS; i++) {                        /* barra de etapas */
        float x1 = 52.0f + i * 136.0f, x2 = x1 + 128.0f;
        int cur = (i == setStep), done = (i < setStep);
        char lb[24], nb[4];
        u64 dc = cur ? COL(255,215,60) : (done ? COL(90,200,120) : COL(60,70,88));
        cham(x1 - 1.5f, 38.5f, x2 + 1.5f, 59.5f, 6.0f, dc);                                 /* pilula da etapa */
        cham(x1, 40, x2, 58, 5.0f, cur ? COL(128,84,12) : (done ? COL(22,76,48) : COL(30,26,60)));
        rect(x1 + 5, 40, x2 - 5, 47, CA(255, 255, 255, 0x14));
        if (i > 0) rect(x1 - 8, 49, x1 - 1.5f, 50, done || cur ? COL(90,200,120) : COL(60,70,88));
        discN(x1 + 11, 49, 7.5f, dc, 12);
        sprintf(nb, "%d", i + 1);
        text(x1 + 11 - tw(0.26f, nb) * 0.5f, 45, 0.26f, COL(10,14,24), nb);
        strcpy(lb, STEPN[i]);
        text(x1 + 24, 44, 0.28f, cur ? COL(255,240,150) : (done ? COL(150,225,170) : COL(120,130,148)), lb);
    }
    if (setStep == 1) {                                   /* etapa 1: seletor de personagem em grade (estilo jogo de luta) */
        static const int fk1[4] = { 7, 0, 1, 8 };
        static const char *const fl1[4] = { "MOVER", "CONFIRMAR", "VOLTAR", "ETAPA" };
        drawCharSelect();
        uiFoot(4, fk1, fl1);
        return;
    }
    panel(60, 72, 580, y2);

    for (i = 0; i < n; i++) {
        int row = v[i], sel = (i == setSel);
        float y = 86.0f + i * rh, vx;
        u64 tc = sel ? COL(255,240,150) : COL(175,185,195), vc = COL(255,255,255);

        if (row == SR_NEXT || row == SR_GO) {                     /* botao verde grande (como o JOGAR do 8 Ball Pool) */
            cham(70.0f, y - 6, 570.0f, y + 25, 6.0f, sel ? COL(255,236,120) : COL(18,70,24));
            cham(72.0f, y - 4, 568.0f, y + 23, 5.0f, COL(36,140,40));
            rect(76, y - 4, 564, y + 9, sel ? COL(98,214,64) : COL(72,184,56));
            rect(76, y - 4, 564, y - 1, CA(255, 255, 255, 0x30));
        } else if (sel) selBar(68, 572, y, 24);
        if (row == SR_NEXT) {
            char nx[32];
            text(100, y + 2, 0.5f, CA(0, 40, 0, 0x70), LBL[row]);
            text(98, y, 0.5f, COL(255,255,255), LBL[row]);
            sprintf(nx, "%d: %s", setStep + 2, STEPN[setStep + 1]);
            text(556 - tw(0.34f, nx), y + 3, 0.34f, COL(225,250,225), nx);
            continue;
        }
        if (row == SR_RPL) {
            text(98, y, 0.5f, sel ? COL(255,225,110) : COL(230,190,70), LBL[row]);
            text(556 - tw(0.34f, "X: ASSISTIR"), y + 3, 0.34f, COL(150,160,175), "X: ASSISTIR");
            continue;
        }
        if (row == SR_GO) {
            text(100, y + 2, 0.5f, CA(0, 40, 0, 0x70), LBL[row]);
            text(98, y, 0.5f, COL(255,255,255), LBL[row]);
            if (oppSel == 1)
                text(556 - tw(0.34f, "PROXIMO: NOME"), y + 3, 0.34f, COL(225,250,225), "PROXIMO: NOME");
            continue;
        }
        if (row == SR_CH0)      text(98, y, 0.5f, tc, oppSel ? "PERSONAGEM JOGADOR 1" : "SEU PERSONAGEM");
        else if (row == SR_CH1) text(98, y, 0.5f, tc, oppSel ? "PERSONAGEM JOGADOR 2" : "PERSONAGEM DO BOT");
        else if (row == SR_WB && oppSel) text(98, y, 0.5f, tc, "BOLA BRANCA (OS DOIS)");
        else if (row == SR_GD && oppSel) text(98, y, 0.5f, tc, "LINHA GUIA (OS DOIS)");
        else                    text(98, y, 0.5f, tc, LBL[row]);
        switch (row) {
        case SR_OPP:   strcpy(buf, oppSel ? "< JOGADOR 2 >" : "< BOT >"); break;
        case SR_LVL:   sprintf(buf, "< %s >", LVL_NAME[botLevel]);
                       vc = COL(LC[botLevel][0], LC[botLevel][1], LC[botLevel][2]); break;
        case SR_TIME:  if (timeSel) sprintf(buf, "< %d SEGUNDOS >", TIME_SEC[timeSel]);
                       else strcpy(buf, "< SEM TEMPO >");
                       vc = timeSel ? COL(255,230,110) : COL(255,255,255); break;
        case SR_MODE:  sprintf(buf, "< %s >", GMN[gmode]); break;
        case SR_TABLE: sprintf(buf, "< %s >", (tableRandom && !tableAuto) ? "ALEATORIA" : TABLES[effTable()].name); break;
        case SR_ABIL:  { int off = tourney ? !tAbil : abilSel; strcpy(buf, off ? "< DESLIGADAS >" : "< LIGADAS >"); vc = off ? COL(255,130,120) : COL(110,255,140); } break;
        case SR_TAUTO: if (tableAuto && CHAR_TAB[charSel[0]] >= 0) sprintf(buf, "< SIM: %s >", TABLES[CHAR_TAB[charSel[0]]].name);
                       else strcpy(buf, tableAuto ? "< SIM (SEM MESA) >" : "< NAO >");
                       vc = tableAuto ? COL(110,255,140) : COL(255,255,255); break;
        case SR_BALLS: sprintf(buf, "< %s >", THEME_NAME[ballTheme]); break;
        case SR_C0:    sprintf(buf, "< %s >", BRC_NAME[colSel[0]]); break;
        case SR_CH0:   sprintf(buf, "< %s >", charRand[0] ? "ALEATORIO" : CHAR_NAME[charSel[0]]); break;
        case SR_CH1:   sprintf(buf, "< %s >", charRand[1] ? "ALEATORIO" : CHAR_NAME[charSel[1]]); break;
        case SR_WB:    sprintf(buf, "< %s >", WB_NAME[wbSel[0]]); break;
        case SR_GD:    sprintf(buf, "< %s >", GD_NAME[gdSel[0]]); break;
        default:       sprintf(buf, "< %s >", BRC_NAME[colSel[1]]); break;
        }
        vx = 556 - tw(0.5f, buf);
        text(vx, y, 0.5f, vc, buf);
        if (row == SR_WB) drawSwatch(0, wbSel[0], vx - 18, y + 7);
        else if (row == SR_GD) drawSwatch(1, gdSel[0], vx - 24, y + 7);
        if (row == SR_TABLE) {                          /* mini mesa */
            const TableDef *T = &TABLES[effTable()];
            rect(vx - 46, y - 1, vx - 12, y + 16, shade(T->wood, 0.9f, 0));
            rect(vx - 43, y + 2, vx - 15, y + 13, shade(T->felt, 1.0f, 0));
        } else if (row == SR_C0 || row == SR_C1) {      /* bolinha da cor */
            const u8 *bc = BC0[BRC[colSel[row == SR_C0 ? 0 : 1]]];
            discN(vx - 18, y + 7, 8.0f, shade(bc, 0.55f, 0), 12);
            discN(vx - 18, y + 7, 7.2f, shade(bc, 1.0f, 0), 12);
            discN(vx - 20.5f, y + 4.5f, 1.8f, COL(255,255,255), 6);
        } else if (row == SR_BALLS) {                   /* as 8 cores do tema */
            int q;
            for (q = 0; q < 8; q++) {
                const u8 *bc = THEME[ballTheme][q];
                float bx = vx - 112.0f + q * 13.0f;
                discN(bx, y + 7, 6.0f, shade(bc, 0.55f, 0), 10);
                discN(bx, y + 7, 5.2f, shade(bc, 1.0f, 0), 10);
                discN(bx - 1.8f, y + 4.8f, 1.4f, COL(255,255,255), 6);
            }
        }
    }

    if (v[setSel] == SR_MODE)
        hint = gmode == GM_BR ? "4 BOLAS CADA. ERROU A SUA: O OUTRO TIRA UMA DAS SUAS. BRANCA NA CACAPA: PERDE"
             : (gmode == GM_MINI ? "BOLA 8 MINI: SO 3 LISAS, 3 LISTRADAS E A 8 - PARTIDA RAPIDA"
             : (gmode == GM_SUDDEN ? "MORTE SUBITA: QUEM ENCACAPAR A PRIMEIRA BOLA (SEM FALTA) GANHA. A 8 DERROTA"
             : (gmode == GM_RACE ? "CORRIDA DE 3: O PRIMEIRO A ENCACAPAR 3 BOLAS (SEM FALTA) GANHA. A 8 DERROTA"
             : (gmode == GM_DUEL ? "BOLA 8 DUELO: SO 2 LISAS, 2 LISTRADAS E A 8 - PARTIDA RELAMPAGO"
                                 : "BOLA 8: LISAS X LISTRADAS - A BOLA 8 FICA PARA O FINAL"))));
    else if (v[setSel] == SR_TIME)
        hint = "PASSOU O TEMPO DA JOGADA: PERDE A VEZ";
    else if (v[setSel] == SR_ABIL && tourney)
        hint = tAbil ? "LIGADAS: VOCE ESCOLHE O TACO (SELECT) E USA PODERES (O). OS RIVAIS JOGAM SEM PODERES" : "DESLIGADAS: TODOS JOGAM SO COM O TACO CLASSICO";
    else if (v[setSel] == SR_ABIL)
        hint = "LIGADAS: TACOS DE PERSONAGEM TEM PODERES (O NA MIRA). DESLIGADAS: SO OS ATRIBUTOS";
    else if (v[setSel] == SR_TAUTO)
        hint = "USA A MESA DO PERSONAGEM DO JOGADOR 1 (VEJA NO CATALOGO DE TACOS)";
    else if (v[setSel] == SR_LVL && botLevel == 3)
        hint = "PROFISSIONAL: MIRA QUASE PERFEITA, RESPONDE NA HORA E NAO PERDOA ERRO";
    else if (v[setSel] == SR_LVL && botLevel == 4)
        hint = "MESTRE: COMO O PROFISSIONAL, USA TACOS E PODERES, E A BOLA 8 EXIGE ESCOLHER A CACAPA";
    else if (v[setSel] == SR_WB)
        hint = "A COR DA BOLA BRANCA VALE PARA A PARTIDA TODA";
    else if (v[setSel] == SR_GD)
        hint = "DURANTE O JOGO, START MUDA A COR DA LINHA GUIA";
    else if (v[setSel] == SR_BALLS)
        hint = "MUDA AS CORES DAS BOLAS LISAS E LISTRADAS (BOLA 8)";
    else if (v[setSel] == SR_RPL)
        hint = "X: REVER A ULTIMA BOLA DA PARTIDA ANTERIOR   (X OU START NO REPLAY: VOLTAR)";
    else if (v[setSel] == SR_NEXT)
        hint = "X OU START: PROXIMA ETAPA   O: VOLTAR";
    else if (v[setSel] == SR_GO)
        hint = tourney ? "X OU START: ESCOLHER O NOME E COMECAR O TORNEIO   O: ETAPA ANTERIOR" : "X OU START: INICIAR   O: ETAPA ANTERIOR";
    else
        hint = "";
    if (hint[0]) textC(y2 + 10, 0.3f, COL(170,180,195), hint);

    if (setStep == 2) {                                   /* previa grande da mesa */
        if (tourney) textC(184, 0.4f, COL(255,215,70), "A MESA MUDA A CADA PARTIDA DO TORNEIO");
        drawTablePreview(320.0f, 346.0f, 230.0f, &TABLES[tourney ? (menuT / 150) % NTABLES : effTable()]);
    } else if (setStep == 3) {                            /* bola branca e linha guia em destaque */
        const u8 *c = WB_COL[wbSel[0]];
        float bx = 118.0f, by = 346.0f;
        int r, gg, bb;
        if (gdSel[0] == NGD - 1) hue3(menuT * 0.01f, &r, &gg, &bb);
        else { r = GD_COL[gdSel[0]][0]; gg = GD_COL[gdSel[0]][1]; bb = GD_COL[gdSel[0]][2]; }
        discN(bx + 5, by + 8, 30.0f, CA(0, 0, 0, 0x30), 18);
        discN(bx, by, 30.0f, shade(c, 0.55f, 0), 20);
        discN(bx - 1.0f, by - 1.0f, 27.5f, shade(c, 1.0f, 0), 20);
        discN(bx - 10.0f, by - 11.0f, 7.0f, CA(255, 255, 255, 0x58), 10);
        text(bx - tw(0.3f, "BOLA BRANCA") * 0.5f, by + 40, 0.3f, COL(175,185,195), "BOLA BRANCA");
        drawTablePreview(320.0f, 350.0f, 190.0f, &TABLES[tourney ? (menuT / 150) % NTABLES : effTable()]);
        beam(432.0f, by, 560.0f, by, 8.0f, CA(r, gg, bb, 0x18));
        beam(432.0f, by, 560.0f, by, 4.0f, CA(r, gg, bb, 0x34));
        beam(432.0f, by, 560.0f, by, 1.8f, COL(r, gg, bb));
        ringN(574.0f, by, 9.0f, 1.4f, 18, COL(r, gg, bb));
        text(500.0f - tw(0.3f, "LINHA GUIA") * 0.5f, by + 40, 0.3f, COL(175,185,195), "LINHA GUIA");
    } else {                                              /* etapas 0 e 1: os dois personagens */
        float sc = setStep == 1 ? 1.35f : 0.85f, cy = setStep == 1 ? 327.0f : 354.0f, px0 = setStep == 1 ? 170.0f : 150.0f;
        for (i = 0; i < 2; i++) {
            float px = SOLO ? 320.0f : (i == 0 ? px0 : 640.0f - px0);
            int hot;
            if (SOLO && i == 1) continue;                        /* torneio: os rivais sao sorteados; treino: sem adversario */
            hot = (setStep == 1 && v[setSel] == (i == 0 ? SR_CH0 : SR_CH1));
            float bob = hot ? -4.0f * fabsf(sinf(menuT * 0.15f)) : 0.0f;
            const char *nm = (i == 1 && oppSel == 0) ? "BOT" : pnames[i];
            int ci = charRand[i] ? (menuT / 90) % NCHARS : charSel[i];
            drawChar(ci, px, cy + bob, sc, hot ? 1 : 0, menuT, 0);
            text(px - tw(0.34f, charRand[i] ? "ALEATORIO" : CHAR_NAME[ci]) * 0.5f, 402, 0.34f, hot ? COL(255,240,150) : COL(175,185,195), charRand[i] ? "ALEATORIO" : CHAR_NAME[ci]);
            text(px - tw(0.28f, nm) * 0.5f, 416, 0.28f, COL(130,140,155), nm);
            text(px - tw(0.28f, RAR_NAME[CHAR_RAR[ci]]) * 0.5f, setStep == 1 ? 361 : 388, 0.28f, rarCol(CHAR_RAR[ci]), RAR_NAME[CHAR_RAR[ci]]);
            if (setStep == 1) {                                       /* kit do personagem: taco e mesa */
                char kt[40];
                sprintf(kt, "TACO: %s", CHAR_CUE[ci] >= 0 ? CUES[CHAR_CUE[ci]].name : "PADRAO");
                text(px - tw(0.26f, kt) * 0.5f, 374, 0.26f, COL(255,190,110), kt);
                sprintf(kt, "MESA: %s", CHAR_TAB[ci] >= 0 ? TABLES[CHAR_TAB[ci]].name : "LIVRE");
                text(px - tw(0.26f, kt) * 0.5f, 387, 0.26f, COL(150,220,255), kt);
            }
        }
        if (setStep == 0 && !SOLO) textC(340, 0.9f, COL(255,215,70), "VS");
        else if (setStep == 0 && training) textC(312, 0.4f, COL(255,215,70), "TREINO LIVRE: SEM ADVERSARIO");
        else if (setStep == 0) textC(312, 0.4f, COL(255,215,70), "MATA-MATA: PERDEU, ESTA FORA!");
    }
    {
        static const int fk[4] = { 6, 7, 8, 1 };
        static const char *const fl[4] = { "ESCOLHER", "MUDAR", "ETAPA", "VOLTAR" };
        uiFoot(4, fk, fl);                                /* por ultimo: nada cobre o rodape */
    }
}

static void drawNameScreen(void)
{
    const float x0 = 82.0f, y0 = 118.0f, cw = 44.0f, ch = 34.0f, gap = 4.0f;
    char one[2] = { 0, 0 }, ttl[32], dflt[16];
    int i;

    sprintf(ttl, "NOME DO JOGADOR %d", nameWho + 1);
    sprintf(dflt, "JOGADOR %d", nameWho + 1);
    uiTitle(ttl);

    hbox(170, 72, 470, 106, CA(255, 215, 60, 0x7c), CA(6, 10, 20, 0x78));
    if (nameLen == 0) {
        textC(80, 0.5f, COL(110,118,132), dflt);
    } else {
        textC(78, 0.6f, COL(255,255,255), nameBuf);
        if ((menuT / 20) & 1) {
            float xe = 320.0f + tw(0.6f, nameBuf) * 0.5f;
            rect(xe + 3, 94, xe + 13, 97, COL(255,230,60));
        }
    }

    for (i = 0; i < 40; i++) {
        int col = i % 10, row = i / 10, sel = (nameCur == i) || (i == 38 && nameCur == 39);
        float x = x0 + col * (cw + gap), y = y0 + row * (ch + gap), w = cw;
        const char *lb;
        if (i == 39) continue;                          /* o OK ocupa duas celulas */
        if (i == 38) w = cw * 2 + gap;
        hbox(x, y, x + w, y + ch, sel ? CA(255, 215, 60, 0x7c) : CA(80, 88, 106, 0x60),
             sel ? CA(70, 60, 14, 0x78) : CA(8, 12, 22, 0x68));
        if (i < 36) { one[0] = KBCH[i]; lb = one; }
        else lb = i == 36 ? "ESP" : (i == 37 ? "APAG" : "OK");
        {
            float sc = i < 36 ? 0.5f : (i == 38 ? 0.5f : 0.3f);
            text(x + w * 0.5f - tw(sc, lb) * 0.5f, y + (ch - 24.0f * sc) * 0.5f, sc,
                 sel ? COL(255,240,150) : COL(190,198,210), lb);
        }
    }
    {
        static const int fk[4] = { 0, 1, 4, 5 };
        static const char *const fl[4] = { "ESCOLHER", "APAGAR", "CONFIRMAR", "VOLTAR" };
        uiFoot(4, fk, fl);
    }
}

/* ---------- OPCOES (som, musica, volume, efeitos, dicas, creditos) ---------- */
enum { OP_SND, OP_MUS, OP_VOL, OP_FX, OP_TIPS, OP_CRED, NOP };
static int optSel;

static void optInput(u32 pr)
{
    int d = ((pr & PAD_RIGHT) ? 1 : 0) - ((pr & PAD_LEFT) ? 1 : 0), tog = d || (pr & PAD_CROSS);
    if (pr & PAD_UP)   optSel = (optSel + NOP - 1) % NOP;
    if (pr & PAD_DOWN) optSel = (optSel + 1) % NOP;
    if (pr & (PAD_CIRCLE | PAD_START)) { state = S_MENU; return; }
    switch (optSel) {
    case OP_SND:  if (tog) sndOn = !sndOn; break;
    case OP_MUS:  if (tog) { musOn = !musOn; musicSetEnabled(musOn); } break;
    case OP_VOL:  if (d) { volLvl += d; if (volLvl < 0) volLvl = 0; if (volLvl > 10) volLvl = 10; musicSetVolume(volLvl); } break;
    case OP_FX:   if (tog) fxOn = !fxOn; break;
    case OP_TIPS: if (tog) tipsOn = !tipsOn; break;
    case OP_CRED: if (pr & PAD_CROSS) state = S_CREDITS; break;
    }
}

/* linha de selecao animada (faixa dourada + seta) */
static void selBar(float x1, float x2, float y, float h)
{
    float bob = 4.0f * sinf(menuT * 0.2f);
    int pul = 0x10 + (int)(0x0c * (1.0f + sinf(menuT * 0.12f)));
    cham(x1 - 1.5f, y - 7.5f, x2 + 1.5f, y + h + 1.5f, 5.0f, CA(255, 215, 60, 0xa0));           /* contorno dourado */
    cham(x1, y - 6, x2, y + h, 4.0f, COL(70, 38, 140));                                         /* corpo roxo */
    rect(x1 + 4, y - 6, x2 - 4, y - 6 + (h + 6) * 0.45f, CA(255, 255, 255, 0x18));              /* brilho de vidro */
    cham(x1, y - 6, x2, y + h, 4.0f, CA(255, 215, 60, pul));                                    /* pulso */
    rect(x1, y - 2, x1 + 3, y + h - 2, COL(255,230,60));
    tri(x1 + 10 + bob, y + 1, x1 + 10 + bob, y + 15, x1 + 23 + bob, y + 8, COL(255,230,60));
}

static void drawOptionsScreen(void)
{
    static const char *LB[NOP] = { "SOM", "MUSICA", "VOLUME DA MUSICA", "EFEITOS DA MESA", "DICAS NA TELA", "CREDITOS" };
    static const char *DS[NOP] = { "EFEITOS SONOROS DA MESA, DOS MENUS E DA VITORIA", "LIGA E DESLIGA AS MUSICAS DO PEN DRIVE",
                                   "VOLUME DAS MUSICAS (OS EFEITOS NAO MUDAM)", "ANIMACOES DAS MESAS E BRILHO DA MIRA",
                                   "BARRA DE TECLAS E DICAS NA MIRA", "CRIADOR E VERSAO DO JOGO" };
    static const float OY0[NOP] = { 74, 102, 130, 182, 207, 278 };
    static const int fk[4] = { 6, 7, 0, 1 };
    static const char *const fl[4] = { "ESCOLHER", "MUDAR", "OK", "VOLTAR" };
    char buf[32];
    int i;
    uiTitle("OPCOES");
    panel(70, 46, 570, 306);
    uiSection(94, 548, 54, COL(120,200,255), "AUDIO");
    uiSection(94, 548, 164, COL(200,140,255), "VISUAL");
    uiSection(94, 548, 260, COL(255,190,110), "SOBRE");
    for (i = 0; i < NOP; i++) {
        float y = OY0[i];
        int sel = (i == optSel);
        u64 tc = sel ? COL(255,240,150) : COL(180,190,200);
        if (sel) selBar(78, 562, y, 22);
        text(112, y + 1, 0.46f, tc, LB[i]);
        switch (i) {
        case OP_SND:  uiToggle(544, y, sndOn, "LIGADO", "DESLIGADO"); break;
        case OP_MUS:  uiToggle(544, y, musOn, "LIGADA", "DESLIGADA"); break;
        case OP_FX:   uiToggle(544, y, fxOn, "LIGADOS", "DESLIGADOS"); break;
        case OP_TIPS: uiToggle(544, y, tipsOn, "LIGADAS", "DESLIGADAS"); break;
        case OP_VOL: {
            int q;
            for (q = 0; q < 10; q++) {
                float bx = 412.0f + q * 13.0f;
                int on = q < volLvl;
                rect(bx, y + 2, bx + 10, y + 20, on ? COL(255, 205 - q * 6, 60) : COL(40,46,60));
            }
            sprintf(buf, "%d%%", volLvl * 10);
            text(404 - tw(0.34f, buf), y + 4, 0.34f, COL(255,255,255), buf);
            break; }
        default:
            text(544 - tw(0.5f, ">"), y + 1, 0.5f, COL(255,230,60), ">");
            break;
        }
    }
    hbox(70, 322, 570, 372, CA(255, 215, 60, 0x50), CA(8, 14, 26, 0x70));
    text(86, 330, 0.28f, COL(150,160,178), "SOBRE");
    textC(346, 0.32f, COL(150,220,255), DS[optSel]);
    if (optSel == OP_SND && !musicReady())            textC(360, 0.28f, COL(255,190,120), musicStatus());
    else if (optSel == OP_MUS || optSel == OP_VOL)    textC(360, 0.28f, COL(255,235,170), musicStatus());
    uiFoot(4, fk, fl);
}

/* ---------- chaveamento do torneio ---------- */
static float tCx(int c) { return 14.0f + c * 152.0f; }
static float tCy(int c, int i) { return 71.0f + 43.0f * ((float)(i << c) + 0.5f * (float)((1 << c) - 1)); }

static void drawBracketScreen(void)
{
    static const char *CN[4] = { "QUARTAS", "SEMIFINAL", "FINAL", "CAMPEAO" };
    char buf[96];
    int c, i, n;
    float pulse = 0.5f + 0.5f * sinf(menuT * 0.12f);

    uiTitle("TORNEIO");
    for (c = 0; c < 4; c++)
        text(tCx(c) + 62.0f - tw(0.26f, CN[c]) * 0.5f, 38, 0.26f, c == tRound && tRes <= 1 ? COL(255,240,150) : COL(140,150,168), CN[c]);

    for (c = 0; c < 3; c++)                                       /* ligacoes */
        for (i = 0; i < (8 >> c) / 2; i++) {
            float ya = tCy(c, 2 * i), yb = tCy(c, 2 * i + 1), ym = tCy(c + 1, i);
            float xr = tCx(c) + 124.0f, xm = xr + 13.0f;
            u64 lc = CA(120, 170, 150, 0x78);
            line(xr, ya, xm, ya, lc); line(xr, yb, xm, yb, lc);
            line(xm, ya, xm, yb, lc); line(xm, ym, tCx(c + 1), ym, lc);
        }

    for (c = 0; c < 4; c++) {                                     /* caixas */
        n = (c == 3) ? 1 : (8 >> c);
        for (i = 0; i < n; i++) {
            int comp = tSlot[c][i], me = (comp == 0), lost = 0, now;
            float x = tCx(c), y = tCy(c, i) - 17.0f;
            if (c < 3 && comp >= 0) { int nx = tSlot[c + 1][i / 2]; lost = (nx >= 0 && nx != comp); }
            now = (c == tRound && tRes <= 1 && comp >= 0 && (me || comp == tOpp));
            rect(x - 1, y - 1, x + 125, y + 35, me ? COL(255,215,70) : COL(70,84,100));
            rect(x, y, x + 124, y + 34, me ? COL(58,46,10) : COL(10,18,30));
            if (comp < 0) { text(x + 56, y + 6, 0.5f, COL(80,92,112), "?"); continue; }
            drawChar(tChar[comp], x + 19, y + 19, 0.3f, 0, menuT, 0);
            text(x + 40, y + 4, 0.3f, lost ? COL(105,110,122) : (me ? COL(255,240,150) : COL(220,228,240)), tName[comp]);
            text(x + 40, y + 19, 0.22f, lost ? COL(90,96,108) : rarCol(CHAR_RAR[tChar[comp]]), CHAR_NAME[tChar[comp]]);   /* nome do personagem */
            if (lost) rect(x, y, x + 124, y + 34, CA(0, 0, 0, 0x48));
            if (now) frameR(x - 3, y - 3, x + 127, y + 37, 1.6f, me ? CA(255, 230, 90, 0x50 + (int)(0x30 * pulse)) : CA(255, 255, 255, 0x40));
        }
    }
    if (tRes == 3) drawTrophy(tCx(3) + 62.0f, 126.0f, 0.7f, menuT);

    if (tRes == 0)      sprintf(buf, "%s: %s X %s", tRound == 0 ? "QUARTAS" : (tRound == 1 ? "SEMI" : "FINAL"), pname(0), tName[tOpp]);
    else if (tRes == 1) sprintf(buf, "VOCE AVANCOU! %s CONTRA %s", tRound == 0 ? "QUARTAS" : (tRound == 1 ? "SEMI" : "FINAL"), tName[tOpp]);
    else if (tRes == 2) sprintf(buf, "VOCE FOI ELIMINADO! CAMPEAO: %s (%s)", tName[tChamp], CHAR_NAME[tChar[tChamp]]);
    else                sprintf(buf, "PARABENS, %s! VOCE E O CAMPEAO!", pname(0));
    textC(388, 0.38f, tRes == 2 ? COL(255,160,140) : COL(255,240,170), buf);
    if (tRes <= 1) {
        char rl[96];
        static const int fk[2] = { 0, 5 };
        static const char *const fl[2] = { "JOGAR", "SAIR DO TORNEIO" };
        sprintf(buf, "%s X %s   MESA: %s", CHAR_NAME[tChar[0]], CHAR_NAME[tChar[tOpp]], TABLES[tTable].name);
        textC(402, 0.3f, COL(150,220,255), buf);                                                  /* personagens da partida */
        uiFoot(2, fk, fl);
        if (timeSel) sprintf(rl, "REGRAS: %s | %s | TEMPO %dS | %s", GMN[gmode], tAbil ? "COM HABILIDADES" : "TACO CLASSICO", TIME_SEC[timeSel], LVL_NAME[botLevel]);
        else         sprintf(rl, "REGRAS: %s | %s | SEM TEMPO | %s", GMN[gmode], tAbil ? "COM HABILIDADES" : "TACO CLASSICO", LVL_NAME[botLevel]);
        textC(413, 0.26f, COL(130,140,158), rl);
    } else {
        static const int fk[1] = { 0 };
        static const char *const fl[1] = { "VOLTAR AO MENU" };
        uiFoot(1, fk, fl);
    }
}

/* ================= LOJA: efeitos (giz, rastro, cacapa, moldura) e a tela da loja ================= */
static const u8 SH_CH[5][4][3] = {       /* giz: sombra, cor, brilho, po */
    { {20,40,90},  {60,120,225}, {120,175,250}, {170,210,255} },
    { {10,70,30},  {50,200,90},  {130,250,160}, {170,255,190} },
    { {110,20,70}, {235,80,170}, {255,160,215}, {255,190,230} },
    { {110,80,0},  {235,180,30}, {255,225,110}, {255,235,130} },
    { {50,20,100}, {140,80,230}, {190,150,255}, {210,180,255} } };

static void chalkCol(int st, int k, int *r, int *gg, int *bb)
{
    if (st >= 5) {                                       /* ARCO-IRIS: muda de cor sozinho */
        hue3(menuT * 0.02f + k * 0.05f, r, gg, bb);
        if (k == 0) { *r /= 3; *gg /= 3; *bb /= 3; }
        else if (k >= 2) { *r = (*r + 255) / 2; *gg = (*gg + 255) / 2; *bb = (*bb + 255) / 2; }
        return;
    }
    *r = SH_CH[st][k][0]; *gg = SH_CH[st][k][1]; *bb = SH_CH[st][k][2];
}

static void cubeQuad(float cx, float cy, float ux, float uy, float vx, float vy, float h, float off, u64 c)
{
    quad(cx - ux*h + off - vx*h, cy - uy*h + off - vy*h, cx + ux*h + off - vx*h, cy + uy*h + off - vy*h,
         cx + ux*h + off + vx*h, cy + uy*h + off + vy*h, cx - ux*h + off + vx*h, cy - uy*h + off + vy*h, c);
}

/* cubo de giz esfregando na ponta (tx,ty) do taco que aponta para o angulo a, com nuvenzinhas de po */
static void chalkFxAt(float tx, float ty, float a, int an, int st)
{
    float ux = cosf(a), uy = sinf(a), vx = -uy, vy = ux;
    float dist = 9.0f + 2.5f * sinf(an * 0.9f), wob = 3.0f * sinf(an * 0.9f + 1.0f);
    float cx = tx + vx * dist + ux * wob, cy = ty + vy * dist + uy * wob;
    int k, r0, g0, b0, np = st > 0 ? 10 : 6;
    chalkCol(st, 0, &r0, &g0, &b0); cubeQuad(cx, cy, ux, uy, vx, vy, 6.5f, 2.0f, COL(r0, g0, b0));
    chalkCol(st, 1, &r0, &g0, &b0); cubeQuad(cx, cy, ux, uy, vx, vy, 6.5f, 0.0f, COL(r0, g0, b0));
    chalkCol(st, 2, &r0, &g0, &b0); cubeQuad(cx, cy, ux, uy, vx, vy, 4.0f, 0.0f, COL(r0, g0, b0));
    chalkCol(st, 3, &r0, &g0, &b0);
    for (k = 0; k < np; k++) {
        float age = fmodf(an * 0.8f + k * 7.0f, 24.0f);
        disc(tx + sinf(k * 5.1f) * (st > 0 ? 9.0f : 6.0f), ty - age * 0.8f, 1.4f + age * 0.08f, CA(r0, g0, b0, (int)(0x50 * (1.0f - age / 24.0f))));
    }
    if (st >= 3)                                         /* dourado, roxo e arco-iris: faiscas */
        for (k = 0; k < 4; k++) {
            float age = fmodf(an * 1.1f + k * 6.0f, 24.0f), f = 1.0f - age / 24.0f;
            star4(tx + sinf(k * 3.7f) * 9.0f, ty - 2.0f - age * 0.9f, 2.6f * f, CA(255, 255, 230, (int)(0xa0 * f)));
        }
}

/* ---- rastro colorido atras da bola ---- */
static void trailDraw(int st, float x, float y, float vx, float vy)
{
    float sp = sqrtf(vx * vx + vy * vy), ux, uy, len;
    int q;
    if (st <= 0 || sp < 35.0f) return;
    ux = vx / sp; uy = vy / sp; len = sp * 0.11f; if (len > 30.0f) len = 30.0f;
    for (q = 8; q >= 1; q--) {
        float d = len * q / 6.0f, rr = R * (0.95f - 0.09f * q);
        int al = (int)(0x50 * (1.0f - q / 9.0f)), rc, gc, bc;
        if (st == 1)      { rc = 255; gc = 210 - q * 22; bc = 30; }
        else if (st == 2) { rc = 150 + q * 5; gc = 225; bc = 255; }
        else if (st == 3) { rc = 80; gc = 255; bc = 120 + q * 4; }
        else              hue3(menuT * 0.03f + q * 0.07f, &rc, &gc, &bc);
        discN(x - ux * d, y - uy * d, rr, CA(rc, gc, bc, al), 10);
    }
}

/* ---- particulas dos efeitos de cacapa ---- */
static unsigned pfSeed = 12345u;
static int pfRnd(void) { pfSeed = pfSeed * 1664525u + 1013904223u; return (int)((pfSeed >> 16) & 0x7fff); }
static float pfF(void) { return pfRnd() / 32767.0f; }
static Pfx *pfNew(void)
{
    int i;
    for (i = 0; i < NPF; i++) if (pf[i].life <= 0) return &pf[i];
    return NULL;
}

static void pfxBurst(int kind, float x, float y)
{
    int i, n;
    Pfx *p;
    if (kind <= 0) return;
    n = kind == 1 ? 30 : (kind == 2 ? 24 : 12);
    for (i = 0; i < n; i++) {
        float a = pfF() * 6.2831853f, sp;
        p = pfNew(); if (!p) return;
        p->x = x; p->y = y; p->kind = kind; p->c = pfRnd() % 1000;
        if (kind == 1)      { sp = 40.0f + pfF() * 110.0f; p->vx = cosf(a) * sp; p->vy = sinf(a) * sp - 30.0f; p->life = p->max = 50 + pfRnd() % 30; }
        else if (kind == 2) { sp = 10.0f + pfF() * 50.0f;  p->vx = cosf(a) * sp; p->vy = -30.0f - pfF() * 70.0f; p->life = p->max = 28 + pfRnd() % 22; }
        else                { sp = 60.0f + pfF() * 120.0f; p->vx = cosf(a) * sp; p->vy = sinf(a) * sp; p->life = p->max = 14 + pfRnd() % 10; }
    }
    if (kind == 3) {                                     /* raio: um relampago caindo do alto */
        p = pfNew();
        if (p) { p->x = x; p->y = y; p->vx = p->vy = 0.0f; p->kind = 4; p->life = p->max = 16; p->c = pfRnd() % 1000; }
    }
    if (kind >= 2) {                                     /* clarao em anel */
        p = pfNew();
        if (p) { p->x = x; p->y = y; p->vx = p->vy = 0.0f; p->kind = 5; p->life = p->max = 12; p->c = kind; }
    }
}

static void pfxUpdate(void)
{
    int i;
    for (i = 0; i < NPF; i++) {
        Pfx *p = &pf[i];
        if (p->life <= 0) continue;
        p->life--;
        p->x += p->vx / 60.0f; p->y += p->vy / 60.0f;
        if (p->kind == 1) { p->vy += 2.0f; p->vx *= 0.985f; }
        else if (p->kind == 2) { p->vx *= 0.97f; p->vy -= 0.3f; }
    }
}

static void pfxDraw(void)
{
    int i, q;
    for (i = 0; i < NPF; i++) {
        Pfx *p = &pf[i];
        float t;
        if (p->life <= 0) continue;
        t = (float)p->life / (float)p->max;
        if (p->kind == 1) {                              /* confete girando */
            float a = p->c + menuT * 0.2f, ux = cosf(a) * 3.2f, uy = sinf(a) * 3.2f, vx = -sinf(a) * 1.6f, vy = cosf(a) * 1.6f;
            quad(p->x - ux - vx, p->y - uy - vy, p->x + ux - vx, p->y + uy - vy, p->x + ux + vx, p->y + uy + vy, p->x - ux + vx, p->y - uy + vy,
                 hueCol(p->c * 0.0071f, t > 0.5f ? 0xa0 : (int)(0x140 * t)));
        } else if (p->kind == 2) {                       /* chama */
            disc(p->x, p->y, 2.0f + (1.0f - t) * 3.5f, CA(255, 60 + (int)(160 * t), 10, (int)(0x90 * t)));
        } else if (p->kind == 3) {                       /* fagulha do raio */
            line(p->x, p->y, p->x - p->vx * 0.05f, p->y - p->vy * 0.05f, CA(255, 255, 190, (int)(0xc0 * t)));
        } else if (p->kind == 4) {                       /* relampago em zigue-zague */
            float px = p->x, py = p->y - 130.0f;
            for (q = 0; q < 9; q++) {
                float ny = p->y - 130.0f + (q + 1) * (130.0f / 9.0f), nx = q == 8 ? p->x : p->x + sinf(p->c * 12.9898f + q * 78.233f) * 13.0f;
                beam(px, py, nx, ny, 5.0f, CA(120, 170, 255, (int)(0x50 * t)));
                beam(px, py, nx, ny, 1.8f, CA(255, 255, 255, (int)(0xd0 * t)));
                px = nx; py = ny;
            }
        } else {                                         /* anel de clarao */
            ringN(p->x, p->y, 4.0f + (1.0f - t) * 26.0f, 2.0f, 20, p->c == 2 ? CA(255, 170, 40, (int)(0x90 * t)) : CA(170, 210, 255, (int)(0x90 * t)));
        }
    }
}

/* ---- moldura do painel do jogador ---- */
static void shopFrameDraw(int st, float x1, float y1, float x2, float y2)
{
    float pul = 0.5f + 0.5f * sinf(menuT * 0.15f);
    int k;
    if (st <= 0) return;
    if (st == 1) {                                       /* DOURADA */
        frameR(x1 - 2, y1 - 2, x2 + 2, y2 + 2, 2.0f, COL(255,205,60));
        frameR(x1, y1, x2, y2, 1.0f, CA(255, 240, 150, 0x90));
        discN(x1 - 2, y1 - 2, 3.0f, COL(255,235,120), 8); discN(x2 + 2, y1 - 2, 3.0f, COL(255,235,120), 8);
        discN(x1 - 2, y2 + 2, 3.0f, COL(255,235,120), 8); discN(x2 + 2, y2 + 2, 3.0f, COL(255,235,120), 8);
    } else if (st == 2) {                                /* NEON AZUL */
        frameR(x1 - 2, y1 - 2, x2 + 2, y2 + 2, 2.0f, CA(60, 200, 255, 0x70 + (int)(0x50 * pul)));
        frameR(x1 - 4, y1 - 4, x2 + 4, y2 + 4, 1.0f, CA(120, 230, 255, 0x30 + (int)(0x30 * pul)));
    } else if (st == 3) {                                /* FOGO */
        frameR(x1 - 2, y1 - 2, x2 + 2, y2 + 2, 2.0f, CA(255, 110 + (int)(80 * pul), 20, 0xb0));
        for (k = 0; k < 10; k++) {
            float fx = x1 + (x2 - x1) * (k + 0.5f) / 10.0f, fh = 5.0f + 4.0f * sinf(menuT * 0.3f + k * 1.7f);
            tri(fx - 4.0f, y1 - 2.0f, fx + 4.0f, y1 - 2.0f, fx, y1 - 2.0f - fh, CA(255, 150 + (int)(70 * pul), 30, 0xa0));
        }
    } else if (st == 4) {                                /* GELO */
        frameR(x1 - 2, y1 - 2, x2 + 2, y2 + 2, 2.0f, CA(170, 225, 255, 0xb0));
        for (k = 0; k < 10; k++) {
            float fx = x1 + (x2 - x1) * (k + 0.5f) / 10.0f;
            tri(fx - 3.0f, y2 + 2.0f, fx + 3.0f, y2 + 2.0f, fx, y2 + 6.0f + (k % 3) * 2.0f, CA(220, 245, 255, 0xb0));
        }
    } else {                                             /* ARCO-IRIS */
        for (k = 0; k < 12; k++) {
            float xa = x1 + (x2 - x1) * k / 12.0f, xb = x1 + (x2 - x1) * (k + 1) / 12.0f;
            rect(xa, y1 - 3, xb, y1 - 1, hueCol(menuT * 0.01f + k / 12.0f, 0xc0));
            rect(xa, y2 + 1, xb, y2 + 3, hueCol(menuT * 0.01f + 0.5f + k / 12.0f, 0xc0));
        }
        for (k = 0; k < 5; k++) {
            float ya = y1 + (y2 - y1) * k / 5.0f, yb = y1 + (y2 - y1) * (k + 1) / 5.0f;
            rect(x1 - 3, ya, x1 - 1, yb, hueCol(menuT * 0.01f + 0.25f + k / 5.0f, 0xc0));
            rect(x2 + 1, ya, x2 + 3, yb, hueCol(menuT * 0.01f + 0.75f + k / 5.0f, 0xc0));
        }
    }
}

/* ---- tela da loja ---- */
static int shCat, shSel, shNote;
static char shNoteTxt[40];
static const int SH_PREV[4] = { 1, 5, 9, 12 };

static void shopNote(const char *t) { strncpy(shNoteTxt, t, 39); shNoteTxt[39] = 0; shNote = 110; }

/* mostra no atlas o estilo de bola que esta sob o cursor (so as 4 bolas da amostra) */
static void shopPrevBalls(int style)
{
    int k, i;
    if (!useTex) return;
    skinDefaults();
    for (i = 1; i <= 8; i++) memcpy(BC[i], THEME[0][i - 1], 3);
    skinStyleEff = style;
    for (k = 0; k < 4; k++) renderSkin(SH_PREV[k], (SH_PREV[k] % 4) * SKW, (SH_PREV[k] / 4) * SKH);
    gsKit_texture_upload(g, &ballTex);
    skinKey = -1;
}

static void shopHover(void)
{
    if (shCat == SC_BOLA) shopPrevBalls(shSel);
    else if (shCat == SC_CACAPA) pfxBurst(shSel, 480.0f, 225.0f);
}

static void shopOpen(void)
{
    state = S_SHOP; shCat = 0; shSel = shEq[0]; shNote = 0;
}

static void shopClose(void)
{
    ballStyle = shEq[SC_BOLA];
    skinKey = -1; skinStyleEff = ballStyle;
    if (useTex) setSkins(0);
    state = S_MENU;
}

static void shopInput(u32 pr)
{
    int oc = shCat, os = shSel, n;
    if (pr & (PAD_LEFT | PAD_L1))  { shCat = (shCat + SH_CATS - 1) % SH_CATS; shSel = shEq[shCat]; }
    if (pr & (PAD_RIGHT | PAD_R1)) { shCat = (shCat + 1) % SH_CATS; shSel = shEq[shCat]; }
    n = SH_N[shCat];
    if (pr & PAD_UP)   shSel = (shSel + n - 1) % n;
    if (pr & PAD_DOWN) shSel = (shSel + 1) % n;
    if (pr & PAD_CROSS) {
        const ShopItem *it = &SH_IT[shCat][shSel];
        if ((shOwn[shCat] >> shSel) & 1) {
            shEq[shCat] = shSel; ballStyle = shEq[SC_BOLA]; shopSave(); shopNote("EQUIPADO!");
        } else if (shCoins >= it->price) {
            shCoins -= it->price; shOwn[shCat] |= 1 << shSel; shEq[shCat] = shSel; ballStyle = shEq[SC_BOLA];
            shopSave(); shopNote("COMPRADO E EQUIPADO!");
        } else shopNote("MOEDAS INSUFICIENTES");
    }
    if (pr & (PAD_CIRCLE | PAD_START)) { shopClose(); return; }
    if (shCat != oc || shSel != os) shopHover();
}

static void drawShopScreen(void)
{
    static const int fk[4] = { 6, 7, 0, 1 };
    static const char *const fl[4] = { "ITEM", "ABA", "COMPRAR / USAR", "VOLTAR" };
    char buf[48];
    int i, n = SH_N[shCat];
    float pul = 0.5f + 0.5f * sinf(menuT * 0.12f);
    const ShopItem *cur_it = &SH_IT[shCat][shSel];

    uiTitle("LOJA");
    sprintf(buf, "%d", shCoins);
    cham(521.5f, 40.5f, 612.5f, 66.5f, 7.0f, CA(255, 205, 60, 0x90));                  /* pilula das moedas */
    cham(523, 42, 611, 65, 6.0f, COL(30, 16, 56));
    discN(538, 54, 8.0f, COL(255,205,60), 12); discN(538, 54, 5.5f, COL(210,150,20), 10);
    text(552, 46, 0.4f, COL(255,235,120), buf);
#if USE_MEMCARD
    if (shSaveFail) text(36, 46, 0.28f, COL(255,170,120), "SEM MEMORY CARD 1: O PROGRESSO NAO SERA SALVO");
    else
#endif
    text(36, 46, 0.3f, COL(150,160,178), "MOEDAS GANHAS NAS PARTIDAS");

    for (i = 0; i < SH_CATS; i++) {                      /* abas */
        float x1 = 36.0f + i * 94.0f, x2 = x1 + 90.0f;
        int sel = (i == shCat);
        static const unsigned char TC[6][6] = {            /* cor do topo e da base de cada aba */
            {92,206,60, 40,140,34}, {150,84,226, 84,44,160}, {40,196,196, 18,120,134},
            {255,176,40, 222,108,12}, {112,126,150, 62,72,94}, {74,104,200, 40,62,142} };
        const unsigned char *tc = TC[i % 6];
        float ty1 = sel ? 66.0f : 70.0f, ty2 = sel ? 95.0f : 92.0f;
        cham(x1 - 1.5f, ty1 - 1.5f, x2 + 1.5f, ty2 + 1.5f, 5.0f, sel ? COL(255,236,120) : COL(14,12,30));
        cham(x1, ty1, x2, ty2, 4.0f, COL(tc[3], tc[4], tc[5]));
        rect(x1 + 4, ty1, x2 - 4, ty1 + (ty2 - ty1) * 0.5f, COL(tc[0], tc[1], tc[2]));
        rect(x1 + 4, ty1, x2 - 4, ty1 + 2, CA(255, 255, 255, 0x30));
        if (!sel) cham(x1, ty1, x2, ty2, 4.0f, CA(0, 0, 10, 0x40));
        text(x1 + 46.0f - tw(0.3f, SH_CATN[i]) * 0.5f, ty1 + 6, 0.3f, CA(0, 0, 0, 0x70), SH_CATN[i]);
        text(x1 + 45.0f - tw(0.3f, SH_CATN[i]) * 0.5f, ty1 + 5, 0.3f, COL(255,255,255), SH_CATN[i]);
    }

    panel(36, 104, 340, 352);
    for (i = 0; i < n; i++) {
        float y = 114.0f + i * 38.0f;
        int sel = (i == shSel), own = (shOwn[shCat] >> i) & 1, eq = (shEq[shCat] == i);
        const ShopItem *it = &SH_IT[shCat][i];
        if (sel) selBar(40, 336, y + 2, 24);
        hudText(66, y + 6, 0.4f, sel ? COL(255,240,150) : COL(180,190,200), it->name, 160.0f);
        if (eq) text(330 - tw(0.28f, "EQUIPADO"), y + 8, 0.28f, COL(110,255,140), "EQUIPADO");
        else if (own) text(330 - tw(0.28f, "COMPRADO"), y + 8, 0.28f, COL(150,200,255), "COMPRADO");
        else {
            sprintf(buf, "%d $", it->price);
            text(330 - tw(0.34f, buf), y + 7, 0.34f, shCoins >= it->price ? COL(255,225,100) : COL(255,120,110), buf);
        }
    }

    panel(356, 104, 604, 352);
    textC(112, 0.4f, COL(255,215 + (int)(30 * pul),70), cur_it->name);
    rect(376, 134, 584, 135, CA(255, 205, 60, 0x50));
    switch (shCat) {
    case SC_GIZ: {
        float tx = 530.0f, ty = 222.0f;
        rect(tx - 150, ty - 3, tx - 4, ty + 3, COL(205,165,105));
        rect(tx - 4, ty - 3, tx, ty + 3, COL(90,150,255));
        chalkFxAt(tx, ty, 0.0f, menuT, shSel);
        break; }
    case SC_BOLA: {
        int k;
        for (k = 0; k < 4; k++) {
            float a = menuT * 0.03f + k * 0.9f, c = cosf(a), sn = sinf(a), *M = ballM[SH_PREV[k]];
            M[0] = c; M[1] = 0; M[2] = sn; M[3] = 0; M[4] = 1; M[5] = 0; M[6] = -sn; M[7] = 0; M[8] = c;
            drawBallAt(SH_PREV[k], 392.0f + k * 48.0f, 220.0f, 19.0f);
        }
        break; }
    case SC_RASTRO: {
        float x = 390.0f + fmodf(menuT * 1.6f, 180.0f);
        trailDraw(shSel, x, 222.0f, 160.0f, 0.0f);
        drawBallAt(8, x, 222.0f, R + 3.0f);
        break; }
    case SC_CACAPA:
        discN(480, 225, 28.0f, COL(8,8,12), 20); ringN(480, 225, 30.0f, 4.0f, 24, COL(110,70,40));
        if (shSel == 0) textC(300, 0.3f, COL(150,160,178), "SEM EFEITO ESPECIAL");
        else if (menuT % 100 == 0) pfxBurst(shSel, 480.0f, 225.0f);
        break;
    default: {
        float x1 = 392.0f, y1 = 176.0f, x2 = 568.0f, y2 = 236.0f;
        hbox(x1, y1, x2, y2, CA(255, 205, 70, 0x78), CA(38, 32, 12, 0x74));
        text(x1 + 12, y1 + 8, 0.42f, COL(255,235,110), "JOGADOR 1");
        if (shCat == SC_TITULO) {
            if (shSel > 0) hudText(x1 + 12, y1 + 32, 0.3f, COL(255,215,70), cur_it->name, x2 - x1 - 24.0f);
            else text(x1 + 12, y1 + 32, 0.3f, COL(130,138,155), "(SEM TITULO)");
        } else shopFrameDraw(shSel, x1, y1, x2, y2);
        break; }
    }
    pfxDraw();
    textC(366, 0.32f, COL(150,220,255), cur_it->desc);
    if (shNote > 0) { shNote--; textC(386, 0.4f, COL(255,230,60), shNoteTxt); }
    uiFoot(4, fk, fl);
}

/* ---------- menu inicial no estilo 8 Ball Pool: barra no topo, cartoes coloridos, mesa embaixo ---------- */
static const int MENU_ORD[6] = { 1, 2, 3, 4, 5, 0 };      /* ordem dos cartoes da esquerda p/ direita (indices dos itens) */
static int menuSlot(int sel)
{
    int k;
    for (k = 0; k < 6; k++) if (MENU_ORD[k] == sel) return k;
    return 0;
}

static void textCF(float cx, float y, float sc, u64 c, const char *t, float maxw)    /* centralizado, encolhe para caber */
{
    float w = tw(sc, t);
    if (w > maxw && w > 0.0f) sc *= maxw / w;
    text(cx - tw(sc, t) * 0.5f, y, sc, c, t);
}

static void drawMenuTop(void)                                        /* barra do topo: jogador, musica e moedas */
{
    char tb[40];
    const char *mn;
    float ax1 = 9.0f, ay1 = 3.0f, ax2 = 49.0f, ay2 = 43.0f, acx = (ax1 + ax2) * 0.5f;
    rect(0, 0, 640, 46, CA(14, 8, 32, 0x74));
    rect(0, 0, 640, 2, CA(255, 255, 255, 0x12));
    rect(0, 46, 640, 48, CA(255, 205, 60, 0x80));
    rect(236, 5, 237, 41, CA(255, 205, 60, 0x40));
    rect(424, 5, 425, 41, CA(255, 205, 60, 0x40));
    /* jogador */
    cham(ax1 - 2.5f, ay1 - 2.5f, ax2 + 2.5f, ay2 + 2.5f, 4.5f, COL(112, 72, 8));
    cham(ax1 - 2.0f, ay1 - 2.0f, ax2 + 2.0f, ay2 + 2.0f, 4.0f, COL(228, 174, 46));
    cham(ax1, ay1, ax2, ay2, 3.0f, COL(40, 78, 128));
    discN(acx, ay1 + 16.0f, 16.0f, CA(255, 255, 255, 0x0c), 14);
    drawChar(charSel[0], acx, ay1 + 26.0f, 0.36f, 0, menuT, 0);
    textFit(60, 7, 0.4f, COL(255, 235, 110), pnames[0], 170.0f);
    sprintf(tb, "VITORIAS: %d", winsBot[0] + winsVs[0]);
    textFit(60, 27, 0.28f, COL(190, 200, 225), tb, 170.0f);
    /* musica */
    discN(254, 31, 4.0f, COL(255, 215, 70), 8);
    rect(257, 14, 259, 31, COL(255, 215, 70));
    rect(259, 14, 267, 18, COL(255, 215, 70));
    text(274, 6, 0.26f, COL(255, 215, 70), "MUSICA");
    mn = (musicState() == 1 && musicCur() >= 0) ? musicName(musicCur()) : musicStatus();
    textFit(274, 23, 0.3f, COL(200, 210, 225), mn, 144.0f);
    /* moedas */
    coinStack(446.0f, 27.0f);
    text(466, 6, 0.26f, COL(255, 215, 70), "MOEDAS");
    sprintf(tb, "%d", shCoins);
    textFit(466, 18, 0.5f, COL(255, 235, 120), tb, 160.0f);
}

static void drawMenuCard(int it, float x, float y, float w, float h, int sel)
{
    static const unsigned char CARD_C[6][6] = {        /* cor do topo (r,g,b) e da base (r,g,b) de cada item */
        {74, 104, 200, 40, 62, 142},                   /* 0 modo treino: azul */
        {92, 206, 60, 40, 140, 34},                    /* 1 jogar: verde */
        {150, 84, 226, 84, 44, 160},                   /* 2 torneio: roxo */
        {40, 196, 196, 18, 120, 134},                  /* 3 player: azul-petroleo */
        {255, 176, 40, 222, 108, 12},                  /* 4 loja: laranja */
        {112, 126, 150, 62, 72, 94} };                 /* 5 opcoes: cinza-azulado */
    static const char *L1[6] = { "MODO", "JOGAR", "JOGAR", "PLAYER", "LOJA", "OPCOES" };
    static const char *L2[6] = { "TREINO", "PARTIDA", "TORNEIO", "DE MUSICAS", "DE ITENS", "E CREDITOS" };
    const unsigned char *c = CARD_C[it];
    float pul = 0.5f + 0.5f * sinf(menuT * 0.12f), cx = x + w * 0.5f, y1;
    int k, nb = 6;
    if (sel) rect(x - 6, y - 6, x + w + 6, y + h + 6, CA(255, 225, 90, 0x16 + (int)(0x14 * pul)));
    rect(x + 3, y + 5, x + w + 3, y + h + 6, CA(0, 0, 0, 0x38));                                 /* sombra */
    cham(x - 2, y - 2, x + w + 2, y + h + 2, 6.0f, sel ? COL(255, 236, 120) : COL(14, 12, 30));   /* borda */
    cham(x, y, x + w, y + h, 5.0f, COL(c[3] * 6 / 10, c[4] * 6 / 10, c[5] * 6 / 10));            /* aro escuro */
    for (k = 0; k < nb; k++) {                                                                   /* degrade do cartao */
        float u = k / (nb - 1.0f), ya = y + 3 + (h - 6) * k / nb, yb = y + 3 + (h - 6) * (k + 1) / nb + 1.0f;
        if (yb > y + h - 3) yb = y + h - 3;
        rect(x + 3, ya, x + w - 3, yb, COL(c[0] + (int)((c[3] - c[0]) * u), c[1] + (int)((c[4] - c[1]) * u), c[2] + (int)((c[5] - c[2]) * u)));
    }
    rect(x + 3, y + 3, x + w - 3, y + 3 + h * 0.22f, CA(255, 255, 255, 0x16));                   /* brilho no topo */
    frameR(x + 7, y + 7, x + w - 7, y + h - 7, 1.0f, CA(255, 255, 255, 0x30));                   /* moldura interna */
    menuIcon(it, cx, y + h * 0.32f, sel ? 1.5f : 1.3f, 1);
    y1 = y + h * 0.58f;
    textCF(cx + 1.5f, y1 + 2.0f, 0.46f, CA(0, 0, 0, 0x70), L1[it], w - 14.0f);
    textCF(cx, y1, 0.46f, COL(255, 255, 255), L1[it], w - 14.0f);
    textCF(cx, y1 + 22.0f, 0.3f, COL(235, 245, 255), L2[it], w - 12.0f);
}

static void drawMenuScreen(void)
{
    static const char *ITEM[6] = { "MODO TREINO", "JOGAR", "TORNEIO", "PLAYER DE MUSICAS", "LOJA", "OPCOES" };
    static const char *DESC[6] = { "JOGUE SOZINHO NA MESA, SEM ADVERSARIO, TEMPO OU FALTAS",
                                   "BOLA 8, BRASILEIRA, MINI, SUBITA, CORRIDA E DUELO",
                                   "MATA-MATA COM 8 JOGADORES - SO TACO CLASSICO",
                                   "OUCA AS MUSICAS DO SEU PEN DRIVE",
                                   "GASTE SUAS MOEDAS: GIZ, BOLAS, RASTROS, EFEITOS E MOLDURAS",
                                   "SOM, MUSICA, VOLUME, EFEITOS E CREDITOS" };
    char buf[48];
    float s1 = sinf(menuT * 0.05f), s2 = sinf(menuT * 0.05f + 2.1f);

    drawBackdrop();

    if (state == S_SETUP) { drawSetupScreen(); return; }
    if (state == S_NAME)  { drawNameScreen();  return; }
    if (state == S_OPTS)  { drawOptionsScreen(); return; }
    if (state == S_BRACKET) { drawBracketScreen(); return; }
    if (state == S_SHOP) { drawShopScreen(); return; }

    if (state == S_CREDITS) {
        u64 pc = COL(255, 200 + (int)(40 * s1), 50 + (int)(30 * s2));
        panel(150, 100, 490, 372);
        uiTitle("CREDITOS");
        rect(200, 150, 440, 152, COL(60,110,90));
        textC(168, 0.42f, COL(190,200,225), "8 BALL REIS - PS2 EDITION");
        textC(214, 0.5f, COL(160,170,190), "CRIADOR");
        textC(236, 1.0f, pc, GAME_AUTHOR);
        rect(230, 286, 410, 288, COL(60,110,90));
        rect(230 + fmodf(menuT * 2.5f, 150.0f), 285, 270 + fmodf(menuT * 2.5f, 150.0f), 289, COL(240,240,200));
        textC(302, 0.5f, COL(160,170,190), "VERSAO");
        textC(324, 0.9f, COL(255,255,255), GAME_VERSION);
        {
            static const int fk[2] = { 0, 1 };
            static const char *const fl[2] = { "OK", "VOLTAR" };
            uiFoot(2, fk, fl);
        }
        return;
    }

    drawMenuTop();

    /* logo */
    {
        float ty = 54.0f + 2.0f * sinf(menuT * 0.06f), sc = 1.0f;
        const char *tt = "8 BALL REIS";
        float x0 = 320.0f - tw(sc, tt) * 0.5f;
        u64 glow = GS_SETREG_RGBAQ(140, 70, 10, 0x40, 0x00);
        u64 tc = COL(255, 200 + (int)(45 * s1), 40 + (int)(30 * s2));
        int e;
        text(x0 + 4, ty + 5, sc, GS_SETREG_RGBAQ(0, 0, 0, 0x60, 0x00), tt);          /* sombra */
        text(x0 - 3, ty - 3, sc, glow, tt); text(x0 + 3, ty - 3, sc, glow, tt);
        text(x0 - 3, ty + 3, sc, glow, tt); text(x0 + 3, ty + 4, sc, glow, tt);
        text(x0, ty, sc, tc, tt);
        text(x0, ty - 1.5f, sc, GS_SETREG_RGBAQ(255, 250, 205, 0x30, 0x00), tt);      /* brilho metalico */
        for (e = 0; e < 2; e++) {                                                    /* bolas 8 nos lados */
            float ex = e ? x0 + tw(sc, tt) + 28.0f : x0 - 28.0f, ey = ty + 14.0f;
            discN(ex + 3, ey + 5, 15.0f, GS_SETREG_RGBAQ(0, 0, 0, 0x30, 0x00), 18);
            discN(ex, ey, 15.0f, COL(12, 12, 18), 18);
            discN(ex - 4.5f, ey - 5.5f, 4.0f, GS_SETREG_RGBAQ(255, 255, 255, 0x40, 0x00), 8);
            discN(ex, ey, 7.5f, COL(245, 245, 245), 14);
            text(ex - tw(0.45f, "8") * 0.5f + 1.0f, ey - 7.0f, 0.45f, COL(10, 10, 14), "8");
        }
        textC(ty + 40.0f, 0.3f, COL(255, 225, 150), "BY " GAME_AUTHOR);
    }

    /* cartoes dos modos + caixa de informacoes */
    {
        static const char *BUL[6][3] = {
            { "A AVENTURA CHEGA EM BREVE", "PERSONAGENS, MESAS E TACOS", "UMA CAMPANHA CONTRA RIVAIS" },
            { "CONTRA O BOT OU UM AMIGO", "6 MODOS DE JOGO", "18 PERSONAGENS, 28 MESAS" },
            { "MATA-MATA COM 8 JOGADORES", "QUARTAS, SEMI E FINAL", "PERDEU, ESTA FORA!" },
            { "MP3 E WAV DO PEN DRIVE", "ORDEM ALEATORIA", "" },
            { "GIZ, BOLAS E RASTROS", "EFEITOS DE CACAPA", "TITULOS E MOLDURAS" },
            { "SOM, MUSICA E VOLUME", "EFEITOS E DICAS NA TELA", "CREDITOS DO JOGO" } };
        static const int fk[2] = { 7, 0 };
        static const char *const fl[2] = { "ESCOLHER", "CONFIRMAR" };
        char mb[160];
        int sl, q;
        const float CW = 94.0f, CG = 10.0f, CY = 138.0f, CH = 122.0f;

        for (sl = 0; sl < 6; sl++) {                                  /* o selecionado por ultimo: fica por cima e maior */
            int it = MENU_ORD[sl];
            if (it != menuSel) drawMenuCard(it, 13.0f + sl * (CW + CG), CY, CW, CH, 0);
        }
        sl = menuSlot(menuSel);
        drawMenuCard(menuSel, 13.0f + sl * (CW + CG) - 4.0f, CY - 10.0f, CW + 8.0f, CH + 14.0f, 1);

        panel(36, 292, 604, 352);
        textCF(320.0f, 298.0f, 0.46f, COL(255, 215 + (int)(30 * s1), 70), ITEM[menuSel], 540.0f);
        textCF(320.0f, 322.0f, 0.3f, COL(150, 220, 255), DESC[menuSel], 540.0f);
        mb[0] = 0;
        for (q = 0; q < 3; q++) {
            char one[40];
            const char *bl = BUL[menuSel][q];
            if (menuSel == 3 && q == 2) { sprintf(one, "%d MUSICAS ENCONTRADAS", musicCount()); bl = one; }
            if (menuSel == 4 && q == 0) { sprintf(one, "VOCE TEM %d MOEDAS", shCoins); bl = one; }
            if (!bl[0]) continue;
            if (mb[0]) strcat(mb, "  -  ");
            strcat(mb, bl);
        }
        textCF(320.0f, 337.0f, 0.26f, COL(190,200,215), mb, 540.0f);

        rect(36, 392, 604, 393, CA(80, 110, 100, 0x50));
        text(36, 400, 0.28f, COL(215,225,235), "CRIADOR: " GAME_AUTHOR);
        sprintf(buf, "VERSAO %s", GAME_VERSION);
        text(604.0f - tw(0.28f, buf), 400, 0.28f, COL(215,225,235), buf);
        uiFoot(2, fk, fl);
    }
}

/* ---------- pausa (START) ---------- */
static int paused, pauseSel, pauseView;          /* pauseView: 0 = lista, 1 = controles e dicas */
enum { PI_RESUME, PI_REPLAY, PI_SND, PI_MUS, PI_VOL, PI_FX, PI_TIPS, PI_WB0, PI_GD0, PI_WB1, PI_GD1, PI_PLAYER, PI_CTRL, PI_RESTART, PI_MENU, NPI };

/* linhas visiveis do menu de pausa: as cores do jogador 2 so aparecem no versus */
static int pauseRows(int *v)
{
    int n = 0, k;
    for (k = 0; k < NPI; k++) {
        if (k == PI_WB0 || k == PI_WB1) continue;
        if (tourney && k == PI_RESTART) continue;           /* torneio: nao da para reiniciar a partida */            /* cor da bola branca: so no menu Configurar partida */
        if (mode == 1 || k != PI_GD1) v[n++] = k;
    }
    return n;
}
static int plSel;                                /* cursor da lista de musicas */

static void pauseOpen(void) { paused = 1; pauseSel = 0; pauseView = 0; plSel = musicCur() > 0 ? musicCur() : 0; }

/* entrada do player de musicas; devolve 1 quando o jogador quer sair dele */
static int playerInput(u32 pr)
{
    int d = ((pr & PAD_RIGHT) ? 1 : 0) - ((pr & PAD_LEFT) ? 1 : 0);
    {
        int n = musicCount();
        if (pr & (PAD_CIRCLE | PAD_START)) return 1;
        if (n > 0) {
            if (pr & PAD_UP)   plSel = (plSel + n - 1) % n;
            if (pr & PAD_DOWN) plSel = (plSel + 1) % n;
            if (pr & PAD_CROSS)    { musOn = 1; musicSetEnabled(1); musicPlay(plSel); }
            if (pr & PAD_SQUARE)   { musicToggle(); musOn = (musicState() == 1); }
            if (pr & PAD_L1)       { musicNext(-1); plSel = musicCur() >= 0 ? musicCur() : plSel; }
            if (pr & PAD_R1)       { musicNext(1);  plSel = musicCur() >= 0 ? musicCur() : plSel; }
        }
        if (d) { volLvl += d; if (volLvl < 0) volLvl = 0; if (volLvl > 10) volLvl = 10; musicSetVolume(volLvl); }
        if (pr & PAD_TRIANGLE) { musicScan(); plSel = 0; }
    }
    return 0;
}

static void pauseInput(u32 pr)
{
    int d = ((pr & PAD_RIGHT) ? 1 : 0) - ((pr & PAD_LEFT) ? 1 : 0), tog = d || (pr & PAD_CROSS);
    if (pauseView == 1) {
        if (pr & (PAD_CROSS | PAD_CIRCLE | PAD_START)) pauseView = 0;
        return;
    }
    if (pauseView == 2) {                            /* player de musicas */
        if (playerInput(pr)) pauseView = 0;
        return;
    }
    int pv[NPI], pn = pauseRows(pv), row;
    if (pauseSel >= pn) pauseSel = 0;
    if (pr & PAD_UP)   pauseSel = (pauseSel + pn - 1) % pn;
    if (pr & PAD_DOWN) pauseSel = (pauseSel + 1) % pn;
    if (pr & (PAD_START | PAD_CIRCLE)) { paused = 0; return; }
    row = pv[pauseSel];
    switch (row) {
    case PI_GD0: case PI_GD1: if (tog) { int w = row == PI_GD0 ? 0 : 1, dd = d ? d : 1; gdSel[w] = (gdSel[w] + dd + NGD) % NGD; } break;
    case PI_SND:     if (tog) sndOn = !sndOn; break;
    case PI_MUS:     if (tog) { musOn = !musOn; musicSetEnabled(musOn); } break;
    case PI_VOL:     if (d) { volLvl += d; if (volLvl < 0) volLvl = 0; if (volLvl > 10) volLvl = 10; musicSetVolume(volLvl); } break;
    case PI_FX:      if (tog) fxOn = !fxOn; break;
    case PI_TIPS:    if (tog) tipsOn = !tipsOn; break;
    case PI_RESUME:  if (pr & PAD_CROSS) paused = 0; break;
    case PI_REPLAY:  if ((pr & PAD_CROSS) && repKeepN > 1) { paused = 0; repLaunch(state); } break;   /* rever a ultima bola que caiu */
    case PI_PLAYER:  if (pr & PAD_CROSS) pauseView = 2; break;
    case PI_CTRL:    if (pr & PAD_CROSS) pauseView = 1; break;
    case PI_RESTART: if (pr & PAD_CROSS) { paused = 0; rs += 7919 + menuT; newGame(mode); } break;
    case PI_MENU:    if (pr & PAD_CROSS) { paused = 0; tourney = 0; menuInit(); state = S_MENU; } break;
    }
}

/* rodape de duas linhas (para telas com muitos botoes): barra mais alta, cada linha centralizada */
static void uiFootRow(float ky, float ty, int n, const int *ks, const char *const *ls)
{
    float tot = 0.0f, x;
    int i;
    for (i = 0; i < n; i++) tot += UIKW[ks[i]] + 6.0f + tw(0.3f, ls[i]) + (i + 1 < n ? 18.0f : 0.0f);
    x = 320.0f - tot * 0.5f;
    for (i = 0; i < n; i++) {
        uiKey(x, ky, ks[i]);
        x += UIKW[ks[i]] + 6.0f;
        text(x, ty, 0.3f, COL(185,195,210), ls[i]);
        x += tw(0.3f, ls[i]) + 18.0f;
    }
}

static void uiFoot2(int n1, const int *k1, const char *const *l1, int n2, const int *k2, const char *const *l2)
{
    rect(0, 398, 640, 448, CA(0, 0, 12, 0x58));
    rect(0, 398, 640, 399, CA(255, 205, 60, 0x44));
    uiFootRow(401, 404, n1, k1, l1);
    uiFootRow(425, 428, n2, k2, l2);
}

static void drawPause(void)
{
    static const char *LB[NPI] = { "CONTINUAR", "REPLAY", "SOM (EFEITOS)", "MUSICA", "VOLUME DA MUSICA", "EFEITOS", "DICAS NA TELA", "", "", "", "",
                                   "PLAYER DE MUSICAS", "CONTROLES E DICAS", "REINICIAR PARTIDA", "MENU PRINCIPAL" };
    char buf[32];
    int i;
    rect(0, 0, 640, 448, GS_SETREG_RGBAQ(0, 0, 0, 0x60, 0x00));

    if (pauseView == 2) {                            /* player de musicas */
        static const int fa[4] = { 6, 0, 3, 8 };
        static const char *const fla[4] = { "ESCOLHER", "TOCAR", "PAUSAR", "FAIXA" };
        static const int fb2[3] = { 7, 2, 1 };
        static const char *const flb[3] = { "VOLUME", "PROCURAR", "VOLTAR" };
        int n = musicCount(), top, rows = 8;
        char b[96];
        uiTitle("MUSICAS");
        panel(40, 46, 600, 390);
        snprintf(b, sizeof b, "%s   VOLUME %d%%", musicStatus(), volLvl * 10);
        textCF(320, 56, 0.34f, COL(150,220,255), b, 500.0f);
        if (musicSource()[0]) { snprintf(b, sizeof b, "PASTA: %s", musicSource()); textCF(320, 74, 0.28f, COL(140,150,165), b, 500.0f); }
        rect(64, 92, 576, 93, CA(120, 140, 160, 0x50));
        if (n == 0) {
            textCF(320, 128, 0.4f, COL(255,235,170), "COLOQUE SUAS MUSICAS (MP3 OU WAV)", 520.0f);
            textCF(320, 154, 0.4f, COL(255,235,170), "NA PASTA MUSIC DO PEN DRIVE OU HD USB", 520.0f);
            textCF(320, 180, 0.4f, COL(255,235,170), "(OU AO LADO DO JOGO, EM UMA PASTA MUSIC)", 520.0f);
            textCF(320, 226, 0.34f, COL(190,200,215), "ESPERE UNS SEGUNDOS E APERTE TRIANGULO PARA PROCURAR", 520.0f);
        } else {
            int i;
            top = plSel - rows / 2; if (top > n - rows) top = n - rows; if (top < 0) top = 0;
            for (i = 0; i < rows && top + i < n; i++) {
                int k = top + i;
                float y = 106.0f + i * 29.0f;
                u64 tc = (k == plSel) ? COL(255,240,150) : COL(175,185,195);
                if (k == plSel) selBar(52, 588, y, 22);
                if (k == musicCur() && musicState() == 1) tri(88, y + 1, 88, y + 15, 99, y + 8, COL(90,255,130));
                else if (k == musicCur()) rect(88, y + 1, 96, y + 15, COL(255,200,70));
                snprintf(b, sizeof b, "%d. %s", k + 1, musicName(k));
                textFit(112, y, 0.45f, tc, b, 460.0f);
            }
            snprintf(b, sizeof b, "%d / %d", plSel + 1, n);
            text(576 - tw(0.3f, b), 352, 0.3f, COL(140,150,165), b);
        }
        uiFoot2(4, fa, fla, 3, fb2, flb);
        return;
    }
    if (pauseView == 1) {
        static const int fk[3] = { 0, 1, 4 };
        static const char *const fl[3] = { "", "", "VOLTAR" };
        static const char *K[] = { "ESQ/DIR", "L1/R1", "X", "O", "L2/R2", "CIMA/BAIXO", "TRIANGULO", "QUADRADO", "SELECT", "BOLA NA MAO", "START" };
        static const char *V[] = {
            "GIRAR A MIRA", "AJUSTE FINO LENTO (PRECISAO)", "BARRA DE FORCA; X DE NOVO DISPARA", "CANCELAR A TACADA",
            "EFEITO LATERAL NA BRANCA", "ROLA (CIMA) / PUXA (BAIXO)", "PULO: A BRANCA SOBE E PASSA POR CIMA", "PASSAR GIZ NO TACO", "TROCAR DE TACO", "SETAS MOVEM A BRANCA, X CONFIRMA", "PAUSAR O JOGO"
        };
        static const char *T[] = {
            "PULO: MAIS FORCA E TACO COM EFEITO ALTO = PULO MAIS ALTO",
            "BARRA DE FORCA: APERTE X QUANDO ELA ESTIVER NO ALTO",
            "TACO SNIPER: LASER MOSTRA A TACADA INTEIRA, MAS BATE FRACO",
            "BRASILEIRA: SE VOCE ERRAR, O ADVERSARIO TIRA UMA BOLA DELE",
            "BRANCA NA CACAPA NA BRASILEIRA = DERROTA NA HORA"
        };
        uiTitle("CONTROLES");
        panel(30, 46, 610, 394);
        for (i = 0; i < 11; i++) {
            float y = 54.0f + i * 17.0f;
            if (i & 1) rect(40, y - 3, 600, y + 14, CA(255,255,255,0x0c));
            text(56, y, 0.38f, COL(255,230,110), K[i]);
            text(200, y, 0.38f, COL(225,230,240), V[i]);
        }
        uiSection(52, 588, 250, COL(255,190,110), "DICAS");
        for (i = 0; i < 5; i++) textFit(56, 270.0f + i * 22.0f, 0.34f, COL(190,200,215), T[i], 530.0f);
        uiFoot(3, fk, fl);
        return;
    }

    {
        static const int fk[4] = { 6, 7, 0, 1 };
        static const char *const fl[4] = { "ESCOLHER", "MUDAR", "OK", "CONTINUAR" };
        int pv[NPI], pn = pauseRows(pv), row;
        float sp = pn > 12 ? 22.0f : (pn > 10 ? 25.0f : 29.5f);
        float bh = sp - 7.5f;
        if (bh > 22.0f) bh = 22.0f;
        uiTitle("PAUSADO");
        panel(110, 46, 530, 396);
        if (pauseSel >= pn) pauseSel = 0;
        for (i = 0; i < pn; i++) {
            float y = 62.0f + i * sp;
            int sel = (i == pauseSel);
            u64 tc = sel ? COL(255,240,150) : COL(180,190,200);
            char lb[24];
            row = pv[i];
            if (sel) selBar(118, 522, y, bh);
            if (row == PI_WB0 || row == PI_WB1) sprintf(lb, mode == 0 ? "BOLA BRANCA" : "BRANCA J%d", row == PI_WB0 ? 1 : 2);
            else if (row == PI_GD0 || row == PI_GD1) sprintf(lb, mode == 0 ? "LINHA GUIA" : "GUIA J%d", row == PI_GD0 ? 1 : 2);
            else strcpy(lb, LB[row]);
            text(152, y + 1, 0.46f, tc, lb);
            switch (row) {
            case PI_SND:  uiToggle(510, y - 1, sndOn, "LIGADO", "DESLIGADO"); break;
            case PI_MUS:  uiToggle(510, y - 1, musOn, "LIGADA", "DESLIGADA"); break;
            case PI_FX:   uiToggle(510, y - 1, fxOn, "LIGADOS", "DESLIGADOS"); break;
            case PI_TIPS: uiToggle(510, y - 1, tipsOn, "LIGADAS", "DESLIGADAS"); break;
            case PI_VOL: {
                int q;
                for (q = 0; q < 10; q++) {
                    float bx = 380.0f + q * 13.0f;
                    rect(bx, y + 1, bx + 10, y + 17, q < volLvl ? COL(255, 205 - q * 6, 60) : COL(40,46,60));
                }
                sprintf(buf, "%d%%", volLvl * 10);
                text(372 - tw(0.34f, buf), y + 3, 0.34f, COL(255,255,255), buf);
                break; }
            case PI_WB0: case PI_WB1: case PI_GD0: case PI_GD1: {
                int w = (row == PI_WB0 || row == PI_GD0) ? 0 : 1, isWb = (row == PI_WB0 || row == PI_WB1);
                sprintf(buf, "< %s >", isWb ? WB_NAME[wbSel[w]] : GD_NAME[gdSel[w]]);
                text(510 - tw(0.4f, buf), y + 2, 0.4f, COL(255,255,255), buf);
                drawSwatch(isWb ? 0 : 1, isWb ? wbSel[w] : gdSel[w], 510 - tw(0.4f, buf) - (isWb ? 14 : 20), y + 8);
                break; }
            case PI_RESUME: break;
            default:
                text(510 - tw(0.46f, ">"), y + 1, 0.46f, COL(255,230,60), ">");
                break;
            }
        }
        row = pv[pauseSel];
        if (row == PI_REPLAY)
            textC(404, 0.3f, repKeepN > 1 ? COL(150,220,255) : COL(255,190,120), repKeepN > 1 ? "X: REVER A ULTIMA BOLA QUE CAIU" : "AINDA NAO CAIU NENHUMA BOLA");
        else if (row == PI_SND)
            textC(404, 0.3f, COL(255,190,120), musicReady() ? "EFEITOS SONOROS ATIVOS" : musicStatus());
        else if (row == PI_MUS || row == PI_VOL || row == PI_PLAYER)
            textC(404, 0.3f, COL(150,220,255), musicStatus());
        else if (row == PI_GD0 || row == PI_GD1) {
            int w = (row == PI_GD0) ? 0 : 1;
            char hb[64];
            if (mode == 0) strcpy(hb, "A COR DA LINHA MUDA NA HORA");
            else sprintf(hb, "COR DE %s: VALE QUANDO E A VEZ DELE", pnames[w]);
            textC(404, 0.3f, COL(150,220,255), hb);
        }
        uiFoot(4, fk, fl);
    }
}

static void drawChalkFx(float bx, float by, float d0)
{
    chalkFxAt(bx - cosf(ang) * d0, by - sinf(ang) * d0, ang, chalkAnim, shEq[SC_GIZ]);
}

static void drawRewind(void)
{
    float pulse = 0.5f + 0.5f * sinf(menuT * 0.12f);
    rect(0, 0, 640, 448, CA(10, 14, 40, 0x50));
    hbox(110, 150, 530, 300, CA(120, 200, 255, 0x90), CA(12, 16, 38, 0x60));
    ringN(320, 180, 14.0f + 2.0f * pulse, 2.0f, 28, CA(255, 214, 90, 0x90));
    line(320, 180, 320, 170, CA(255, 214, 90, 0xa0));
    line(320, 180, 328, 184, CA(120, 200, 255, 0xa0));
    textC(204, 0.44f, COL(255,214,90), "MESTRE DO TEMPO");
    textC(228, 0.3f, COL(255,150,130), tmNote);
    textC(246, 0.3f, COL(215,220,232), "VOLTAR NO TEMPO E REFAZER A JOGADA?");
    text(190, 272, 0.4f, tmSel == 0 ? COL(110,255,140) : COL(130,138,155), tmSel == 0 ? "> SIM" : "  SIM");
    text(350, 272, 0.4f, tmSel == 1 ? COL(255,120,110) : COL(130,138,155), tmSel == 1 ? "> NAO" : "  NAO");
    textC(290, 0.24f, COL(150,155,170), "SETAS ESCOLHEM   X CONFIRMA");
}

/* POLICIAL / COVEIRO: marcas na mesa (algemas na bola presa, alvo das algemas, fantasma da ressuscitada e a sirene) */
static void drawCuffs(float x, float y, int al)
{
    ringN(x - 4.2f, y, 4.4f, 1.8f, 14, CA(236, 238, 246, al));
    ringN(x + 4.2f, y, 4.4f, 1.8f, 14, CA(236, 238, 246, al));
    line(x - 1.0f, y, x + 1.0f, y, CA(150, 158, 176, al));
}
static void drawCopMarks(void)
{
    int rb = (menuT / 8) & 1;
    if (state != S_REPLAY && lockBall > 0 && b[lockBall].in) {      /* bola presa: aro vermelho/azul e algemas por cima */
        float x = b[lockBall].x, y = b[lockBall].y, pu = 1.2f * sinf(menuT * 0.25f);
        ringN(x, y, R + 2.6f + pu, 2.2f, 22, rb ? CA(255, 60, 70, 0xa0) : CA(80, 140, 255, 0xa0));
        discN(x, y, R * 0.62f, CA(20, 24, 40, 0x90), 12);
        drawCuffs(x, y, 0xd0);
    }
    if (state != S_REPLAY && cornBall > 0 && b[cornBall].in) {      /* CHAPOLIN: bola paralisada pela corneta (aro amarelo/vermelho e um Z) */
        float x = b[cornBall].x, y = b[cornBall].y, pu = 1.2f * sinf(menuT * 0.25f);
        ringN(x, y, R + 2.6f + pu, 2.2f, 22, rb ? CA(255, 214, 40, 0xa0) : CA(255, 80, 60, 0xa0));
        discN(x, y, R * 0.62f, CA(20, 24, 40, 0x90), 12);
        line(x - 3.0f, y - 3.0f, x + 3.0f, y - 3.0f, CA(255, 255, 255, 0xc0));
        line(x + 3.0f, y - 3.0f, x - 3.0f, y + 3.0f, CA(255, 255, 255, 0xc0));
        line(x - 3.0f, y + 3.0f, x + 3.0f, y + 3.0f, CA(255, 255, 255, 0xc0));
    }
    if (state == S_AIM && !isBot() && abilArm && curAb() == AB_MARRETA && chOpen && chSel == 0 && copTgt > 0 && b[copTgt].in) {   /* alvo da corneta */
        float bx = b[copTgt].x, by = b[copTgt].y, bob = 3.0f * sinf(menuT * 0.2f);
        ringN(bx, by, R + 3.5f, 1.8f, 24, CA(255, 214, 40, 0x7c));
        ringN(bx, by, R + 6.5f + bob * 0.4f, 1.2f, 24, CA(255, 214, 40, 0x40));
        tri(bx - 6, by - R - 18 - bob, bx + 6, by - R - 18 - bob, bx, by - R - 9 - bob, COL(255,214,40));
    }
    if (state == S_AIM && !isBot() && abilArm && curAb() == AB_POLICIAL && copSel == 0 && copTgt > 0 && b[copTgt].in) {   /* alvo das algemas */
        float bx = b[copTgt].x, by = b[copTgt].y, bob = 3.0f * sinf(menuT * 0.2f);
        ringN(bx, by, R + 3.5f, 1.8f, 24, CA(255, 230, 60, 0x7c));
        ringN(bx, by, R + 6.5f + bob * 0.4f, 1.2f, 24, CA(255, 230, 60, 0x40));
        tri(bx - 6, by - R - 18 - bob, bx + 6, by - R - 18 - bob, bx, by - R - 9 - bob, COL(255,230,60));
        drawCuffs(bx, by, 0xa0);
    }
    if (ghostBall > 0 && b[ghostBall].in && (state == S_ROLL || state == S_STRIKE)) {   /* ressuscitada: fantasma (a branca passa por ela) */
        float gx = b[ghostBall].x, gy = b[ghostBall].y, pu = 1.5f * sinf(menuT * 0.3f);
        ringN(gx, gy, R + 2.5f + pu, 1.6f, 22, CA(150, 255, 170, 0x90));
        discN(gx, gy, R + 1.0f, CA(150, 255, 170, 0x18), 16);
    }
    if (chiqOn && (state == S_AIM || state == S_ROLL))               /* CHAPOLIN: aviso da chiquitolina */
        textC(88, 0.28f, COL(255,235,90), "CHIQUITOLINA! BOLAS DO ADVERSARIO PEQUENAS E LENTAS");
    if (state == S_AIM && !isBot() && sirenT[cur]) {                /* SIRENE sobre quem esta mirando */
        u64 sc = rb ? CA(255, 50, 60, 0x50) : CA(70, 130, 255, 0x50);
        rect(0, 0, 640, 8, sc); rect(0, 440, 640, 448, sc); rect(0, 0, 8, 448, sc); rect(632, 0, 640, 448, sc);
        textC(74, 0.34f, rb ? COL(255,110,120) : COL(130,175,255), "SIRENE! SEM LINHA GUIA");
    }
}

static void drawBats(void)
{
    int k;
    if (!batOn) return;
    for (k = 0; k < NBAT; k++) {
        float hv = (state == S_ROLL) ? 0.0f : 2.5f * sinf(menuT * 0.12f + k * 1.7f);
        float fl = sinf(menuT * 0.45f + k * 2.1f);
        float x = batX[k], y = batY[k] + hv;
        discN(x + 2.0f, y + 7.0f, 7.0f, CA(0, 0, 0, 0x30), 10);      /* sombra */
        batShape(x, y, 1.1f, fl, 0xe0);
        disc(x - 1.6f, y - 0.5f, 0.9f, CA(255, 40, 40, 0xe0));
        disc(x + 1.6f, y - 0.5f, 0.9f, CA(255, 40, 40, 0xe0));
    }
}

static void drawReplayTag(void)                       /* etiqueta REPLAY e dica para pular */
{
    int blink = (menuT / 20) & 1;
    hbox(TX0 + 10, TY0 + 8, TX0 + 92, TY0 + 28, CA(255, 80, 80, 0x70), CA(10, 12, 20, 0x70));
    discN(TX0 + 22, TY0 + 18, 4.0f, blink ? CA(255, 50, 50, 0x80) : CA(120, 30, 30, 0x80), 10);
    textFit(TX0 + 32, TY0 + 12, 0.34f, COL(255,255,255), "REPLAY", 56.0f);
    hbox(TCX - 78, TY1 - 28, TCX + 78, TY1 - 8, CA(255, 215, 60, 0x60), CA(10, 12, 20, 0x70));
    textFit(TCX - 70, TY1 - 24, 0.30f, COL(255,215,70), "X: PULAR O REPLAY", 140.0f);
}

static void drawScene(void)
{
    int i;
    if (state == S_MENU || state == S_CREDITS || state == S_OPTS || state == S_SETUP || state == S_NAME || state == S_BRACKET || state == S_SHOP) { drawMenuScreen(); return; }
    if (state == S_PLAYER) { drawBackdrop(); pauseView = 2; drawPause(); return; }
    if (state == S_OVER) {
        drawOverScreen();
        if (musicToast()) {
            char tb[80];
            snprintf(tb, sizeof tb, "MUSICA: %s", musicToast());
            textC(436, 0.28f, COL(190,220,255), tb);
        }
        return;
    }
    drawBackdropEx(0);                                              /* cenario (por do sol + chao) atras da mesa */
    drawTable();
    if ((state == S_ROLL || state == S_STRIKE) && abilKind == AB_COVA) drawGraveHole(1.0f);          /* COVEIRO: a cova aberta */
    else if (state == S_AIM && !isBot() && abilArm && curAb() == AB_COVEIRO && graveSel == 1) drawGraveHole(0.45f);
    if (state != S_PLACE) for (i = 0; i < 16; i++) drawShadow(i);
    else for (i = 1; i < 16; i++) drawShadow(i);
    if (state == S_ROLL && shEq[SC_RASTRO]) for (i = 0; i < 16; i++) if (b[i].in && b[i].z <= 0.0f) trailDraw(shEq[SC_RASTRO], b[i].x, b[i].y, b[i].vx, b[i].vy);   /* LOJA: rastro */
    for (i = 1; i < 16; i++) if (b[i].in) {
        if (b[i].z > 0.0f) drawBallAt(i, b[i].x, b[i].y - b[i].z * 0.55f, R * (1.0f + b[i].z * 0.022f));   /* CHAPULIN: a bola sobe na tela */
        else drawBallAt(i, b[i].x, b[i].y, ballRad(i));   /* CHIQUITOLINA: bolas do adversario menores */
    }
    if (b[0].in) {
        if (state == S_PLACE) {
            if (validPlace(b[0].x, b[0].y)) drawBallAt(0, b[0].x, b[0].y, R);
            else disc(b[0].x, b[0].y, R, COL(230,60,60));
        } else if (b[0].z > 0.0f) {                         /* pulo: a bola cresce e sobe na tela */
            float z = b[0].z, r = R * (1.0f + z * 0.022f), yy = b[0].y - z * 0.55f;
            drawBallAt(0, b[0].x, yy, r);
            ringN(b[0].x, yy, r + 1.2f, 1.2f, 18, CA(255, 255, 255, 0x2c));
        } else drawBallAt(0, b[0].x, b[0].y, R);
    }
    for (i = 0; i < 16; i++) if (fallT[i] > 0) drawFall(i);
    drawBats();
    pfxDraw();
    drawCopMarks();
    if (state == S_ROLL || state == S_STRIKE) {                      /* efeitos das habilidades */
        if (abilKind == AB_CONGELAR) {
            rect(TX0, TY0, TX1, TY1, CA(150, 215, 255, 0x12));
            for (i = 0; i < 18; i++) {
                float sx = TX0 + fmodf(i * 97.3f, TX1 - TX0), sy = TY0 + fmodf(i * 53.7f, TY1 - TY0), tw2 = sinf(menuT * 0.15f + i);
                if (tw2 > 0.2f) { line(sx - 3, sy, sx + 3, sy, CA(235, 248, 255, 0x60)); line(sx, sy - 3, sx, sy + 3, CA(235, 248, 255, 0x60)); }
            }
        }
        if (pockMul > 1.0f) {                                          /* CHAMINE: aros dourados nas cacapas maiores */
            int pk;
            for (pk = 0; pk < 6; pk++) {
                float pr = PRC[pk] * pockMul, pu = 1.5f * sinf(menuT * 0.25f + pk);
                ringN(PX[pk], PY[pk], pr + pu, 2.4f, 32, CA(255, 214, 80, 0x90));
                ringN(PX[pk], PY[pk], pr * 0.7f, 1.4f, 28, CA(255, 245, 190, 0x50));
            }
        }
        if (abilKind == AB_RASTREIO && firstHit < 0 && rastT > 0 && b[rastT].in) {      /* ALIEN: mira verde na bola perseguida */
            float rp = 2.0f * sinf(menuT * 0.3f);
            ringN(b[rastT].x, b[rastT].y, R + 4.0f + rp, 1.8f, 20, CA(110, 255, 150, 0x90));
            line(b[0].x, b[0].y, b[rastT].x, b[rastT].y, CA(110, 255, 150, 0x30));
        }
        if (abilKind == AB_SUGAR && abilT > 0 && firstHit > 0 && b[firstHit].in)
            ringN(b[firstHit].x, b[firstHit].y, R + 4.0f + 2.0f * sinf(menuT * 0.3f), 1.8f, 20, CA(255, 40, 70, 0x90));
        if (abilKind == AB_ENTERRO && buryT > 0 && buryBall > 0 && b[buryBall].in) {      /* COVEIRO: bola enterrada e a cacapa do destino */
            ringN(b[buryBall].x, b[buryBall].y, R + 4.0f + 2.0f * sinf(menuT * 0.3f), 1.8f, 20, CA(150, 255, 170, 0x90));
            ringN(PX[buryP], PY[buryP], PR[buryP] + 7.0f + 2.0f * sinf(menuT * 0.3f), 1.6f, 24, CA(150, 255, 170, 0x70));
        }
        if (abilFx > 0 && abilKind != AB_NONE) {
            float fl = abilFx / 36.0f, u = 1.0f - fl;
            if (abilKind == AB_TIRO) {
                float ex = fxX + cosf(ang) * (R + 4.0f), ey = fxY + sinf(ang) * (R + 4.0f);
                beam(ex, ey, ex + cosf(ang) * 34.0f * fl, ey + sinf(ang) * 34.0f * fl, 6.0f * fl, CA(255, 225, 90, (int)(0x78 * fl)));
                discN(ex, ey, 7.0f * fl, CA(255, 255, 210, (int)(0x78 * fl)), 10);
            } else if (abilKind == AB_FOGO) {
                ringN(fxX, fxY, 8.0f + u * 70.0f, 3.0f, 28, CA(255, 120, 30, (int)(0x78 * fl)));
                ringN(fxX, fxY, 4.0f + u * 45.0f, 2.0f, 24, CA(255, 220, 90, (int)(0x78 * fl)));
            } else if (abilKind == AB_CHIQUI) {                        /* CHAPOLIN: ondas verdes que encolhem */
                ringN(TCX, TCY, 140.0f * (1.0f - u) + 6.0f, 2.4f, 36, CA(120, 255, 110, (int)(0x70 * fl)));
                ringN(TCX, TCY, 70.0f * (1.0f - u) + 4.0f, 1.6f, 28, CA(255, 240, 120, (int)(0x60 * fl)));
            } else if (abilKind == AB_BIONICA) {                       /* CHAPOLIN: faiscas laranja saem da branca */
                ringN(fxX, fxY, 6.0f + u * 34.0f, 2.6f, 20, CA(255, 150, 40, (int)(0x78 * fl)));
                line(fxX - 18.0f * u - 4.0f, fxY, fxX + 18.0f * u + 4.0f, fxY, CA(255, 240, 160, (int)(0x78 * fl)));
                line(fxX, fxY - 18.0f * u - 4.0f, fxX, fxY + 18.0f * u + 4.0f, CA(255, 240, 160, (int)(0x78 * fl)));
            } else if (abilKind == AB_CORNETA) {
                ringN(fxX, fxY, 6.0f + u * 30.0f, 3.0f, 24, CA(255, 214, 40, (int)(0x78 * fl)));
                for (i = 0; i < 4; i++) {
                    float aa = i * 1.5708f + 0.785f, rr = 10.0f + u * 28.0f;
                    line(fxX + cosf(aa) * rr * 0.5f, fxY + sinf(aa) * rr * 0.5f, fxX + cosf(aa) * rr, fxY + sinf(aa) * rr, CA(255, 240, 120, (int)(0x78 * fl)));
                }
            } else if (abilKind == AB_CORTE) {
                line(fxX - 22.0f * u - 6, fxY - 22.0f * u - 6, fxX + 22.0f * u + 6, fxY + 22.0f * u + 6, CA(255, 255, 255, (int)(0x80 * fl)));
                ringN(fxX, fxY, R + 2.0f + u * 16.0f, 1.6f, 20, CA(235, 235, 245, (int)(0x70 * fl)));
            } else if (abilKind == AB_RASTREIO) {                      /* ALIEN: ondas verdes saem da branca */
                ringN(fxX, fxY, R + 2.0f + u * 24.0f, 2.2f, 24, CA(110, 255, 150, (int)(0x78 * fl)));
                ringN(fxX, fxY, R + 2.0f + u * 14.0f, 1.4f, 20, CA(200, 130, 255, (int)(0x60 * fl)));
            } else if (abilKind == AB_IMA) {                           /* ROBO: aneis de ima nas suas bolas */
                int k2;
                for (k2 = 1; k2 < 16; k2++)
                    if (b[k2].in && ownBall(k2)) {
                        ringN(b[k2].x, b[k2].y, R + 2.0f + fl * 12.0f, 1.8f, 16, CA(255, 200, 60, (int)(0x78 * fl)));
                        ringN(b[k2].x, b[k2].y, R + 2.0f + fl * 6.0f, 1.2f, 12, CA(150, 200, 255, (int)(0x60 * fl)));
                    }
            } else if (abilKind == AB_MALDICAO) {                      /* MAGO: aneis roxos giram e se abrem em volta da branca */
                int k3;
                ringN(fxX, fxY, R + 4.0f + u * 30.0f, 2.4f, 8, CA(190, 90, 255, (int)(0x78 * fl)));
                ringN(fxX, fxY, R + 2.0f + u * 16.0f, 1.4f, 8, CA(90, 20, 140, (int)(0x70 * fl)));
                for (k3 = 0; k3 < 6; k3++) {
                    float aa = k3 * 1.0472f + u * 4.0f, rr = R + 6.0f + u * 26.0f;
                    discN(fxX + cosf(aa) * rr, fxY + sinf(aa) * rr, 2.0f * fl + 0.6f, CA(220, 150, 255, (int)(0x80 * fl)), 6);
                }
            } else if (abilKind == AB_VIDAS) {                         /* GATO: uma patinha rosa sobe da branca */
                float py2 = fxY - 12.0f - u * 14.0f;
                int t2;
                discN(fxX, py2, 4.0f * fl + 1.0f, CA(255, 150, 190, (int)(0x80 * fl)), 10);
                for (t2 = -1; t2 <= 1; t2++) discN(fxX + t2 * 5.0f, py2 - 6.0f * fl - 1.0f, 2.0f * fl + 0.6f, CA(255, 190, 215, (int)(0x80 * fl)), 8);
            } else if (abilKind == AB_ENTERRO) {                       /* COVEIRO: terra voando e uma cruz */
                for (i = 0; i < 6; i++) {
                    float aa = i * 1.0472f + 0.5f, rr = 4.0f + u * 22.0f;
                    disc(fxX + cosf(aa) * rr, fxY + sinf(aa) * rr - 10.0f * sinf(u * 3.14159f), 2.2f * fl + 0.5f, CA(110, 76, 42, (int)(0x90 * fl)));
                }
                rect(fxX - 1.0f, fxY - 14.0f - u * 6.0f, fxX + 1.0f, fxY - 2.0f - u * 6.0f, CA(215, 225, 215, (int)(0x90 * fl)));
                rect(fxX - 4.0f, fxY - 10.0f - u * 6.0f, fxX + 4.0f, fxY - 8.0f - u * 6.0f, CA(215, 225, 215, (int)(0x90 * fl)));
            } else if (abilKind == AB_COVA) {                          /* COVEIRO: a terra se abre no meio da mesa */
                ringN(TCX, TCY, 6.0f + u * 40.0f, 2.4f, 26, CA(130, 90, 50, (int)(0x80 * fl)));
            } else if (abilKind == AB_RESSUS) {                        /* COVEIRO: a bola sobe do cemiterio */
                ringN(fxX, fxY, 5.0f + u * 34.0f, 2.0f, 24, CA(150, 255, 170, (int)(0x80 * fl)));
                for (i = 0; i < 4; i++) discN(fxX + (i - 1.5f) * 5.0f, fxY - u * 30.0f - i * 3.0f, 2.2f * fl + 0.5f, CA(190, 255, 200, (int)(0x70 * fl)), 8);
            } else if (abilKind == AB_MAREALTA) {                      /* MARE ALTA: uma onda varre a mesa da esquerda para a direita */
                float wx = TX0 + u * (TX1 - TX0) * 1.15f, wl2 = wx < TX1 ? wx : TX1;
                int j2;
                rect(TX0, TY0, wl2, TY1, CA(40, 140, 235, (int)(0x48 * fl)));
                if (wx < TX1) {
                    for (j2 = 0; j2 < 22; j2++) {
                        float y0w = TY0 + j2 * ((TY1 - TY0) / 22.0f), y1w = y0w + (TY1 - TY0) / 22.0f;
                        float x0w = wx + 6.0f * sinf(j2 * 0.9f + menuT * 0.3f), x1w = wx + 6.0f * sinf((j2 + 1) * 0.9f + menuT * 0.3f);
                        line(x0w, y0w, x1w, y1w, CA(210, 248, 255, (int)(0xa0 * fl)));
                        line(x0w - 12.0f, y0w, x1w - 12.0f, y1w, CA(120, 215, 255, (int)(0x70 * fl)));
                        if (j2 % 3 == 0) disc(x0w + 3.0f, y0w, 2.0f, CA(255, 255, 255, (int)(0x90 * fl)));
                    }
                }
            } else {
                ringN(b[0].x, b[0].y, R + 3.0f + u * 18.0f, 1.6f, 20, CA(120, 230, 255, (int)(0x60 * fl)));
            }
            if (grinchOn) {                                            /* GRINCH: ondas verdes e um brilho amarelo */
                ringN(fxX, fxY, 5.0f + u * 44.0f, 2.4f, 26, CA(110, 220, 70, (int)(0x80 * fl)));
                ringN(fxX, fxY, 3.0f + u * 28.0f, 1.6f, 22, CA(255, 230, 70, (int)(0x70 * fl)));
            }
        }
    }
    /* o taco (290 de comprimento) nao pode vazar da moldura da mesa para a barra de forca / HUD */
    gsKit_set_scissor(g, GS_SETREG_SCISSOR((int)(TX0 - 30.0f + OX), (int)(TX1 + 30.0f + OX), (int)(TY0 - 30.0f + OY), (int)(TY1 + 30.0f + OY)));
    cueChalk = isBot() ? 1.0f : chalk[cur];
    if (state == S_AIM) {
        float cd0 = R + 4 + pwr * 70;
        if (chalkAnim > 0) cd0 += 16.0f + 4.0f * sinf(chalkAnim * 0.7f);
        if (!isBot()) { if (!sirenT[cur]) drawGuide(); if (jumpArm) drawJumpMark(); }   /* SIRENE: sem linha guia */
        drawCue(b[0].x, b[0].y, cd0, 0x80, ang, curCue());
        if (chalkAnim > 0) drawChalkFx(b[0].x, b[0].y, cd0);
    } else if (state == S_STRIKE) {
        float t = (float)strikeK / STRIKEN, tip = R + 1.0f;
        drawCue(b[0].x, b[0].y, tip + (strikeD0 - tip) * (1.0f - t * t), 0x80, ang, curCue());
    } else if (state == S_ROLL && follow > 0) {
        float f = (float)(FOLLOWN - follow) / FOLLOWN;
        drawCue(hitX, hitY, R + 1.0f - 16.0f * f, (int)(0x80 * (1.0f - f)), ang, hitCue);
    }
    gsKit_set_scissor(g, GS_SCISSOR_RESET);
    if (state == S_TAKE && takeN > 0) {
        float bob = 3.0f * sinf(menuT * 0.2f);
        float bx = b[takeList[takeSel]].x, by = b[takeList[takeSel]].y;
        ringN(bx, by, R + 3.5f, 1.8f, 24, CA(255, 230, 60, 0x7c));
        ringN(bx, by, R + 6.5f + bob * 0.4f, 1.2f, 24, CA(255, 230, 60, 0x40));
        tri(bx - 6, by - R - 18 - bob, bx + 6, by - R - 18 - bob, bx, by - R - 9 - bob, COL(255,230,60));
    }
    {   /* MESTRE: marca a cacapa cantada para a bola 8 */
        int ck = -1;
        if (state == S_AIM) { if (eightCall()) ck = callPk; }
        else if ((state == S_STRIKE || state == S_ROLL) && b[8].in) ck = callShotPk;
        if (ck >= 0) {
            float pl = 0.5f + 0.5f * sinf(menuT * 0.15f);
            ringN(PX[ck], PY[ck], PR[ck] * tabPock() + 7.0f + 2.5f * pl, 2.2f, 28, CA(255, 225, 60, 0xa0));
            ringN(PX[ck], PY[ck], PR[ck] * tabPock() + 11.5f + 2.5f * pl, 1.4f, 24, CA(255, 225, 60, 0x50));
            discN(PX[ck], PY[ck], 7.5f, COL(255, 225, 60), 14);
            text(PX[ck] - 3.4f, PY[ck] - 5.0f, 0.34f, COL(12, 12, 12), "8");
        }
    }
    drawHUD();
    if (state == S_REPLAY) drawReplayTag();
    if (state == S_CUES) {
        gsKit_queue_exec(g);          /* descarrega a cena para a GS antes do catalogo (evita estourar o buffer do quadro) */
        drawCueMenu();
    }
    if (!paused && musicToast()) {
        char tb[80];
        snprintf(tb, sizeof tb, "MUSICA: %s", musicToast());
        textC(14, 0.32f, COL(190,220,255), tb);
    }
    if (state == S_REWIND) drawRewind();
    if (paused) drawPause();
}

/* categoria da tela: 1 = menus, 2 = fim de jogo, 3 = partida */
static int stCat(int st)
{
    if (st == S_OVER) return 2;
    if (st == S_MENU || st == S_CREDITS || st == S_OPTS || st == S_SETUP || st == S_NAME || st == S_PLAYER || st == S_BRACKET || st == S_SHOP) return 1;
    return 3;
}

/* desenha a tela e aplica um fade rapido ao trocar de tela */
static void draw(void)
{
    static int lastSt = -1, fadeT = 0, fadeN = 14;
    if (state != lastSt) {
        if (lastSt >= 0) {
            if (stCat(state) != stCat(lastSt)) { fadeT = fadeN = 16; }
            else if (stCat(state) == 1)        { fadeT = fadeN = 6; }
        }
        lastSt = state;
    }
    drawScene();
    if (fadeT > 0) {
        rect(-OX, -OY, 640.0f + OX, 448.0f + OY, CA(0, 0, 0, fadeT * 0x80 / fadeN));
        fadeT--;
    }
}

/* tela de abertura: logo com fade, barra de carregamento; X ou START pula */
static void splashScreen(void)
{
    int f, total = 200;
    for (f = 0; f < total; f++) {
        float sc = 1.5f;
        int a = 0x80;
        const char *tt = "8 BALL REIS";
        float x0 = 320.0f - tw(sc, tt) * 0.5f, ty = 150.0f + 4.0f * sinf(f * 0.07f);
        if (f < 36) a = f * 0x80 / 36;
        if (f > total - 30) a = (total - f) * 0x80 / 30;
        if (f > 40 && (readpad(0) & (PAD_START | PAD_CROSS))) { f = total - 30; }
        musicUpdate();                                  /* busca musicas e gera os efeitos ja na abertura (nao trava depois) */
        menuT++;
        menuUpdate();
        gsKit_clear(g, GS_SETREG_RGBAQ(0x10, 0x10, 0x18, 0x80, 0x00));
        gsKit_prim_sprite(g, 0, 0, (float)g->Width, (float)g->Height, 1, COL(16, 16, 24));
        drawBackdrop();
        text(x0 + 5, ty + 7, sc, GS_SETREG_RGBAQ(0, 0, 0, a / 2, 0x00), tt);
        text(x0 - 3, ty - 3, sc, GS_SETREG_RGBAQ(140, 70, 10, a / 2, 0x00), tt);
        text(x0 + 3, ty + 4, sc, GS_SETREG_RGBAQ(140, 70, 10, a / 2, 0x00), tt);
        text(x0, ty, sc, GS_SETREG_RGBAQ(255, 215, 70, a, 0x00), tt);
        text(320.0f - tw(0.5f, "PS2 EDITION") * 0.5f, 214, 0.5f, GS_SETREG_RGBAQ(190, 200, 225, a, 0x00), "PS2 EDITION");
        rect(200, 244, 440, 246, CA(60, 110, 90, a));
        rect(200, 244, 200 + 240.0f * (f < total - 30 ? (float)f / (total - 30) : 1.0f), 246, CA(255, 225, 120, a));
        text(320.0f - tw(0.34f, "CRIADO POR " GAME_AUTHOR) * 0.5f, 268, 0.34f, GS_SETREG_RGBAQ(170, 180, 195, a, 0x00), "CRIADO POR " GAME_AUTHOR);
        if (f > 40 && (f / 24) % 2 == 0)
            text(320.0f - tw(0.34f, "APERTE X OU START") * 0.5f, 400, 0.34f, GS_SETREG_RGBAQ(255, 235, 140, a, 0x00), "APERTE X OU START");
        text(626.0f - tw(0.3f, "VERSAO " GAME_VERSION), 424, 0.3f, GS_SETREG_RGBAQ(150, 160, 175, a, 0x00), "VERSAO " GAME_VERSION);
        gsKit_queue_exec(g);
        gsKit_sync_flip(g);
    }
}

/* ---------- habilidades: ativar na mira ---------- */
/* HACK: planejador proprio (sem erro aleatorio do bot). Para cada bola legal e cada cacapa calcula a bola fantasma,
   confere caminho livre, angulo de corte e de entrada na cacapa, risco de a branca cair (suicidio) e a FORCA exata
   usando o atrito real da mesa. Guarda as 5 melhores tacadas; O de novo troca de opcao. */
static float stopDist(float v)                   /* distancia que a bola percorre ate parar (v' = -FRIC_K v - FRIC_C) */
{
    const float h = FRIC_C / FRIC_K;
    return v <= 0.0f ? 0.0f : (v - h * logf(1.0f + v / h)) / FRIC_K;
}

static float speedForDist(float d)               /* inverso: velocidade inicial para percorrer d */
{
    float lo = 0.0f, hi = 4000.0f;
    int k;
    for (k = 0; k < 26; k++) { float m = (lo + hi) * 0.5f; if (stopDist(m) < d) lo = m; else hi = m; }
    return hi;
}

static int cueRisk(float x, float y, float vx, float vy)   /* a branca, andando de (x,y) com velocidade (vx,vy), cai numa cacapa? */
{
    float sp = sqrtf(vx * vx + vy * vy), dist, ux, uy;
    int p;
    if (sp < 8.0f) return 0;
    dist = stopDist(sp) * 0.9f; ux = vx / sp; uy = vy / sp;
    for (p = 0; p < 6; p++) {
        float px = PX[p] - x, py = PY[p] - y, t = px * ux + py * uy, ex, ey;
        if (t < 0.0f) continue;
        if (t > dist) t = dist;
        ex = px - ux * t; ey = py - uy * t;
        if (ex * ex + ey * ey < (PRC[p] + 4.0f) * (PRC[p] + 4.0f)) return 1;
    }
    return 0;
}

/* simula a tacada na mesa de verdade (mesma fisica do jogo) e diz se ela da certo:
   1 = primeira bola tocada e legal, encacapou uma bola sua e a branca ficou na mesa. Guarda e restaura todo o estado. */
static int hackSim(float a, float p, int safeOnly)
{
    Ball sb[16];
    int spk[16], sfT[16], sfP[16], sfh = firstHit, sak = abilKind, sat = abilT;
    float sfX[16], sfY[16], ssc = spinCur, ssv = spinVCur, ssk = spinK, ssvk = spinVK, sfm = fricMul;
    int i, st = 0, mv, ok, gr = grp[cur], gdone = (gr >= 0 && countGroup(gr) == 0), legal, ownp = 0;
    memcpy(sb, b, sizeof b); memcpy(spk, pocketed, sizeof pocketed); memcpy(sfT, fallT, sizeof fallT);
    memcpy(sfP, fallP, sizeof fallP); memcpy(sfX, fallX, sizeof fallX); memcpy(sfY, fallY, sizeof fallY);
    memset(pocketed, 0, sizeof pocketed); firstHit = -1;
    spinCur = spinVCur = spinK = spinVK = 0.0f; abilKind = AB_SUGAR; abilT = 0; fricMul = 1.0f;   /* igual a tacada real do HACK (startStrike liga o SUGAR) */
    b[0].vx = cosf(a) * p * MAXV * cuePowMul(curCue());
    b[0].vy = sinf(a) * p * MAXV * cuePowMul(curCue());
    sfxMute++;                                        /* simulacao: sem som */
    do {
        mv = 0;
        for (i = 0; i < 8; i++) stepPhysics((1.0f / 60.0f) / 8.0f);
        for (i = 0; i < 16; i++) if (b[i].in && (b[i].vx != 0.0f || b[i].vy != 0.0f || b[i].z > 0.0f)) mv = 1;
        st++;
    } while (mv && st < 420);
    sfxMute--;
    legal = firstHit > 0 && (gr < 0 ? firstHit != 8 : (ballGroup(firstHit) == gr || (firstHit == 8 && gdone)));
    for (i = 1; i < 16; i++)
        if (pocketed[i] && (gr < 0 ? i != 8 : (ballGroup(i) == gr || (i == 8 && gdone)))) ownp = 1;
    ok = legal && !pocketed[0] && (safeOnly || ownp) && !(pocketed[8] && !(gr >= 0 && gdone));
    memcpy(b, sb, sizeof b); memcpy(pocketed, spk, sizeof pocketed); memcpy(fallT, sfT, sizeof fallT);
    memcpy(fallP, sfP, sizeof fallP); memcpy(fallX, sfX, sizeof fallX); memcpy(fallY, sfY, sizeof fallY);
    firstHit = sfh; spinCur = ssc; spinVCur = ssv; spinK = ssk; spinVK = ssvk; abilKind = sak; abilT = sat; fricMul = sfm;
    return ok;
}

/* a tabela so abre na boca da cacapa: a bola alvo precisa cruzar a linha da tabela dentro da largura do buraco */
static float pocketEntryOff(float tx, float ty, int p)     /* desvio do centro na linha da tabela (0 = entra no meio) */
{
    float dx = PX[p] - tx, dy = PY[p] - ty, bt = 1e9f, bo = 0.0f, tt;
    if (PY[p] < TY0 + R && dy < -0.01f && ty > TY0 + R) {
        tt = (TY0 + R - ty) / dy; if (tt < bt) { bt = tt; bo = tx + dx * tt - PX[p]; }
    } else if (PY[p] > TY1 - R && dy > 0.01f && ty < TY1 - R) {
        tt = (TY1 - R - ty) / dy; if (tt < bt) { bt = tt; bo = tx + dx * tt - PX[p]; }
    }
    if (PX[p] < TX0 + R && dx < -0.01f && tx > TX0 + R) {
        tt = (TX0 + R - tx) / dx; if (tt < bt) { bt = tt; bo = ty + dy * tt - PY[p]; }
    } else if (PX[p] > TX1 - R && dx > 0.01f && tx < TX1 - R) {
        tt = (TX1 - R - tx) / dx; if (tt < bt) { bt = tt; bo = ty + dy * tt - PY[p]; }
    }
    return bt > 1e8f ? 0.0f : fabsf(bo);
}

static void hackAdd(float ang_, float pw, float sc)
{
    int k, j;
    for (k = 0; k < hCandN && hCandSc[k] <= sc; k++) ;
    if (k >= 10) return;
    if (hCandN < 10) hCandN++;
    for (j = hCandN - 1; j > k; j--) { hCandAng[j] = hCandAng[j-1]; hCandPow[j] = hCandPow[j-1]; hCandSc[j] = hCandSc[j-1]; }
    hCandAng[k] = ang_; hCandPow[k] = pw; hCandSc[k] = sc;
}

static void hackSolve(void)
{
    int t, p, gr = grp[cur];
    float cx = b[0].x, cy = b[0].y, pmul = MAXV * cuePowMul(curCue());
    hCandN = 0; hCandI = 0;

    if (breakShot) {                                         /* quebra: reto na bola da ponta do triangulo, forca total */
        hCandAng[0] = atan2f(b[1].y - cy, b[1].x - cx); hCandPow[0] = 1.0f; hCandSc[0] = 0.0f; hCandN = 1;
        return;
    }
    for (t = 1; t < 16; t++) {
        if (!b[t].in) continue;
        if (LOCKED(t)) continue;                  /* bola presa pelo policial: nao da para encacapar */
        if (t == 8) { if (!(gr >= 0 && countGroup(gr) == 0)) continue; }
        else if (gr >= 0 && ballGroup(t) != gr) continue;
        for (p = 0; p < 6; p++) {
            float tx = b[t].x, ty = b[t].y, dx = PX[p] - tx, dy = PY[p] - ty;
            float dl = sqrtf(dx * dx + dy * dy), gx, gy, ax, ay, al, cc, ox, oy, ol, appr, vt, vc, v0, pw, sc, eo;
            float cvx, cvy;
            if (dl < 1.0f) continue;
            dx /= dl; dy /= dl;
            ox = PX[p] < TCX ? -1.0f : (PX[p] > TCX ? 1.0f : 0.0f);        /* eixo de entrada da cacapa */
            oy = PY[p] < TCY ? -1.0f : 1.0f;
            ol = sqrtf(ox * ox + oy * oy); ox /= ol; oy /= ol;
            appr = dx * ox + dy * oy;                                       /* 1 = entra de frente */
            if (appr < (ox != 0.0f ? 0.34f : 0.55f)) continue;              /* entra de lado demais: bate na quina */
            eo = pocketEntryOff(tx, ty, p);
            if (eo > PRC[p] + 5.0f) continue;                               /* rasante na tabela: nao entra */
            gx = tx - dx * 2 * R; gy = ty - dy * 2 * R;
            if (gx < TX0 + R || gx > TX1 - R || gy < TY0 + R || gy > TY1 - R) continue;
            ax = gx - cx; ay = gy - cy; al = sqrtf(ax * ax + ay * ay);
            if (al < 1.0f) continue;
            ax /= al; ay /= al;
            cc = ax * dx + ay * dy;
            if (cc < 0.30f) continue;                                       /* corte fino demais */
            if (!pathClear(cx, cy, gx, gy, t)) continue;
            if (!pathClear(tx, ty, PX[p], PY[p], t)) continue;
            /* forca: a bola alvo sai com ~0.98*cos do corte e precisa chegar na cacapa com folga */
            vt = speedForDist(dl + 12.0f);
            vc = vt / (0.98f * cc);
            v0 = speedForDist(al + stopDist(vc));
            pw = v0 / pmul;
            if (pw > 1.0f) continue;                                        /* nao ha forca para essa tacada */
            if (pw < 0.12f) pw = 0.12f;
            /* para onde a branca vai depois da batida (risco de cair na cacapa) */
            cvx = vc * ax - 0.98f * vc * cc * dx; cvy = vc * ay - 0.98f * vc * cc * dy;
            sc = al * 0.8f + dl + (1.0f - cc) * 520.0f + (1.0f - appr) * 160.0f + pw * 90.0f + eo * 14.0f;
            if (cueRisk(gx, gy, cvx, cvy)) sc += 450.0f;
            hackAdd(atan2f(gy - cy, gx - cx), pw, sc);
        }
    }
    if (hCandN < 6) {                                                       /* poucas tacadas diretas: tenta tacadas de TABELA (1 rebote) */
        for (t = 1; t < 16; t++) {
            if (!b[t].in || LOCKED(t)) continue;
            if (t == 8) { if (!(gr >= 0 && countGroup(gr) == 0)) continue; }
            else if (gr >= 0 && ballGroup(t) != gr) continue;
            for (p = 0; p < 6; p++) {
                int w;
                for (w = 0; w < 4; w++) {
                    float tx = b[t].x, ty = b[t].y, mx, my, dx, dy, dl, wl, tt, bx_, by_, gx, gy, ax, ay, al, cc, vt, vc, v0, pw, sc;
                    if (w == 0)      { mx = PX[p]; my = 2.0f * (TY0 + R) - PY[p]; }
                    else if (w == 1) { mx = PX[p]; my = 2.0f * (TY1 - R) - PY[p]; }
                    else if (w == 2) { mx = 2.0f * (TX0 + R) - PX[p]; my = PY[p]; }
                    else             { mx = 2.0f * (TX1 - R) - PX[p]; my = PY[p]; }
                    dx = mx - tx; dy = my - ty; dl = sqrtf(dx * dx + dy * dy);
                    if (dl < 1.0f) continue;
                    dx /= dl; dy /= dl;
                    if (w < 2) { wl = w == 0 ? TY0 + R : TY1 - R; if (fabsf(dy) < 0.05f) continue; tt = (wl - ty) / dy; bx_ = tx + dx * tt; by_ = wl; }
                    else       { wl = w == 2 ? TX0 + R : TX1 - R; if (fabsf(dx) < 0.05f) continue; tt = (wl - tx) / dx; bx_ = wl; by_ = ty + dy * tt; }
                    if (tt < 2.0f * R || tt > dl - R) continue;
                    if (bx_ < TX0 + 3.0f * R || bx_ > TX1 - 3.0f * R || by_ < TY0 + 3.0f * R || by_ > TY1 - 3.0f * R) continue;
                    gx = tx - dx * 2.0f * R; gy = ty - dy * 2.0f * R;
                    if (gx < TX0 + R || gx > TX1 - R || gy < TY0 + R || gy > TY1 - R) continue;
                    ax = gx - cx; ay = gy - cy; al = sqrtf(ax * ax + ay * ay);
                    if (al < 1.0f) continue;
                    ax /= al; ay /= al;
                    cc = ax * dx + ay * dy;
                    if (cc < 0.35f) continue;
                    if (!pathClear(cx, cy, gx, gy, t)) continue;
                    if (!pathClear(tx, ty, bx_, by_, t)) continue;
                    vt = speedForDist(dl + 12.0f) * 1.25f;               /* o rebote tira forca */
                    vc = vt / (0.98f * cc);
                    v0 = speedForDist(al + stopDist(vc));
                    pw = v0 / pmul;
                    if (pw > 1.0f) continue;
                    if (pw < 0.15f) pw = 0.15f;
                    sc = 800.0f + al * 0.8f + dl + (1.0f - cc) * 520.0f + pw * 90.0f;
                    hackAdd(atan2f(gy - cy, gx - cx), pw, sc);
                }
            }
        }
    }
    {                                                                       /* confere cada tacada na simulacao: so ficam as que dao certo */
        int k, n = 0;
        for (k = 0; k < hCandN; k++) {
            if (n >= 4) break;
            if (hackSim(hCandAng[k], hCandPow[k], 0)) { hCandAng[n] = hCandAng[k]; hCandPow[n] = hCandPow[k]; hCandSc[n] = hCandSc[k]; n++; }
        }
        if (n == 0 && hCandN > 0) {                                         /* nenhuma confirmada: ajusta fino o angulo e a forca das melhores */
            static const float da[8] = { 0.008f, -0.008f, 0.018f, -0.018f, 0.0f, 0.0f, 0.012f, -0.012f };
            static const float dp[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.06f, -0.06f, 0.08f, -0.08f };
            float ra[4], rp[4], rs[4];
            int top = hCandN < 4 ? hCandN : 4, v, nn = 0;
            for (k = 0; k < top; k++)
                for (v = 0; v < 8; v++) {
                    float pw2 = hCandPow[k] + dp[v];
                    if (pw2 > 1.0f) pw2 = 1.0f;
                    if (pw2 < 0.12f) pw2 = 0.12f;
                    if (hackSim(hCandAng[k] + da[v], pw2, 0)) { ra[nn] = hCandAng[k] + da[v]; rp[nn] = pw2; rs[nn] = hCandSc[k]; nn++; break; }
                }
            for (k = 0; k < nn; k++) { hCandAng[k] = ra[k]; hCandPow[k] = rp[k]; hCandSc[k] = rs[k]; }
            n = nn;
        }
        hCandN = n;
    }
    if (hCandN == 0) {                                                      /* sem tacada direta: jogada de seguranca */
        float bd[16], ba[16], bp[16];
        int order[16], n = 0, k, j, best = -1;
        for (t = 1; t < 16; t++) {
            float dx, dy, d;
            if (!b[t].in) continue;
            if (t == 8 && !(gr >= 0 && countGroup(gr) == 0)) continue;
            if (gr >= 0 && t != 8 && ballGroup(t) != gr) continue;
            if (gr < 0 && t == 8) continue;
            dx = b[t].x - cx; dy = b[t].y - cy; d = sqrtf(dx * dx + dy * dy);
            bd[n] = d + (pathClear(cx, cy, b[t].x, b[t].y, t) ? 0.0f : 400.0f);   /* prefere bola com caminho livre */
            ba[n] = atan2f(dy, dx);
            bp[n] = speedForDist(d + 40.0f) / pmul;
            if (bp[n] < 0.15f) bp[n] = 0.15f;
            if (bp[n] > 1.0f) bp[n] = 1.0f;
            order[n] = n; n++;
        }
        for (k = 0; k < n; k++)                                             /* ordena pela distancia */
            for (j = k + 1; j < n; j++)
                if (bd[order[j]] < bd[order[k]]) { int tmp = order[k]; order[k] = order[j]; order[j] = tmp; }
        for (k = 0; k < n && k < 4; k++)
            if (hackSim(ba[order[k]], bp[order[k]], 1)) { best = order[k]; break; }
        if (best < 0 && n > 0) best = order[0];
        if (best >= 0) { hCandAng[0] = ba[best]; hCandPow[0] = bp[best]; }
        else { hCandAng[0] = atan2f(TCY - cy, FOOTX - cx); hCandPow[0] = 0.4f; }
        hCandSc[0] = 1e6f; hCandN = 1;
    }
}

static void hackApply(void)                      /* trava a mira na opcao escolhida */
{
    char tb[64];
    ang = hCandAng[hCandI]; hackPow = hCandPow[hCandI]; hackOn = 1;
    spinSel = 0; spinV = 0; jumpArm = 0; charging = 0; pwr = 0;
    if (hCandSc[hCandI] >= 1e5f) strcpy(tb, "HACK: SEM TACADA DIRETA - JOGADA DE SEGURANCA");
    else sprintf(tb, hCandSc[hCandI] >= 800.0f ? "HACK %d/%d: TABELA, FORCA %d%% - O TROCA, X ATIRA" : "HACK %d/%d: FORCA %d%% - O TROCA, X ATIRA", hCandI + 1, hCandN, (int)(hackPow * 100.0f + 0.5f));
    setmsg(tb);
}

static void useAbility(void)
{
    int ab = curAb();
    char tb[64];
    if (ab == AB_NONE) { setmsg(abilOn ? "ESTE TACO NAO TEM HABILIDADE" : "HABILIDADES DESLIGADAS NESTA PARTIDA"); return; }
    if (ab == AB_HACK && hackOn) {                       /* ja ativo: O troca para a proxima melhor tacada (e depois libera a mira) */
        hCandI++;
        if (hCandI >= hCandN) { hackOn = 0; hCandI = 0; abilCD[cur] = 0; setmsg("HACK DESLIGADO: MIRA LIVRE (RECARGA DEVOLVIDA)"); }   /* nao atirou: nao gasta a habilidade */
        else hackApply();
        return;
    }
    if (abilCD[cur] > 0) { sprintf(tb, "%s RECARREGA EM %d TACADA(S)", AB_NAME[ab], abilCD[cur]); setmsg(tb); return; }
    if (ab == AB_HACK) {
        hackSolve(); hackApply(); abilCD[cur] = AB_COOL + 1; SFX(SFX_ABIL, 70);
        return;
    }
    if (ab == AB_COVEIRO) {                               /* O: abre o seletor e passa ENTERRO > COVA > RESSUSCITAR > fechar */
        const char *why;
        if (!graveOpen) { graveOpen = 1; graveSel = 0; } else graveSel++;
        if (graveSel > 2) { graveOpen = 0; graveSel = 0; abilArm = 0; setmsg("HABILIDADE CANCELADA: COVEIRO"); return; }
        why = graveWhy(graveSel);
        abilArm = (why == NULL);                          /* poder indisponivel: fica so selecionado (nao arma) */
        SFX(SFX_ABIL, 70);
        if (why) setmsg(why);
        else { sprintf(tb, "COVEIRO: %s PRONTO (O: TROCAR)", GV_NAME[graveSel]); setmsg(tb); }
        return;
    }
    if (ab == AB_POLICIAL) {                              /* O: abre o seletor e passa ALGEMAS > SIRENE > fechar */
        const char *why;
        if (!copOpen) { copOpen = 1; copSel = 0; } else copSel++;
        if (copSel > 1) { copOpen = 0; copSel = 0; abilArm = 0; setmsg("HABILIDADE CANCELADA: POLICIAL"); return; }
        why = copWhy(copSel);
        abilArm = (why == NULL);
        if (copSel == 0 && abilArm) copTgt = copBest();   /* alvo inicial: a bola do adversario mais perto da cacapa */
        SFX(SFX_ABIL, 70);
        if (why) setmsg(why);
        else if (copSel == 0) { sprintf(tb, "ALGEMAS NA BOLA %d (QUADRADO TROCA O ALVO)", copTgt); setmsg(tb); }
        else setmsg("SIRENE PRONTA (O: TROCAR)");
        return;
    }
    if (ab == AB_MARRETA) {                               /* O: abre o seletor e passa CORNETA > CHIQUITOLINA > MARRETA BIONICA > fechar */
        const char *why;
        if (!chOpen) { chOpen = 1; chSel = 0; } else chSel++;
        if (chSel > 2) { chOpen = 0; chSel = 0; abilArm = 0; setmsg("HABILIDADE CANCELADA: CHAPOLIN"); return; }
        why = chWhy(chSel);
        abilArm = (why == NULL);
        if (chSel == 0 && abilArm) copTgt = cornBest();   /* alvo inicial: a bola do adversario mais perto da cacapa */
        SFX(SFX_ABIL, 70);
        if (why) setmsg(why);
        else if (chSel == 0) { sprintf(tb, "CORNETA NA BOLA %d (QUADRADO TROCA O ALVO)", copTgt); setmsg(tb); }
        else { sprintf(tb, "%s PRONTA (O: TROCAR)", CH_NAME[chSel]); setmsg(tb); }
        return;
    }
    abilArm = !abilArm;
    if (abilArm) SFX(SFX_ABIL, 70);
    sprintf(tb, abilArm ? "HABILIDADE PRONTA: %s" : "HABILIDADE CANCELADA: %s", abLabel(ab));
    setmsg(tb);
}

/* ---------- main ---------- */
int main(int argc, char *argv[])
{
    u32 in, prev = 0, pressed, p0, p1;
    int i;

    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    shopInit();                                    /* LOJA: carrega moedas e itens do memory card */
    padInit(0);
    padPortOpen(0, 0, padbuf0);
    padPortOpen(1, 0, padbuf1);
    musicInit(argc > 0 ? argv[0] : NULL);          /* player de musicas (USB / pasta do jogo) */
    musicSetVolume(volLvl);

    g = gsKit_init_global_custom(1024 * 1024, 256 * 1024);   /* buffer maior: o menu de tacos desenha muita coisa num quadro so */
    g->ZBuffering = GS_SETTING_OFF;
    g->DoubleBuffering = GS_SETTING_ON;
    g->PrimAlphaEnable = GS_SETTING_ON;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(g);
    gsKit_mode_switch(g, GS_ONESHOT);
    gsKit_set_test(g, GS_ZTEST_OFF);
    gsKit_set_primalpha(g, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
    fm = gsKit_init_fontm();
    gsKit_fontm_upload(g, fm);
#if USE_TEXTURE_BALLS
    initBallTex();
#endif
    initFontTex();                                 /* fonte nitida com contorno (se falhar, usa a fonte padrao) */
    OX = (g->Width  - 640) / 2.0f; if (OX < 0) OX = 0;
    OY = (g->Height - 448) / 2.0f; if (OY < 0) OY = 0;
    menuInit();
    splashScreen();
    menuInit();

    for (;;) {
        musicUpdate();
        p0 = readpad(0);
        p1 = (mode == 1 || state == S_MENU || state == S_OPTS || state == S_SETUP || state == S_NAME || state == S_PLAYER || state == S_BRACKET || state == S_SHOP) ? readpad(1) : 0;
        if (state == S_MENU || state == S_OVER || state == S_REPLAY || state == S_CREDITS || state == S_OPTS || state == S_SETUP || state == S_NAME || state == S_PLAYER || state == S_BRACKET || state == S_SHOP) in = p0 | p1;
        else if (mode == 1) in = p0 | p1;
        else in = (cur == 0) ? p0 : 0;
        if (paused) in = p0 | p1;
        else if (state == S_AIM || state == S_ROLL || state == S_PLACE || state == S_TAKE || state == S_STRIKE)
            in |= p0 & PAD_START;
        {   /* ANALOGICO: o direcional continua valendo. Stick esquerdo = mira / mover a bola / menus; direito = efeito */
            int rd1 = (mode == 1 || state == S_MENU || state == S_OPTS || state == S_SETUP || state == S_NAME || state == S_PLAYER || state == S_BRACKET || state == S_SHOP);
            int all = paused || mode == 1 || state == S_MENU || state == S_OVER || state == S_CREDITS || state == S_OPTS ||
                      state == S_SETUP || state == S_NAME || state == S_PLAYER || state == S_BRACKET || state == S_SHOP;
            float jx = 0.0f, jy = 0.0f, kx = 0.0f, ky = 0.0f;
            if (all || cur == 0) { jx = stX[0]; jy = stY[0]; kx = rsX[0]; ky = rsY[0]; }
            if (all && rd1)      { jx += stX[1]; jy += stY[1]; kx += rsX[1]; ky += rsY[1]; }
            if (jx > 1.0f) jx = 1.0f; if (jx < -1.0f) jx = -1.0f; if (jy > 1.0f) jy = 1.0f; if (jy < -1.0f) jy = -1.0f;
            if (kx > 1.0f) kx = 1.0f; if (kx < -1.0f) kx = -1.0f; if (ky > 1.0f) ky = 1.0f; if (ky < -1.0f) ky = -1.0f;
            if (paused || (state != S_AIM && state != S_PLACE))      /* fora da mira e da bola na mao o stick vira direcional */
                in |= (jx < -0.6f ? PAD_LEFT : 0) | (jx > 0.6f ? PAD_RIGHT : 0) | (jy < -0.6f ? PAD_UP : 0) | (jy > 0.6f ? PAD_DOWN : 0);
            anX += (jx - anX) * 0.4f;  if (jx == 0.0f && fabsf(anX) < 0.02f) anX = 0.0f;      /* suaviza: a mira desliza */
            anY += (jy - anY) * 0.4f;  if (jy == 0.0f && fabsf(anY) < 0.02f) anY = 0.0f;
            anRX += (kx - anRX) * 0.4f; if (kx == 0.0f && fabsf(anRX) < 0.02f) anRX = 0.0f;
            anRY += (ky - anRY) * 0.4f; if (ky == 0.0f && fabsf(anRY) < 0.02f) anRY = 0.0f;
        }
        pressed = in & ~prev;
        prev = in;
        {                                                 /* sons de menu */
            int ui = paused || state == S_MENU || state == S_OPTS || state == S_SETUP || state == S_NAME || state == S_PLAYER ||
                     state == S_BRACKET || state == S_SHOP || state == S_CUES || state == S_CREDITS || state == S_OVER || state == S_TAKE;
            if (ui) {
                if (pressed & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_L1 | PAD_R1)) SFX(SFX_MOVE, 60);
                if (pressed & (PAD_CROSS | PAD_START)) SFX(SFX_OK, 70);
                else if (pressed & (PAD_CIRCLE | PAD_SELECT)) SFX(SFX_BACK, 60);
            } else if (state == S_PLACE && (pressed & PAD_CROSS)) SFX(SFX_OK, 60);
        }
        menuT++;
        if (state == S_OVER) overT++;
        if (state == S_MENU || state == S_CREDITS || state == S_OPTS || state == S_SETUP || state == S_NAME || state == S_PLAYER || state == S_BRACKET || state == S_SHOP) menuUpdate();

        if (paused) pauseInput(pressed);
        else if ((pressed & PAD_START) && (state == S_AIM || state == S_ROLL || state == S_PLACE ||
                                           state == S_TAKE || state == S_STRIKE)) pauseOpen();
        else switch (state) {
        case S_MENU:
            if (pressed & (PAD_LEFT | PAD_UP))    menuSel = MENU_ORD[(menuSlot(menuSel) + 5) % 6];
            if (pressed & (PAD_RIGHT | PAD_DOWN)) menuSel = MENU_ORD[(menuSlot(menuSel) + 1) % 6];
            if (pressed & (PAD_CROSS | PAD_START)) {
                if (menuSel == 0) { training = 1; tourney = 0; oppSel = 0; state = S_SETUP; setSel = 0; setStep = 0; }   /* modo treino */
                else if (menuSel == 1) { training = 0; tourney = 0; state = S_SETUP; setSel = 0; setStep = 0; }
                else if (menuSel == 2) { training = 0; tourney = 1; oppSel = 0; state = S_SETUP; setSel = 0; setStep = 0; }   /* torneio */
                else if (menuSel == 3) { state = S_PLAYER; plSel = musicCur() > 0 ? musicCur() : 0; }
                else if (menuSel == 4) shopOpen();
                else { state = S_OPTS; optSel = 0; }
            }
            break;

        case S_OPTS:
            optInput(pressed);
            break;

        case S_PLAYER:
            if (playerInput(pressed)) { pauseView = 0; state = S_MENU; }
            break;

        case S_SETUP:
            setupInput(pressed);
            break;

        case S_NAME:
            nameInput(pressed);
            break;

        case S_BRACKET:
            bracketInput(pressed);
            break;

        case S_SHOP:
            shopInput(pressed);
            break;

        case S_CREDITS:
            if (pressed & (PAD_CROSS | PAD_CIRCLE | PAD_START)) state = S_OPTS;
            break;

        case S_AIM:
            if (!eightCall()) { callPk = -1; callManual = 0; callAnn = 0; }       /* MESTRE: so na tacada da bola 8 */
            else if (!isBot()) {
                if (!callManual) callPk = eightBestPocket();
                if (!callAnn) { callAnn = 1; setmsg("BOLA 8: ESCOLHA A CACAPA (L3/R3)"); }
                if (pressed & (PAD_R3 | PAD_L3)) {
                    callPk = (callPk + ((pressed & PAD_R3) ? 1 : 5)) % 6; callManual = 1;
                    { char cb[64]; sprintf(cb, "CACAPA DA 8: %s", PK_NAME[callPk]); setmsg(cb); }
                    SFX(SFX_OK, 70);
                }
            }
            if (!isBot()) {
                if (chalkAnim > 0) {                                  /* passando giz: espera a animacao */
                    chalkAnim--;
                    if (chalkAnim == 22) { chalk[cur] = 1.0f; setmsg("GIZ PASSADO NO TACO!"); }
                    break;
                }
                if (hackOn) { fineHold = 0; }                    /* HACK: mira, efeito e pulo travados */
                else {
                if (((in & PAD_LEFT) != 0) != ((in & PAD_RIGHT) != 0)) {   /* direcional: comeca fino e acelera aos poucos */
                    float ds;
                    if (dpadHold < 60) dpadHold++;
                    ds = 0.010f + 0.020f * (dpadHold > 24 ? 1.0f : dpadHold / 24.0f);
                    ang += ((in & PAD_RIGHT) ? ds : -ds) * (curse[cur] ? -1.0f : 1.0f);   /* MALDICAO: botoes invertidos */
                } else dpadHold = 0;
                if (anX != 0.0f) ang += (anX < 0.0f ? -1.0f : 1.0f) * powf(fabsf(anX), 1.5f) * 0.032f * (curse[cur] ? -1.0f : 1.0f);   /* analogico: quanto mais inclina, mais gira */
                if (((in & PAD_L1) != 0) != ((in & PAD_R1) != 0)) {
                    float st;                                   /* ajuste fino: comeca bem devagar e so acelera um pouco segurando */
                    if (fineHold < 600) fineHold++;
                    st = 0.0007f + (fineHold > 20 ? (fineHold - 20) * 0.00002f : 0.0f);
                    if (st > 0.0028f) st = 0.0028f;
                    ang += ((in & PAD_R1) ? st : -st) * (curse[cur] ? -1.0f : 1.0f);
                } else fineHold = 0;
                if (in & PAD_L2) { spinSel -= 0.03f; if (spinSel < -1.0f) spinSel = -1.0f; }
                if (in & PAD_R2) { spinSel += 0.03f; if (spinSel >  1.0f) spinSel =  1.0f; }
                if (in & PAD_UP)   { spinV += 0.03f; if (spinV >  1.0f) spinV =  1.0f; }   /* rola */
                if (in & PAD_DOWN) { spinV -= 0.03f; if (spinV < -1.0f) spinV = -1.0f; }   /* puxa */
                if (anRX != 0.0f) { spinSel += anRX * 0.03f; if (spinSel < -1.0f) spinSel = -1.0f; if (spinSel > 1.0f) spinSel = 1.0f; }   /* stick direito: efeito */
                if (anRY != 0.0f) { spinV -= anRY * 0.03f; if (spinV < -1.0f) spinV = -1.0f; if (spinV > 1.0f) spinV = 1.0f; }
                }
                if (!charging) {
                    if ((pressed & PAD_CROSS) && hackOn) { pwr = hackPow; startStrike(hackPow); }          /* HACK: X atira na forca ideal exata */
                    else if (pressed & PAD_CROSS) { charging = 1; pwr = 0; barDir = 1.0f; }
                    else if (pressed & PAD_SELECT) { if (hackOn) setmsg("HACK ATIVO: TACO TRAVADO"); else openCueMenu(); }
                    else if ((pressed & PAD_TRIANGLE) && hackOn) setmsg("HACK ATIVO: MIRA TRAVADA");
                    else if (pressed & PAD_TRIANGLE) {
                        jumpArm = !jumpArm;
                        setmsg(jumpArm ? "PULO ATIVADO: A BRANCA VAI PULAR" : "PULO DESLIGADO");
                    }
                    else if (pressed & PAD_SQUARE) {
                        if (curAb() == AB_MARRETA && chOpen && chSel == 0 && abilArm) {   /* CHAPOLIN: QUADRADO troca a bola que sera paralisada */
                            char tb2[48];
                            int nt = cornNext(copTgt, 1);
                            if (nt > 0) copTgt = nt;
                            sprintf(tb2, "CORNETA NA BOLA %d", copTgt); setmsg(tb2);
                        }
                        else if (curAb() == AB_POLICIAL && copOpen && copSel == 0 && abilArm) {   /* POLICIAL: QUADRADO troca a bola que sera presa */
                            char tb2[48];
                            int nt = copNext(copTgt, 1);
                            if (nt > 0) copTgt = nt;
                            sprintf(tb2, "ALGEMAS NA BOLA %d", copTgt); setmsg(tb2);
                        }
                        else if (chalk[cur] < 0.97f) { chalkAnim = 50; SFX(SFX_CHALK, 70); }
                        else setmsg("O TACO JA ESTA COM GIZ");
                    }
                    else if (pressed & PAD_CIRCLE) useAbility();
                } else {
                    pwr += barDir * cueBarSpeed(curCue()) * (hackOn ? 0.6f : 1.0f);   /* sobe e desce sozinha (HACK: mais lenta) */
                    if (pwr >= 1.0f) { pwr = 1.0f; barDir = -1.0f; }
                    if (pwr <= 0.0f) { pwr = 0.0f; barDir = 1.0f; }
                    if (pressed & PAD_CIRCLE) { charging = 0; pwr = 0; }
                    else if (pressed & PAD_CROSS) {
                        charging = 0;
                        if (hackOn && fabsf(pwr - hackPow) <= 0.06f) pwr = hackPow;   /* HACK: ima na forca ideal */
                        if (pwr > 0.05f) startStrike(pwr); else pwr = 0;
                    }
                }
            } else {
                botTimer++;
                if (botTimer == BOT_DELAY[botLevel]) { botPlan(); botPlanned = 1; }
                if (botPlanned) {
                    float d = normAng(botAng - ang);
                    if (fabsf(d) > 0.04f) ang += d > 0 ? 0.04f : -0.04f;
                    else {
                        ang = botAng;
                        if (botTimer > BOT_DELAY[botLevel] + 40) {
                            pwr += 0.02f;
                            if (pwr >= botPow) startStrike(botPow);
                        }
                    }
                }
            }
            break;

        case S_CUES:
            if (pressed & (PAD_LEFT | PAD_UP))    cueMenuSel = catStep(-1);
            if (pressed & (PAD_RIGHT | PAD_DOWN)) cueMenuSel = catStep(1);
            if (pressed & PAD_CROSS) {
                char tb[48];
                cueSel[cur] = cueMenuSel;
                sprintf(tb, "TACO: %s", CUES[cueMenuSel].name);
                setmsg(tb);
                state = cueRet;
            } else if (pressed & (PAD_CIRCLE | PAD_SELECT)) state = cueRet;
            break;

        case S_REWIND:
            if (pressed & (PAD_LEFT | PAD_RIGHT)) tmSel = !tmSel;
            if (pressed & PAD_CIRCLE) tmSel = 1;
            if (pressed & (PAD_CROSS | PAD_CIRCLE)) {
                if (tmSel == 0) {                                   /* SIM: volta a mesa para antes da tacada */
                    memcpy(b, tmB, sizeof(b)); grp[0] = tmGrp[0]; grp[1] = tmGrp[1];
                    memset(pocketed, 0, sizeof(pocketed)); memset(fallT, 0, sizeof(fallT));
                    firstHit = -1; b[0].vx = b[0].vy = 0; pwr = 0; charging = 0; abilArm = 0; graveOpen = 0; hackOn = 0;
                    abilKind = AB_NONE; abilT = 0; fricMul = 1.0f; pockMul = 1.0f; abilForgive = 0; abilExtra = 0; memset(graveIn, 0, sizeof(graveIn)); buryBall = 0; buryT = 0;
                    overNote[0] = 0; botTimer = 0; botPlanned = 0; state = S_AIM;
                    setmsg("VOLTOU NO TEMPO! JOGUE DE NOVO");
                } else {                                            /* NAO: a jogada vale; resolve normalmente */
                    tmSnap = 0; tmArmed = 0; tmNow = 0;
                    resolve(); repMaybeStart();
                }
                tmSnap = 0;
            }
            break;

        case S_STRIKE:
            strikeK++;
            if (strikeK >= STRIKEN) shoot(strikePwr);
            break;

        case S_ROLL:
            for (i = 0; i < 8; i++) stepPhysics((1.0f / 60.0f) / 8.0f);
            for (i = 0; i < 16; i++)
                if (b[i].in && (b[i].vx != 0 || b[i].vy != 0)) spinBall(i, 1.0f / 60.0f);
            {
                int mv = 0, fl = 0;
                for (i = 0; i < 16; i++) {
                    if (b[i].in && (b[i].vx != 0 || b[i].vy != 0 || b[i].z > 0.0f)) mv = 1;
                    if (fallT[i] > 0) fl = 1;
                }
                repRecord();
                if (!mv && !fl) { resolve(); repMaybeStart(); }
            }
            break;

        case S_REPLAY:
            repUpdate(pressed);
            break;

        case S_PLACE:
            if (!isBot()) {
                if (in & PAD_LEFT)  b[0].x -= 2;
                if (in & PAD_RIGHT) b[0].x += 2;
                if (in & PAD_UP)    b[0].y -= 2;
                if (in & PAD_DOWN)  b[0].y += 2;
                b[0].x += anX * 2.6f; b[0].y += anY * 2.6f;           /* analogico: movimento suave */
                if (b[0].x < TX0+R) b[0].x = TX0+R;
                if (b[0].x > TX1-R) b[0].x = TX1-R;
                if (b[0].y < TY0+R) b[0].y = TY0+R;
                if (b[0].y > TY1-R) b[0].y = TY1-R;
                if ((pressed & PAD_CROSS) && validPlace(b[0].x, b[0].y)) { state = S_AIM; pwr = 0; }
                else if (pressed & PAD_SELECT) openCueMenu();
            } else {
                if (++botTimer > 40) {
                    int tries;
                    b[0].x = HEADX; b[0].y = TCY;
                    for (tries = 0; tries < 500 && !validPlace(b[0].x, b[0].y); tries++) {
                        b[0].x = TX0 + R + (rnd() % (int)(TX1 - TX0 - 2*R));
                        b[0].y = TY0 + R + (rnd() % (int)(TY1 - TY0 - 2*R));
                    }
                    state = S_AIM; botTimer = 0; botPlanned = 0; pwr = 0;
                }
            }
            break;

        case S_TAKE:
            msgTimer = 60;
            if (!isBot()) {
                if (pressed & PAD_LEFT)  takeMove(-1, 0);
                if (pressed & PAD_RIGHT) takeMove(1, 0);
                if (pressed & PAD_UP)    takeMove(0, -1);
                if (pressed & PAD_DOWN)  takeMove(0, 1);
                if (pressed & PAD_CROSS) takeConfirm();
            } else {
                botTimer++;
                if (botTimer == BOT_DELAY[botLevel]) botTakePick();
                if (botTimer > BOT_DELAY[botLevel] + 40) takeConfirm();
            }
            break;

        case S_OVER:
            if (tourney) { if (pressed & (PAD_CROSS | PAD_START)) tourneyAfter(); }
            else if (pressed & PAD_CROSS) { rs += 7919 + menuT; newGame(mode); }
            else if (pressed & PAD_START) { menuInit(); state = S_MENU; }
            break;
        }

        if (batWait && cur == batFor && (state == S_AIM || state == S_PLACE)) {   /* MORCEGOS chegam na vez do adversario */
            batWait = 0; spawnBats(); batOn = 1;
            setmsg("MORCEGOS NA MESA!");
        }
        {   /* inicio de cada jogada (vinda do fim da tacada, da bola na mao ou da bola tirada): a mira ja aponta para uma bola sua */
            static int prevSt = -1;
            if (state == S_AIM && prevSt != S_AIM && prevSt != S_CUES && prevSt != S_REWIND && !isBot() && !paused) autoAim();
            prevSt = state;
        }
        /* tempo por jogada (so no versus): passou o tempo, perde a vez */
        {
            int inTurn = (state == S_AIM || state == S_PLACE || state == S_TAKE || state == S_CUES) && !isBot();   /* torneio: o relogio so corre nas jogadas do jogador */
            if (!(timeSel && (mode == 1 || tourney))) { turnReset = 1; turnT = -1; }
            else if (!inTurn) turnReset = 1;
            else {
                if (turnReset || cur != turnCur) { turnT = TIME_SEC[timeSel] * 60; turnCur = cur; turnReset = 0; }
                if (!paused && --turnT <= 0) {
                    char tb[64];
                    if (state == S_PLACE || (state == S_CUES && cueRet == S_PLACE)) {   /* bola na mao: deixa a branca num lugar valido */
                        int tries;
                        for (tries = 0; tries < 500 && !validPlace(b[0].x, b[0].y); tries++) {
                            b[0].x = TX0 + R + (rnd() % (int)(TX1 - TX0 - 2*R));
                            b[0].y = TY0 + R + (rnd() % (int)(TY1 - TY0 - 2*R));
                        }
                    }
                    sprintf(tb, "TEMPO ESGOTADO! %s PERDE A VEZ", pname(cur));
                    charging = 0; pwr = 0; spinSel = 0; spinV = 0; jumpArm = 0; chalkAnim = 0; abilArm = 0; graveOpen = 0; hackOn = 0;
                    cur = 1 - cur; state = S_AIM; botTimer = 0; botPlanned = 0;
                    turnCur = cur; turnT = TIME_SEC[timeSel] * 60;
                    setmsg(tb);
                }
            }
        }

        {                                                 /* fim de partida: jingle de vitoria ou derrota */
            static int lastSt = -1;
            if (state == S_OVER && lastSt != S_OVER) SFX((mode == 1 || winner == 0) ? SFX_WIN : SFX_LOSE, 90);
            lastSt = state;
        }

        /* animacoes por frame: barra de forca, bolas na cacapa, taco */
        {
            float target = (state == S_STRIKE) ? strikePwr : pwr;
            if (state == S_MENU || state == S_OVER) target = 0;
            if (barLevel < target) barLevel = target;
            else { barLevel -= 0.04f; if (barLevel < target) barLevel = target; }
            for (i = 0; i < 16; i++) if (fallT[i] > 0) fallT[i]--;
            if (follow > 0) follow--;
            if (abilFx > 0) abilFx--;
            pfxUpdate();
        }

        gsKit_clear(g, GS_SETREG_RGBAQ(0x10, 0x10, 0x18, 0x80, 0x00));
        gsKit_prim_sprite(g, 0, 0, (float)g->Width, (float)g->Height, 1, COL(16, 16, 24));
        draw();
        gsKit_queue_exec(g);
        gsKit_sync_flip(g);
    }
    return 0;
}
