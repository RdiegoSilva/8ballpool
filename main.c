/* 8 BALL REIS para PS2 - PS2SDK + gsKit + libpad
 * Modos: 1 jogador (vs BOT, 4 niveis) e 2 jogadores (dois controles).
 * Criador: Diego Reis | Versao 1.8.0
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
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "music.h"

#define R     8.0f
#define TX0   70.0f
#define TX1   598.0f
#define TY0   100.0f
#define TY1   364.0f
#define TCX   ((TX0 + TX1) * 0.5f)
#define TCY   ((TY0 + TY1) * 0.5f)
#define HEADX (TX0 + (TX1 - TX0) * 0.25f)
#define FOOTX (TX0 + (TX1 - TX0) * 0.75f)
#define MAXV  640.0f

enum { S_MENU, S_AIM, S_ROLL, S_PLACE, S_OVER, S_STRIKE, S_CUES, S_CREDITS, S_SETUP, S_NAME, S_TAKE, S_PLAYER, S_OPTS };
enum { GM_8BALL, GM_BR, NGM };            /* modos de sinuca */

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
static int gmode = GM_8BALL, tableSel = 0;       /* modo de jogo e mesa escolhidos */
static int colSel[2] = { 1, 2 };                 /* cores das bolas no modo brasileiro */
static char pnames[2][12] = { "JOGADOR 1", "JOGADOR 2" };
static int takeList[16], takeN, takeSel;         /* bolas que podem ser tiradas apos uma falta (brasileira) */
static int timeSel, turnT, turnCur = -1, turnReset = 1;   /* tempo por jogada (versus): 0 = sem tempo */
static const int TIME_SEC[5] = { 0, 15, 20, 25, 30 };
#define NTIMES 5
static char overNote[48];                         /* motivo do fim de jogo */
static int winsBot[2], winsVs[2], menuNote;   /* placar de vitorias (vs bot / versus play) */
static float botAng, botPow;
static char msg[64];
static int msgTimer;

/* animacoes: tacada, barra de forca e bolas caindo na cacapa */
#define STRIKEN 6             /* frames da tacada (taco indo ate a bola) */
#define FOLLOWN 10            /* frames do taco depois da tacada */
#define FALLN   16            /* frames da bola caindo na cacapa */
static float barLevel, strikePwr, strikeD0, hitX, hitY;
static int   strikeK, follow;
static int   fallT[16], fallP[16];
static float fallX[16], fallY[16];
static float spinSel, spinCur, spinK, barDir = 1.0f;   /* efeito e vai-e-vem da barra */
static int   cueSel[2], cueMenuSel, cueRet, hitCue;    /* taco de cada jogador / menu */
static float chalk[2] = { 1.0f, 1.0f }, cueChalk;      /* giz no taco (1 = cheio) e giz mostrado na ponta */
static int   chalkAnim;                                /* animacao de passar giz (frames) */
static int   fineHold;                                 /* tempo segurando L1/R1 (ajuste fino acelera devagar) */

/* pulo e efeito vertical (v1.6) */
#define JUMP_G     900.0f                          /* gravidade da bola no ar */
#define JUMP_CLEAR 11.0f                           /* acima desta altura a branca passa por cima das outras bolas */
static float spinV, spinVCur, spinVK;              /* efeito vertical: + rola (segue), - puxa (volta) */
static int   jumpArm;                              /* 1 = a proxima tacada faz a branca pular */
static int   charSel[2] = { 0, 1 }, overT;         /* personagens (caricaturas) e relogio da tela de fim */

#define PO 4.0f                                   /* o centro das cacapas fica um pouco para fora da mesa (na moldura) */
static const float PX[6] = { TX0 - PO, TCX, TX1 + PO, TX0 - PO, TCX, TX1 + PO };
static const float PY[6] = { TY0 - PO, TY0 - PO, TY0 - PO, TY1 + PO, TY1 + PO, TY1 + PO };
static const float PR[6] = { 12.6f, 10.0f, 12.6f, 12.6f, 10.0f, 12.6f };   /* raio das cacapas (menores = mais dificil) */
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
#define NCUES 13
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
};

#define GAME_VERSION "1.9.0"
#define GAME_AUTHOR  "DIEGO REIS"

/* dificuldade do BOT: facil / medio / dificil */
#define NLEVELS 4
static int botLevel = 1, menuT;
static int sndOn = 1, musOn = 1, volLvl = 8, fxOn = 1, tipsOn = 1;   /* opcoes do menu de pausa */
static const char  *LVL_NAME[NLEVELS]  = { "FACIL", "MEDIO", "DIFICIL", "PROFISSIONAL" };
static const float  BOT_ERR[NLEVELS]   = { 0.075f, 0.02f, 0.004f, 0.0006f };  /* erro de mira (rad) */
static const float  BOT_POWV[NLEVELS]  = { 0.20f, 0.06f, 0.0f, 0.0f };        /* erro de forca */
static const int    BOT_MISS[NLEVELS]  = { 30, 0, 0, 0 };                      /* % de jogadas ruins */
static const int    BOT_DELAY[NLEVELS] = { 75, 40, 22, 12 };                   /* tempo para pensar */

static unsigned int rs = 2463534242u;
static unsigned int rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

#define COL(r,gg,bb) GS_SETREG_RGBAQ((r),(gg),(bb),0x80,0x00)

/* ---------- desenho ---------- */
static void rect(float x1, float y1, float x2, float y2, u64 c)
{ gsKit_prim_sprite(g, x1+OX, y1+OY, x2+OX, y2+OY, 1, c); }

static void line(float x1, float y1, float x2, float y2, u64 c)
{ gsKit_prim_line(g, x1+OX, y1+OY, x2+OX, y2+OY, 1, c); }

static void discN(float cx, float cy, float r, u64 c, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n;
        gsKit_prim_triangle(g, cx+OX, cy+OY,
                            cx+OX+cosf(a0)*r, cy+OY+sinf(a0)*r,
                            cx+OX+cosf(a1)*r, cy+OY+sinf(a1)*r, 1, c);
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
    for (i = 0; i < n; i++) {
        float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n;
        line(cx+cosf(a0)*r, cy+sinf(a0)*r, cx+cosf(a1)*r, cy+sinf(a1)*r, c);
    }
}

static void text(float x, float y, float s, u64 c, const char *t)
{ gsKit_fontm_print_scaled(g, fm, x+OX, y+OY, 2, s, c, (char *)t); }

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
    disc(cx, cy, r * 0.56f, COL(250,250,245));
    if (n < 10) {
        float p = r * 0.145f;
        drawDigit(n, cx - 1.5f*p, cy - 2.5f*p, p);
    } else {
        float p = r * 0.1f;
        drawDigit(n / 10, cx - 3.5f*p, cy - 2.5f*p, p);
        drawDigit(n % 10, cx + 0.5f*p,  cy - 2.5f*p, p);
    }
}

