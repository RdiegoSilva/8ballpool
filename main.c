/* 8 BALL POOL para PS2 - PS2SDK + gsKit + libpad
 * Modos: 1 jogador (vs BOT) e 2 jogadores (dois controles).
 * Controles na mira:  Esq/Dir = girar | L1/R1 = ajuste fino
 *                     segurar X = carregar forca, soltar = tacada
 *                     O = cancelar tacada
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

#define R     9.0f
#define TX0   70.0f
#define TX1   570.0f
#define TY0   105.0f
#define TY1   355.0f
#define TCY   230.0f
#define MAXV  900.0f

enum { S_MENU, S_AIM, S_ROLL, S_PLACE, S_OVER };

typedef struct { float x, y, vx, vy; int in; } Ball;

static GSGLOBAL *g;
static GSFONTM  *fm;
static float OX, OY;

static char padbuf0[256] __attribute__((aligned(64)));
static char padbuf1[256] __attribute__((aligned(64)));

static Ball b[16];
static int state = S_MENU, mode = 0, menuSel = 0;
static int cur, grp[2], breakShot, charging;
static float ang, pwr;
static int pocketed[16], firstHit, gAtStart, startOwnLeft;
static int winner, botTimer, botPlanned;
static float botAng, botPow;
static char msg[64];
static int msgTimer;

static const float PX[6] = { TX0, 320, TX1, TX0, 320, TX1 };
static const float PY[6] = { TY0, TY0, TY0, TY1, TY1, TY1 };
static const float PR[6] = { 18, 16, 18, 18, 16, 18 };

static const u8 BC[9][3] = {
    {255,255,255}, {240,200,20}, {30,60,200}, {210,30,30}, {120,40,150},
    {240,120,20},  {20,140,60},  {130,20,30}, {15,15,15}
};

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
    if (b[i].in) discN(b[i].x + 2.5f, b[i].y + 2.5f, R, COL(14,85,48), 10);
}

static u64 shade(const u8 *c, float f, int add)
{
    int r = (int)(c[0]*f) + add, gg = (int)(c[1]*f) + add, bb = (int)(c[2]*f) + add;
    if (r > 255) r = 255; if (gg > 255) gg = 255; if (bb > 255) bb = 255;
    return COL(r, gg, bb);
}

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
    if (!useTex) { drawBallVec(i, x, y, r); return; }
    drawBallMesh(i, x, y, r);
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

static void rack(void)
{
    static const int order[15] = {1,9,2,10,8,3,11,4,12,5,13,6,14,7,15};
    int i, r, c, k = 0;
    for (i = 0; i < 16; i++) { b[i].vx = b[i].vy = 0; b[i].in = 1; resetOrient(i); }
    b[0].x = TX0 + 125; b[0].y = TCY;
    for (r = 0; r < 5; r++)
        for (c = 0; c <= r; c++) {
            int id = order[k++];
            b[id].x = TX0 + 375 + r * (R * 1.76f);
            b[id].y = TCY + (c - r * 0.5f) * (2 * R + 0.4f);
        }
}

static void newGame(int m)
{
    mode = m; rack(); grp[0] = grp[1] = -1; cur = 0; ang = 0; pwr = 0;
    charging = 0; breakShot = 1; botTimer = 0; botPlanned = 0; state = S_AIM;
    setmsg("JOGADOR 1 - QUEBRA");
}

static void shoot(float p)
{
    memset(pocketed, 0, sizeof(pocketed));
    firstHit = -1;
    gAtStart = grp[cur];
    startOwnLeft = gAtStart >= 0 ? countGroup(gAtStart) : 99;
    b[0].vx = cosf(ang) * p * MAXV;
    b[0].vy = sinf(ang) * p * MAXV;
    breakShot = 0; pwr = 0; charging = 0; state = S_ROLL;
}

static int stepPhysics(float dt)
{
    int i, j, p, moving = 0;
    for (i = 0; i < 16; i++) {
        if (!b[i].in) continue;
        b[i].x += b[i].vx * dt; b[i].y += b[i].vy * dt;
        for (p = 0; p < 6; p++) {
            float dx = b[i].x - PX[p], dy = b[i].y - PY[p];
            if (dx*dx + dy*dy < PR[p]*PR[p]) {
                b[i].in = 0; b[i].vx = b[i].vy = 0; pocketed[i] = 1; break;
            }
        }
        if (!b[i].in) continue;
        if (b[i].x < TX0+R) { b[i].x = TX0+R; b[i].vx = -b[i].vx * 0.9f; }
        if (b[i].x > TX1-R) { b[i].x = TX1-R; b[i].vx = -b[i].vx * 0.9f; }
        if (b[i].y < TY0+R) { b[i].y = TY0+R; b[i].vy = -b[i].vy * 0.9f; }
        if (b[i].y > TY1-R) { b[i].y = TY1-R; b[i].vy = -b[i].vy * 0.9f; }
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
        s2 = sp * (1.0f - 0.9f * dt) - 20.0f * dt;
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
        if (!b[0].in) { b[0].in = 1; b[0].x = TX0 + 125; b[0].y = TCY; }
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
        if (bd > 1e8f) botAng = atan2f(TCY - cy, TX0 + 375 - cx);
        botPow = 0.65f;
    }
    if (breakShot) botPow = 1.0f;
    if (botPow > 1.0f) botPow = 1.0f;
    botAng += (((int)(rnd() % 2001)) - 1000) / 1000.0f * 0.02f;
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
static void drawTable(void)
{
    int p;
    rect(TX0-26, TY0-26, TX1+26, TY1+26, COL(95,55,25));
    rect(TX0-10, TY0-10, TX1+10, TY1+10, COL(20,90,50));
    rect(TX0, TY0, TX1, TY1, COL(25,125,70));
    for (p = 0; p < 6; p++) disc(PX[p], PY[p], PR[p] + 1, COL(5,5,5));
    line(TX0+125, TY0, TX0+125, TY1, COL(40,150,90));
}

static void thick(float x1, float y1, float x2, float y2, u64 c)
{
    float dx = x2 - x1, dy = y2 - y1, l = sqrtf(dx*dx + dy*dy), nx, ny;
    if (l < 0.01f) return;
    nx = -dy / l * 0.6f; ny = dx / l * 0.6f;
    line(x1 + nx, y1 + ny, x2 + nx, y2 + ny, c);
    line(x1 - nx, y1 - ny, x2 - nx, y2 - ny, c);
}

/* mira estilo 8 Ball Pool: linha ate a bola fantasma, linha da bola alvo
   e desvio da bola branca; nas tabelas mostra o rebote */
