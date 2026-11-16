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

static void disc(float cx, float cy, float r, u64 c)
{
    int i, n = 14;
    for (i = 0; i < n; i++) {
        float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n;
        gsKit_prim_triangle(g, cx+OX, cy+OY,
                            cx+OX+cosf(a0)*r, cy+OY+sinf(a0)*r,
                            cx+OX+cosf(a1)*r, cy+OY+sinf(a1)*r, 1, c);
    }
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

static void drawBallAt(int i, float x, float y, float r)
{
    int id = (i > 8) ? i - 8 : i;
    if (i == 0 || i > 8) {
        disc(x, y, r, COL(250,250,250));
        if (i > 8) rect(x-r*0.85f, y-r*0.5f, x+r*0.85f, y+r*0.5f,
                        COL(BC[id][0], BC[id][1], BC[id][2]));
    } else {
        disc(x, y, r, COL(BC[i][0], BC[i][1], BC[i][2]));
        if (i != 8) disc(x, y, r*0.38f, COL(250,250,250));
    }
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
    for (i = 0; i < 16; i++) { b[i].vx = b[i].vy = 0; b[i].in = 1; }
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

static void drawGuide(void)
{
    float dx = cosf(ang), dy = sinf(ang), cx = b[0].x, cy = b[0].y;
    float tmin = 1e9f, t;
    int i;
    for (i = 1; i < 16; i++) {
        float fx, fy, proj, perp2;
        if (!b[i].in) continue;
        fx = b[i].x - cx; fy = b[i].y - cy;
        proj = fx*dx + fy*dy;
        if (proj < 0) continue;
        perp2 = fx*fx + fy*fy - proj*proj;
        if (perp2 > 4*R*R) continue;
        t = proj - sqrtf(4*R*R - perp2);
        if (t < tmin) tmin = t;
    }
    if (dx > 0.0001f)  { t = (TX1-R-cx)/dx; if (t < tmin) tmin = t; }
    if (dx < -0.0001f) { t = (TX0+R-cx)/dx; if (t < tmin) tmin = t; }
    if (dy > 0.0001f)  { t = (TY1-R-cy)/dy; if (t < tmin) tmin = t; }
    if (dy < -0.0001f) { t = (TY0+R-cy)/dy; if (t < tmin) tmin = t; }
    if (tmin < 0) tmin = 0;
    line(cx + dx*R, cy + dy*R, cx + dx*tmin, cy + dy*tmin, COL(240,240,240));
    ring(cx + dx*tmin, cy + dy*tmin, R, COL(240,240,240));
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

    g = gsKit_init_global_custom(192 * 1024, 256 * 1024);
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

        gsKit_clear(g, GS_SETREG_RGBAQ(0x10, 0x10, 0x18, 0x00, 0x00));
        draw();
        gsKit_queue_exec(g);
        gsKit_sync_flip(g);
    }
    return 0;
}
