/* 8 BALL POOL para PS2 - PS2SDK + gsKit + libpad
 * Modos: 1 jogador (vs BOT, 3 niveis) e 2 jogadores (dois controles).
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

enum { S_MENU, S_AIM, S_ROLL, S_PLACE, S_OVER, S_STRIKE, S_CUES, S_CREDITS };

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
static const float PR[6] = { 14.5f, 13.0f, 14.5f, 14.5f, 13.0f, 14.5f };

static const u8 BC[9][3] = {
    {255,255,255}, {240,200,20}, {30,60,200}, {210,30,30}, {120,40,150},
    {240,120,20},  {20,140,60},  {130,20,30}, {15,15,15}
};

/* tacos: estatisticas de 0 a 100 + cores para o desenho */
typedef struct {
    const char *name;
    int force, effect, aim, ctl;          /* forca, efeito, linha de mira, controle da barra */
    u8 tip[3], wood[3], butt[3], inlay[3], wrap[3];
} CueDef;
#define NCUES 5
static const CueDef CUES[NCUES] = {
    { "CLASSICO",   55,  30,  60,  60, { 58,104,172}, {228,192,130}, { 94, 48, 22}, {208,168, 72}, { 30, 24, 24} },
    { "POTENCIA",  100,  20,  45,  35, {190, 60, 50}, {222,170,110}, {150, 26, 26}, {210,210,215}, { 25, 20, 20} },
    { "EFEITO",     50, 100,  50,  55, { 70,150,230}, {226,196,140}, { 28, 64,160}, {130,210,255}, { 20, 26, 50} },
    { "MIRA LONGA", 50,  30, 100,  65, { 60,170,100}, {238,218,166}, { 20,112, 62}, {236,230,120}, { 20, 40, 28} },
    { "PRECISAO",   45,  40,  70, 100, {200,200,210}, {176,178,186}, { 44, 44, 50}, {205,205,215}, { 20, 20, 24} },
};

#define GAME_VERSION "1.0.0"
#define GAME_AUTHOR  "DIEGO REIS"