static void drawGuide(void)
{
    u64 W = COL(250,250,250), D = COL(190,190,190);
    float dx = cosf(ang), dy = sinf(ang), cx = b[0].x, cy = b[0].y;
    float tmin = 1e9f, t, gx, gy;
    int i, hit = -1, wall = 0;

    for (i = 1; i < 16; i++) {
        float fx, fy, proj, perp2;
        if (!b[i].in) continue;
        fx = b[i].x - cx; fy = b[i].y - cy;
        proj = fx*dx + fy*dy;
        if (proj < 0) continue;
        perp2 = fx*fx + fy*fy - proj*proj;
        if (perp2 > 4*R*R) continue;
        t = proj - sqrtf(4*R*R - perp2);
        if (t < tmin) { tmin = t; hit = i; wall = 0; }
    }
    if (dx > 0.0001f)  { t = (TX1-R-cx)/dx; if (t < tmin) { tmin = t; hit = -1; wall = 1; } }
    if (dx < -0.0001f) { t = (TX0+R-cx)/dx; if (t < tmin) { tmin = t; hit = -1; wall = 1; } }
    if (dy > 0.0001f)  { t = (TY1-R-cy)/dy; if (t < tmin) { tmin = t; hit = -1; wall = 2; } }
    if (dy < -0.0001f) { t = (TY0+R-cy)/dy; if (t < tmin) { tmin = t; hit = -1; wall = 2; } }
    if (tmin < 0) tmin = 0;

    gx = cx + dx*tmin; gy = cy + dy*tmin;
    thick(cx + dx*R, cy + dy*R, gx - dx*R, gy - dy*R, W);
    ring(gx, gy, R, W);
    ring(gx, gy, R - 0.7f, W);

    if (hit > 0) {
        float nx = b[hit].x - gx, ny = b[hit].y - gy, nl = sqrtf(nx*nx + ny*ny);
        float dot, tx, ty, tl, L;
        if (nl < 0.01f) return;
        nx /= nl; ny /= nl;
        dot = dx*nx + dy*ny; if (dot < 0) dot = 0;
        /* caminho da bola alvo */
        L = 35.0f + 110.0f * dot;
        thick(b[hit].x + nx*R, b[hit].y + ny*R,
              b[hit].x + nx*(R + L), b[hit].y + ny*(R + L), W);
        /* desvio da bola branca */
        tx = dx - dot*nx; ty = dy - dot*ny; tl = sqrtf(tx*tx + ty*ty);
        if (tl > 0.05f) {
            tx /= tl; ty /= tl;
            L = 18.0f + 60.0f * tl;
            thick(gx + tx*R, gy + ty*R, gx + tx*(R + L), gy + ty*(R + L), D);
        }
    } else if (wall) {
        float rx = dx, ry = dy;
        if (wall == 1) rx = -rx; else ry = -ry;
        thick(gx + rx*R, gy + ry*R, gx + rx*(R + 70), gy + ry*(R + 70), D);
    }
}

