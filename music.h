/* music.h - player de musicas (WAV e MP3) para o pool8
 * Procura as musicas numa pasta do pen drive/HD USB (mass:), na pasta do proprio jogo ou em host:
 * e toca durante as partidas usando audsrv. */
#ifndef MUSIC_H
#define MUSIC_H

#define MUS_MAX 256

/* carrega os modulos (USB, fileXio, audsrv). argv0 = caminho do .elf (para achar a pasta MUSIC ao lado). 1 = audio ok */
int  musicInit(const char *argv0);
void musicUpdate(void);                 /* chamar uma vez por frame */

int  musicScan(void);                   /* (re)procura as pastas; devolve o numero de faixas */
void musicPlay(int i);                  /* toca a faixa i */
void musicNext(int d);                  /* d = +1 proxima, -1 anterior */
void musicToggle(void);                 /* pausa / continua */
void musicSetVolume(int v);             /* 0..10 */
void musicSetEnabled(int on);           /* 0 = silencia e pausa, 1 = volta a tocar */

int         musicCount(void);
const char *musicName(int i);
int         musicCur(void);             /* faixa atual (-1 = nenhuma) */
int         musicState(void);           /* 0 parado, 1 tocando, 2 pausado */
int         musicReady(void);           /* 1 se o audio iniciou */
const char *musicStatus(void);          /* texto curto para a tela */
const char *musicSource(void);          /* pasta onde as musicas foram achadas */
const char *musicToast(void);           /* nome da faixa que acabou de comecar (ou NULL) */

#endif
