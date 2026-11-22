/* music.c - player de musicas (WAV PCM 8/16 bits e MP3 via minimp3) */
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <audsrv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <math.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include "music.h"

/* printf no PS2 (SIO/ps2link) e LENTO e bloqueia o quadro: so liga com -DMUS_DEBUG */
#ifdef MUS_DEBUG
#define DBGP(...) printf(__VA_ARGS__)
#else
#define DBGP(...) ((void)0)
#endif

int __iomanX_id = -1;
int __fileXio_id = -1;

/* modulos IOP embutidos no .elf (veja o Makefile) */
#define IRX(n) extern unsigned char n##_irx[] __attribute__((aligned(16))); extern unsigned int size_##n##_irx
IRX(iomanX); IRX(fileXio); IRX(usbd); IRX(audsrv);
#ifdef USE_BDM
IRX(bdm); IRX(bdmfs_fatfs); IRX(usbmass_bd);
#else
IRX(usbhdfsd);
#endif

#define PATH_LEN 200
#define NAME_LEN 44

typedef struct { char path[PATH_LEN]; char name[NAME_LEN]; } Track;

static Track tr[MUS_MAX];
static int idleRun, budget, idleTail;
static int autoWait = -1, prefill;
static int dbgT, dbgM, dbgN, dbgLibsd, dbgAud, dbgInit, lateTries, lateWait;   /* diagnostico do audsrv */
static int nTr, cur = -1, state, audioOK, volume = 8, enabled = 1;
static int scanTries, scanWait = 120, scanDone, wantAuto = 1, ending, failStreak, toastT;
static char elfDir[PATH_LEN], srcDir[PATH_LEN];

/* decodificador */
static FILE *fp;
static int   isMp3, fmtSet, fmtHz, fmtCh, fmtBits;
static long  wavLeft;
static mp3dec_t dec;
static unsigned char mp3in[16 * 1024];
static int   mp3len, mp3pos, mp3eof;
static short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
static int   pcmLen, pcmPos;

/* ---------------- modulos / audio ---------------- */
int musicInit(const char *argv0)
{
    int i, last = -1;
    elfDir[0] = 0;
    if (argv0) {                                  /* pasta onde esta o .elf */
        for (i = 0; argv0[i] && i < PATH_LEN - 8; i++) if (argv0[i] == '/' || argv0[i] == ':') last = i;
        if (last >= 0) { memcpy(elfDir, argv0, last + 1); elfDir[last + 1] = 0; }
    }
    sbv_patch_enable_lmb();                       /* permite carregar modulos da memoria */
    __iomanX_id = SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
    __fileXio_id = SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
    fileXioInit();
    SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, NULL);
#ifdef USE_BDM
    SifExecModuleBuffer(bdm_irx, size_bdm_irx, 0, NULL, NULL);
    SifExecModuleBuffer(bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL, NULL);
    SifExecModuleBuffer(usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, NULL);
#else
    SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, NULL);
#endif
    dbgLibsd = SifLoadModule("rom0:LIBSD", 0, NULL);
    dbgAud = SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, NULL);
    for (i = 0; i < 30 && !audioOK; i++) {   /* no PS2 real o audsrv demora a subir: tenta de novo */
        volatile int w;
        dbgInit = audsrv_init();
        audioOK = (dbgInit == 0);
        if (!audioOK) for (w = 0; w < 4000000; w++) ;
    }
    printf("pool8: LIBSD=%d audsrv=%d init=%d\n", dbgLibsd, dbgAud, dbgInit);
    if (audioOK) { audsrv_set_volume(100); sfxPlay(SFX_OK, 100); }   /* bipe de teste ao iniciar */
    return audioOK;
}

/* ---------------- lista de faixas ---------------- */
static int extOf(const char *n)                   /* 1 = mp3, 2 = wav, 0 = outro */
{
    const char *e = strrchr(n, '.');
    if (!e) return 0;
    if (!strcasecmp(e, ".mp3")) return 1;
    if (!strcasecmp(e, ".wav")) return 2;
    return 0;
}