static void drawCue(void)
{
    float dx = cosf(ang), dy = sinf(ang), nx = -dy, ny = dx;
    float d0 = R + 4 + pwr * 70, d1 = d0 + 170;
    float x1 = b[0].x - dx*d0, y1 = b[0].y - dy*d0;
    float x2 = b[0].x - dx*d1, y2 = b[0].y - dy*d1;
    int k;
    for (k = -1; k <= 1; k++)
        line(x1 + nx*k, y1 + ny*k, x2 + nx*k, y2 + ny*k, COL(215,170,100));
}

static void drawHUD(void)
{
    char buf[64];
    int pl, i;
    sprintf(buf, "JOGADOR 1%s", ""); 
    for (pl = 0; pl < 2; pl++) {
        float x = pl == 0 ? 70 : 400;
        const char *nm = (pl == 1 && mode == 0) ? "BOT" : (pl == 0 ? "JOGADOR 1" : "JOGADOR 2");
        const char *gn = grp[pl] < 0 ? "" : (grp[pl] == 0 ? " (LISAS)" : " (LISTRADAS)");
        sprintf(buf, "%s%s", nm, gn);
        text(x, 38, 0.6f, pl == cur ? COL(255,230,60) : COL(160,160,160), buf);
        if (pl == cur) rect(x, 62, x + 160, 65, COL(255,230,60));
        if (grp[pl] >= 0) {
            int k = 0;
            for (i = 1; i < 16; i++)
                if (ballGroup(i) == grp[pl] && !b[i].in) {
                    drawBallAt(i, x + 8 + k*17, 420, 7); k++;
                }
        }
    }
    if (msgTimer > 0) { msgTimer--; text(150, 80, 0.55f, COL(255,255,255), msg); }
    if (state == S_AIM && !isBot()) {
        rect(220, 390, 420, 402, COL(60,60,60));
        rect(220, 390, 220 + 200*pwr, 402,
             COL((int)(255*pwr), (int)(255*(1-pwr)), 40));
        text(250, 404, 0.45f, COL(200,200,200), "X: FORCA   O: CANCELAR");
    }
}

