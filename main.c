/* 8 BALL POOL para PS2 - PS2SDK + gsKit + libpad
 * Modos: 1 jogador (vs BOT, 4 niveis) e 2 jogadores (dois controles).
 * Criador: Diego Reis | Versao 1.0.0
 * Controles na mira:  Esq/Dir = girar | L1/R1 = ajuste fino
 *                     X = a barra de forca (esquerda) sobe e desce sozinha,
 *                         X de novo = tacada | O = cancelar a tacada
 *                     L2/R2 = efeito na bola branca | SELECT = trocar de taco
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

enum { S_MENU, S_AIM, S_ROLL, S_PLACE, S_OVER, S_STRIKE, S_CUES, S_CREDITS, S_SETUP, S_NAME, S_TAKE };
enum { GM_8BALL, GM_BR, NGM };            /* modos de sinuca */

typedef struct { float x, y, vx, vy; int in; } Ball;

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

static const float PX[6] = { TX0, TCX, TX1, TX0, TCX, TX1 };
static const float PY[6] = { TY0, TY0, TY0, TY1, TY1, TY1 };
static const float PR[6] = { 14.6f, 12.0f, 14.6f, 14.6f, 12.0f, 14.6f };   /* raio das cacapas */
#define POCK_PULL 450.0f                          /* "funil": puxao suave da bola que entra na boca da cacapa */

static const u8 BC[15][3] = {
    {255,255,255}, {240,200,20}, {30,60,200}, {210,30,30}, {120,40,150},
    {240,120,20},  {20,140,60},  {130,20,30}, {15,15,15},
    {255,110,185}, {0,200,225},  {125,72,30}, {150,155,165}, {165,230,40}, {215,175,70}
};

/* cores escolhaveis no modo brasileiro (indice em BC) */
#define NBRC 12
static const int   BRC[NBRC]      = { 2, 3, 1, 6, 4, 5, 9, 10, 11, 12, 13, 14 };
static const char *BRC_NAME[NBRC] = { "AZUL", "VERMELHA", "AMARELA", "VERDE", "ROXA", "LARANJA",
                                      "ROSA", "CIANO", "MARROM", "CINZA", "LIMA", "DOURADA" };

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
} CueDef;
#define NCUES 6
#define CUE_SNIPER 5
static const CueDef CUES[NCUES] = {
    { "CLASSICO",   55,  30,  60,  60, { 58,104,172}, {228,192,130}, { 94, 48, 22}, {208,168, 72}, { 30, 24, 24} },
    { "POTENCIA",  100,  20,  45,  35, {190, 60, 50}, {222,170,110}, {150, 26, 26}, {210,210,215}, { 25, 20, 20} },
    { "EFEITO",     50, 100,  50,  55, { 70,150,230}, {226,196,140}, { 28, 64,160}, {130,210,255}, { 20, 26, 50} },
    { "MIRA LONGA", 50,  30, 100,  65, { 60,170,100}, {238,218,166}, { 20,112, 62}, {236,230,120}, { 20, 40, 28} },
    { "PRECISAO",   45,  40,  70, 100, {200,200,210}, {176,178,186}, { 44, 44, 50}, {205,205,215}, { 20, 20, 24} },
    /* SNIPER: laser vermelho que mostra a tacada inteira (ate 3 rebotes e o caminho completo da bola alvo);
       em troca tem pouca forca e quase nenhum efeito */
    { "SNIPER",     40,  10, 100, 100, {235, 60, 55}, { 96,104, 92}, { 38, 44, 36}, {255, 70, 60}, { 18, 22, 18} },
};

#define GAME_VERSION "1.3.0"
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
    if (!b[i].in) return;
    discN(b[i].x + 3.6f, b[i].y + 4.2f, R * 1.10f, GS_SETREG_RGBAQ(0, 18, 8, 0x14, 0x00), 12);
    discN(b[i].x + 2.4f, b[i].y + 3.0f, R * 0.96f, GS_SETREG_RGBAQ(0, 14, 6, 0x24, 0x00), 12);
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
    const u8 *c = (i == 0 || skinStripe[i]) ? WHITE : BC[id];
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
    int i, k, key = br ? 1 + colSel[0] * NBRC + colSel[1] : 0;
    if (key == skinKey) return;
    skinKey = key;
    skinDefaults();
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
            colr[a][c] = GS_SETREG_RGBAQ(cv, cv, cv, 0x80, 0x00);
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

static void rack(void)
{
    static const int order[15] = {1,9,2,10,8,3,11,4,12,5,13,6,14,7,15};
    int i, r, c, k = 0;
    memset(fallT, 0, sizeof(fallT));
    for (i = 0; i < 16; i++) { b[i].vx = b[i].vy = 0; b[i].in = 1; resetOrient(i); }
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
    for (i = 0; i < 16; i++) { b[i].vx = b[i].vy = 0; b[i].in = 0; resetOrient(i); }
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
    sprintf(buf, gmode == GM_BR ? "%s COMECA" : "%s - QUEBRA", pname(0));
    setmsg(buf);
}