static void makeName(const char *file, char *out)   /* sem extensao, maiusculas, so caracteres da fonte */
{
    int i, n = 0;
    for (i = 0; file[i] && n < NAME_LEN - 1; i++) {
        char c = file[i];
        if (c == '.' && !strcasecmp(file + i, ".mp3")) break;
        if (c == '.' && !strcasecmp(file + i, ".wav")) break;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '.' ||
              c == '(' || c == ')' || c == '&' || c == '\'')) c = (c == '_') ? ' ' : '?';
        out[n++] = c;
    }
    out[n] = 0;
}

static void joinPath(char *out, const char *dir, const char *name)
{
    int l = (int)strlen(dir);
    snprintf(out, PATH_LEN, "%s%s%s", dir, (l && (dir[l - 1] == '/' || dir[l - 1] == ':')) ? "" : "/", name);
}

static void scanDir(const char *dir, int depth)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    if (!d) return;
    while ((e = readdir(d)) != NULL && nTr < MUS_MAX) {
        char p[PATH_LEN];
        if (e->d_name[0] == '.') continue;
        joinPath(p, dir, e->d_name);
        if (extOf(e->d_name)) {
            strcpy(tr[nTr].path, p);
            makeName(e->d_name, tr[nTr].name);
            nTr++;
        } else if (depth < 2 && !strchr(e->d_name, '.')) {     /* provavelmente uma pasta: entra nela */
            scanDir(p, depth + 1);
        }
    }
    closedir(d);
}

static int cmpTrack(const void *a, const void *b) { return strcmp(((const Track *)a)->name, ((const Track *)b)->name); }

int musicScan(void)
{
    char c[10][PATH_LEN];
    int i, n = 0;
    nTr = 0; srcDir[0] = 0;
    if (!audioOK) return 0;
    snprintf(c[n++], PATH_LEN, "mass:/MUSIC");
    snprintf(c[n++], PATH_LEN, "mass:/MUSICA");
    snprintf(c[n++], PATH_LEN, "mass:/MUSICAS");
    snprintf(c[n++], PATH_LEN, "mass:/MP3");
    snprintf(c[n++], PATH_LEN, "mass1:/MUSIC");
    if (elfDir[0]) { joinPath(c[n++], elfDir, "MUSIC"); joinPath(c[n++], elfDir, "MUSICA"); }
    snprintf(c[n++], PATH_LEN, "host:MUSIC");
    snprintf(c[n++], PATH_LEN, "mass:/");                       /* por ultimo: raiz do pen drive */
    if (elfDir[0]) snprintf(c[n++], PATH_LEN, "%s", elfDir);
    for (i = 0; i < n && nTr == 0; i++) {
        scanDir(c[i], 0);
        if (nTr) snprintf(srcDir, PATH_LEN, "%s", c[i]);
    }
    if (nTr > 1) qsort(tr, nTr, sizeof(Track), cmpTrack);
    if (cur >= nTr) cur = -1;
    return nTr;
}

/* ---------------- decodificacao ---------------- */
static void closeTrack(void)
{
    if (fp) { fclose(fp); fp = NULL; }
    pcmLen = pcmPos = 0; fmtSet = 0; mp3len = mp3pos = 0; mp3eof = 0; ending = 0;
}

static int le16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static long le32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((long)p[3] << 24); }

