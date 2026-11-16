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
    SifLoadModule("rom0:LIBSD", 0, NULL);
    SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, NULL);
    audioOK = (audsrv_init() == 0);
    if (audioOK) audsrv_set_volume(volume * 10);
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

static int setFmt(void)                          /* aplica o formato da faixa no audsrv */
{
    struct audsrv_fmt_t f;
    audsrv_stop_audio();
    f.freq = fmtHz; f.bits = fmtBits; f.channels = fmtCh;
    if (audsrv_set_format(&f) != AUDSRV_ERR_NOERROR) return 0;
    fmtSet = 1;
    return 1;
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
    if (audioOK) audsrv_set_volume(volume * 10);
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
    int it;
    if (!audioOK) return;
    if (toastT > 0) toastT--;

    if (nTr == 0 && !scanDone && --scanWait <= 0) {              /* o USB demora um pouco para aparecer */
        scanWait = 240;
        if (musicScan() > 0) scanDone = 1;
        else if (++scanTries >= 8) scanDone = 1;
    }
    if (nTr > 0 && wantAuto) {                                    /* comeca sozinho, numa faixa qualquer */
        wantAuto = 0;
        if (enabled) { srand((unsigned)(scanTries * 7919 + nTr)); musicPlay(rand() % nTr); }
    }
    if (state != 1) return;

    if (ending) {                                                 /* espera o som acabar e vai para a proxima */
        if (audsrv_queued() < 1024) musicNext(1);
        return;
    }
    for (it = 0; it < 2; it++) {
        int avail, n;
        if (pcmPos >= pcmLen) {
            if (!decodeNext()) { if (!fmtSet) { if (++failStreak > nTr) { state = 0; return; } musicNext(1); } else ending = 1; return; }
        }
        avail = audsrv_available();
        if (avail <= 0) break;
        n = pcmLen - pcmPos;
        if (n > avail) n = avail;
        if (n <= 0) break;
        audsrv_play_audio((const char *)pcm + pcmPos, n);
        pcmPos += n;
    }
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
    if (!audioOK) return "AUDIO INDISPONIVEL";
    if (nTr > 0) return state == 1 ? "TOCANDO" : (state == 2 ? "PAUSADO" : "PARADO");
    return scanDone ? "NENHUMA MUSICA ENCONTRADA" : "PROCURANDO MUSICAS...";
}