static void drawShadow(int i)
{
    float z = (i == 0 && b[0].z > 0.0f) ? b[0].z : 0.0f;      /* no pulo a sombra fica no chao, menor e mais fraca */
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
#define NLON 12               /* malha da esfera */
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
        cs = 0.30f;
        ix = (int)floorf((gu + 1.5f*cs) / cs); iy = (int)floorf((gv + 2.5f*cs) / cs);
        if (ix < 0 || ix > 2 || iy < 0 || iy > 4) return 0;
        return DIG[n][iy*3 + ix] == '1';
    }
    cs = 0.20f;
    ix = (int)floorf((gu + 3.5f*cs) / cs); iy = (int)floorf((gv + 2.5f*cs) / cs);
    if (iy < 0 || iy > 4 || ix < 0 || ix > 6) return 0;
    if (ix < 3) return DIG[n / 10][iy*3 + ix] == '1';
    if (ix == 3) return 0;
    return DIG[n % 10][iy*3 + (ix - 4)] == '1';
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
                {
                    float d[4]  = { z, x, -z, -x };
                    float gu[4] = { x, -z, -x, z };
                    float gv = -y;
                    for (k = 0; k < 4; k++)
                        if (d[k] > 0 && gu[k]*gu[k] + gv*gv < (skinNum[id] ? 0.2116f : 0.016f)) {
                            cr = cg = cb = 248;
                            if (skinNum[id] && digitInk(id, gu[k] / 0.46f, gv / 0.46f)) cr = cg = cb = 20;
                            break;
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
    int i, k, key = br ? 1 + colSel[0] * NBRC + colSel[1] : (ballTheme ? 5000 + ballTheme : 0);
    if (key == skinKey) return;
    skinKey = key;
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

    discN(x, y, r * 1.03f, GS_SETREG_RGBAQ(10, 40, 25, 0x50, 0x00), 12);  /* borda suave */

    for (a = 0; a <= NLAT; a++)
        for (c = 0; c <= NLON; c++) {
            const float *p = mP[a][c];
            float vx = M[0]*p[0] + M[1]*p[1] + M[2]*p[2];
            float vy = M[3]*p[0] + M[4]*p[1] + M[5]*p[2];
            float vz = M[6]*p[0] + M[7]*p[1] + M[8]*p[2];
            float dif = vx * -0.42f + vy * 0.52f + vz * 0.74f;
            int cv;
            if (dif < 0) dif = 0;
            cv = (int)((0.38f + 0.68f * dif) * 150.0f);
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

    /* brilho da luz (fixo na tela) */
    discN(x - r*0.30f, y - r*0.32f, r*0.42f, GS_SETREG_RGBAQ(255, 255, 255, 0x1c, 0x00), 8);
    discN(x - r*0.38f, y - r*0.40f, r*0.20f, GS_SETREG_RGBAQ(255, 255, 255, 0x78, 0x00), 6);
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
    float x = fallX[i] + (PX[fallP[i]] - fallX[i]) * e;
    float y = fallY[i] + (PY[fallP[i]] - fallY[i]) * e;
    float r = R * (0.42f + 0.58f * t);
    if (useTex) drawBallMesh(i, x, y, r); else drawBallVec(i, x, y, r);
    discN(x, y, r * 1.02f, GS_SETREG_RGBAQ(0, 0, 0, (int)(0x78 * (1.0f - t)), 0x00), 10);
}

/* ---------- logica ---------- */
static int ballGroup(int i) { return i == 0 ? -1 : (i == 8 ? 2 : (i < 8 ? 0 : 1)); }

static int countGroup(int gr)
{
    int i, n = 0;
    for (i = 1; i < 16; i++) if (b[i].in && ballGroup(i) == gr) n++;
    return n;
}

static void setmsg(const char *s) { strncpy(msg, s, 63); msg[63] = 0; msgTimer = 150; }
static int isBot(void) { return mode == 0 && cur == 1; }
static const char *pname(int pl) { return (pl == 1 && mode == 0) ? "BOT" : pnames[pl]; }
static int curCue(void) { return isBot() ? 0 : cueSel[cur]; }
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

static void newGame(int m)
{
    char buf[40];
    mode = m;
    if (gmode == GM_BR) { rackBR(); grp[0] = 0; grp[1] = 1; }
    else                { rack();   grp[0] = grp[1] = -1; }
    cur = 0; ang = 0; pwr = 0;
    charging = 0; breakShot = (gmode == GM_8BALL); botTimer = 0; botPlanned = 0; state = S_AIM;
    spinSel = spinCur = 0; barDir = 1.0f;
    spinV = spinVCur = 0; jumpArm = 0; overT = 0;
    chalk[0] = chalk[1] = 1.0f; chalkAnim = 0;
    turnReset = 1; turnCur = -1;
    sprintf(buf, gmode == GM_BR ? "%s COMECA" : "%s - QUEBRA", pname(0));
    setmsg(buf);
}

static void shoot(float p)
{
    float ch = isBot() ? 1.0f : chalk[cur], a = ang, pm = 1.0f;
    memset(pocketed, 0, sizeof(pocketed));
    firstHit = -1;
    gAtStart = grp[cur];
    startOwnLeft = gAtStart >= 0 ? countGroup(gAtStart) : 99;
    hitX = b[0].x; hitY = b[0].y; hitCue = curCue();
    if (!isBot()) {                                   /* giz: ponta sem giz escorrega e a tacada sai torta */
        float miss = (1.0f - ch) * 0.045f, dev = ((int)(rnd() % 2001) - 1000) / 1000.0f;
        if (ch < 0.25f && (fabsf(spinSel) > 0.6f || fabsf(spinV) > 0.6f || jumpArm)) { miss *= 2.0f; pm = 0.9f; setmsg("ESCORREGOU! PASSE GIZ NO TACO"); }
        else if (ch < 0.25f) setmsg("POUCO GIZ! PASSE GIZ NO TACO");
        a += dev * miss;
        chalk[cur] -= 0.12f + 0.08f * fabsf(spinSel) + 0.05f * fabsf(spinV) + (jumpArm ? 0.08f : 0.0f);
        if (chalk[cur] < 0.0f) chalk[cur] = 0.0f;
    }
    b[0].vx = cosf(a) * p * MAXV * cuePowMul(hitCue) * pm;
    b[0].vy = sinf(a) * p * MAXV * cuePowMul(hitCue) * pm;
    spinCur = spinSel * (0.8f + 0.2f * ch); spinK = cueSpinK(hitCue); spinSel = 0;
    spinVCur = spinV * (0.8f + 0.2f * ch); spinVK = cueVSpin(hitCue); spinV = 0;
    if (jumpArm && !isBot()) {                        /* pulo: a branca sobe e passa por cima das outras bolas */
        b[0].vz = (150.0f + 90.0f * p) * cueJumpMul(hitCue);
        b[0].z = 0.01f; b[0].vx *= 0.9f; b[0].vy *= 0.9f;
    }
    jumpArm = 0;
    breakShot = 0; pwr = 0; charging = 0; follow = FOLLOWN; state = S_ROLL;
}

/* menu de tacos (SELECT) */
static void openCueMenu(void)
{
    cueMenuSel = cueSel[cur]; cueRet = state; charging = 0; state = S_CUES;
}

/* o taco vai para tras com a forca e e disparado para a bola */
static void startStrike(float p)
{
    strikePwr = p; strikeD0 = R + 4 + pwr * 70; strikeK = 0;
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

static int stepPhysics(float dt)
{
    int i, j, p, moving = 0;
    for (i = 0; i < 16; i++) {
        int inMouth = 0, air, wl = 0;
        float mdx = 0.0f, mdy = 0.0f, mpr = 0.0f, mpx = 0.0f, mpy = 0.0f;
        if (!b[i].in) continue;
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
            float dx = b[i].x - PX[p], dy = b[i].y - PY[p], d2 = dx*dx + dy*dy, mouth = PR[p] + R;
            if (!air && d2 < PR[p]*PR[p]) {
                fallT[i] = FALLN; fallP[i] = p; fallX[i] = b[i].x; fallY[i] = b[i].y;
                b[i].in = 0; b[i].vx = b[i].vy = 0; b[i].z = b[i].vz = 0; pocketed[i] = 1; break;
            }
            if (d2 < mouth*mouth) {                       /* na boca da cacapa: a tabela abre, a bola pode entrar */
                float d = sqrtf(d2), toward = -(b[i].vx * dx + b[i].vy * dy);
                inMouth = 1; mdx = fabsf(dx); mdy = fabsf(dy); mpr = PR[p]; mpx = PX[p]; mpy = PY[p];
                if (d > 0.001f && toward > 0.0f && POCK_PULL > 0.0f) {   /* so ajuda quem vai em direcao ao buraco */
                    b[i].vx -= dx / d * POCK_PULL * dt;
                    b[i].vy -= dy / d * POCK_PULL * dt;
                }
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
        if (wl && i == 0 && spinCur != 0.0f) {          /* efeito lateral muda o angulo do rebote na tabela */
            float kx = -b[0].vy, ky = b[0].vx;
            b[0].vx += kx * spinCur * 0.12f; b[0].vy += ky * spinCur * 0.12f;
            spinCur *= 0.55f;
        }
    }
    for (i = 0; i < 16; i++) {
        if (!b[i].in) continue;
        for (j = i + 1; j < 16; j++) {
            float dx, dy, d2, d, nx, ny, rv, ov;
            if (!b[j].in) continue;
            if (i == 0 && b[0].z > JUMP_CLEAR) break;      /* pulando: passa por cima de todas */
            dx = b[j].x - b[i].x; dy = b[j].y - b[i].y; d2 = dx*dx + dy*dy;
            if (d2 >= 4*R*R || d2 < 0.0001f) continue;
            d = sqrtf(d2); nx = dx / d; ny = dy / d;
            ov = (2*R - d) * 0.5f;
            b[i].x -= nx*ov; b[i].y -= ny*ov; b[j].x += nx*ov; b[j].y += ny*ov;
            rv = (b[i].vx - b[j].vx) * nx + (b[i].vy - b[j].vy) * ny;
            if (rv > 0) {
                float cvx = b[i].vx, cvy = b[i].vy;
                rv *= 0.98f;                      /* choque quase elastico */
                b[i].vx -= rv*nx; b[i].vy -= rv*ny;
                b[j].vx += rv*nx; b[j].vy += rv*ny;
                if (i == 0 && firstHit < 0) firstHit = j;
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
    for (i = 0; i < 16; i++) {
        float sp, s2;
        if (!b[i].in) continue;
        if (b[i].z > 0.0f) { moving = 1; continue; }       /* no ar nao ha atrito */
        sp = sqrtf(b[i].vx*b[i].vx + b[i].vy*b[i].vy);
        if (sp < 6.0f) { b[i].vx = b[i].vy = 0; continue; }
        s2 = sp * (1.0f - 0.6f * dt) - 24.0f * dt;
        if (sp < 40.0f) s2 -= 30.0f * dt;     /* freio suave no final */
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
    return 1;
}

static void endTurn(int foul, int ownPocket)
{
    char buf[64];
    botTimer = 0; botPlanned = 0;
    if (foul) {
        if (!b[0].in) { b[0].in = 1; fallT[0] = 0; b[0].x = (gmode == GM_BR) ? TCX : HEADX; b[0].y = TCY; }
        b[0].vx = b[0].vy = 0; b[0].z = b[0].vz = 0;
        cur = 1 - cur;
        state = S_PLACE;
        sprintf(buf, "FALTA! BOLA NA MAO - %s", pname(cur));
        setmsg(buf);
    } else {
        if (!ownPocket) cur = 1 - cur;
        state = S_AIM;
        sprintf(buf, "VEZ DE %s", pname(cur));
        setmsg(buf);
    }
}

static void endGame(int w)
{
    winner = w; overT = 0;
    (mode == 0 ? winsBot : winsVs)[w]++;
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
static void resolveBR(void)
{
    int i, foul, own = 0, mine, theirs;
    char buf[64];
    overNote[0] = 0;
    if (pocketed[0]) {
        strcpy(overNote, "SUICIDIO - BRANCA NA CACAPA");
        endGame(1 - cur);
        return;
    }
    foul = firstHit < 0 || ballGroup(firstHit) != grp[cur];
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

static void resolve(void)
{
    int i, foul, ownPocket = 0;

    overNote[0] = 0;
    if (gmode == GM_BR) { resolveBR(); return; }

    if (pocketed[8]) {
        int ok = gAtStart >= 0 && countGroup(gAtStart) == 0 && !pocketed[0];
        winner = ok ? cur : 1 - cur;
        (mode == 0 ? winsBot : winsVs)[winner]++;
        state = S_OVER;
        return;
    }
    foul = pocketed[0] || firstHit < 0;
    if (!foul && gAtStart >= 0) {
        int fg = ballGroup(firstHit);
        if (fg != gAtStart && !(fg == 2 && startOwnLeft == 0)) foul = 1;
    }
    if (!foul && gAtStart < 0 && firstHit == 8) foul = 1;

    if (!foul && grp[cur] < 0) {
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

static void botPlan(void)
{
    int t, p, found = 0, gr = grp[cur];
    float best = 1e9f, cx = b[0].x, cy = b[0].y;

    for (t = 1; t < 16; t++) {
        if (!b[t].in) continue;
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
            sc = al + dl + (1.0f - cc) * (botLevel == 3 ? 900.0f : 400.0f);   /* profissional prefere tacadas retas */
            if (sc < best) {
                best = sc; found = 1;
                botAng = atan2f(gy - cy, gx - cx);
                botPow = (botLevel == 3 ? 0.30f : 0.35f) + (al + dl) / (botLevel == 3 ? 640.0f : 600.0f);
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
    botPow += (((int)(rnd() % 2001)) - 1000) / 1000.0f * BOT_POWV[botLevel];
    if (botPow < 0.25f) botPow = 0.25f;
    if (breakShot) botPow = 1.0f;
    if (botPow > 1.0f) botPow = 1.0f;
    botAng += (((int)(rnd() % 2001)) - 1000) / 1000.0f * BOT_ERR[botLevel];
}

/* ---------- entrada ---------- */
static u32 readpad(int port)
{
    struct padButtonStatus s;
    int st = padGetState(port, 0);
    if (st != PAD_STATE_STABLE && st != PAD_STATE_FINDCTP1) return 0;
    if (padRead(port, 0, &s) == 0) return 0;
    return 0xffff ^ s.btns;
}

/* ---------- desenho do jogo ---------- */
typedef struct { const char *name; u8 felt[3], wood[3], dia[3]; int style; } TableDef;   /* style: 0 normal, 1 preta/ouro, 2 neon animada */
#define NTABLES 16
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
};
static u64 tcCush, tcCushHi, tcCushLo, tcDia;      /* cores derivadas da mesa escolhida */

static void cushH(float xa, float xb, float yo, float yi)
{
    quad(xa - 7, yo, xb + 7, yo, xb, yi, xa, yi, tcCush);
    line(xa, yi, xb, yi, tcCushHi);
    line(xa - 7, yo, xb + 7, yo, tcCushLo);
}

static void cushV(float ya, float yb, float xo, float xi)
{
    quad(xo, ya - 7, xo, yb + 7, xi, yb, xi, ya, tcCush);
    line(xi, ya, xi, yb, tcCushHi);
    line(xo, ya - 7, xo, yb + 7, tcCushLo);
}

static void diamond(float cx, float cy)
{
    quad(cx, cy - 3, cx + 3, cy, cx, cy + 3, cx - 3, cy, tcDia);
}


/* ---------- mesas especiais: PRETA OURO (detalhes dourados) e NEON (animada) ---------- */
static u64 CA(int r, int gg, int bb, int a);        /* definida mais abaixo */
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

/* stage 0: moldura (antes do feltro) | 1: feltro (antes das tabelas) | 2: por cima das cacapas */
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
        case 9:                                              /* faroeste: bola de mato e poeira */
            if (i == 0) ring(x0 + fmodf(mt * 0.5f, w), y1 - 6.0f - fabsf(sinf(mt * 0.06f)) * 6.0f, 2.6f, CA(112, 76, 38, 0x90));
            if ((i & 3) == 0) { float dx2 = x0 + fmodf(mt * (0.6f + i * 0.05f) + i * 17.0f, w); line(dx2, y0 + h * 0.3f + i * 1.4f, dx2 + 5.0f, y0 + h * 0.3f + i * 1.4f, CA(240, 215, 165, 0x28)); }
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
    float f = 26.0f * k, c = 11.0f * k, pk = 12.0f * k, pr = R * k;
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
        discN(qx[i], qy[i], pk + 1.4f, COL(52,31,14), 10);
        discN(qx[i], qy[i], pk, COL(3,3,5), 10);
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
        pvBall(cx, cy, pr, BC[0]);
    } else {
        int n = 0, r2, cc;
        for (r2 = 0; r2 < 5; r2++)
            for (cc = 0; cc <= r2; cc++) {
                bx = FOOTX + r2 * (R * 1.76f); by = TCY + (cc - r2 * 0.5f) * (2 * R + 0.4f);
                pvBall(x0 + (bx - TX0) * k, y0 + (by - TY0) * k, pr, THEME[ballTheme][n++ % 8]);
            }
        pvBall(x0 + (HEADX - TX0) * k, cy, pr, BC[0]);
    }
}

static void drawTable(void)
{
    const TableDef *T = &TABLES[tableSel];
    u64 w0 = shade(T->wood, 0.36f, 0), w1 = shade(T->wood, 0.80f, 0), w2 = shade(T->wood, 1.0f, 0);
    u64 whi = shade(T->wood, 1.28f, 14), wlo = shade(T->wood, 0.48f, 0), wgr = shade(T->wood, 0.42f, 0);
    u64 ga = shade(T->wood, 0.90f, 0), gb = shade(T->wood, 1.10f, 5);
    int p, k;
    float x, y;

    tcCush = shade(T->felt, 0.50f, 0); tcCushHi = shade(T->felt, 1.35f, 12); tcCushLo = shade(T->felt, 0.33f, 0);
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
    for (k = 1; k < 8; k++)
        rect(TX0 + k*22, TY0 + k*14, TX1 - k*22, TY1 - k*14, shade(T->felt, 1.0f, k * 3));
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
    cushH(TX0 + PR[0] + 4, TCX - PR[1] - 4, TY0 - 11, TY0);
    cushH(TCX + PR[1] + 4, TX1 - PR[0] - 4, TY0 - 11, TY0);
    cushH(TX0 + PR[0] + 4, TCX - PR[1] - 4, TY1 + 11, TY1);
    cushH(TCX + PR[1] + 4, TX1 - PR[0] - 4, TY1 + 11, TY1);
    cushV(TY0 + PR[0] + 4, TY1 - PR[0] - 4, TX0 - 11, TX0);
    cushV(TY0 + PR[0] + 4, TY1 - PR[0] - 4, TX1 + 11, TX1);

    /* diamantes de mira na madeira */
    for (k = 1; k <= 3; k++) {
        x = TX0 + (TCX - TX0) * k / 4.0f;
        diamond(x, TY0 - 18.5f);  diamond(x, TY1 + 18.5f);
        x = TCX + (TX1 - TCX) * k / 4.0f;
        diamond(x, TY0 - 18.5f);  diamond(x, TY1 + 18.5f);
        y = TY0 + (TY1 - TY0) * k / 4.0f;
        diamond(TX0 - 18.5f, y);  diamond(TX1 + 18.5f, y);
    }

    /* cacapas: aro de couro, borda dourada e buraco */
    for (p = 0; p < 6; p++) {
        discN(PX[p], PY[p], PR[p] + 7, CA(0, 0, 0, 0x1a), 20);
        discN(PX[p], PY[p], PR[p] + 4, COL(52,31,14), 20);
        discN(PX[p] - 1.0f, PY[p] - 1.5f, PR[p] + 2.5f, COL(70,42,20), 20);
        ring(PX[p], PY[p], PR[p] + 4, COL(176,132,58));
        ring(PX[p], PY[p], PR[p] + 3, COL(214,172,86));
        discN(PX[p], PY[p], PR[p] + 0.5f, COL(3,3,5), 20);
        ring(PX[p], PY[p], PR[p] + 0.5f, COL(28,20,12));
        discN(PX[p] - PR[p] * 0.55f, PY[p] - PR[p] * 0.75f, 1.6f, CA(255, 240, 200, 0x40), 6);
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
    for (i = 0; i < n; i++) {
        float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n;
        beam(cx + cosf(a0)*r, cy + sinf(a0)*r, cx + cosf(a1)*r, cy + sinf(a1)*r, w, c);
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
        if (ex*ex + ey*ey < (PR[k] + 4.0f) * (PR[k] + 4.0f)) return k;
    }
    return -1;
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
    int sn = (curCue() == CUE_SNIPER);
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
            int h2, w2, pk;
            if (nl < 0.01f) return;
            nx /= nl; ny /= nl;
            dot = dx*nx + dy*ny; if (dot < 0) dot = 0;

            /* caminho da bola alvo: ate a tabela/outra bola, destacando cacapa */
            L = (22.0f + 70.0f * dot) * gs;
            if (sn) L = 1.0e6f;                           /* sniper: caminho completo */
            tmax = castRay(b[hit].x, b[hit].y, nx, ny, hit, &h2, &w2);
            if (L >= tmax - 0.5f) {
                L = tmax;
                if (h2 < 0) {
                    pk = nearPocket(b[hit].x + nx*L, b[hit].y + ny*L);
                    if (pk >= 0) {
                        ringN(PX[pk], PY[pk], PR[pk] + 2.0f, 2.0f, 24, CA(90, 255, 120, 0x78));
                        ringN(PX[pk], PY[pk], PR[pk] + 5.0f, 1.2f, 24, CA(90, 255, 120, 0x40));
                    }
                }
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

/* caixa do HUD: sombra, borda, fundo escuro translucido, brilho no topo e base mais escura */
static void hbox(float x1, float y1, float x2, float y2, u64 border, u64 fill)
{
    rect(x1 - 1.5f, y1 - 1.5f, x2 + 3.5f, y2 + 3.5f, GS_SETREG_RGBAQ(0, 0, 0, 0x16, 0x00));
    rect(x1 - 1.5f, y1 - 1.5f, x2 + 1.5f, y2 + 1.5f, border);
    rect(x1, y1, x2, y2, fill);
    rect(x1, y1, x2, y1 + (y2 - y1) * 0.42f, CA(255, 255, 255, 0x07));
    rect(x1, y1 + (y2 - y1) * 0.62f, x2, y2, CA(0, 0, 0, 0x0e));
    rect(x1, y1, x2, y1 + 1.0f, CA(255, 255, 255, 0x22));
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

    rect(bx - 4, by0 - 4, bx + bw + 4, by1 + 4, COL(170,170,176));
    rect(bx - 3, by0 - 3, bx + bw + 3, by1 + 3, COL(24,24,28));
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
    text(5, 58, 0.36f, COL(230,230,230), "FORCA");
    sprintf(buf, "%d%%", (int)(barLevel * 100 + 0.5f));
    text(8, by1 + 12, 0.36f, COL(255,230,60), buf);
}

/* indicador de efeito: o ponto vermelho mostra o lado (L2/R2) e o rola/puxa (CIMA/BAIXO); PULO acende com TRIANGULO */
static void drawSpinIndicator(void)
{
    const float cx = 30.0f, cy = 404.0f, r = 10.0f;
    discN(cx, cy, r + 1.5f, COL(60,60,66), 16);
    discN(cx, cy, r, COL(236,236,230), 16);
    line(cx, cy - r, cx, cy + r, COL(190,190,184));
    line(cx - r, cy, cx + r, cy, COL(190,190,184));
    discN(cx + spinSel * (r - 3.5f), cy - spinV * (r - 3.5f), 2.8f, COL(215,40,40), 8);
    text(8, 418, 0.3f, COL(200,200,205), "EFEITO");
    text(8, 430, 0.3f, jumpArm ? (((menuT / 10) & 1) ? COL(120,255,150) : COL(60,200,100)) : COL(95,100,112), "PULO");
}

/* giz do jogador da vez (canto inferior direito) */
static void drawChalkIndicator(void)
{
    const float x = 604.0f, y = 399.0f;
    float lv = chalk[cur];
    u64 fc;
    if (isBot()) return;
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

/* painel de cada jogador (topo): nome, grupo e as bolas que ainda restam */
static void drawPlayerPanel(int pl)
{
    float x1 = pl == 0 ? 70.0f : 378.0f, x2 = x1 + 220.0f, y1 = 6.0f, y2 = 64.0f;
    int act = (pl == cur), k;
    const char *nm = pname(pl);
    const char *gn = gmode == GM_BR ? BRC_NAME[colSel[pl]] :
                     (grp[pl] < 0 ? "MESA ABERTA" : (grp[pl] == 0 ? "LISAS" : "LISTRADAS"));
    int nb = gmode == GM_BR ? 4 : 7;
    float sp = gmode == GM_BR ? 36.0f : 26.0f;

    hbox(x1, y1, x2, y2, act ? CA(255, 215, 60, 0x7c) : CA(70, 78, 96, 0x60),
         act ? CA(44, 38, 10, 0x74) : CA(8, 12, 22, 0x68));
    rect(x1, y1, x1 + 5, y2, act ? COL(255,215,60) : COL(70,78,96));
    text(x1 + 14, y1 + 5, 0.5f, act ? COL(255,235,110) : COL(165,170,180), nm);
    text(x1 + 14, y1 + 23, 0.32f, act ? COL(255,255,255) : COL(130,138,152), gn);
    drawChar(charSel[pl], x2 - 22.0f, y1 + 28.0f, 0.42f, 0, menuT, 0);          /* avatar do personagem */

    if (act && timeSel && mode == 1 && turnT >= 0 && state != S_ROLL && state != S_STRIKE) {
        int sec = (turnT + 59) / 60;
        float fr = (float)turnT / (float)(TIME_SEC[timeSel] * 60);
        char tb[8];
        u64 tcol = sec <= 5 ? ((menuT / 8) & 1 ? COL(255,90,80) : COL(255,160,140)) : COL(255,255,255);
        if (fr < 0) fr = 0; if (fr > 1) fr = 1;
        sprintf(tb, "%dS", sec);
        text(x2 - 44 - tw(0.5f, tb), y1 + 21, 0.5f, tcol, tb);
        rect(x1, y2 + 3, x2, y2 + 7, CA(40, 44, 56, 0x70));
        rect(x1, y2 + 3, x1 + (x2 - x1) * fr, y2 + 7, sec <= 5 ? COL(255,90,80) : (fr < 0.5f ? COL(255,200,60) : COL(110,230,130)));
    }
    if (grp[pl] < 0) return;
    for (k = 0; k < nb; k++) {
        int id = grp[pl] == 0 ? k + 1 : k + 9;
        float bx = x1 + 24 + k * sp, by = y1 + 46.0f;
        if (b[id].in) drawBallAt(id, bx, by, 7.0f);
        else          ring(bx, by, 6.0f, COL(70,76,90));
    }
    if (gmode == GM_8BALL && countGroup(grp[pl]) == 0) /* so falta a bola 8 */
        drawBallAt(8, x1 + 24 + 7 * 26.0f, y1 + 46.0f, 7.0f);
}

/* placar de vitorias (centro do topo) */
static void drawScoreBoard(void)
{
    int *w = mode == 0 ? winsBot : winsVs;
    char buf[16];
    float s;
    hbox(296, 6, 372, 64, CA(70, 78, 96, 0x60), CA(8, 12, 22, 0x68));
    text(334 - tw(0.28f, "PLACAR") * 0.5f, 13, 0.28f, COL(150,158,175), "PLACAR");
    rect(304, 28, 364, 29, CA(120, 128, 146, 0x30));
    sprintf(buf, "%d : %d", w[0] > 99 ? 99 : w[0], w[1] > 99 ? 99 : w[1]);
    s = strlen(buf) <= 5 ? 0.55f : 0.4f;
    text(334 - tw(s, buf) * 0.5f, 37, s, COL(255,235,110), buf);
}

static void drawHUD(void)
{
    float x;
    int pl;
    for (pl = 0; pl < 2; pl++) drawPlayerPanel(pl);
    drawScoreBoard();

    drawPowerBar();
    drawSpinIndicator();
    drawChalkIndicator();

    /* barra inferior: mensagem + teclas + taco equipado */
    hbox(70, 396, 598, 443, CA(70, 78, 96, 0x60), CA(8, 12, 22, 0x70));
    if (msgTimer > 0) {
        msgTimer--;
        text(334 - tw(0.42f, msg) * 0.5f, 401, 0.42f, COL(255,235,110), msg);
    }

    x = 80.0f;
    if (state == S_AIM && !isBot()) {
        if (charging) {
            x += tipKey(x, 421, "X", "DISPARAR", COL(255,230,60), COL(235,235,240)) + 16;
            tipKey(x, 421, "O", "CANCELAR", COL(255,230,60), COL(235,235,240));
        } else {
            if (tipsOn && ((menuT / 360) & 1)) {                    /* 2a pagina de dicas */
                x += tipKey(x, 421, "CIMA/BAIXO", "ROLA/PUXA", COL(255,230,60), COL(235,235,240)) + 14;
                tipKey(x, 421, "TRI", jumpArm ? "PULO LIGADO" : "PULO", COL(255,230,60), COL(235,235,240));
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

    if (state == S_AIM || state == S_CUES) {
        const CueDef *cd = &CUES[curCue()];
        char buf[40];
        sprintf(buf, "TACO: %s", cd->name);
        rect(590 - tw(0.36f, buf) - 16, 424, 590 - tw(0.36f, buf) - 8, 432,
             COL(cd->tip[0], cd->tip[1], cd->tip[2]));
        text(590 - tw(0.36f, buf), 423, 0.36f, COL(205,210,220), buf);
    }
}

static void tri(float x1, float y1, float x2, float y2, float x3, float y3, u64 c)
{ gsKit_prim_triangle(g, x1+OX, y1+OY, x2+OX, y2+OY, x3+OX, y3+OY, 1, c); }

/* ---------- caricaturas (10 personagens, desenhadas com formas) ---------- */
#define NCHARS 10
#define CPI 3.14159265f
static const char *CHAR_NAME[NCHARS] = { "PINGUIM", "ROBO", "PIRATA", "NINJA", "ALIEN",
                                         "COWBOY", "VAMPIRO", "MAGO", "VIKING", "GATO" };

static void ell(float cx, float cy, float rx, float ry, u64 c)
{
    int i, n = 16;
    for (i = 0; i < n; i++) {
        float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n;
        tri(cx, cy, cx + cosf(a0) * rx, cy + sinf(a0) * ry, cx + cosf(a1) * rx, cy + sinf(a1) * ry, c);
    }
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
static void drawCueMenu(void)
{
    static const char *SN[4] = { "FORCA", "EFEITO", "MIRA", "CONTROLE" };
    static const u8 SC[4][3] = { {235,95,55}, {90,155,255}, {95,215,115}, {240,210,70} };
    const CueDef *cd = &CUES[cueMenuSel];
    int val[4], k;
    char buf[48];

    val[0] = cd->force; val[1] = cd->effect; val[2] = cd->aim; val[3] = cd->ctl;

    rect(0, 0, 640, 448, GS_SETREG_RGBAQ(0, 0, 0, 0x50, 0x00));
    rect(108, 84, 532, 386, COL(84,88,104));
    rect(110, 86, 530, 384, COL(18,20,30));
    rect(110, 86, 530, 116, COL(34,42,70));
    sprintf(buf, "TACOS - %s", pname(cur));
    text(128, 90, 0.6f, COL(255,230,60), buf);

    /* foto do taco (palco de feltro) */
    rect(126, 126, 514, 180, COL(70,50,28));
    rect(128, 128, 512, 178, COL(24,104,64));
    drawCue(175.0f, 153.0f, 0.0f, 0x80, 3.14159265f, cueMenuSel);
    tri(122, 143, 122, 163, 112, 153, COL(255,230,60));
    tri(518, 143, 518, 163, 528, 153, COL(255,230,60));

    text(130, 186, 0.7f, COL(255,255,255), cd->name);
    if (cueSel[cur] == cueMenuSel) text(380, 192, 0.5f, COL(110,230,130), "EM USO");
    else text(380, 192, 0.5f, COL(255,230,60), "X: EQUIPAR");

    for (k = 0; k < 4; k++) {
        float y = 224.0f + k * 30.0f;
        text(130, y, 0.5f, COL(210,210,220), SN[k]);
        rect(270, y + 2, 510, y + 17, COL(40,42,52));
        rect(270, y + 2, 270 + 2.4f * val[k], y + 17, COL(SC[k][0], SC[k][1], SC[k][2]));
    }

    for (k = 0; k < NCUES; k++) {
        float x = 320.0f + (k - (NCUES - 1) * 0.5f) * 30.0f;
        disc(x, 348, k == cueMenuSel ? 6.0f : 4.0f,
             k == cueMenuSel ? COL(255,230,60) : COL(100,100,110));
        if (k == cueSel[cur]) ring(x, 348, 9.0f, COL(110,230,130));
    }
    text(170, 362, 0.4f, COL(170,170,180), "ESQ/DIR: ESCOLHER   X: EQUIPAR   O/SELECT: VOLTAR");
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
    for (i = 0; i < NMB; i++) {
        Ball *p = &b[MB_ID[i]];
        p->x += p->vx * dt; p->y += p->vy * dt;
        if (p->x < MBR)          { p->x = MBR;          p->vx =  fabsf(p->vx); }
        if (p->x > 640.0f - MBR) { p->x = 640.0f - MBR; p->vx = -fabsf(p->vx); }
        if (p->y < MBR)          { p->y = MBR;          p->vy =  fabsf(p->vy); }
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

/* fundo: degrade, brilho do feltro, feixes de luz, bolas rolando */
static void drawBackdrop(void)
{
    int k;
    float t = menuT * 0.02f;
    for (k = 0; k < 28; k++)                                  /* degrade suave (28 faixas) */
        rect(0, k * 16.0f, 640, k * 16.0f + 16.0f, COL(5 + k / 4, 8 + k / 2, 34 - k / 2));
    for (k = 0; k < 5; k++)                                   /* brilho verde do feltro */
        discN(320.0f + 40.0f * sinf(t), 250.0f, 330.0f - k * 55.0f,
              GS_SETREG_RGBAQ(24, 120, 78, 0x0c, 0x00), 28);
    for (k = 0; k < 3; k++)                                   /* luz quente do alto */
        discN(320.0f - 60.0f * sinf(t * 0.7f), -40.0f, 300.0f - k * 80.0f,
              GS_SETREG_RGBAQ(255, 190, 90, 0x06, 0x00), 28);
    for (k = 0; k < 2; k++) {
        float x = fmodf(menuT * 1.6f + k * 420.0f, 1100.0f) - 260.0f;
        quad(x, 0, x + 50, 0, x - 70, 448, x - 120, 448, GS_SETREG_RGBAQ(255,255,255,0x0a,0x00));
    }
    for (k = 0; k < 14; k++) {                                /* particulas de luz subindo */
        float px = fmodf(k * 97.0f + menuT * (0.15f + (k % 4) * 0.08f), 680.0f) - 20.0f;
        float py = 448.0f - fmodf(k * 61.0f + menuT * (0.35f + (k % 3) * 0.15f), 470.0f);
        float pr = 2.0f + (k % 3) * 1.5f;
        discN(px, py, pr, CA(255, 225, 150, 0x10 + (k % 3) * 4), 8);
        discN(px, py, pr * 0.5f, CA(255, 240, 200, 0x24), 6);
    }
    for (k = 0; k < NMB; k++) {
        int id = MB_ID[k];
        discN(b[id].x + 5, b[id].y + 7, MBR * 1.05f, GS_SETREG_RGBAQ(0, 0, 0, 0x28, 0x00), 14);
        drawBallAt(id, b[id].x, b[id].y, MBR);
    }
    for (k = 0; k < 6; k++) {                                 /* vinheta nas bordas */
        int a = 0x2c - k * 6;
        float d = k * 12.0f;
        rect(0, d, 640, d + 12, CA(0, 0, 8, a));
        rect(0, 448 - d - 12, 640, 448 - d, CA(0, 0, 8, a));
        rect(d, 0, d + 12, 448, CA(0, 0, 8, a - 6));
        rect(640 - d - 12, 0, 640 - d, 448, CA(0, 0, 8, a - 6));
    }
    rect(0, 0, 640, 448, GS_SETREG_RGBAQ(0, 0, 10, 0x38, 0x00));   /* escurece p/ ler o menu */
}

static void panel(float x1, float y1, float x2, float y2)
{
    int k, n = 8;
    float L = 14.0f;
    u64 gold = CA(255, 215, 80, 0x7c);
    rect(x1 - 5, y1 - 4, x2 + 6, y2 + 7, GS_SETREG_RGBAQ(0, 0, 0, 0x28, 0x00));      /* sombra */
    for (k = 0; k < n; k++)                                                          /* fundo em degrade */
        rect(x1, y1 + (y2 - y1) * k / n, x2, y1 + (y2 - y1) * (k + 1) / n,
             CA(5 + (n - k) * 2, 12 + (n - k) * 2, 22 + (n - k) * 3, 0x68));
    rect(x1, y1, x2, y1 + (y2 - y1) * 0.35f, CA(255, 255, 255, 0x06));
    frameR(x1 - 2, y1 - 2, x2 + 2, y2 + 2, 1.0f, CA(150, 200, 170, 0x44));            /* friso externo */
    frameR(x1, y1, x2, y2, 1.0f, CA(20, 40, 36, 0x70));                                /* friso interno */
    rect(x1 + 2, y1 + 1, x2 - 2, y1 + 2, CA(255, 255, 255, 0x20));                     /* brilho */
    /* cantoneiras douradas */
    rect(x1 - 3, y1 - 3, x1 - 3 + L, y1 - 1, gold); rect(x1 - 3, y1 - 3, x1 - 1, y1 - 3 + L, gold);
    rect(x2 + 3 - L, y1 - 3, x2 + 3, y1 - 1, gold); rect(x2 + 1, y1 - 3, x2 + 3, y1 - 3 + L, gold);
    rect(x1 - 3, y2 + 1, x1 - 3 + L, y2 + 3, gold); rect(x1 - 3, y2 + 3 - L, x1 - 1, y2 + 3, gold);
    rect(x2 + 3 - L, y2 + 1, x2 + 3, y2 + 3, gold); rect(x2 + 1, y2 + 3 - L, x2 + 3, y2 + 3, gold);
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
    int *ws = mode == 0 ? winsBot : winsVs;
    const char *hint = "X: REVANCHE   START: MENU";

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
    if (mode == 0) strcpy(buf, youLost ? "O BOT VENCEU... TENTE DE NOVO!" : "VOCE VENCEU O BOT!");
    else sprintf(buf, "PARABENS, %s!", pname(w));
    textC(62, 0.4f, youLost ? COL(255,190,180) : COL(255,245,190), buf);
    if (overNote[0]) textC(82, 0.32f, COL(255,150,130), overNote);

    /* sombras no chao */
    ell(wx, base + 108 * sc, 36 * sc * (1.0f - jump * 0.012f), 6 * sc, CA(0, 0, 0, 0x38));
    ell(lx, base + 108 * sc, 36 * sc, 6 * sc, CA(0, 0, 0, 0x38));

    /* quem perdeu: cabisbaixo, chorando, com nuvem de chuva */
    drawChar(charSel[l], lx, base + 8.0f + up * 0.6f + sinf(tt * 0.35f) * 1.5f, sc, 2, overT, 1);
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
    sprintf(buf, "%s %d X %d %s", pname(0), ws[0], ws[1], pname(1));
    textC(380, 0.5f, COL(255,255,255), buf);
    textC(410, 0.36f, ((overT / 25) & 1) ? COL(255,255,255) : COL(200,205,215), hint);

    if (tt < 36.0f) rect(0, 0, 640, 448, CA(255, 255, 255, (int)(0x50 * (1.0f - ent))));   /* flash de entrada */
}

/* ---------- configuracao da partida e nome do jogador 2 ---------- */
enum { SR_OPP, SR_LVL, SR_TIME, SR_MODE, SR_TABLE, SR_BALLS, SR_C0, SR_C1, SR_CH0, SR_CH1, SR_WB, SR_GD, SR_GO, NSR };
static int oppSel, setSel;                 /* adversario: 0 = BOT, 1 = outro jogador */
static char nameBuf[12];
static int nameLen, nameCur, nameWho;      /* nameWho: 0 = jogador 1, 1 = jogador 2 */
static const char KBCH[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static int setRows(int *v)                 /* linhas visiveis na tela de configuracao */
{
    int n = 0;
    v[n++] = SR_OPP;
    if (oppSel == 0) v[n++] = SR_LVL;
    else             v[n++] = SR_TIME;       /* tempo por jogada so contra outro jogador */
    v[n++] = SR_MODE;
    v[n++] = SR_TABLE;
    if (gmode == GM_BR) { v[n++] = SR_C0; v[n++] = SR_C1; }
    else                { v[n++] = SR_BALLS; }
    v[n++] = SR_CH0; v[n++] = SR_CH1;                 /* personagens (caricaturas) */
    v[n++] = SR_WB;  v[n++] = SR_GD;                  /* cor da bola branca e da linha guia */
    v[n++] = SR_GO;
    return n;
}

static void cycleColor(int pl, int d)      /* cada jogador usa uma cor diferente */
{
    int o = colSel[1 - pl];
    do { colSel[pl] = (colSel[pl] + d + NBRC) % NBRC; } while (colSel[pl] == o);
}

static void cycleChar(int pl, int d)       /* cada jogador usa um personagem diferente */
{
    int o = charSel[1 - pl];
    do { charSel[pl] = (charSel[pl] + d + NCHARS) % NCHARS; } while (charSel[pl] == o);
}

static void startMatch(void)
{
    rs += 7919 + menuT;
    wbSel[1] = wbSel[0];                         /* a cor da bola branca vale para a partida toda */
    setSkins(gmode == GM_BR);
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
    int v[NSR], n = setRows(v), d = 0, row;
    if (pr & PAD_UP)    setSel = (setSel + n - 1) % n;
    if (pr & PAD_DOWN)  setSel = (setSel + 1) % n;
    if (pr & PAD_LEFT)  d = -1;
    if (pr & PAD_RIGHT) d = 1;
    if (pr & PAD_CROSS) d = 1;
    if (pr & PAD_CIRCLE) { state = S_MENU; return; }
    row = v[setSel];
    if ((pr & PAD_START) || ((pr & PAD_CROSS) && row == SR_GO)) {
        nameOpen(0);                                             /* abre a tela para digitar o nome */
        return;
    }
    if (d == 0) return;
    switch (row) {
    case SR_OPP:   oppSel = 1 - oppSel; break;
    case SR_LVL:   botLevel = (botLevel + d + NLEVELS) % NLEVELS; break;
    case SR_TIME:  timeSel = (timeSel + d + NTIMES) % NTIMES; break;
    case SR_MODE:  gmode = (gmode + d + NGM) % NGM; break;
    case SR_TABLE: tableSel = (tableSel + d + NTABLES) % NTABLES; break;
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

static void drawSetupScreen(void)
{
    static const char *LBL[NSR] = { "ADVERSARIO", "DIFICULDADE", "TEMPO POR JOGADA", "MODO", "MESA",
                                    "COR DAS BOLAS", "COR JOGADOR 1", "COR JOGADOR 2", "PERSONAGEM", "PERSONAGEM", "BOLA BRANCA", "LINHA GUIA", "INICIAR PARTIDA" };
    static const char *GMN[NGM] = { "BOLA 8", "SINUCA BRASILEIRA" };
    static const u8 LC[NLEVELS][3] = { {90,230,110}, {245,215,70}, {240,80,70}, {200,110,255} };
    int v[NSR], n = setRows(v), i;
    float rh = (n >= 10) ? 21.0f : ((n >= 9) ? 24.0f : ((n >= 8) ? 27.0f : 31.0f));
    float y2 = 90.0f + n * rh + 6.0f;
    const char *hint;
    char buf[40];

    textC(24, 0.7f, COL(255,215,70), "CONFIGURAR PARTIDA");
    rect(120, 62, 520, 64, COL(50,90,70));
    panel(60, 76, 580, y2);

    for (i = 0; i < n; i++) {
        int row = v[i], sel = (i == setSel);
        float y = 90.0f + i * rh, bob = 4.0f * sinf(menuT * 0.2f), vx;
        u64 tc = sel ? COL(255,240,150) : COL(175,185,195), vc = COL(255,255,255);

        if (sel) {
            rect(68, y - 6, 572, y + 21,
                 GS_SETREG_RGBAQ(255, 220, 60, 0x1c + (int)(0x12 * (1.0f + sinf(menuT * 0.12f))), 0x00));
            tri(74 + bob, y, 74 + bob, y + 14, 85 + bob, y + 7, COL(255,230,60));
        }
        if (row == SR_GO) {
            text(98, y, 0.5f, sel ? COL(120,255,150) : COL(90,200,120), LBL[row]);
            if (oppSel == 1)
                text(556 - tw(0.34f, "PROXIMO: NOME"), y + 3, 0.34f, COL(150,160,175), "PROXIMO: NOME");
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
        case SR_TABLE: sprintf(buf, "< %s >", TABLES[tableSel].name); break;
        case SR_BALLS: sprintf(buf, "< %s >", THEME_NAME[ballTheme]); break;
        case SR_C0:    sprintf(buf, "< %s >", BRC_NAME[colSel[0]]); break;
        case SR_CH0:   sprintf(buf, "< %s >", CHAR_NAME[charSel[0]]); break;
        case SR_CH1:   sprintf(buf, "< %s >", CHAR_NAME[charSel[1]]); break;
        case SR_WB:    sprintf(buf, "< %s >", WB_NAME[wbSel[0]]); break;
        case SR_GD:    sprintf(buf, "< %s >", GD_NAME[gdSel[0]]); break;
        default:       sprintf(buf, "< %s >", BRC_NAME[colSel[1]]); break;
        }
        vx = 556 - tw(0.5f, buf);
        text(vx, y, 0.5f, vc, buf);
        if (row == SR_WB) drawSwatch(0, wbSel[0], vx - 18, y + 7);
        else if (row == SR_GD) drawSwatch(1, gdSel[0], vx - 24, y + 7);
        if (row == SR_TABLE) {                          /* mini mesa */
            const TableDef *T = &TABLES[tableSel];
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
                              : "BOLA 8: LISAS X LISTRADAS - A BOLA 8 FICA PARA O FINAL";
    else if (v[setSel] == SR_TIME)
        hint = "PASSOU O TEMPO DA JOGADA: PERDE A VEZ";
    else if (v[setSel] == SR_LVL && botLevel == 3)
        hint = "PROFISSIONAL: MIRA QUASE PERFEITA, RESPONDE NA HORA E NAO PERDOA ERRO";
    else if (v[setSel] == SR_WB || v[setSel] == SR_GD)
        hint = "DURANTE O JOGO, START MUDA A COR (CADA JOGADOR A SUA)";
    else if (v[setSel] == SR_BALLS)
        hint = "MUDA AS CORES DAS BOLAS LISAS E LISTRADAS (BOLA 8)";
    else
        hint = "CIMA/BAIXO: ESCOLHER   ESQ/DIR: MUDAR   X: OK   O: VOLTAR";
    textC(y2 + 10, 0.3f, COL(170,180,195), hint);
    drawTablePreview(320.0f, 392.0f, 176.0f, &TABLES[tableSel]);
    for (i = 0; i < 2; i++) {                         /* os dois personagens escolhidos (o da linha selecionada comemora) */
        float px = i == 0 ? 112.0f : 528.0f;
        int hot = (v[setSel] == (i == 0 ? SR_CH0 : SR_CH1));
        float bob = hot ? -3.0f * fabsf(sinf(menuT * 0.15f)) : 0.0f;
        const char *nm = (i == 1 && oppSel == 0) ? "BOT" : pnames[i];
        drawChar(charSel[i], px, 388.0f + bob, 0.9f, hot ? 1 : 0, menuT, 0);
        text(px - tw(0.34f, CHAR_NAME[charSel[i]]) * 0.5f, 418, 0.34f, hot ? COL(255,240,150) : COL(175,185,195), CHAR_NAME[charSel[i]]);
        text(px - tw(0.28f, nm) * 0.5f, 432, 0.28f, COL(130,140,155), nm);
    }
}

static void drawNameScreen(void)
{
    const float x0 = 82.0f, y0 = 118.0f, cw = 44.0f, ch = 34.0f, gap = 4.0f;
    char one[2] = { 0, 0 }, ttl[32], dflt[16];
    int i;

    sprintf(ttl, "NOME DO JOGADOR %d", nameWho + 1);
    sprintf(dflt, "JOGADOR %d", nameWho + 1);
    textC(24, 0.7f, COL(255,215,70), ttl);
    rect(120, 62, 520, 64, COL(50,90,70));

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
    textC(y0 + 4 * (ch + gap) + 12, 0.34f, COL(170,180,195), "X: ESCOLHER   O: APAGAR   START: CONFIRMAR");
    textC(y0 + 4 * (ch + gap) + 32, 0.34f, COL(150,160,175), "SELECT: VOLTAR");
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
    int k, pul = 0x1c + (int)(0x12 * (1.0f + sinf(menuT * 0.12f)));
    for (k = 0; k < 10; k++)                                  /* brilho que some para a direita */
        rect(x1 + (x2 - x1) * k / 10.0f, y - 6, x1 + (x2 - x1) * (k + 1) / 10.0f, y + h,
             CA(255, 215, 60, pul + 12 - k * 4));
    rect(x1, y - 6, x2, y - 5, CA(255, 240, 150, 0x30));
    rect(x1, y + h - 1, x2, y + h, CA(255, 200, 60, 0x20));
    rect(x1, y - 6, x1 + 3, y + h, COL(255,230,60));
    tri(x1 + 10 + bob, y + 1, x1 + 10 + bob, y + 15, x1 + 23 + bob, y + 8, COL(255,230,60));
}

static void drawOptionsScreen(void)
{
    static const char *LB[NOP] = { "SOM", "MUSICA", "VOLUME DA MUSICA", "EFEITOS DA MESA", "DICAS NA TELA", "CREDITOS" };
    char buf[32];
    int i;
    u64 pc = COL(255, 205 + (int)(40 * sinf(menuT * 0.05f)), 60);
    panel(100, 100, 540, 372);
    textC(112, 0.8f, pc, "OPCOES");
    rect(180, 150, 460, 152, COL(60,110,90));
    rect(180 + fmodf(menuT * 2.5f, 240.0f), 149, 220 + fmodf(menuT * 2.5f, 240.0f), 153, COL(240,240,200));
    for (i = 0; i < NOP; i++) {
        float y = 168.0f + i * 31.0f;
        int sel = (i == optSel);
        u64 tc = sel ? COL(255,240,150) : COL(175,185,195);
        if (i == OP_CRED) rect(124, y - 9, 516, y - 8, COL(60,90,80));      /* separa os creditos */
        if (sel) selBar(108, 532, y, 22);
        text(150, y, 0.5f, tc, LB[i]);
        switch (i) {
        case OP_SND:  strcpy(buf, sndOn ? "< LIGADO >" : "< DESLIGADO >"); break;
        case OP_MUS:  strcpy(buf, musOn ? "< LIGADA >" : "< DESLIGADA >"); break;
        case OP_VOL:  sprintf(buf, "< %d%% >", volLvl * 10); break;
        case OP_FX:   strcpy(buf, fxOn ? "< LIGADOS >" : "< DESLIGADOS >"); break;
        case OP_TIPS: strcpy(buf, tipsOn ? "< LIGADAS >" : "< DESLIGADAS >"); break;
        default:      strcpy(buf, ">"); break;
        }
        text(520 - tw(0.5f, buf), y, 0.5f, i == OP_CRED ? COL(255,230,60) : COL(255,255,255), buf);
    }
    if (optSel == OP_SND)
        textC(390, 0.3f, COL(255,190,120), "SONS DOS EFEITOS EM BREVE: A OPCAO JA FICA SALVA");
    else if (optSel == OP_MUS || optSel == OP_VOL)
        textC(390, 0.3f, COL(150,220,255), musicReady() ? musicStatus() : "AUDIO INDISPONIVEL NESTE APARELHO");
    textC(412, 0.3f, COL(170,180,195), "CIMA/BAIXO: ESCOLHER   ESQ/DIR: MUDAR   X: OK   O: VOLTAR");
}

static void drawMenuScreen(void)
{
    static const char *ITEM[4] = { "MODO HISTORIA", "JOGAR", "PLAYER DE MUSICAS", "OPCOES" };
    static const char *DESC[4] = { "A HISTORIA DO JOGO CHEGA EM BREVE",
                                   "BOLA 8 OU SINUCA BRASILEIRA, CONTRA O BOT OU UM AMIGO",
                                   "OUCA AS MUSICAS DO SEU PEN DRIVE",
                                   "SOM, MUSICA, VOLUME, EFEITOS E CREDITOS" };
    char buf[48];
    int i;
    float s1 = sinf(menuT * 0.05f), s2 = sinf(menuT * 0.05f + 2.1f);

    drawBackdrop();

    if (state == S_SETUP) { drawSetupScreen(); return; }
    if (state == S_NAME)  { drawNameScreen();  return; }
    if (state == S_OPTS)  { drawOptionsScreen(); return; }

    if (state == S_CREDITS) {
        u64 pc = COL(255, 200 + (int)(40 * s1), 50 + (int)(30 * s2));
        panel(150, 100, 490, 372);
        textC(114, 0.8f, pc, "CREDITOS");
        rect(200, 150, 440, 152, COL(60,110,90));
        textC(168, 0.42f, COL(190,200,225), "8 BALL REIS - PS2 EDITION");
        textC(214, 0.5f, COL(160,170,190), "CRIADOR");
        textC(236, 1.0f, pc, GAME_AUTHOR);
        rect(230, 286, 410, 288, COL(60,110,90));
        rect(230 + fmodf(menuT * 2.5f, 150.0f), 285, 270 + fmodf(menuT * 2.5f, 150.0f), 289, COL(240,240,200));
        textC(302, 0.5f, COL(160,170,190), "VERSAO");
        textC(324, 0.9f, COL(255,255,255), GAME_VERSION);
        textC(386, 0.4f, COL(190,200,215), "O OU X: VOLTAR");
        return;
    }

    /* titulo com brilho e leve movimento */
    {
        float ty = 34.0f + 3.0f * sinf(menuT * 0.06f), sc = 1.2f;
        const char *tt = "8 BALL REIS";
        float x0 = 320.0f - tw(sc, tt) * 0.5f;
        u64 glow = GS_SETREG_RGBAQ(140, 70, 10, 0x40, 0x00);
        u64 tc = COL(255, 200 + (int)(45 * s1), 40 + (int)(30 * s2));
        int e;
        text(x0 + 4, ty + 6, sc, GS_SETREG_RGBAQ(0, 0, 0, 0x60, 0x00), tt);          /* sombra */
        text(x0 - 3, ty - 3, sc, glow, tt); text(x0 + 3, ty - 3, sc, glow, tt);
        text(x0 - 3, ty + 3, sc, glow, tt); text(x0 + 3, ty + 4, sc, glow, tt);
        text(x0, ty, sc, tc, tt);
        text(x0, ty - 1.5f, sc, GS_SETREG_RGBAQ(255, 250, 205, 0x30, 0x00), tt);      /* brilho metalico */
        for (e = 0; e < 2; e++) {                                                    /* bolas 8 nos lados */
            float ex = e ? x0 + tw(sc, tt) + 30.0f : x0 - 30.0f, ey = ty + 16.0f;
            discN(ex + 3, ey + 5, 17.0f, GS_SETREG_RGBAQ(0, 0, 0, 0x30, 0x00), 18);
            discN(ex, ey, 17.0f, COL(12, 12, 18), 18);
            discN(ex - 5.0f, ey - 6.0f, 4.5f, GS_SETREG_RGBAQ(255, 255, 255, 0x40, 0x00), 8);
            discN(ex, ey, 8.5f, COL(245, 245, 245), 14);
            text(ex - tw(0.5f, "8") * 0.5f + 1.0f, ey - 8.0f, 0.5f, COL(10, 10, 14), "8");
        }
    }
    textC(80, 0.42f, COL(190,200,225), "PS2 EDITION");
    rect(180, 104, 460, 106, COL(50,90,70));
    rect(180 + fmodf(menuT * 3.0f, 240.0f), 103, 220 + fmodf(menuT * 3.0f, 240.0f), 107, COL(240,240,200));

    /* opcoes */
    panel(100, 122, 540, 300);
    for (i = 0; i < 4; i++) {
        float y = 138.0f + i * 40.0f;
        int sel = (i == menuSel);
        u64 tc = i == 0 ? (sel ? COL(170,175,190) : COL(105,112,128))
                        : (sel ? COL(255,240,150) : COL(170,180,190));
        if (i > 0) rect(124, y - 11, 516, y - 10, COL(40,70,62));          /* divisorias */
        if (sel) selBar(108, 532, y - 1, 26);
        text(150, y, 0.65f, tc, ITEM[i]);
        if (i == 0) text(520 - tw(0.36f, "EM BREVE"), y + 4, 0.36f, COL(150,130,70), "EM BREVE");
    }
    if (menuNote > 0) { menuNote--; textC(318, 0.38f, COL(255,230,60), "MODO HISTORIA EM BREVE!"); }
    else textC(318, 0.34f, COL(150,220,255), DESC[menuSel]);
    textC(344, 0.34f, COL(170,180,195), "CIMA/BAIXO: ESCOLHER   X: CONFIRMAR");

    rect(14, 416, 626, 417, COL(40,70,62));
    text(14, 424, 0.34f, COL(150,160,175), "CRIADOR: " GAME_AUTHOR);
    sprintf(buf, "VERSAO %s", GAME_VERSION);
    text(626.0f - tw(0.34f, buf), 424, 0.34f, COL(150,160,175), buf);
}

/* ---------- pausa (START) ---------- */
static int paused, pauseSel, pauseView;          /* pauseView: 0 = lista, 1 = controles e dicas */
enum { PI_RESUME, PI_SND, PI_MUS, PI_VOL, PI_FX, PI_TIPS, PI_WB0, PI_GD0, PI_WB1, PI_GD1, PI_PLAYER, PI_CTRL, PI_RESTART, PI_MENU, NPI };

/* linhas visiveis do menu de pausa: as cores do jogador 2 so aparecem no versus */
static int pauseRows(int *v)
{
    int n = 0, k;
    for (k = 0; k < NPI; k++)
        if (k == PI_WB0 || k == PI_WB1) continue;            /* cor da bola branca: so no menu Configurar partida */
        if (mode == 1 || k != PI_GD1) v[n++] = k;
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
    case PI_PLAYER:  if (pr & PAD_CROSS) pauseView = 2; break;
    case PI_CTRL:    if (pr & PAD_CROSS) pauseView = 1; break;
    case PI_RESTART: if (pr & PAD_CROSS) { paused = 0; rs += 7919 + menuT; newGame(mode); } break;
    case PI_MENU:    if (pr & PAD_CROSS) { paused = 0; menuInit(); state = S_MENU; } break;
    }
}

static void drawPause(void)
{
    static const char *LB[NPI] = { "CONTINUAR", "SOM (EFEITOS)", "MUSICA", "VOLUME DA MUSICA", "EFEITOS", "DICAS NA TELA", "", "", "", "",
                                   "PLAYER DE MUSICAS", "CONTROLES E DICAS", "REINICIAR PARTIDA", "MENU PRINCIPAL" };
    char buf[32];
    int i;
    rect(0, 0, 640, 448, GS_SETREG_RGBAQ(0, 0, 0, 0x58, 0x00));

    if (pauseView == 2) {                            /* player de musicas */
        int n = musicCount(), top, rows = 8;
        char b[96];
        hbox(60, 14, 580, 434, CA(255, 215, 60, 0x7c), CA(6, 10, 20, 0x88));
        textC(22, 0.7f, COL(255,215,70), "MUSICAS");
        rect(180, 56, 460, 58, CA(255,215,60,0x50));
        snprintf(b, sizeof b, "%s   VOLUME %d%%", musicStatus(), volLvl * 10);
        textC(66, 0.34f, COL(150,220,255), b);
        if (musicSource()[0]) { snprintf(b, sizeof b, "PASTA: %s", musicSource()); textC(82, 0.28f, COL(140,150,165), b); }
        if (n == 0) {
            textC(150, 0.4f, COL(255,235,170), "COLOQUE SUAS MUSICAS (MP3 OU WAV)");
            textC(176, 0.4f, COL(255,235,170), "NA PASTA MUSIC DO PEN DRIVE OU HD USB");
            textC(202, 0.4f, COL(255,235,170), "(OU AO LADO DO JOGO, EM UMA PASTA MUSIC)");
            textC(240, 0.34f, COL(190,200,215), "ESPERE UNS SEGUNDOS E APERTE TRIANGULO PARA PROCURAR");
        } else {
            int i;
            top = plSel - rows / 2; if (top > n - rows) top = n - rows; if (top < 0) top = 0;
            for (i = 0; i < rows && top + i < n; i++) {
                int k = top + i;
                float y = 100.0f + i * 30.0f;
                u64 tc = (k == plSel) ? COL(255,240,150) : COL(175,185,195);
                if (k == plSel) rect(76, y - 5, 564, y + 22, GS_SETREG_RGBAQ(255, 220, 60, 0x1c + (int)(0x12 * (1.0f + sinf(menuT * 0.12f))), 0x00));
                if (k == musicCur() && musicState() == 1) tri(82, y + 1, 82, y + 15, 93, y + 8, COL(90,255,130));
                else if (k == musicCur()) rect(82, y + 1, 90, y + 15, COL(255,200,70));
                snprintf(b, sizeof b, "%d. %s", k + 1, musicName(k));
                text(104, y, 0.45f, tc, b);
            }
            snprintf(b, sizeof b, "%d / %d", plSel + 1, n);
            text(564 - tw(0.3f, b), 346, 0.3f, COL(140,150,165), b);
        }
        textC(372, 0.3f, COL(170,180,195), "CIMA/BAIXO: ESCOLHER   X: TOCAR   QUADRADO: PAUSAR/CONTINUAR");
        textC(390, 0.3f, COL(170,180,195), "L1/R1: ANTERIOR/PROXIMA   ESQ/DIR: VOLUME   TRIANGULO: PROCURAR DE NOVO");
        textC(408, 0.3f, COL(170,180,195), "O / START: VOLTAR");
        return;
    }
    if (pauseView == 1) {
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
        hbox(40, 20, 600, 428, CA(255, 215, 60, 0x7c), CA(6, 10, 20, 0x84));
        textC(30, 0.7f, COL(255,215,70), "CONTROLES");
        rect(160, 62, 480, 64, CA(255,215,60,0x50));
        for (i = 0; i < 11; i++) {
            float y = 72.0f + i * 17.5f;
            if (i & 1) rect(52, y - 3, 588, y + 14, CA(255,255,255,0x0c));
            text(64, y, 0.38f, COL(255,230,110), K[i]);
            text(200, y, 0.38f, COL(225,230,240), V[i]);
        }
        textC(268, 0.55f, COL(255,215,70), "DICAS");
        rect(220, 294, 420, 295, CA(255,215,60,0x50));
        for (i = 0; i < 5; i++) text(64, 304.0f + i * 18.0f, 0.34f, COL(190,200,215), T[i]);
        textC(398, 0.34f, COL(150,160,175), "X / O / START: VOLTAR");
        return;
    }

    hbox(130, 20, 510, 428, CA(255, 215, 60, 0x7c), CA(6, 10, 20, 0x84));
    textC(30, 0.7f, COL(255,215,70), "PAUSADO");
    rect(180, 62, 460, 64, CA(255,215,60,0x50));
    {
        int pv[NPI], pn = pauseRows(pv), row;
        float sp = pn > 12 ? 22.0f : (pn > 10 ? 25.0f : 29.5f);
        if (pauseSel >= pn) pauseSel = 0;
        for (i = 0; i < pn; i++) {
            float y = 74.0f + i * sp, bob = 4.0f * sinf(menuT * 0.2f);
            int sel = (i == pauseSel), hasVal = 1;
            u64 tc = sel ? COL(255,240,150) : COL(175,185,195);
            char lb[24];
            row = pv[i];
            if (sel) {
                rect(138, y - 4, 502, y + 19, GS_SETREG_RGBAQ(255, 220, 60, 0x1c + (int)(0x12 * (1.0f + sinf(menuT * 0.12f))), 0x00));
                tri(144 + bob, y + 1, 144 + bob, y + 15, 155 + bob, y + 8, COL(255,230,60));
            }
            if (row == PI_WB0 || row == PI_WB1) sprintf(lb, mode == 0 ? "BOLA BRANCA" : "BRANCA J%d", row == PI_WB0 ? 1 : 2);
            else if (row == PI_GD0 || row == PI_GD1) sprintf(lb, mode == 0 ? "LINHA GUIA" : "GUIA J%d", row == PI_GD0 ? 1 : 2);
            else strcpy(lb, LB[row]);
            text(166, y, 0.5f, tc, lb);
            switch (row) {
            case PI_SND:  strcpy(buf, sndOn ? "< LIGADO >" : "< DESLIGADO >"); break;
            case PI_MUS:  strcpy(buf, musOn ? "< LIGADA >" : "< DESLIGADA >"); break;
            case PI_VOL:  sprintf(buf, "< %d%% >", volLvl * 10); break;
            case PI_FX:   strcpy(buf, fxOn ? "< LIGADOS >" : "< DESLIGADOS >"); break;
            case PI_TIPS: strcpy(buf, tipsOn ? "< LIGADAS >" : "< DESLIGADAS >"); break;
            case PI_WB0:  sprintf(buf, "< %s >", WB_NAME[wbSel[0]]); break;
            case PI_WB1:  sprintf(buf, "< %s >", WB_NAME[wbSel[1]]); break;
            case PI_GD0:  sprintf(buf, "< %s >", GD_NAME[gdSel[0]]); break;
            case PI_GD1:  sprintf(buf, "< %s >", GD_NAME[gdSel[1]]); break;
            default:      hasVal = 0; buf[0] = 0; break;
            }
            if (hasVal) text(494 - tw(0.5f, buf), y, 0.5f, COL(255,255,255), buf);
            if (hasVal && (row == PI_WB0 || row == PI_WB1)) drawSwatch(0, wbSel[row == PI_WB0 ? 0 : 1], 494 - tw(0.5f, buf) - 14, y + 7);
            if (hasVal && (row == PI_GD0 || row == PI_GD1)) drawSwatch(1, gdSel[row == PI_GD0 ? 0 : 1], 494 - tw(0.5f, buf) - 20, y + 7);
        }
        row = pv[pauseSel];
        if (row == PI_SND)
            textC(388, 0.3f, COL(255,190,120), "SONS DOS EFEITOS EM BREVE: A OPCAO JA FICA SALVA");
        else if (row == PI_MUS || row == PI_VOL || row == PI_PLAYER)
            textC(388, 0.3f, COL(150,220,255), musicReady() ? musicStatus() : "AUDIO INDISPONIVEL NESTE APARELHO");
        else if (row == PI_GD0 || row == PI_GD1) {
            int w = (row == PI_GD0) ? 0 : 1;
            char hb[64];
            if (mode == 0) strcpy(hb, "A COR DA LINHA MUDA NA HORA");
            else sprintf(hb, "COR DE %s: VALE QUANDO E A VEZ DELE", pnames[w]);
            textC(388, 0.3f, COL(150,220,255), hb);
        }
    }
    textC(404, 0.3f, COL(170,180,195), "CIMA/BAIXO: ESCOLHER   ESQ/DIR: MUDAR   X: OK   START/O: CONTINUAR");
}

/* cubo de giz esfregando na ponta do taco, com nuvenzinhas de po */
static void drawChalkFx(float bx, float by, float d0)
{
    float ux = cosf(ang), uy = sinf(ang), vx = -uy, vy = ux;
    float tx = bx - ux * d0, ty = by - uy * d0;
    float dist = 9.0f + 2.5f * sinf(chalkAnim * 0.9f), wob = 3.0f * sinf(chalkAnim * 0.9f + 1.0f);
    float cx = tx + vx * dist + ux * wob, cy = ty + vy * dist + uy * wob, h = 6.5f;
    int k;
    quad(cx - ux*h + 2 - vx*h, cy - uy*h + 2 - vy*h, cx + ux*h + 2 - vx*h, cy + uy*h + 2 - vy*h,
         cx + ux*h + 2 + vx*h, cy + uy*h + 2 + vy*h, cx - ux*h + 2 + vx*h, cy - uy*h + 2 + vy*h, COL(20,40,90));
    quad(cx - ux*h - vx*h, cy - uy*h - vy*h, cx + ux*h - vx*h, cy + uy*h - vy*h,
         cx + ux*h + vx*h, cy + uy*h + vy*h, cx - ux*h + vx*h, cy - uy*h + vy*h, COL(60,120,225));
    h = 4.0f;
    quad(cx - ux*h - vx*h, cy - uy*h - vy*h, cx + ux*h - vx*h, cy + uy*h - vy*h,
         cx + ux*h + vx*h, cy + uy*h + vy*h, cx - ux*h + vx*h, cy - uy*h + vy*h, COL(120,175,250));
    for (k = 0; k < 6; k++) {
        float age = fmodf(chalkAnim * 0.8f + k * 7.0f, 24.0f);
        disc(tx + sinf(k * 5.1f) * 6.0f, ty - age * 0.8f, 1.4f + age * 0.08f, CA(170, 210, 255, (int)(0x50 * (1.0f - age / 24.0f))));
    }
}

static void drawScene(void)
{
    int i;
    if (state == S_MENU || state == S_CREDITS || state == S_OPTS || state == S_SETUP || state == S_NAME) { drawMenuScreen(); return; }
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
    drawTable();
    if (state != S_PLACE) for (i = 0; i < 16; i++) drawShadow(i);
    else for (i = 1; i < 16; i++) drawShadow(i);
    for (i = 1; i < 16; i++) if (b[i].in) drawBallAt(i, b[i].x, b[i].y, R);
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
    cueChalk = isBot() ? 1.0f : chalk[cur];
    if (state == S_AIM) {
        float cd0 = R + 4 + pwr * 70;
        if (chalkAnim > 0) cd0 += 16.0f + 4.0f * sinf(chalkAnim * 0.7f);
        if (!isBot()) { drawGuide(); if (jumpArm) drawJumpMark(); }
        drawCue(b[0].x, b[0].y, cd0, 0x80, ang, curCue());
        if (chalkAnim > 0) drawChalkFx(b[0].x, b[0].y, cd0);
    } else if (state == S_STRIKE) {
        float t = (float)strikeK / STRIKEN, tip = R + 1.0f;
        drawCue(b[0].x, b[0].y, tip + (strikeD0 - tip) * (1.0f - t * t), 0x80, ang, curCue());
    } else if (state == S_ROLL && follow > 0) {
        float f = (float)(FOLLOWN - follow) / FOLLOWN;
        drawCue(hitX, hitY, R + 1.0f - 16.0f * f, (int)(0x80 * (1.0f - f)), ang, hitCue);
    }
    if (state == S_TAKE && takeN > 0) {
        float bob = 3.0f * sinf(menuT * 0.2f);
        float bx = b[takeList[takeSel]].x, by = b[takeList[takeSel]].y;
        ringN(bx, by, R + 3.5f, 1.8f, 24, CA(255, 230, 60, 0x7c));
        ringN(bx, by, R + 6.5f + bob * 0.4f, 1.2f, 24, CA(255, 230, 60, 0x40));
        tri(bx - 6, by - R - 18 - bob, bx + 6, by - R - 18 - bob, bx, by - R - 9 - bob, COL(255,230,60));
    }
    drawHUD();
    if (state == S_CUES) drawCueMenu();
    if (!paused && musicToast()) {
        char tb[80];
        snprintf(tb, sizeof tb, "MUSICA: %s", musicToast());
        textC(14, 0.32f, COL(190,220,255), tb);
    }
    if (paused) drawPause();
}

/* categoria da tela: 1 = menus, 2 = fim de jogo, 3 = partida */
static int stCat(int st)
{
    if (st == S_OVER) return 2;
    if (st == S_MENU || st == S_CREDITS || st == S_OPTS || st == S_SETUP || st == S_NAME || st == S_PLAYER) return 1;
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

/* ---------- main ---------- */
int main(int argc, char *argv[])
{
    u32 in, prev = 0, pressed, p0, p1;
    int i;

    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    padInit(0);
    padPortOpen(0, 0, padbuf0);
    padPortOpen(1, 0, padbuf1);
    musicInit(argc > 0 ? argv[0] : NULL);          /* player de musicas (USB / pasta do jogo) */
    musicSetVolume(volLvl);

    g = gsKit_init_global_custom(384 * 1024, 256 * 1024);
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
    OX = (g->Width  - 640) / 2.0f; if (OX < 0) OX = 0;
    OY = (g->Height - 448) / 2.0f; if (OY < 0) OY = 0;
    menuInit();
    splashScreen();
    menuInit();

    for (;;) {
        musicUpdate();
        p0 = readpad(0);
        p1 = (mode == 1 || state == S_MENU || state == S_OPTS || state == S_SETUP || state == S_NAME || state == S_PLAYER) ? readpad(1) : 0;
        if (state == S_MENU || state == S_OVER || state == S_CREDITS || state == S_OPTS || state == S_SETUP || state == S_NAME || state == S_PLAYER) in = p0 | p1;
        else if (mode == 1) in = p0 | p1;
        else in = (cur == 0) ? p0 : 0;
        if (paused) in = p0 | p1;
        else if (state == S_AIM || state == S_ROLL || state == S_PLACE || state == S_TAKE || state == S_STRIKE)
            in |= p0 & PAD_START;
        pressed = in & ~prev;
        prev = in;
        menuT++;
        if (state == S_OVER) overT++;
        if (state == S_MENU || state == S_CREDITS || state == S_OPTS || state == S_SETUP || state == S_NAME || state == S_PLAYER) menuUpdate();

        if (paused) pauseInput(pressed);
        else if ((pressed & PAD_START) && (state == S_AIM || state == S_ROLL || state == S_PLACE ||
                                           state == S_TAKE || state == S_STRIKE)) pauseOpen();
        else switch (state) {
        case S_MENU:
            if (pressed & PAD_UP)   menuSel = (menuSel + 3) % 4;
            if (pressed & PAD_DOWN) menuSel = (menuSel + 1) % 4;
            if (pressed & (PAD_CROSS | PAD_START)) {
                if (menuSel == 0) menuNote = 120;                       /* modo historia: em breve */
                else if (menuSel == 1) { state = S_SETUP; setSel = 0; }
                else if (menuSel == 2) { state = S_PLAYER; plSel = musicCur() > 0 ? musicCur() : 0; }
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

        case S_CREDITS:
            if (pressed & (PAD_CROSS | PAD_CIRCLE | PAD_START)) state = S_OPTS;
            break;

        case S_AIM:
            if (!isBot()) {
                if (chalkAnim > 0) {                                  /* passando giz: espera a animacao */
                    chalkAnim--;
                    if (chalkAnim == 22) { chalk[cur] = 1.0f; setmsg("GIZ PASSADO NO TACO!"); }
                    break;
                }
                if (in & PAD_LEFT)  ang -= 0.025f;
                if (in & PAD_RIGHT) ang += 0.025f;
                if (((in & PAD_L1) != 0) != ((in & PAD_R1) != 0)) {
                    float st;                                   /* ajuste fino: comeca bem devagar e so acelera um pouco segurando */
                    if (fineHold < 600) fineHold++;
                    st = 0.0007f + (fineHold > 20 ? (fineHold - 20) * 0.00002f : 0.0f);
                    if (st > 0.0028f) st = 0.0028f;
                    ang += (in & PAD_R1) ? st : -st;
                } else fineHold = 0;
                if (in & PAD_L2) { spinSel -= 0.03f; if (spinSel < -1.0f) spinSel = -1.0f; }
                if (in & PAD_R2) { spinSel += 0.03f; if (spinSel >  1.0f) spinSel =  1.0f; }
                if (in & PAD_UP)   { spinV += 0.03f; if (spinV >  1.0f) spinV =  1.0f; }   /* rola */
                if (in & PAD_DOWN) { spinV -= 0.03f; if (spinV < -1.0f) spinV = -1.0f; }   /* puxa */
                if (!charging) {
                    if (pressed & PAD_CROSS) { charging = 1; pwr = 0; barDir = 1.0f; }
                    else if (pressed & PAD_SELECT) openCueMenu();
                    else if (pressed & PAD_TRIANGLE) {
                        jumpArm = !jumpArm;
                        setmsg(jumpArm ? "PULO ATIVADO: A BRANCA VAI PULAR" : "PULO DESLIGADO");
                    }
                    else if (pressed & PAD_SQUARE) {
                        if (chalk[cur] < 0.97f) chalkAnim = 50;
                        else setmsg("O TACO JA ESTA COM GIZ");
                    }
                } else {
                    pwr += barDir * cueBarSpeed(cueSel[cur]);        /* sobe e desce sozinha */
                    if (pwr >= 1.0f) { pwr = 1.0f; barDir = -1.0f; }
                    if (pwr <= 0.0f) { pwr = 0.0f; barDir = 1.0f; }
                    if (pressed & PAD_CIRCLE) { charging = 0; pwr = 0; }
                    else if (pressed & PAD_CROSS) {
                        charging = 0;
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
            if (pressed & (PAD_LEFT | PAD_UP))    cueMenuSel = (cueMenuSel + NCUES - 1) % NCUES;
            if (pressed & (PAD_RIGHT | PAD_DOWN)) cueMenuSel = (cueMenuSel + 1) % NCUES;
            if (pressed & PAD_CROSS) {
                char tb[48];
                cueSel[cur] = cueMenuSel;
                sprintf(tb, "TACO: %s", CUES[cueMenuSel].name);
                setmsg(tb);
                state = cueRet;
            } else if (pressed & (PAD_CIRCLE | PAD_SELECT)) state = cueRet;
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
                if (!mv && !fl) resolve();
            }
            break;

        case S_PLACE:
            if (!isBot()) {
                if (in & PAD_LEFT)  b[0].x -= 2;
                if (in & PAD_RIGHT) b[0].x += 2;
                if (in & PAD_UP)    b[0].y -= 2;
                if (in & PAD_DOWN)  b[0].y += 2;
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
            if (pressed & PAD_CROSS) { rs += 7919 + menuT; newGame(mode); }
            else if (pressed & PAD_START) { menuInit(); state = S_MENU; }
            break;
        }

        /* tempo por jogada (so no versus): passou o tempo, perde a vez */
        {
            int inTurn = (state == S_AIM || state == S_PLACE || state == S_TAKE || state == S_CUES);
            if (!(timeSel && mode == 1)) { turnReset = 1; turnT = -1; }
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
                    charging = 0; pwr = 0; spinSel = 0; spinV = 0; jumpArm = 0; chalkAnim = 0;
                    cur = 1 - cur; state = S_AIM; botTimer = 0; botPlanned = 0;
                    turnCur = cur; turnT = TIME_SEC[timeSel] * 60;
                    setmsg(tb);
                }
            }
        }

        /* animacoes por frame: barra de forca, bolas na cacapa, taco */
        {
            float target = (state == S_STRIKE) ? strikePwr : pwr;
            if (state == S_MENU || state == S_OVER) target = 0;
            if (barLevel < target) barLevel = target;
            else { barLevel -= 0.04f; if (barLevel < target) barLevel = target; }
            for (i = 0; i < 16; i++) if (fallT[i] > 0) fallT[i]--;
            if (follow > 0) follow--;
        }

        gsKit_clear(g, GS_SETREG_RGBAQ(0x10, 0x10, 0x18, 0x80, 0x00));
        gsKit_prim_sprite(g, 0, 0, (float)g->Width, (float)g->Height, 1, COL(16, 16, 24));
        draw();
        gsKit_queue_exec(g);
        gsKit_sync_flip(g);
    }
    return 0;
}