static void shoot(float p)
{
    memset(pocketed, 0, sizeof(pocketed));
    firstHit = -1;
    gAtStart = grp[cur];
    startOwnLeft = gAtStart >= 0 ? countGroup(gAtStart) : 99;
    hitX = b[0].x; hitY = b[0].y; hitCue = curCue();
    b[0].vx = cosf(ang) * p * MAXV * cuePowMul(hitCue);
    b[0].vy = sinf(ang) * p * MAXV * cuePowMul(hitCue);
    spinCur = spinSel; spinK = cueSpinK(hitCue); spinSel = 0;
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

static int stepPhysics(float dt)
{
    int i, j, p, moving = 0;
    for (i = 0; i < 16; i++) {
        int inMouth = 0;
        if (!b[i].in) continue;
        if (i == 0 && spinCur != 0.0f) {          /* efeito: a trajetoria curva */
            float a = spinCur * spinK * dt, c = cosf(a), sn = sinf(a);
            float ovx = b[0].vx, ovy = b[0].vy;
            b[0].vx = ovx * c - ovy * sn; b[0].vy = ovx * sn + ovy * c;
            spinCur *= 1.0f - 1.1f * dt;
        }
        b[i].x += b[i].vx * dt; b[i].y += b[i].vy * dt;
        for (p = 0; p < 6; p++) {
            float dx = b[i].x - PX[p], dy = b[i].y - PY[p], d2 = dx*dx + dy*dy, mouth = PR[p] + R;
            if (d2 < PR[p]*PR[p]) {
                fallT[i] = FALLN; fallP[i] = p; fallX[i] = b[i].x; fallY[i] = b[i].y;
                b[i].in = 0; b[i].vx = b[i].vy = 0; pocketed[i] = 1; break;
            }
            if (d2 < mouth*mouth) {                       /* na boca da cacapa: a tabela abre, a bola pode entrar */
                float d = sqrtf(d2), toward = -(b[i].vx * dx + b[i].vy * dy);
                inMouth = 1;
                if (d > 0.001f && toward > 0.0f && POCK_PULL > 0.0f) {   /* so ajuda quem vai em direcao ao buraco */
                    b[i].vx -= dx / d * POCK_PULL * dt;
                    b[i].vy -= dy / d * POCK_PULL * dt;
                }
            }
        }
        if (!b[i].in) continue;
        if (inMouth) continue;                            /* sem rebote enquanto esta na boca */
        if (b[i].x < TX0+R) { b[i].x = TX0+R; if (b[i].vx < 0) { b[i].vx = -b[i].vx * cushionE(b[i].vx); b[i].vy *= 0.985f; } }
        if (b[i].x > TX1-R) { b[i].x = TX1-R; if (b[i].vx > 0) { b[i].vx = -b[i].vx * cushionE(b[i].vx); b[i].vy *= 0.985f; } }
        if (b[i].y < TY0+R) { b[i].y = TY0+R; if (b[i].vy < 0) { b[i].vy = -b[i].vy * cushionE(b[i].vy); b[i].vx *= 0.985f; } }
        if (b[i].y > TY1-R) { b[i].y = TY1-R; if (b[i].vy > 0) { b[i].vy = -b[i].vy * cushionE(b[i].vy); b[i].vx *= 0.985f; } }
    }
    for (i = 0; i < 16; i++) {
        if (!b[i].in) continue;
        for (j = i + 1; j < 16; j++) {
            float dx, dy, d2, d, nx, ny, rv, ov;
            if (!b[j].in) continue;
            dx = b[j].x - b[i].x; dy = b[j].y - b[i].y; d2 = dx*dx + dy*dy;
            if (d2 >= 4*R*R || d2 < 0.0001f) continue;
            d = sqrtf(d2); nx = dx / d; ny = dy / d;
            ov = (2*R - d) * 0.5f;
            b[i].x -= nx*ov; b[i].y -= ny*ov; b[j].x += nx*ov; b[j].y += ny*ov;
            rv = (b[i].vx - b[j].vx) * nx + (b[i].vy - b[j].vy) * ny;
            if (rv > 0) {
                rv *= 0.98f;                      /* choque quase elastico */
                b[i].vx -= rv*nx; b[i].vy -= rv*ny;
                b[j].vx += rv*nx; b[j].vy += rv*ny;
                if (i == 0 && firstHit < 0) firstHit = j;
            }
        }
    }
    for (i = 0; i < 16; i++) {
        float sp, s2;
        if (!b[i].in) continue;
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
        b[0].vx = b[0].vy = 0;
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
    winner = w;
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
   - suicidio (branca na cacapa): derrota na hora */
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
#define NTABLES 7
static const TableDef TABLES[NTABLES] = {
    { "CLASSICA", { 24,114, 68}, {124, 74, 33}, {236,226,190} },
    { "FENIX",    { 14,128, 52}, { 34, 34, 38}, {240,240,244} },
    { "OCEANO",   { 28, 92,170}, {118, 78, 40}, {236,226,190} },
    { "VINHO",    {130, 28, 44}, { 70, 38, 22}, {230,210,170} },
    { "ROYAL",    { 98, 52,150}, { 30, 30, 38}, {230,226,240} },
    { "PRETA OURO", { 26, 26, 34}, { 18, 16, 14}, {212,175, 80}, 1 },
    { "NEON",     {  8, 18, 46}, { 14, 14, 28}, {120,220,255}, 2 },
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
    }
}

/* ---------- previa da mesa (tela de configurar partida) ---------- */
static void pvBall(float x, float y, float r, const u8 *c)
{
    discN(x, y, r, shade(c, 0.55f, 0), 8);
    discN(x - r * 0.1f, y - r * 0.1f, r * 0.8f, shade(c, 1.0f, 0), 8);
    rect(x - r * 0.55f, y - r * 0.6f, x - r * 0.2f, y - r * 0.28f, COL(255,255,255));
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
    }

    /* bolas na posicao inicial do modo escolhido */
    if (gmode == GM_BR) {
        const u8 *c0 = BC[BRC[colSel[0]]], *c1 = BC[BRC[colSel[1]]];
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
                pvBall(x0 + (bx - TX0) * k, y0 + (by - TY0) * k, pr, BC[1 + (n++ % 8)]);
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

    /* sulco entre a madeira e a borracha */
    rect(TX0 - 12, TY0 - 12, TX1 + 12, TY1 + 12, wgr);
    tableDeco(0, T);

    /* feltro com leve brilho no centro (luz de cima) */
    rect(TX0, TY0, TX1, TY1, shade(T->felt, 1.0f, 0));
    for (k = 1; k < 8; k++)
        rect(TX0 + k*22, TY0 + k*14, TX1 - k*22, TY1 - k*14, shade(T->felt, 1.0f, k * 3));
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
        discN(PX[p], PY[p], PR[p] + 4, COL(52,31,14), 20);
        ring(PX[p], PY[p], PR[p] + 4, COL(176,132,58));
        discN(PX[p], PY[p], PR[p] + 0.5f, COL(3,3,5), 20);
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

    for (seg = 0; seg < maxSeg; seg++) {
        int hit, wall, al = seg == 0 ? 0x7c : (sn ? 0x64 - seg * 0x14 : 0x50);
        float t = castRay(cx, cy, dx, dy, -1, &hit, &wall);
        float gx = cx + dx*t, gy = cy + dy*t;
        float sx = cx + (seg == 0 ? dx*R : 0), sy = cy + (seg == 0 ? dy*R : 0);
        float ex = gx - dx*R, ey = gy - dy*R;

        if ((ex - sx)*dx + (ey - sy)*dy > 0.5f) glow(sx, sy, ex, ey, al, 255, lg, lg);
        discN(gx, gy, R, CA(255, lg, lg, 0x12), 20);
        ringN(gx, gy, R,        1.6f, 28, CA(255, lg, lg, al));
        ringN(gx, gy, R - 1.6f, 1.0f, 28, CA(255, lg, lg, al / 2));

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
                                b[hit].x + nx*L,  b[hit].y + ny*L, 0x7c, L >= tmax - 0.5f ? 0x50 : 0x10, 255, 255, 255);

            /* desvio da bola branca */
            tx = dx - dot*nx; ty = dy - dot*ny; tl = sqrtf(tx*tx + ty*ty);
            if (tl > 0.05f) {
                tx /= tl; ty /= tl;
                L = (12.0f + 40.0f * tl) * gs;
                if (sn) L = 1.0e6f;
                tmax = castRay(gx, gy, tx, ty, hit, &h2, &w2);
                if (L > tmax) L = tmax;
                if (L > R) glowFade(gx + tx*R, gy + ty*R, gx + tx*L, gy + ty*L, 0x60, 0x10, 190, 215, 255);
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
    const CueDef *cd = &CUES[ci];
    float ax, ay, bx, by, cx, cy, ex, ey;

    cueDx = cosf(a); cueDy = sinf(a);
    cueBx = px; cueBy = py; cueD0 = d0; cueAl = al;

    /* sombra no feltro */
    cuePt(0, -2.2f, &ax, &ay);   cuePt(290, -6.2f, &bx, &by);
    cuePt(290, 6.2f, &cx, &cy);  cuePt(0, 2.2f, &ex, &ey);
    quad(ax + 4, ay + 5, bx + 6, by + 7, cx + 6, cy + 7, ex + 4, ey + 5,
         GS_SETREG_RGBAQ(0, 12, 5, (al * 30) / 128, 0x00));

    cueSeg(0.0f,   3.5f,   cd->tip);
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

/* largura aproximada do texto da fonte (medida no emulador): ~24.5 px por letra em escala 1 */
#define FADV 24.5f
static float tw(float s, const char *t) { return strlen(t) * FADV * s; }

/* caixa do HUD: borda + fundo escuro translucido + brilho no topo */
static void hbox(float x1, float y1, float x2, float y2, u64 border, u64 fill)
{
    rect(x1 - 1.5f, y1 - 1.5f, x2 + 1.5f, y2 + 1.5f, border);
    rect(x1, y1, x2, y2, fill);
    rect(x1, y1, x2, y1 + (y2 - y1) * 0.42f, CA(255, 255, 255, 0x07));
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

/* indicador de efeito (L2/R2): o ponto vermelho mostra o lado do efeito */
static void drawSpinIndicator(void)
{
    const float cx = 30.0f, cy = 410.0f, r = 10.0f;
    discN(cx, cy, r + 1.5f, COL(60,60,66), 16);
    discN(cx, cy, r, COL(236,236,230), 16);
    line(cx, cy - r, cx, cy + r, COL(190,190,184));
    line(cx - r, cy, cx + r, cy, COL(190,190,184));
    discN(cx + spinSel * (r - 3.5f), cy, 2.8f, COL(215,40,40), 8);
    text(8, 424, 0.3f, COL(200,200,205), "EFEITO");
}

static float tipKey(float x, float y, const char *k, const char *l, u64 c1, u64 c2)
{ return tipsOn ? keycap(x, y, k, l, c1, c2) : 0.0f; }

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
            x += tipKey(x, 421, "X", "FORCA", COL(255,230,60), COL(235,235,240)) + 14;
            x += tipKey(x, 421, "L2/R2", "EFEITO", COL(255,230,60), COL(235,235,240)) + 14;
            tipKey(x, 421, "SELECT", "TACOS", COL(255,230,60), COL(235,235,240));
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
    for (k = 0; k < 14; k++)
        rect(0, k * 32.0f, 640, k * 32.0f + 32.0f, COL(6 + k/2, 10 + k, 34 - k));
    for (k = 0; k < 5; k++)
        discN(320.0f + 40.0f * sinf(t), 250.0f, 330.0f - k * 55.0f,
              GS_SETREG_RGBAQ(24, 120, 78, 0x0c, 0x00), 28);
    for (k = 0; k < 2; k++) {
        float x = fmodf(menuT * 1.6f + k * 420.0f, 1100.0f) - 260.0f;
        quad(x, 0, x + 50, 0, x - 70, 448, x - 120, 448, GS_SETREG_RGBAQ(255,255,255,0x0a,0x00));
    }
    for (k = 0; k < NMB; k++) {
        int id = MB_ID[k];
        discN(b[id].x + 5, b[id].y + 7, MBR * 1.05f, GS_SETREG_RGBAQ(0, 0, 0, 0x28, 0x00), 14);
        drawBallAt(id, b[id].x, b[id].y, MBR);
    }
    rect(0, 0, 640, 448, GS_SETREG_RGBAQ(0, 0, 10, 0x38, 0x00));   /* escurece p/ ler o menu */
}

static void panel(float x1, float y1, float x2, float y2)
{
    rect(x1 - 2, y1 - 2, x2 + 2, y2 + 2, GS_SETREG_RGBAQ(90, 150, 120, 0x50, 0x00));
    rect(x1, y1, x2, y2, GS_SETREG_RGBAQ(4, 10, 18, 0x60, 0x00));
}

/* ---------- configuracao da partida e nome do jogador 2 ---------- */
enum { SR_OPP, SR_LVL, SR_MODE, SR_TABLE, SR_C0, SR_C1, SR_GO, NSR };
static int oppSel, setSel;                 /* adversario: 0 = BOT, 1 = outro jogador */
static char nameBuf[12];
static int nameLen, nameCur;
static const char KBCH[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static int setRows(int *v)                 /* linhas visiveis na tela de configuracao */
{
    int n = 0;
    v[n++] = SR_OPP;
    if (oppSel == 0) v[n++] = SR_LVL;
    v[n++] = SR_MODE;
    v[n++] = SR_TABLE;
    if (gmode == GM_BR) { v[n++] = SR_C0; v[n++] = SR_C1; }
    v[n++] = SR_GO;
    return n;
}

static void cycleColor(int pl, int d)      /* cada jogador usa uma cor diferente */
{
    int o = colSel[1 - pl];
    do { colSel[pl] = (colSel[pl] + d + NBRC) % NBRC; } while (colSel[pl] == o);
}

static void startMatch(void)
{
    rs += 7919 + menuT;
    setSkins(gmode == GM_BR);
    newGame(oppSel);
}

static void confirmName(void)
{
    int n = nameLen;
    while (n > 0 && nameBuf[n - 1] == ' ') n--;
    nameBuf[n] = 0; nameLen = n;
    strcpy(pnames[1], n > 0 ? nameBuf : "JOGADOR 2");
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
        if (oppSel == 1) { state = S_NAME; nameCur = 0; }       /* abre a tela para digitar o nome */
        else startMatch();
        return;
    }
    if (d == 0) return;
    switch (row) {
    case SR_OPP:   oppSel = 1 - oppSel; break;
    case SR_LVL:   botLevel = (botLevel + d + NLEVELS) % NLEVELS; break;
    case SR_MODE:  gmode = (gmode + d + NGM) % NGM; break;
    case SR_TABLE: tableSel = (tableSel + d + NTABLES) % NTABLES; break;
    case SR_C0:    cycleColor(0, d); break;
    case SR_C1:    cycleColor(1, d); break;
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
    if (pr & PAD_SELECT) { state = S_SETUP; return; }
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
    static const char *LBL[NSR] = { "ADVERSARIO", "DIFICULDADE", "MODO", "MESA",
                                    "COR JOGADOR 1", "COR JOGADOR 2", "INICIAR PARTIDA" };
    static const char *GMN[NGM] = { "BOLA 8", "SINUCA BRASILEIRA" };
    static const u8 LC[NLEVELS][3] = { {90,230,110}, {245,215,70}, {240,80,70}, {200,110,255} };
    int v[NSR], n = setRows(v), i;
    float y2 = 90.0f + n * 31.0f + 6.0f;
    const char *hint;
    char buf[40];

    textC(24, 0.7f, COL(255,215,70), "CONFIGURAR PARTIDA");
    rect(120, 62, 520, 64, COL(50,90,70));
    panel(60, 76, 580, y2);

    for (i = 0; i < n; i++) {
        int row = v[i], sel = (i == setSel);
        float y = 90.0f + i * 31.0f, bob = 4.0f * sinf(menuT * 0.2f), vx;
        u64 tc = sel ? COL(255,240,150) : COL(175,185,195), vc = COL(255,255,255);

        if (sel) {
            rect(68, y - 7, 572, y + 22,
                 GS_SETREG_RGBAQ(255, 220, 60, 0x1c + (int)(0x12 * (1.0f + sinf(menuT * 0.12f))), 0x00));
            tri(74 + bob, y, 74 + bob, y + 14, 85 + bob, y + 7, COL(255,230,60));
        }
        if (row == SR_GO) {
            text(98, y, 0.5f, sel ? COL(120,255,150) : COL(90,200,120), LBL[row]);
            if (oppSel == 1)
                text(556 - tw(0.34f, "PROXIMO: NOME"), y + 3, 0.34f, COL(150,160,175), "PROXIMO: NOME");
            continue;
        }
        text(98, y, 0.5f, tc, LBL[row]);
        switch (row) {
        case SR_OPP:   strcpy(buf, oppSel ? "< JOGADOR 2 >" : "< BOT >"); break;
        case SR_LVL:   sprintf(buf, "< %s >", LVL_NAME[botLevel]);
                       vc = COL(LC[botLevel][0], LC[botLevel][1], LC[botLevel][2]); break;
        case SR_MODE:  sprintf(buf, "< %s >", GMN[gmode]); break;
        case SR_TABLE: sprintf(buf, "< %s >", TABLES[tableSel].name); break;
        case SR_C0:    sprintf(buf, "< %s >", BRC_NAME[colSel[0]]); break;
        default:       sprintf(buf, "< %s >", BRC_NAME[colSel[1]]); break;
        }
        vx = 556 - tw(0.5f, buf);
        text(vx, y, 0.5f, vc, buf);
        if (row == SR_TABLE) {                          /* mini mesa */
            const TableDef *T = &TABLES[tableSel];
            rect(vx - 46, y - 1, vx - 12, y + 16, shade(T->wood, 0.9f, 0));
            rect(vx - 43, y + 2, vx - 15, y + 13, shade(T->felt, 1.0f, 0));
        } else if (row == SR_C0 || row == SR_C1) {      /* bolinha da cor */
            const u8 *bc = BC[BRC[colSel[row == SR_C0 ? 0 : 1]]];
            discN(vx - 18, y + 7, 8.0f, shade(bc, 0.55f, 0), 12);
            discN(vx - 18, y + 7, 7.2f, shade(bc, 1.0f, 0), 12);
            discN(vx - 20.5f, y + 4.5f, 1.8f, COL(255,255,255), 6);
        }
    }

    if (v[setSel] == SR_MODE)
        hint = gmode == GM_BR ? "4 BOLAS CADA. ERROU A SUA: O OUTRO TIRA UMA DAS SUAS. BRANCA NA CACAPA: PERDE"
                              : "BOLA 8: LISAS X LISTRADAS - A BOLA 8 FICA PARA O FINAL";
    else if (v[setSel] == SR_LVL && botLevel == 3)
        hint = "PROFISSIONAL: MIRA QUASE PERFEITA, RESPONDE NA HORA E NAO PERDOA ERRO";
    else
        hint = "CIMA/BAIXO: ESCOLHER   ESQ/DIR: MUDAR   X: OK   O: VOLTAR";
    textC(y2 + 10, 0.3f, COL(170,180,195), hint);
    drawTablePreview(320.0f, 392.0f, 176.0f, &TABLES[tableSel]);
}

static void drawNameScreen(void)
{
    const float x0 = 82.0f, y0 = 118.0f, cw = 44.0f, ch = 34.0f, gap = 4.0f;
    char one[2] = { 0, 0 };
    int i;

    textC(24, 0.7f, COL(255,215,70), "NOME DO JOGADOR 2");
    rect(120, 62, 520, 64, COL(50,90,70));

    hbox(170, 72, 470, 106, CA(255, 215, 60, 0x7c), CA(6, 10, 20, 0x78));
    if (nameLen == 0) {
        textC(80, 0.5f, COL(110,118,132), "JOGADOR 2");
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

static void drawMenuScreen(void)
{
    static const char *ITEM[3] = { "MODO HISTORIA", "JOGAR", "CREDITOS" };
    char buf[48];
    int i;
    float s1 = sinf(menuT * 0.05f), s2 = sinf(menuT * 0.05f + 2.1f);

    drawBackdrop();

    if (state == S_SETUP) { drawSetupScreen(); return; }
    if (state == S_NAME)  { drawNameScreen();  return; }

    if (state == S_CREDITS) {
        u64 pc = COL(255, 200 + (int)(40 * s1), 50 + (int)(30 * s2));
        panel(150, 120, 490, 352);
        textC(130, 0.8f, pc, "CREDITOS");
        textC(190, 0.5f, COL(160,170,190), "CRIADOR");
        textC(212, 1.0f, pc, GAME_AUTHOR);
        rect(230, 262, 410, 264, COL(60,110,90));
        rect(230 + fmodf(menuT * 2.5f, 150.0f), 261, 270 + fmodf(menuT * 2.5f, 150.0f), 265, COL(240,240,200));
        textC(276, 0.5f, COL(160,170,190), "VERSAO");
        textC(298, 0.9f, COL(255,255,255), GAME_VERSION);
        textC(368, 0.45f, COL(190,200,215), "O OU X: VOLTAR");
        return;
    }

    /* titulo com brilho e leve movimento */
    {
        float ty = 34.0f + 3.0f * sinf(menuT * 0.06f), sc = 1.2f;
        const char *tt = "8 BALL POOL";
        float x0 = 320.0f - tw(sc, tt) * 0.5f;
        u64 glow = GS_SETREG_RGBAQ(140, 70, 10, 0x40, 0x00);
        u64 tc = COL(255, 200 + (int)(45 * s1), 40 + (int)(30 * s2));
        text(x0 - 3, ty - 3, sc, glow, tt); text(x0 + 3, ty - 3, sc, glow, tt);
        text(x0 - 3, ty + 3, sc, glow, tt); text(x0 + 3, ty + 4, sc, glow, tt);
        text(x0, ty, sc, tc, tt);
    }
    textC(80, 0.42f, COL(190,200,225), "PS2 EDITION");
    rect(180, 104, 460, 106, COL(50,90,70));
    rect(180 + fmodf(menuT * 3.0f, 240.0f), 103, 220 + fmodf(menuT * 3.0f, 240.0f), 107, COL(240,240,200));

    /* opcoes */
    panel(100, 126, 540, 294);
    for (i = 0; i < 3; i++) {
        float y = 144.0f + i * 50.0f, bob = 4.0f * sinf(menuT * 0.2f);
        int sel = (i == menuSel);
        u64 tc = i == 0 ? (sel ? COL(170,175,190) : COL(105,112,128))
                        : (sel ? COL(255,240,150) : COL(170,180,190));
        if (sel) {
            rect(108, y - 9, 532, y + 26,
                 GS_SETREG_RGBAQ(255, 220, 60, 0x1c + (int)(0x12 * (1.0f + sinf(menuT * 0.12f))), 0x00));
            tri(114 + bob, y, 114 + bob, y + 16, 127 + bob, y + 8, COL(255,230,60));
        }
        text(142, y, 0.65f, tc, ITEM[i]);
        if (i == 0) text(520 - tw(0.36f, "EM BREVE"), y + 4, 0.36f, COL(150,130,70), "EM BREVE");
    }
    if (menuNote > 0) { menuNote--; textC(308, 0.38f, COL(255,230,60), "MODO HISTORIA EM BREVE!"); }
    else textC(308, 0.36f, COL(170,180,195), "CIMA/BAIXO: ESCOLHER   X: CONFIRMAR");

    text(14, 424, 0.34f, COL(150,160,175), "CRIADOR: " GAME_AUTHOR);
    sprintf(buf, "VERSAO %s", GAME_VERSION);
    text(626.0f - tw(0.34f, buf), 424, 0.34f, COL(150,160,175), buf);
}

/* ---------- pausa (START) ---------- */
static int paused, pauseSel, pauseView;          /* pauseView: 0 = lista, 1 = controles e dicas */
enum { PI_RESUME, PI_SND, PI_MUS, PI_VOL, PI_FX, PI_TIPS, PI_PLAYER, PI_CTRL, PI_RESTART, PI_MENU, NPI };
static int plSel;                                /* cursor da lista de musicas */

static void pauseOpen(void) { paused = 1; pauseSel = 0; pauseView = 0; plSel = musicCur() > 0 ? musicCur() : 0; }

static void pauseInput(u32 pr)
{
    int d = ((pr & PAD_RIGHT) ? 1 : 0) - ((pr & PAD_LEFT) ? 1 : 0), tog = d || (pr & PAD_CROSS);
    if (pauseView == 1) {
        if (pr & (PAD_CROSS | PAD_CIRCLE | PAD_START)) pauseView = 0;
        return;
    }
    if (pauseView == 2) {                            /* player de musicas */
        int n = musicCount();
        if (pr & (PAD_CIRCLE | PAD_START)) { pauseView = 0; return; }
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
        return;
    }
    if (pr & PAD_UP)   pauseSel = (pauseSel + NPI - 1) % NPI;
    if (pr & PAD_DOWN) pauseSel = (pauseSel + 1) % NPI;
    if (pr & (PAD_START | PAD_CIRCLE)) { paused = 0; return; }
    switch (pauseSel) {
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
    static const char *LB[NPI] = { "CONTINUAR", "SOM (EFEITOS)", "MUSICA", "VOLUME DA MUSICA", "EFEITOS", "DICAS NA TELA",
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
        static const char *L[] = {
            "ESQ/DIR: GIRAR A MIRA        L1/R1: AJUSTE FINO",
            "X: BARRA DE FORCA, X DE NOVO DISPARA     O: CANCELAR",
            "L2/R2: EFEITO NA BOLA BRANCA     SELECT: TROCAR DE TACO",
            "BOLA NA MAO: SETAS MOVEM A BRANCA, X CONFIRMA",
            "START: PAUSAR O JOGO"
        };
        static const char *T[] = {
            "A BARRA SOBE E DESCE: APERTE X NO MOMENTO DE MAIS FORCA SO QUANDO PRECISAR",
            "TACO SNIPER: LASER MOSTRA A TACADA INTEIRA COM REBOTES, MAS BATE FRACO",
            "BRASILEIRA: ERROU A SUA BOLA? O ADVERSARIO TIRA UMA DAS PROPRIAS BOLAS",
            "BRANCA NA CACAPA NA BRASILEIRA = DERROTA NA HORA"
        };
        hbox(40, 20, 600, 428, CA(255, 215, 60, 0x7c), CA(6, 10, 20, 0x84));
        textC(30, 0.7f, COL(255,215,70), "CONTROLES");
        rect(160, 62, 480, 64, CA(255,215,60,0x50));
        for (i = 0; i < 5; i++) text(60, 78.0f + i * 26.0f, 0.4f, COL(225,230,240), L[i]);
        textC(222, 0.55f, COL(255,215,70), "DICAS");
        rect(220, 250, 420, 251, CA(255,215,60,0x50));
        for (i = 0; i < 4; i++) text(60, 262.0f + i * 26.0f, 0.36f, COL(190,200,215), T[i]);
        textC(398, 0.34f, COL(150,160,175), "X / O / START: VOLTAR");
        return;
    }

    hbox(130, 20, 510, 428, CA(255, 215, 60, 0x7c), CA(6, 10, 20, 0x84));
    textC(30, 0.7f, COL(255,215,70), "PAUSADO");
    rect(180, 62, 460, 64, CA(255,215,60,0x50));
    for (i = 0; i < NPI; i++) {
        float y = 74.0f + i * 29.5f, bob = 4.0f * sinf(menuT * 0.2f);
        int sel = (i == pauseSel), hasVal = 1;
        u64 tc = sel ? COL(255,240,150) : COL(175,185,195);
        if (sel) {
            rect(138, y - 5, 502, y + 22, GS_SETREG_RGBAQ(255, 220, 60, 0x1c + (int)(0x12 * (1.0f + sinf(menuT * 0.12f))), 0x00));
            tri(144 + bob, y + 1, 144 + bob, y + 15, 155 + bob, y + 8, COL(255,230,60));
        }
        text(166, y, 0.5f, tc, LB[i]);
        switch (i) {
        case PI_SND:  strcpy(buf, sndOn ? "< LIGADO >" : "< DESLIGADO >"); break;
        case PI_MUS:  strcpy(buf, musOn ? "< LIGADA >" : "< DESLIGADA >"); break;
        case PI_VOL:  sprintf(buf, "< %d%% >", volLvl * 10); break;
        case PI_FX:   strcpy(buf, fxOn ? "< LIGADOS >" : "< DESLIGADOS >"); break;
        case PI_TIPS: strcpy(buf, tipsOn ? "< LIGADAS >" : "< DESLIGADAS >"); break;
        default:      hasVal = 0; buf[0] = 0; break;
        }
        if (hasVal) text(494 - tw(0.5f, buf), y, 0.5f, COL(255,255,255), buf);
    }
    if (pauseSel == PI_SND)
        textC(376, 0.3f, COL(255,190,120), "SONS DOS EFEITOS EM BREVE: A OPCAO JA FICA SALVA");
    else if (pauseSel == PI_MUS || pauseSel == PI_VOL || pauseSel == PI_PLAYER)
        textC(376, 0.3f, COL(150,220,255), musicReady() ? musicStatus() : "AUDIO INDISPONIVEL NESTE APARELHO");
    textC(404, 0.3f, COL(170,180,195), "CIMA/BAIXO: ESCOLHER   ESQ/DIR: MUDAR   X: OK   START/O: CONTINUAR");
}

static void draw(void)
{
    int i;
    char buf[64];
    if (state == S_MENU || state == S_CREDITS || state == S_SETUP || state == S_NAME) { drawMenuScreen(); return; }
    drawTable();
    if (state != S_PLACE) for (i = 0; i < 16; i++) drawShadow(i);
    else for (i = 1; i < 16; i++) drawShadow(i);
    for (i = 1; i < 16; i++) if (b[i].in) drawBallAt(i, b[i].x, b[i].y, R);
    if (b[0].in) {
        if (state == S_PLACE) {
            if (validPlace(b[0].x, b[0].y)) drawBallAt(0, b[0].x, b[0].y, R);
            else disc(b[0].x, b[0].y, R, COL(230,60,60));
        } else drawBallAt(0, b[0].x, b[0].y, R);
    }
    for (i = 0; i < 16; i++) if (fallT[i] > 0) drawFall(i);
    if (state == S_AIM) {
        if (!isBot()) drawGuide();
        drawCue(b[0].x, b[0].y, R + 4 + pwr * 70, 0x80, ang, curCue());
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
    if (state == S_OVER) {
        int *w = mode == 0 ? winsBot : winsVs;
        const char *hint = "X: REVANCHE   START: MENU";
        rect(0, 0, 640, 448, GS_SETREG_RGBAQ(0, 0, 0, 0x40, 0x00));
        hbox(120, 150, 520, 290, CA(255, 215, 60, 0x7c), CA(6, 10, 20, 0x78));
        sprintf(buf, "%s VENCEU!", pname(winner));
        text(320 - tw(0.7f, buf) * 0.5f, 170, 0.7f, COL(255,230,60), buf);
        if (overNote[0]) text(320 - tw(0.32f, overNote) * 0.5f, 190, 0.32f, COL(255,150,130), overNote);
        rect(160, 204, 480, 206, CA(255, 215, 60, 0x50));
        sprintf(buf, "%s %d X %d %s", pname(0), w[0], w[1], pname(1));
        text(320 - tw(0.5f, buf) * 0.5f, 218, 0.5f, COL(255,255,255), buf);
        text(320 - tw(0.36f, hint) * 0.5f, 258, 0.36f, COL(200,205,215), hint);
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

    for (;;) {
        musicUpdate();
        p0 = readpad(0);
        p1 = (mode == 1 || state == S_MENU || state == S_SETUP || state == S_NAME) ? readpad(1) : 0;
        if (state == S_MENU || state == S_OVER || state == S_CREDITS || state == S_SETUP || state == S_NAME) in = p0 | p1;
        else if (mode == 1) in = p0 | p1;
        else in = (cur == 0) ? p0 : 0;
        if (paused) in = p0 | p1;
        else if (state == S_AIM || state == S_ROLL || state == S_PLACE || state == S_TAKE || state == S_STRIKE)
            in |= p0 & PAD_START;
        pressed = in & ~prev;
        prev = in;
        menuT++;
        if (state == S_MENU || state == S_CREDITS || state == S_SETUP || state == S_NAME) menuUpdate();

        if (paused) pauseInput(pressed);
        else if ((pressed & PAD_START) && (state == S_AIM || state == S_ROLL || state == S_PLACE ||
                                           state == S_TAKE || state == S_STRIKE)) pauseOpen();
        else switch (state) {
        case S_MENU:
            if (pressed & PAD_UP)   menuSel = (menuSel + 2) % 3;
            if (pressed & PAD_DOWN) menuSel = (menuSel + 1) % 3;
            if (pressed & (PAD_CROSS | PAD_START)) {
                if (menuSel == 0) menuNote = 120;                       /* modo historia: em breve */
                else if (menuSel == 1) { state = S_SETUP; setSel = 0; }
                else state = S_CREDITS;
            }
            break;

        case S_SETUP:
            setupInput(pressed);
            break;

        case S_NAME:
            nameInput(pressed);
            break;

        case S_CREDITS:
            if (pressed & (PAD_CROSS | PAD_CIRCLE | PAD_START)) state = S_MENU;
            break;

        case S_AIM:
            if (!isBot()) {
                if (in & PAD_LEFT)  ang -= 0.025f;
                if (in & PAD_RIGHT) ang += 0.025f;
                if (in & PAD_L1)    ang -= 0.005f;
                if (in & PAD_R1)    ang += 0.005f;
                if (in & PAD_L2) { spinSel -= 0.03f; if (spinSel < -1.0f) spinSel = -1.0f; }
                if (in & PAD_R2) { spinSel += 0.03f; if (spinSel >  1.0f) spinSel =  1.0f; }
                if (!charging) {
                    if (pressed & PAD_CROSS) { charging = 1; pwr = 0; barDir = 1.0f; }
                    else if (pressed & PAD_SELECT) openCueMenu();
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
                    if (b[i].in && (b[i].vx != 0 || b[i].vy != 0)) mv = 1;
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
