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
static int dbgN, dbgLibsd, dbgAud, dbgInit, lateTries, lateWait;   /* diagnostico do audsrv */
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
typedef struct { const short *d; int len, id, gain; unsigned pos; } Voice;
static Sfx   sfx[SFX_COUNT];
static Voice vc[NVOICE];
static int   nVoice, sfxBuilt;
static int   outHz, outCh, outBits;              /* formato que o audsrv esta usando agora */
static unsigned nzSeed = 12345u;
static unsigned char mixbuf[8192] __attribute__((aligned(64)));   /* alinhado: o audsrv copia por DMA */

static float nz(void) { nzSeed = nzSeed * 1664525u + 1013904223u; return ((nzSeed >> 8) & 0xFFFF) / 32768.0f - 1.0f; }

static int build(int id)                         /* sintetiza o efeito (mono, 16 bits, SFX_HZ) */
{
    static const float DUR[SFX_COUNT] = { 0.12f, 0.09f, 0.15f, 0.45f, 0.04f, 0.14f, 0.14f, 1.00f, 1.00f, 0.40f, 0.28f, 0.50f };
    const float TP = 6.2831853f;
    int i, n = (int)(DUR[id] * SFX_HZ);
    short *o = (short *)malloc((size_t)n * sizeof(short));
    float lp = 0.0f, lp2 = 0.0f, ph = 0.0f;
    if (!o) return 0;
    for (i = 0; i < n; i++) {
        float t = (float)i / SFX_HZ, v = 0.0f, x;
        switch (id) {
        case SFX_HIT:                                        /* "toc" grave do taco na bola branca */
            v = 0.75f * expf(-t * 40.0f) * sinf(TP * 190.0f * t) + 0.55f * nz() * expf(-t * 140.0f)
              + 0.3f * expf(-t * 90.0f) * sinf(TP * 1400.0f * t);
            break;
        case SFX_BALL:                                       /* "clac" seco de bola contra bola */
            v = expf(-t * 75.0f) * (0.55f * sinf(TP * 2300.0f * t) + 0.35f * sinf(TP * 3700.0f * t + 1.0f))
              + 0.3f * nz() * expf(-t * 320.0f);
            break;
        case SFX_CUSHION:                                    /* "pof" surdo da tabela */
            x = nz(); lp += 0.12f * (x - lp);
            v = 0.8f * expf(-t * 32.0f) * sinf(TP * 95.0f * t) + 1.2f * lp * expf(-t * 45.0f);
            break;
        case SFX_POCKET:                                     /* bola cai na cacapa e rola no couro */
            x = nz(); lp += 0.07f * (x - lp);
            ph += TP * (120.0f - 70.0f * t) / SFX_HZ;
            v = 0.8f * expf(-t * 14.0f) * sinf(ph) + 2.2f * lp * expf(-t * 5.0f) * (t < 0.03f ? t / 0.03f : 1.0f)
              + 0.4f * nz() * expf(-t * 200.0f);
            break;
        case SFX_MOVE:   v = 0.5f * expf(-t * 90.0f) * sinf(TP * 1200.0f * t); break;
        case SFX_OK:     v = 0.55f * expf(-(t < 0.05f ? t : t - 0.05f) * 28.0f) * sinf(TP * (t < 0.05f ? 880.0f : 1320.0f) * t); break;
        case SFX_BACK:   v = 0.55f * expf(-(t < 0.05f ? t : t - 0.05f) * 28.0f) * sinf(TP * (t < 0.05f ? 660.0f : 440.0f) * t); break;
        case SFX_WIN: {                                      /* arpejo subindo: do-mi-sol-do */
            static const float F[4] = { 523.25f, 659.25f, 783.99f, 1046.5f };
            int k = (int)(t / 0.17f); float tt;
            if (k > 3) k = 3;
            tt = t - k * 0.17f;
            v = 0.45f * expf(-tt * (k == 3 ? 3.0f : 9.0f)) * (sinf(TP * F[k] * t) + 0.35f * sinf(TP * F[k] * 2.0f * t));
            break; }
        case SFX_LOSE: {                                     /* notas descendo, tristes */
            static const float F[4] = { 392.0f, 369.99f, 349.23f, 261.63f };
            int k = (int)(t / 0.22f); float tt;
            if (k > 3) k = 3;
            tt = t - k * 0.22f;
            v = 0.45f * expf(-tt * (k == 3 ? 2.5f : 6.0f)) * (sinf(TP * F[k] * t) + 0.5f * sinf(TP * F[k] * 1.5f * t));
            break; }
        case SFX_FOUL: {                                     /* dois "bips" ruins */
            float tt = t < 0.18f ? t : t - 0.18f, saw = fmodf(t * 130.0f, 1.0f) * 2.0f - 1.0f;
            v = 0.35f * saw * expf(-tt * 9.0f) * (t < 0.14f || (t > 0.18f && t < 0.38f) ? 1.0f : 0.0f);
            break; }
        case SFX_CHALK:                                      /* giz raspando */
            x = nz(); lp += 0.5f * (x - lp);
            v = 0.55f * (x - lp) * sinf(3.14159f * t / DUR[id]) * (0.6f + 0.4f * sinf(TP * 28.0f * t));
            break;
        case SFX_ABIL:                                       /* varredura subindo */
            ph += TP * (250.0f + 1400.0f * (t / DUR[id])) / SFX_HZ;
            v = 0.5f * sinf(ph) * sinf(3.14159f * t / DUR[id]);
            break;
        }
        (void)lp2;
        if (v > 1.0f) v = 1.0f; else if (v < -1.0f) v = -1.0f;
        o[i] = (short)(v * 30000.0f);
    }
    sfx[id].d = o; sfx[id].len = n;
    return 1;
}

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
            int a, b, s;
            if ((int)idx >= v->len) { vc[k--] = vc[--nVoice]; continue; }
            a = v->d[idx]; b = ((int)idx + 1 < v->len) ? v->d[idx + 1] : 0;
            s = a + (((b - a) * (int)((v->pos & 0xFFFF) >> 1)) >> 15);      /* interpolacao linear */
            s = (s * v->gain) >> 8;
            l += s; r += s;
            v->pos += step;
        }
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
    v->gain = vol * 2;                            /* 100 -> 200/256 (deixa folga para somar com a musica) */
}