static int openWav(void)                         /* le o cabecalho; 1 = ok */
{
    unsigned char h[12], ch[8], f[16];
    int gotFmt = 0;
    if (fread(h, 1, 12, fp) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return 0;
    while (fread(ch, 1, 8, fp) == 8) {
        long sz = le32(ch + 4);
        if (!memcmp(ch, "fmt ", 4) && sz >= 16) {
            if (fread(f, 1, 16, fp) != 16) return 0;
            if (le16(f) != 1) return 0;                          /* so PCM */
            fmtCh = le16(f + 2); fmtHz = (int)le32(f + 4); fmtBits = le16(f + 14);
            if ((fmtCh != 1 && fmtCh != 2) || (fmtBits != 8 && fmtBits != 16)) return 0;
            gotFmt = 1;
            if (sz > 16) fseek(fp, sz - 16 + (sz & 1), SEEK_CUR); else if (sz & 1) fseek(fp, 1, SEEK_CUR);
        } else if (!memcmp(ch, "data", 4)) {
            if (!gotFmt) return 0;
            wavLeft = sz;
            return 1;
        } else {
            fseek(fp, sz + (sz & 1), SEEK_CUR);
        }
    }
    return 0;
}

/* ---------------- efeitos sonoros + mixer ---------------- */
#define SFX_HZ 22050
#define NVOICE 8
#define IDLE_QUEUE 8192                          /* sem musica: mantem so ~45 ms de audio na fila (efeito responde rapido) */

typedef struct { short *d; int len; } Sfx;
typedef struct { const short *d; int len, id, gain, gl, gr, pr; unsigned pos; } Voice;   /* gl/gr: ganho esq/dir; pr: tom (256 = normal) */
static Sfx   sfx[SFX_COUNT];
static Voice vc[NVOICE];
static int   nVoice, sfxBuilt;
static int   outHz, outCh, outBits;              /* formato que o audsrv esta usando agora */
static unsigned nzSeed = 12345u;
static unsigned char mixbuf[8192] __attribute__((aligned(64)));   /* alinhado: o audsrv copia por DMA */

static float nz(void) { nzSeed = nzSeed * 1664525u + 1013904223u; return ((nzSeed >> 8) & 0xFFFF) / 32768.0f - 1.0f; }

static short *bo[SFX_COUNT];                     /* estado da sintese incremental de cada efeito */
static int    bi[SFX_COUNT];
static float  blp[SFX_COUNT], bph[SFX_COUNT];

/* sintetiza o efeito (mono, 16 bits, SFX_HZ) em pedacos de maxN amostras (0 = tudo de uma vez).
   devolve 1 = pronto, 0 = ainda falta, -1 = sem memoria. Fazer tudo num quadro so (sinf/expf em software)
   travava o jogo logo depois de abrir. */
static int buildRun(int id, int maxN)
{
    static const float DUR[SFX_COUNT] = { 0.14f, 0.10f, 0.16f, 0.55f, 0.06f, 0.30f, 0.24f, 1.20f, 1.10f, 0.45f, 0.28f, 0.55f };
    const float TP = 6.2831853f;
    int i, end, n = (int)(DUR[id] * SFX_HZ);
    short *o;
    float lp, ph;
    if (sfx[id].d) return 1;
    if (!bo[id]) {
        bo[id] = (short *)malloc((size_t)n * sizeof(short));
        if (!bo[id]) return -1;
        bi[id] = 0; blp[id] = 0.0f; bph[id] = 0.0f;
    }
    o = bo[id]; lp = blp[id]; ph = bph[id];
    end = (maxN > 0 && bi[id] + maxN < n) ? bi[id] + maxN : n;
    for (i = bi[id]; i < end; i++) {
        float t = (float)i / SFX_HZ, v = 0.0f, x;
        switch (id) {
        case SFX_HIT:                                        /* "tac" seco do taco: estalo de couro + corpo grave que cai de tom */
            x = nz(); lp += 0.6f * (x - lp);
            ph += TP * (140.0f + 90.0f * expf(-t * 60.0f)) / SFX_HZ;
            v = 0.85f * expf(-t * 30.0f) * sinf(ph) + 0.6f * expf(-t * 60.0f) * sinf(TP * 1250.0f * t)
              + 0.7f * lp * expf(-t * 220.0f);
            break;
        case SFX_BALL:                                       /* "clac" de vidro: bola contra bola, parciais agudas e secas */
            v = expf(-t * 95.0f) * (0.5f * sinf(TP * 2850.0f * t) + 0.35f * sinf(TP * 4410.0f * t + 0.7f) + 0.2f * sinf(TP * 6100.0f * t))
              + 0.35f * nz() * expf(-t * 400.0f);
            break;
        case SFX_CUSHION:                                    /* "tum" de borracha: grave que desce + sopro curto */
            x = nz(); lp += 0.10f * (x - lp);
            ph += TP * (60.0f + 60.0f * expf(-t * 35.0f)) / SFX_HZ;
            v = 0.95f * expf(-t * 28.0f) * sinf(ph) + 1.1f * lp * expf(-t * 55.0f);
            break;
        case SFX_POCKET: {                                   /* bola cai: baque, tres quiques de "toc" e rolar na rede */
            static const float TB[3] = { 0.025f, 0.17f, 0.29f }, AB[3] = { 0.8f, 0.5f, 0.3f };
            int k;
            x = nz(); lp += 0.06f * (x - lp);
            v = 0.9f * expf(-t * 22.0f) * sinf(TP * 85.0f * t) + 1.8f * lp * expf(-t * 6.5f) * (t < 0.04f ? t / 0.04f : 1.0f);
            for (k = 0; k < 3; k++) {
                float tt = t - TB[k];
                if (tt > 0.0f) v += AB[k] * expf(-tt * 55.0f) * sinf(TP * (480.0f - 90.0f * k) * tt);
            }
            break; }
        case SFX_MOVE:                                       /* "blip" curto e leve de navegacao */
            v = 0.5f * expf(-t * 60.0f) * (sinf(TP * 1400.0f * t) + 0.25f * sinf(TP * 2800.0f * t));
            break;
        case SFX_OK: {                                       /* confirmar: "ding-ding" de sino (do alto -> sol alto) */
            float t2 = t - 0.09f;
            v = expf(-t * 15.0f) * (0.4f * sinf(TP * 1046.5f * t) + 0.18f * sinf(TP * 2888.0f * t) * expf(-t * 25.0f));
            if (t2 > 0.0f) v += expf(-t2 * 13.0f) * (0.42f * sinf(TP * 1568.0f * t2) + 0.18f * sinf(TP * 4328.0f * t2) * expf(-t2 * 25.0f));
            break; }
        case SFX_BACK: {                                     /* voltar: "bloop" que escorrega para baixo */
            float f = 300.0f + 520.0f * expf(-t * 11.0f);
            ph += TP * f / SFX_HZ;
            v = 0.5f * expf(-t * 8.0f) * (sinf(ph) + 0.2f * sinf(2.0f * ph));
            break; }
        case SFX_WIN: {                                      /* fanfarra: do-mi-sol-do subindo e acorde final cheio */
            static const float F[4] = { 523.25f, 659.25f, 783.99f, 1046.5f };
            if (t < 0.48f) {
                int k = (int)(t / 0.12f); float tt;
                if (k > 3) k = 3;
                tt = t - k * 0.12f;
                v = 0.4f * expf(-tt * 7.0f) * (sinf(TP * F[k] * t) + 0.35f * sinf(TP * F[k] * 2.0f * t) + 0.15f * sinf(TP * F[k] * 3.0f * t));
            } else {
                float tt = t - 0.48f, e = expf(-tt * 2.8f) * (tt < 0.01f ? tt / 0.01f : 1.0f);
                v = 0.22f * e * (sinf(TP * F[0] * t) + sinf(TP * F[1] * t) + sinf(TP * F[2] * t) + sinf(TP * F[3] * t))
                  + 0.1f * e * sinf(TP * F[3] * 2.0f * t);
            }
            break; }
        case SFX_LOSE: {                                     /* "wah wah wah wahhh": trombone triste descendo */
            static const float F[4] = { 233.08f, 220.0f, 207.65f, 196.0f };
            int k = (int)(t / 0.25f); float tt, f, w;
            if (k > 3) k = 3;
            tt = t - k * 0.25f;
            f = F[k] * (1.0f + 0.012f * sinf(TP * 6.0f * t));
            if (k == 3) f *= 1.0f - 0.12f * (tt / 0.35f);
            ph += TP * f / SFX_HZ;
            w = 0.45f + 0.55f * (0.5f + 0.5f * sinf(TP * 3.5f * tt - 1.0f));
            v = 0.42f * (k == 3 ? expf(-tt * 2.4f) : expf(-tt * 3.0f)) * (sinf(ph) + w * 0.6f * sinf(2.0f * ph) + w * 0.35f * sinf(3.0f * ph));
            break; }
        case SFX_FOUL: {                                     /* buzina de erro: dois "bzzt" graves */
            float sq = fmodf(t * 120.0f, 1.0f) < 0.5f ? 1.0f : -1.0f, tt = t < 0.2f ? t : t - 0.2f;
            lp += 0.25f * (sq - lp);
            v = 0.3f * lp * (t < 0.15f || (t > 0.2f && t < 0.38f) ? 1.0f : 0.0f) * expf(-tt * 3.0f);
            break; }
        case SFX_CHALK:                                      /* giz: tres raspadas rapidas */
            x = nz(); lp += 0.35f * (x - lp);
            v = 0.7f * (x - lp) * sinf(3.14159f * t / DUR[id]) * (0.35f + 0.65f * fabsf(sinf(TP * 5.5f * t)));
            break;
        case SFX_ABIL: {                                     /* poder ativado: varredura subindo com brilho */
            float f = 300.0f * expf(1.79f * t / DUR[id]), e = sinf(3.14159f * t / DUR[id]);
            ph += TP * f / SFX_HZ;
            v = 0.42f * e * (sinf(ph) + 0.5f * sinf(2.0f * ph)) + (t > 0.35f ? 0.18f * expf(-(t - 0.35f) * 9.0f) * sinf(TP * 2637.0f * t) : 0.0f);
            break; }
        }
        {   /* entrada/saida suaves (1 ms / 6 ms): sem estalo no comeco e no fim do efeito */
            float fi = (float)i / (SFX_HZ * 0.001f), fo = (float)(n - 1 - i) / (SFX_HZ * 0.006f);
            if (fi < 1.0f) v *= fi;
            if (fo < 1.0f) v *= fo;
        }
        v = v / (1.0f + 0.6f * fabsf(v));                    /* saturacao suave: picos nao estouram */
        o[i] = (short)(v * 26000.0f);
    }
    bi[id] = end; blp[id] = lp; bph[id] = ph;
    if (end < n) return 0;
    sfx[id].d = o; sfx[id].len = n; bo[id] = NULL;
    return 1;
}

static int build(int id) { return buildRun(id, 0) > 0; }

static int ensureFmt(int hz, int ch, int bits, int force)   /* aplica o formato no audsrv (so se mudou) */
{
    struct audsrv_fmt_t f;
    if (!force && hz == outHz && ch == outCh && bits == outBits) return 1;
    audsrv_stop_audio();
    f.freq = hz; f.bits = bits; f.channels = ch;
    if (audsrv_set_format(&f) != AUDSRV_ERR_NOERROR) { outHz = 0; return 0; }
    outHz = hz; outCh = ch; outBits = bits;
    return 1;
}

static int setFmt(void)                          /* aplica o formato da faixa no audsrv */
{
    fmtSet = ensureFmt(fmtHz, fmtCh, fmtBits, 1);
    return fmtSet;
}

/* escreve nbytes (multiplo do frame) em dst: musica (src, ou silencio se NULL) com ganho mg/256 + efeitos */
static void mixOut(unsigned char *dst, const unsigned char *src, int nbytes, int mg)
{
    int fb, frames, f, k;
    unsigned step;
    if (outHz <= 0) return;
    fb = outCh * (outBits / 8); frames = nbytes / fb;
    step = ((unsigned)SFX_HZ << 16) / (unsigned)outHz;
    for (f = 0; f < frames; f++) {
        int l = 0, r = 0;
        if (src) {
            if (outBits == 16) {
                const short *s = (const short *)src;
                l = s[f * outCh]; r = outCh == 2 ? s[f * outCh + 1] : l;
            } else {
                l = ((int)src[f * outCh] - 128) << 8; r = outCh == 2 ? (((int)src[f * outCh + 1] - 128) << 8) : l;
            }
            l = (l * mg) >> 8; r = (r * mg) >> 8;
        }
        for (k = 0; k < nVoice; k++) {
            Voice *v = &vc[k];
            unsigned idx = v->pos >> 16;
            int s;
            if ((int)idx >= v->len) { vc[k--] = vc[--nVoice]; continue; }
            {   /* interpolacao cubica (Catmull-Rom): mais nitida que a linear */
                float y0 = idx > 0 ? (float)v->d[idx - 1] : 0.0f, y1 = (float)v->d[idx];
                float y2 = (int)idx + 1 < v->len ? (float)v->d[idx + 1] : 0.0f, y3 = (int)idx + 2 < v->len ? (float)v->d[idx + 2] : 0.0f;
                float fr = (float)(v->pos & 0xFFFF) * (1.0f / 65536.0f);
                float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                s = (int)(((c3 * fr + c2) * fr + c1) * fr + y1);
            }
            s = (s * v->gain) >> 8;
            l += (s * v->gl) >> 8; r += (s * v->gr) >> 8;
            v->pos += (step * (unsigned)v->pr) >> 8;
        }
        if (l > 22000) l = 22000 + ((l - 22000) >> 2); else if (l < -22000) l = -22000 + ((l + 22000) >> 2);   /* limitador suave */
        if (r > 22000) r = 22000 + ((r - 22000) >> 2); else if (r < -22000) r = -22000 + ((r + 22000) >> 2);
        if (l > 32767) l = 32767; else if (l < -32768) l = -32768;
        if (r > 32767) r = 32767; else if (r < -32768) r = -32768;
        if (outBits == 16) {
            short *d = (short *)dst;
            d[f * outCh] = (short)l; if (outCh == 2) d[f * outCh + 1] = (short)r;
        } else {
            dst[f * outCh] = (unsigned char)((l >> 8) + 128); if (outCh == 2) dst[f * outCh + 1] = (unsigned char)((r >> 8) + 128);
        }
    }
}

void sfxPlay(int id, int vol)
{
    int i, same = 0, pick = -1;
    unsigned best = 0;
    Voice *v;
    if (!audioOK || id < 0 || id >= SFX_COUNT || vol <= 0) return;
    if (!sfx[id].d && !build(id)) return;
    if (vol > 100) vol = 100;
    for (i = 0; i < nVoice; i++) if (vc[i].id == id) same++;
    if (same >= 3 || nVoice >= NVOICE) {          /* sem vaga: troca o que ja tocou mais (do mesmo efeito, se ha muitos) */
        for (i = 0; i < nVoice; i++) {
            if (same >= 3 && vc[i].id != id) continue;
            if (pick < 0 || vc[i].pos > best) { pick = i; best = vc[i].pos; }
        }
        if (pick < 0) return;
        v = &vc[pick];
    } else v = &vc[nVoice++];
    v->d = sfx[id].d; v->len = sfx[id].len; v->id = id; v->pos = 0;
    v->gain = vol * 2;
    {   /* variacao leve de tom e posicao: cada batida soa um pouco diferente */
        int pit = 256, pan = 0;
        if (id == SFX_HIT || id == SFX_BALL || id == SFX_CUSHION) { pit = 256 + (int)(rand() % 29) - 14; pan = (int)(rand() % 97) - 48; }
        else if (id == SFX_POCKET) { pit = 256 + (int)(rand() % 17) - 8; pan = (int)(rand() % 65) - 32; }
        v->pr = pit; v->gl = 256 - (pan > 0 ? pan : 0); v->gr = 256 + (pan < 0 ? pan : 0);
    }                            /* 100 -> 200/256 (deixa folga para somar com a musica) */
}

static void sfxIdle(void)                         /* sem musica tocando: manda so os efeitos */
{
    int avail, n, want, qd;
    DBGP("pool8: musicUpdate state=%d nTr=%d voices=%d fmtSet=%d\n", state, nTr, nVoice, fmtSet);
    if (!nVoice) {                                /* efeitos acabaram: limpa a fila do audsrv para ele nao repetir o ultimo trecho (chiado) */
        if (idleRun && state != 1 && ++idleTail >= 15) { audsrv_stop_audio(); outHz = 0; idleRun = 0; }
        return;
    }
    idleTail = 0;
    if (!(state == 1 && fmtSet) && !ensureFmt(44100, 2, 16, 0)) { DBGP("pool8: ensureFmt falhou\n"); return; }
    avail = audsrv_available();
    /* ritmo de reproducao: a cada quadro entra so o que o som consome em 1/60 s (44100 Hz * 4 bytes / 60 = 2940).
       nao usa audsrv_queued(): em alguns audsrv/PCSX2 ele devolve o mesmo valor de available */
    if (!idleRun) { idleRun = 1; budget = 2940 * 3; }          /* primeira entrega: colchao de ~50 ms */
    else { budget += 2940; if (budget > 2940 * 4) budget = 2940 * 4; }
    qd = audsrv_queued();
    if (qd >= 0 && qd < avail) want = IDLE_QUEUE - qd;         /* valores coerentes: mantem a fila em ~45 ms */
    else want = budget;                                        /* valores suspeitos (iguais): usa o ritmo por quadro */
    n = avail < want ? avail : want;
    if (n > (int)sizeof(mixbuf)) n = (int)sizeof(mixbuf);
    n -= n % (outCh * (outBits / 8));
    if (n <= 0) { DBGP("pool8: sfxIdle sem espaco avail=%d queued=%d want=%d\n", avail, audsrv_queued(), want); return; }
    mixOut(mixbuf, NULL, n, 0);
    FlushCache(0);
    {
        int r = audsrv_play_audio((const char *)mixbuf, n);
        budget -= n;
        (void)r;
        DBGP("pool8: sfx avail=%d queued=%d want=%d n=%d voices=%d play=%d\n", avail, qd, want, n, nVoice, r);
    }
}

static int decodeNext(void)                      /* enche pcm[]; 0 = fim da faixa ou erro */
{
    if (!isMp3) {
        long want = 4096;
        size_t got;
        if (wavLeft <= 0) return 0;
        if (want > wavLeft) want = wavLeft;
        got = fread(pcm, 1, (size_t)want, fp);
        if (got == 0) return 0;
        wavLeft -= (long)got;
        pcmLen = (int)got; pcmPos = 0;
        if (!fmtSet && !setFmt()) return 0;
        return 1;
    }
    for (;;) {
        mp3dec_frame_info_t info;
        int samples;
        if (mp3len - mp3pos < 8192 && !mp3eof) {                 /* reabastece o buffer do arquivo, em pedacos pequenos (evita uma pausa grande de leitura no USB) */
            size_t got, room;
            memmove(mp3in, mp3in + mp3pos, mp3len - mp3pos);
            mp3len -= mp3pos; mp3pos = 0;
            room = sizeof(mp3in) - mp3len;
            if (room > 4096) room = 4096;
            got = fread(mp3in + mp3len, 1, room, fp);
            if (got == 0) mp3eof = 1;
            mp3len += (int)got;
        }
        if (mp3len - mp3pos <= 0) return 0;
        samples = mp3dec_decode_frame(&dec, mp3in + mp3pos, mp3len - mp3pos, pcm, &info);
        mp3pos += info.frame_bytes;
        if (samples > 0) {
            if (!fmtSet) { fmtHz = info.hz; fmtCh = info.channels; fmtBits = 16; if (!setFmt()) return 0; }
            pcmLen = samples * info.channels * 2; pcmPos = 0;
            return 1;
        }
        if (info.frame_bytes == 0) return 0;                     /* acabou */
    }
}

/* ---------------- controle ---------------- */
static void startTrack(int i, int dir)
{
    int tries;
    for (tries = 0; tries < nTr; tries++) {
        const char *ext;
        closeTrack();
        cur = i;
        fp = fopen(tr[i].path, "rb");
        ext = strrchr(tr[i].path, '.');
        isMp3 = (ext && !strcasecmp(ext, ".mp3"));
        if (fp) {
            if (isMp3) { mp3dec_init(&dec); state = 1; prefill = 40; toastT = 240; failStreak = 0; return; }
            if (openWav()) { state = 1; prefill = 40; toastT = 240; failStreak = 0; return; }
        }
        closeTrack();
        i = (i + dir + nTr) % nTr;                              /* arquivo ruim: tenta o proximo */
    }
    state = 0;                                                   /* nenhuma faixa tocavel */
}

void musicPlay(int i)
{
    if (!audioOK || nTr <= 0 || i < 0 || i >= nTr) return;
    startTrack(i, 1);
}

void musicNext(int d)
{
    if (!audioOK || nTr <= 0) return;
    startTrack(((cur < 0 ? 0 : cur) + (d < 0 ? -1 : 1) + nTr) % nTr, d < 0 ? -1 : 1);
}

void musicToggle(void)
{
    if (!audioOK || nTr <= 0) return;
    if (state == 1)      { state = 2; audsrv_stop_audio(); }
    else if (state == 2) { state = 1; }
    else                 musicPlay(cur < 0 ? 0 : cur);
}

void musicSetVolume(int v)
{
    if (v < 0) v = 0;
    if (v > 10) v = 10;
    volume = v;
}

void musicSetEnabled(int on)
{
    enabled = on;
    if (!audioOK) return;
    if (!on && state == 1) { state = 2; audsrv_stop_audio(); }
    else if (on && state == 2) state = 1;
    else if (on && state == 0 && nTr > 0) musicPlay(cur < 0 ? 0 : cur);
}

void musicUpdate(void)
{
    int it, fed = 0, heavy = 0, iters;
    if (!audioOK) {                               /* o audsrv pode subir tarde no console: continua tentando */
        if (lateTries < 20 && --lateWait <= 0) {
            lateWait = 120; lateTries++;
            if (dbgAud < 0)                       /* o modulo nem carregou: carrega de novo */
                dbgAud = SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, NULL);
            dbgInit = audsrv_init();
            if (dbgInit == 0) { audioOK = 1; audsrv_set_volume(100); }
        }
        if (!audioOK) return;
    }
    if (toastT > 0) toastT--;

    if (nTr == 0 && !scanDone && --scanWait <= 0) {              /* o USB demora um pouco para aparecer */
        scanWait = 240; heavy = 1;
        if (musicScan() > 0) scanDone = 1;
        else if (++scanTries >= 8) scanDone = 1;
    }
    if (nTr > 0 && wantAuto && !heavy) {                          /* comeca sozinho, numa faixa qualquer (alguns quadros depois da busca) */
        if (autoWait < 0) autoWait = 20;
        if (--autoWait <= 0) {
            wantAuto = 0; heavy = 1;
            if (enabled) { srand((unsigned)(scanTries * 7919 + nTr)); musicPlay(rand() % nTr); }
        }
    }
    if (!heavy && sfxBuilt < SFX_COUNT) {                         /* gera os efeitos aos poucos (500 amostras por quadro) */
        int r = buildRun(sfxBuilt, 500);
        if (r != 0) sfxBuilt++;
    }

    if (state == 1 && ending) {                                   /* espera o som acabar e vai para a proxima */
        if (audsrv_queued() < 1024 && !nVoice) { musicNext(1); return; }
    } else if (state == 1) {
        iters = prefill > 0 ? 6 : 2;                              /* no comeco enche a fila do audsrv mais rapido (sem falhas se o quadro demorar) */
        if (prefill > 0) prefill--;
        for (it = 0; it < iters; it++) {
            int avail, n, fb;
            if (pcmPos >= pcmLen) {
                if (!decodeNext()) { if (!fmtSet) { if (++failStreak > nTr) { state = 0; return; } musicNext(1); return; } ending = 1; break; }
            }
            if (!ensureFmt(fmtHz, fmtCh, fmtBits, 0)) { state = 0; return; }   /* volta ao formato da musica (os efeitos podem ter mudado) */
            avail = audsrv_available();
            if (avail <= 0) break;
            fb = outCh * (outBits / 8);
            n = pcmLen - pcmPos;
            if (n > avail) n = avail;
            if (n > (int)sizeof(mixbuf)) n = (int)sizeof(mixbuf);
            n -= n % fb;
            if (n <= 0) { if (pcmLen - pcmPos < fb) pcmPos = pcmLen; break; }
            mixOut(mixbuf, (const unsigned char *)pcm + pcmPos, n, volume * 256 / 10);
            FlushCache(0); audsrv_play_audio((const char *)mixbuf, n);
            pcmPos += n; fed = 1;
        }
    }
    if (!fed) sfxIdle();                                          /* parada, pausada, sem faixas ou no fim da faixa: so efeitos */
}

/* ---------------- consulta ---------------- */
int         musicCount(void)  { return nTr; }
const char *musicName(int i)  { return (i >= 0 && i < nTr) ? tr[i].name : ""; }
int         musicCur(void)    { return cur; }
int         musicState(void)  { return state; }
int         musicReady(void)  { return audioOK; }
const char *musicSource(void) { return srcDir; }
const char *musicToast(void)  { return (toastT > 0 && cur >= 0 && cur < nTr) ? tr[cur].name : NULL; }
const char *musicStatus(void)
{
    if (!audioOK) {
        static char dbg[64];
        sprintf(dbg, "AUDIO INDISPONIVEL (L%d A%d I%d)", dbgLibsd, dbgAud, dbgInit);
        return dbg;
    }
    if (nTr > 0) return state == 1 ? "TOCANDO" : (state == 2 ? "PAUSADO" : "PARADO");
    return scanDone ? "NENHUMA MUSICA ENCONTRADA" : "PROCURANDO MUSICAS...";
}