/* dificuldade do BOT: facil / medio / dificil */
static int botLevel = 1, menuT;
static const char  *LVL_NAME[3]  = { "FACIL", "MEDIO", "DIFICIL" };
static const float  BOT_ERR[3]   = { 0.075f, 0.02f, 0.004f };   /* erro de mira (rad) */
static const float  BOT_POWV[3]  = { 0.20f, 0.06f, 0.0f };      /* erro de forca */
static const int    BOT_MISS[3]  = { 30, 0, 0 };                 /* % de jogadas ruins */
static const int    BOT_DELAY[3] = { 75, 40, 22 };               /* tempo para pensar */

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
    int id = (i > 8) ? i - 8 : i;
    const u8 *c = (i == 0 || i > 8) ? WHITE : BC[i];
    float r2 = r * 0.93f;

    discN(x, y, r, shade(c, 0.55f, 0), 12);          /* borda escura */
    discN(x, y, r2, shade(c, 1.0f, 0), 12);          /* cor base */

    if (i > 8) {                                      /* listra colorida */
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

    if (i > 0) drawNumber(i, x, y, r);

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
                int col = id > 8 ? id - 8 : id;
                cr = BC[col][0]; cg = BC[col][1]; cb = BC[col][2];
                if (id > 8 && fabsf(y) > 0.55f) { cr = 245; cg = 245; cb = 238; }
                {
                    float d[4]  = { z, x, -z, -x };
                    float gu[4] = { x, -z, -x, z };
                    float gv = -y;
                    for (k = 0; k < 4; k++)
                        if (d[k] > 0 && gu[k]*gu[k] + gv*gv < 0.2116f) {
                            cr = cg = cb = 248;
                            if (digitInk(id, gu[k] / 0.46f, gv / 0.46f)) cr = cg = cb = 20;
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

static void newGame(int m)
{
    mode = m; rack(); grp[0] = grp[1] = -1; cur = 0; ang = 0; pwr = 0;
    charging = 0; breakShot = 1; botTimer = 0; botPlanned = 0; state = S_AIM;
    spinSel = spinCur = 0; barDir = 1.0f;
    setmsg("JOGADOR 1 - QUEBRA");
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
        if (!b[i].in) continue;
        if (i == 0 && spinCur != 0.0f) {          /* efeito: a trajetoria curva */
            float a = spinCur * spinK * dt, c = cosf(a), sn = sinf(a);
            float ovx = b[0].vx, ovy = b[0].vy;
            b[0].vx = ovx * c - ovy * sn; b[0].vy = ovx * sn + ovy * c;
            spinCur *= 1.0f - 1.1f * dt;
        }
        b[i].x += b[i].vx * dt; b[i].y += b[i].vy * dt;
        for (p = 0; p < 6; p++) {
            float dx = b[i].x - PX[p], dy = b[i].y - PY[p];
            if (dx*dx + dy*dy < PR[p]*PR[p]) {
                fallT[i] = FALLN; fallP[i] = p; fallX[i] = b[i].x; fallY[i] = b[i].y;
                b[i].in = 0; b[i].vx = b[i].vy = 0; pocketed[i] = 1; break;
            }
        }
        if (!b[i].in) continue;
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

static void resolve(void)
{
    int i, foul, ownPocket = 0;
    char buf[64];

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

    botTimer = 0; botPlanned = 0;
    if (foul) {
        if (!b[0].in) { b[0].in = 1; fallT[0] = 0; b[0].x = HEADX; b[0].y = TCY; }
        b[0].vx = b[0].vy = 0;
        cur = 1 - cur;
        state = S_PLACE;
        sprintf(buf, "FALTA! BOLA NA MAO - JOGADOR %d", cur + 1);
        setmsg(buf);
    } else {
        if (!ownPocket) cur = 1 - cur;
        state = S_AIM;
        if (mode == 0 && cur == 1) setmsg("VEZ DO BOT");
        else { sprintf(buf, "VEZ DO JOGADOR %d", cur + 1); setmsg(buf); }
    }
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
            sc = al + dl + (1.0f - cc) * 400.0f;
            if (sc < best) {
                best = sc; found = 1;
                botAng = atan2f(gy - cy, gx - cx);
                botPow = 0.35f + (al + dl) / 600.0f;
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
static void cushH(float xa, float xb, float yo, float yi)
{
    quad(xa - 7, yo, xb + 7, yo, xb, yi, xa, yi, COL(12,84,46));
    line(xa, yi, xb, yi, COL(48,160,98));
    line(xa - 7, yo, xb + 7, yo, COL(8,56,30));
}

static void cushV(float ya, float yb, float xo, float xi)
{
    quad(xo, ya - 7, xo, yb + 7, xi, yb, xi, ya, COL(12,84,46));
    line(xi, ya, xi, yb, COL(48,160,98));
    line(xo, ya - 7, xo, yb + 7, COL(8,56,30));
}

static void diamond(float cx, float cy)
{
    quad(cx, cy - 3, cx + 3, cy, cx, cy + 3, cx - 3, cy, COL(236,226,190));
}

static void drawTable(void)
{
    int p, k;
    float x, y;

    /* sombra da mesa no chao */
    rect(TX0 - 22, TY0 - 20, TX1 + 40, TY1 + 42, GS_SETREG_RGBAQ(0, 0, 0, 0x30, 0x00));

    /* moldura de madeira em camadas (bisel) */
    rect(TX0 - 30, TY0 - 30, TX1 + 30, TY1 + 30, COL(44,24,9));
    rect(TX0 - 28, TY0 - 28, TX1 + 28, TY1 + 28, COL(98,56,25));
    rect(TX0 - 25, TY0 - 25, TX1 + 25, TY1 + 25, COL(124,74,33));
    line(TX0 - 28, TY0 - 28, TX1 + 28, TY0 - 28, COL(158,102,50));
    line(TX0 - 28, TY0 - 28, TX0 - 28, TY1 + 28, COL(158,102,50));
    line(TX0 - 28, TY1 + 28, TX1 + 28, TY1 + 28, COL(60,33,13));
    line(TX1 + 28, TY0 - 28, TX1 + 28, TY1 + 28, COL(60,33,13));

    /* veios da madeira */
    for (k = 0; k < 4; k++) {
        u64 gc = (k & 1) ? COL(112,65,28) : COL(136,84,38);
        line(TX0 - 24, TY0 - 23 + k*3.4f, TX1 + 24, TY0 - 23 + k*3.4f, gc);
        line(TX0 - 24, TY1 + 13 + k*3.4f, TX1 + 24, TY1 + 13 + k*3.4f, gc);
        line(TX0 - 23 + k*3.4f, TY0 - 24, TX0 - 23 + k*3.4f, TY1 + 24, gc);
        line(TX1 + 13 + k*3.4f, TY0 - 24, TX1 + 13 + k*3.4f, TY1 + 24, gc);
    }

    /* sulco entre a madeira e a borracha */
    rect(TX0 - 12, TY0 - 12, TX1 + 12, TY1 + 12, COL(52,29,12));

    /* feltro com leve brilho no centro (luz de cima) */
    rect(TX0, TY0, TX1, TY1, COL(24,114,68));
    for (k = 1; k < 8; k++)
        rect(TX0 + k*22, TY0 + k*14, TX1 - k*22, TY1 - k*14,
             COL(24 + k*2, 114 + k*3, 68 + k*2));

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
    line(HEADX, TY0, HEADX, TY1, COL(52,152,96));
    disc(HEADX, TCY, 2.2f, COL(16,88,52));
    disc(FOOTX, TCY, 2.2f, COL(16,88,52));
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
    beam(x1, y1, x2, y2, 6.0f, CA(r, gg, bb, a / 8));
    beam(x1, y1, x2, y2, 3.6f, CA(r, gg, bb, a / 4));
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
    int seg, maxSeg = CUES[curCue()].aim >= 100 ? 2 : 1;   /* so o taco MIRA LONGA mostra o rebote */

    for (seg = 0; seg < maxSeg; seg++) {
        int hit, wall, al = seg == 0 ? 0x7c : 0x50;
        float t = castRay(cx, cy, dx, dy, -1, &hit, &wall);
        float gx = cx + dx*t, gy = cy + dy*t;
        float sx = cx + (seg == 0 ? dx*R : 0), sy = cy + (seg == 0 ? dy*R : 0);
        float ex = gx - dx*R, ey = gy - dy*R;

        if ((ex - sx)*dx + (ey - sy)*dy > 0.5f) glow(sx, sy, ex, ey, al, 255, 255, 255);
        discN(gx, gy, R, CA(255, 255, 255, 0x12), 20);
        ringN(gx, gy, R,        1.6f, 28, CA(255, 255, 255, al));
        ringN(gx, gy, R - 1.6f, 1.0f, 28, CA(255, 255, 255, al / 2));

        if (hit > 0) {
            float nx = b[hit].x - gx, ny = b[hit].y - gy, nl = sqrtf(nx*nx + ny*ny);
            float dot, tx, ty, tl, L, tmax;
            int h2, w2, pk;
            if (nl < 0.01f) return;
            nx /= nl; ny /= nl;
            dot = dx*nx + dy*ny; if (dot < 0) dot = 0;

            /* caminho da bola alvo: ate a tabela/outra bola, destacando cacapa */
            L = (22.0f + 70.0f * dot) * gs;
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

/* painel de cada jogador (topo): nome, grupo e as bolas que ainda restam */
static void drawPlayerPanel(int pl)
{
    float x1 = pl == 0 ? 70.0f : 378.0f, x2 = x1 + 220.0f, y1 = 6.0f, y2 = 64.0f;
    int act = (pl == cur), k;
    const char *nm = (pl == 1 && mode == 0) ? "BOT" : (pl == 0 ? "JOGADOR 1" : "JOGADOR 2");
    const char *gn = grp[pl] < 0 ? "MESA ABERTA" : (grp[pl] == 0 ? "LISAS" : "LISTRADAS");

    hbox(x1, y1, x2, y2, act ? CA(255, 215, 60, 0x7c) : CA(70, 78, 96, 0x60),
         act ? CA(44, 38, 10, 0x74) : CA(8, 12, 22, 0x68));
    rect(x1, y1, x1 + 5, y2, act ? COL(255,215,60) : COL(70,78,96));
    text(x1 + 14, y1 + 5, 0.5f, act ? COL(255,235,110) : COL(165,170,180), nm);
    text(x1 + 14, y1 + 23, 0.32f, act ? COL(255,255,255) : COL(130,138,152), gn);

    if (grp[pl] < 0) return;
    for (k = 0; k < 7; k++) {
        int id = grp[pl] == 0 ? k + 1 : k + 9;
        float bx = x1 + 24 + k * 26.0f, by = y1 + 46.0f;
        if (b[id].in) drawBallAt(id, bx, by, 7.0f);
        else          ring(bx, by, 6.0f, COL(70,76,90));
    }
    if (countGroup(grp[pl]) == 0)                      /* so falta a bola 8 */
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
            x += keycap(x, 421, "X", "DISPARAR", COL(255,230,60), COL(235,235,240)) + 16;
            keycap(x, 421, "O", "CANCELAR", COL(255,230,60), COL(235,235,240));
        } else {
            x += keycap(x, 421, "X", "FORCA", COL(255,230,60), COL(235,235,240)) + 14;
            x += keycap(x, 421, "L2/R2", "EFEITO", COL(255,230,60), COL(235,235,240)) + 14;
            keycap(x, 421, "SELECT", "TACOS", COL(255,230,60), COL(235,235,240));
        }
    } else if (state == S_PLACE && !isBot()) {
        x += keycap(x, 421, "SETAS", "MOVER", COL(255,230,60), COL(235,235,240)) + 14;
        keycap(x, 421, "X", "CONFIRMAR", COL(255,230,60), COL(235,235,240));
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
    sprintf(buf, "TACOS - JOGADOR %d", cur + 1);
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
        float x = 320.0f + (k - 2) * 30.0f;
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

static void drawMenuScreen(void)
{
    static const char *ITEM[5] = { "MODO HISTORIA", "VERSUS BOT", "VERSUS PLAY", "DIFICULDADE", "CREDITOS" };
    char buf[48];
    int i;
    float s1 = sinf(menuT * 0.05f), s2 = sinf(menuT * 0.05f + 2.1f);

    drawBackdrop();

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

    /* opcoes: rotulo na esquerda, valor alinhado na direita */
    panel(100, 120, 540, 346);
    for (i = 0; i < 5; i++) {
        float y = 134.0f + i * 42.0f, bob = 4.0f * sinf(menuT * 0.2f);
        int sel = (i == menuSel);
        u64 tc = i == 0 ? (sel ? COL(170,175,190) : COL(105,112,128))
                        : (sel ? COL(255,240,150) : COL(170,180,190));
        if (sel) {
            rect(108, y - 9, 532, y + 22,
                 GS_SETREG_RGBAQ(255, 220, 60, 0x1c + (int)(0x12 * (1.0f + sinf(menuT * 0.12f))), 0x00));
            tri(114 + bob, y - 1, 114 + bob, y + 14, 126 + bob, y + 6, COL(255,230,60));
        }
        text(140, y, 0.55f, tc, ITEM[i]);
        if (i == 0) {
            text(520 - tw(0.36f, "EM BREVE"), y + 2, 0.36f, COL(150,130,70), "EM BREVE");
        } else if (i == 3) {
            static const u8 LC[3][3] = { {90,230,110}, {245,215,70}, {240,80,70} };
            u64 lc = COL(LC[botLevel][0], LC[botLevel][1], LC[botLevel][2]);
            sprintf(buf, "< %s >", LVL_NAME[botLevel]);
            text(520 - tw(0.55f, buf), y, 0.55f, lc, buf);
        } else if (i == 1 || i == 2) {
            int *w = i == 1 ? winsBot : winsVs;
            if (w[0] + w[1] > 0) {
                sprintf(buf, "PLACAR %d X %d", w[0], w[1]);
                text(520 - tw(0.36f, buf), y + 2, 0.36f, COL(150,160,175), buf);
            }
        }
    }
    if (menuNote > 0) { menuNote--; textC(358, 0.38f, COL(255,230,60), "MODO HISTORIA EM BREVE!"); }
    else textC(358, 0.36f, COL(170,180,195), menuSel == 3 ?
               "ESQ/DIR: MUDAR NIVEL DO BOT" : "CIMA/BAIXO: ESCOLHER   X: CONFIRMAR");

    text(14, 424, 0.34f, COL(150,160,175), "CRIADOR: " GAME_AUTHOR);
    sprintf(buf, "VERSAO %s", GAME_VERSION);
    text(626.0f - tw(0.34f, buf), 424, 0.34f, COL(150,160,175), buf);
}

static void draw(void)
{
    int i;
    char buf[64];
    if (state == S_MENU || state == S_CREDITS) { drawMenuScreen(); return; }
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
    drawHUD();
    if (state == S_CUES) drawCueMenu();
    if (state == S_OVER) {
        int *w = mode == 0 ? winsBot : winsVs;
        const char *hint = "X: REVANCHE   START: MENU";
        rect(0, 0, 640, 448, GS_SETREG_RGBAQ(0, 0, 0, 0x40, 0x00));
        hbox(120, 150, 520, 290, CA(255, 215, 60, 0x7c), CA(6, 10, 20, 0x78));
        sprintf(buf, "%s VENCEU!", (winner == 1 && mode == 0) ? "BOT" :
                (winner == 0 ? "JOGADOR 1" : "JOGADOR 2"));
        text(320 - tw(0.7f, buf) * 0.5f, 170, 0.7f, COL(255,230,60), buf);
        rect(160, 204, 480, 206, CA(255, 215, 60, 0x50));
        sprintf(buf, "J1 %d X %d %s", w[0], w[1], mode == 0 ? "BOT" : "J2");
        text(320 - tw(0.5f, buf) * 0.5f, 218, 0.5f, COL(255,255,255), buf);
        text(320 - tw(0.36f, hint) * 0.5f, 258, 0.36f, COL(200,205,215), hint);
    }
}

/* ---------- main ---------- */
int main(void)
{
    u32 in, prev = 0, pressed, p0, p1;
    int i;

    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    padInit(0);
    padPortOpen(0, 0, padbuf0);
    padPortOpen(1, 0, padbuf1);

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
        p0 = readpad(0);
        p1 = (mode == 1 || state == S_MENU) ? readpad(1) : 0;
        if (state == S_MENU || state == S_OVER || state == S_CREDITS) in = p0 | p1;
        else if (mode == 1) in = p0 | p1;
        else in = (cur == 0) ? p0 : 0;
        pressed = in & ~prev;
        prev = in;
        menuT++;
        if (state == S_MENU || state == S_CREDITS) menuUpdate();

        switch (state) {
        case S_MENU:
            if (pressed & PAD_UP)   menuSel = (menuSel + 4) % 5;
            if (pressed & PAD_DOWN) menuSel = (menuSel + 1) % 5;
            if (menuSel == 3) {
                if (pressed & PAD_LEFT)  botLevel = (botLevel + 2) % 3;
                if (pressed & PAD_RIGHT) botLevel = (botLevel + 1) % 3;
            }
            if (pressed & (PAD_CROSS | PAD_START)) {
                if (menuSel == 0) menuNote = 120;                       /* modo historia: em breve */
                else if (menuSel <= 2) { rs += 7919 + menuT; newGame(menuSel - 1); }
                else if (menuSel == 3) botLevel = (botLevel + 1) % 3;
                else state = S_CREDITS;
            }
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