static void sfxIdle(void)                         /* sem musica tocando: manda so os efeitos */
{
    int avail, n, want;
    if (!nVoice) return;
    if (!(state == 1 && fmtSet) && !ensureFmt(44100, 2, 16, 0)) { if (dbgN++ < 5) printf("pool8: ensureFmt falhou\n"); return; }
    avail = audsrv_available();
    want = IDLE_QUEUE - audsrv_queued();
    n = avail < want ? avail : want;
    if (n > (int)sizeof(mixbuf)) n = (int)sizeof(mixbuf);
    n -= n % (outCh * (outBits / 8));
    if (n <= 0) return;
    mixOut(mixbuf, NULL, n, 0);
    FlushCache(0);
    {
        int r = audsrv_play_audio((const char *)mixbuf, n);
        if (dbgN++ < 8) printf("pool8: sfxIdle avail=%d want=%d n=%d voices=%d play=%d\n", avail, want, n, nVoice, r);
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
        if (mp3len - mp3pos < 4096 && !mp3eof) {                 /* reabastece o buffer do arquivo */
            size_t got;
            memmove(mp3in, mp3in + mp3pos, mp3len - mp3pos);
            mp3len -= mp3pos; mp3pos = 0;
            got = fread(mp3in + mp3len, 1, sizeof(mp3in) - mp3len, fp);
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
            if (isMp3) { mp3dec_init(&dec); state = 1; toastT = 240; failStreak = 0; return; }
            if (openWav()) { state = 1; toastT = 240; failStreak = 0; return; }
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
    int it, fed = 0;
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
    if (sfxBuilt < SFX_COUNT) { if (!sfx[sfxBuilt].d) build(sfxBuilt); sfxBuilt++; }   /* gera um efeito por quadro */

    if (nTr == 0 && !scanDone && --scanWait <= 0) {              /* o USB demora um pouco para aparecer */
        scanWait = 240;
        if (musicScan() > 0) scanDone = 1;
        else if (++scanTries >= 8) scanDone = 1;
    }
    if (nTr > 0 && wantAuto) {                                    /* comeca sozinho, numa faixa qualquer */
        wantAuto = 0;
        if (enabled) { srand((unsigned)(scanTries * 7919 + nTr)); musicPlay(rand() % nTr); }
    }

    if (state == 1 && ending) {                                   /* espera o som acabar e vai para a proxima */
        if (audsrv_queued() < 1024 && !nVoice) { musicNext(1); return; }
    } else if (state == 1) {
        for (it = 0; it < 2; it++) {
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
