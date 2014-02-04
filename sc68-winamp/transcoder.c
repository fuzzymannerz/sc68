/*
 * @file    transcoder.c
 * @brief   sc68-ng plugin for winamp 5.5 - transcoder functions
 * @author  http://sourceforge.net/users/benjihan
 *
 * Copyright (C) 1998-2014 Benjamin Gerard
 *
 * Time-stamp: <2014-01-24 17:29:38 ben>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 *
 */

/* generated config header */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* winamp sc68 declarations */
#include "wasc68.h"

/* libc */
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

/* sc68 */
#include <sc68/sc68.h>
#include <sc68/file68_features.h>
#include <sc68/file68_str.h>

/* windows */
#include <windows.h>

/* winamp 2 */
#include "winamp/in2.h"

/* in_sc68.c */
EXTERN In_Module g_mod;

/* tracksel.c */
EXTERN int tracksel_dialog(HINSTANCE hinst, HWND hwnd, sc68_t * sc68);

struct transcon {
  sc68_t * sc68;                        /* sc68 instance */
  int done;                             /* 0:not done, 1:done -1:error */
  int allin1;                           /* 1:all tracks at once */
  size_t pcm;                           /* pcm counter */
};

static int get_track_from_uri(const char * uri)
{
  /* $$$ TODO: this function ! */
  return 0;
}

EXPORT
/**
 * Open sc68 transcoder.
 *
 * @param   uri  URI to transcode.
 * @param   siz  pointer to expected raw output size (progress bar)
 * @param   bps  pointer to output bit-per-sample
 * @param   nch  pointer to output number of channel
 * @param   spr  pointer to output sampling rate
 * @return  Transcoding context struct (sc68_t right now)
 * @retval  0 on errror
 */
intptr_t winampGetExtendedRead_open(
  const char *uri,int *siz, int *bps, int *nch, int *spr)
{
  struct transcon * trc;
  int res, ms, tracks, track;

  DBG("(\"%s\")\n", uri);

  trc = malloc(sizeof(struct transcon));
  if (!trc)
    goto error;
  trc->pcm = 0;
  trc->allin1 = 0;
  trc->sc68 = sc68_create(0);
  if (!trc->sc68)
    goto error;
  if (sc68_load_uri(trc->sc68, uri))
      goto error;
  tracks = sc68_cntl(trc->sc68,SC68_GET_TRACKS);
  if (tracks < 2) {
    DBG("only the one track\n");
    track = 1;
  } else {
    track = get_track_from_uri(uri);
    if (track == -1)
      track = SC68_DEF_TRACK;
    else if (track < 1 || track > tracks) {
      track = tracksel_dialog(g_mod.hDllInstance, g_mod.hMainWindow, trc->sc68);
      if (track == REMEMBER_NOT_SET)
        goto error;
      if (!track) {
        DBG("transcode default track\n");
        track = SC68_DEF_TRACK;
      } else if (track < 1 || track > tracks) {
        DBG("transcode all tracks as one\n");
        trc->allin1 = 1;
        track = 1;
      } else {
        DBG("transcode track #%d\n", track);
      }
    }
  }

  if (sc68_play(trc->sc68, track, 1))
    goto error;
  res = sc68_process(trc->sc68, 0, 0);
  if (res == SC68_ERROR)
    goto error;
  trc->done = !!(res & SC68_END);
  ms = sc68_cntl(trc->sc68, trc->allin1 ? SC68_GET_DSKLEN : SC68_GET_LEN);
  *nch = 2;
  *spr = sc68_cntl(trc->sc68, SC68_GET_SPR);
  *bps = 16;
  *siz = (int) ((uint64_t)ms * (*spr) / 1000) << 2;
  DBG("=> %p\n", (void*)trc);
  return (intptr_t)trc;

error:
  if (trc) {
    sc68_destroy(trc->sc68);
    free(trc);
  }
  DBG("=> FAILED\n");
  return 0;
}

EXPORT
/**
 * Run sc68 transcoder.
 *
 * @param   hdl  Pointer to transcoder context
 * @patam   dst  PCM buffer to fill
 * @param   len  dst size in byte
 * @param   end  pointer to a variable set by winamp to query the transcoder
 *               to kindly exit (abort)
 * @return  The number of byte actually filled in dst
 * @retval  0 to notify the end
 */
intptr_t winampGetExtendedRead_getData(
  intptr_t hdl, char *dst, int len, int *end)
{
  struct transcon * trc = (struct transcon *) hdl;
  int pcm, res;

  /* DBG("winampGetExtendedRead_getData(hdl=%p, len=%d)\n", */
  /*     (void *)trc, len); */

  if (*end || trc->done)
    return 0;
  pcm = len >> 2;
  res = sc68_process(trc->sc68, dst, &pcm);
  if (res == SC68_ERROR) {
    trc->done = -1;
    pcm = 0;
    goto error;
  }
  trc->pcm += pcm;
  trc->done = (res & SC68_END) ||
    ((res & (SC68_LOOP|SC68_CHANGE)) && !trc->allin1);
  pcm <<= 2;
 error:
  if (trc->done)
    DBG("(hdl=%p)"
        " => %s (%u pcms)\n",
        (void*)trc, trc->done<0?"FAILED":"DONE", (unsigned)trc->pcm);
  return pcm;
}

EXPORT
/**
 * Close sc68 transcoder.
 *
 * @param   hdl  Pointer to transcoder context
 */
void winampGetExtendedRead_close(intptr_t hdl)
{
  struct transcon * trc = (struct transcon *) hdl;

  DBG("(hdl=%p)\n", (void *)trc);
  if (trc) {
    sc68_destroy(trc->sc68);
    free(trc);
  }
}