static void draw(void)
{
    int i;
    char buf[64];
    if (state == S_MENU) {
        text(150, 100, 1.3f, COL(255,230,60), "8 BALL POOL");
        text(240, 160, 0.6f, COL(200,200,200), "PS2 EDITION");
        text(190, 230, 0.8f, menuSel == 0 ? COL(255,255,255) : COL(120,120,120), "1 JOGADOR (VS BOT)");
        text(190, 280, 0.8f, menuSel == 1 ? COL(255,255,255) : COL(120,120,120), "2 JOGADORES");
        disc(170, menuSel == 0 ? 245 : 295, 6, COL(255,230,60));
        text(190, 370, 0.5f, COL(160,160,160), "CIMA/BAIXO: ESCOLHER   X: INICIAR");
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
        } else drawBallAt(0, b[0].x, b[0].y, R);
    }
    if (state == S_AIM) {
        if (!isBot()) drawGuide();
        drawCue();
    }
    drawHUD();
    if (state == S_OVER) {
        rect(120, 150, 520, 290, COL(0,0,0));
        sprintf(buf, "%s VENCEU!", (winner == 1 && mode == 0) ? "BOT" :
                (winner == 0 ? "JOGADOR 1" : "JOGADOR 2"));
        text(180, 190, 0.9f, COL(255,230,60), buf);
        text(180, 250, 0.5f, COL(220,220,220), "START: VOLTAR AO MENU");
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

    for (;;) {
        p0 = readpad(0);
        p1 = (mode == 1 || state == S_MENU) ? readpad(1) : 0;
        if (state == S_MENU || state == S_OVER) in = p0 | p1;
        else if (mode == 1) in = p0 | p1;
        else in = (cur == 0) ? p0 : 0;
        pressed = in & ~prev;
        prev = in;

        switch (state) {
        case S_MENU:
            if (pressed & (PAD_UP | PAD_DOWN)) menuSel ^= 1;
            if (pressed & (PAD_CROSS | PAD_START)) { rs += 7919; newGame(menuSel); }
            break;

        case S_AIM:
            if (!isBot()) {
                if (in & PAD_LEFT)  ang -= 0.025f;
                if (in & PAD_RIGHT) ang += 0.025f;
                if (in & PAD_L1)    ang -= 0.005f;
                if (in & PAD_R1)    ang += 0.005f;
                if (in & PAD_CROSS) { pwr += 0.015f; if (pwr > 1) pwr = 1; charging = 1; }
                else if (charging) { charging = 0; if (pwr > 0.05f) shoot(pwr); else pwr = 0; }
                if (pressed & PAD_CIRCLE) { charging = 0; pwr = 0; }
            } else {
                botTimer++;
                if (botTimer == 40) { botPlan(); botPlanned = 1; }
                if (botPlanned) {
                    float d = normAng(botAng - ang);
                    if (fabsf(d) > 0.04f) ang += d > 0 ? 0.04f : -0.04f;
                    else {
                        ang = botAng;
                        if (botTimer > 80) {
                            pwr += 0.02f;
                            if (pwr >= botPow) shoot(botPow);
                        }
                    }
                }
            }
            break;

        case S_ROLL:
            for (i = 0; i < 8; i++) stepPhysics((1.0f / 60.0f) / 8.0f);
            for (i = 0; i < 16; i++)
                if (b[i].in && (b[i].vx != 0 || b[i].vy != 0)) spinBall(i, 1.0f / 60.0f);
            {
                int mv = 0;
                for (i = 0; i < 16; i++)
                    if (b[i].in && (b[i].vx != 0 || b[i].vy != 0)) mv = 1;
                if (!mv) resolve();
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
            } else {
                if (++botTimer > 40) {
                    int tries;
                    b[0].x = TX0 + 125; b[0].y = TCY;
                    for (tries = 0; tries < 500 && !validPlace(b[0].x, b[0].y); tries++) {
                        b[0].x = TX0 + R + (rnd() % (int)(TX1 - TX0 - 2*R));
                        b[0].y = TY0 + R + (rnd() % (int)(TY1 - TY0 - 2*R));
                    }
                    state = S_AIM; botTimer = 0; botPlanned = 0; pwr = 0;
                }
            }
            break;

        case S_OVER:
            if (pressed & PAD_START) state = S_MENU;
            break;
        }

        gsKit_clear(g, GS_SETREG_RGBAQ(0x10, 0x10, 0x18, 0x80, 0x00));
        gsKit_prim_sprite(g, 0, 0, (float)g->Width, (float)g->Height, 1, COL(16, 16, 24));
        draw();
        gsKit_queue_exec(g);
        gsKit_sync_flip(g);
    }
    return 0;
}
