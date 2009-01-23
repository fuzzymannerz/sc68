/*
 *             sc68 - Paula IO plugin (Amiga soundchip)
 *             Copyright (C) 1999-2007 Benjamin Gerard
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 *
 */

/*$Id$*/

#include "paula_io.h"

typedef struct {
  io68_t io;
  paula_t paula;
} paula_io68_t;

static void reload(paulav_t * const v, const u8 * const p, const int fix);

static inline unsigned int clearset(const unsigned int v,
				    const unsigned int clrset)
{
  if (clrset & 0x8000) {
    return (v | clrset) & 0x7FFF;
  } else {
    return v & ~clrset;
  }
}

static inline int DMACON(const int dmacon) {
  return (-!!(dmacon&(1<<9))) & dmacon & 0xF;
}

static inline int INTREQ(const int intreq) {
  return intreq & (0xF<<7);
}

static inline int INTENA(const int intena) {
  return (-!!(intena&(1<<14))) & intena & (0xF<<7);
}

/* $$$ I am not sure what really happen in case of byte access on the
       Amiga hardware Word registers. Currently I assume a ``normal''
       behavior.
*/

static int68_t _paula_readB(paula_io68_t * const paulaio,
			    addr68_t const addr)
{
  const int i = (u8)addr;
  paula_t * const paula = &paulaio->paula;
  u8 v;

  switch(i) {
  case PAULA_VHPOSR:
    v = paula->vhpos++;
    break;

  case PAULA_DMACONRH:
    v = /* paula->map[PAULA_DMACONRH] = */ (paula->dmacon>>8)&0x7f;
    break;
  case PAULA_DMACONRL:
    v = /* paula->map[PAULA_DMACONRL] = */ (u8)paula->dmacon;
    break;

  case PAULA_INTENARH:
    v = /* paula->map[PAULA_INTENARH] =  */(paula->intena>>8)&0x7f;
    break;
  case PAULA_INTENARL:
    v = /* paula->map[PAULA_INTENARL] =  */(u8)paula->intena;
    break;

  case PAULA_INTREQRH:
    v = /* paula->map[PAULA_INTREQRH] =  */(paula->intreq>>8)&0x7f;
    break;
  case PAULA_INTREQRL:
    v = /* paula->map[PAULA_INTREQRL] =  */(u8)paula->intreq;
    break;

  case PAULA_ADKCONRH:
    v = /* paula->map[PAULA_ADKCONRH] =  */(paula->adkcon>>8)&0x7f;
    break;
  case PAULA_ADKCONRL:
    v = /* paula->map[PAULA_ADKCONRL] =  */(u8)paula->adkcon;
    break;

  default:
    v = paula->map[i];
    break;
  }
  return v;
}

static int68_t _paula_readW(paula_io68_t * const paulaio,
			    addr68_t const addr)
{
  const int i = (u8)addr;
  paula_t * const paula = &paulaio->paula;
  u16 v;

  switch (i) {
  case PAULA_DMACONR:
    v =  paula->dmacon & 0x7fff;
    break;
  case PAULA_INTENAR:
    v =  paula->intena & 0x7fff;
    break;
  case PAULA_INTREQR:
    v =  paula->intreq & 0x7fff;
    break;
  case PAULA_ADKCON:
    v =  paula->adkcon & 0x7fff;
    break;
  default:
    v = (paula->map[i]<<8) | paula->map[i+1];
    break;
  }
  return v;
}

static void paulaio_readB(io68_t * const io)
{
  io->emu68->bus_data =
    _paula_readB((paula_io68_t *)io, io->emu68->bus_addr);
}

static void paulaio_readW(io68_t * const io)
{
  io->emu68->bus_data =
    _paula_readW((paula_io68_t *)io, io->emu68->bus_addr);
}

static void paulaio_readL(io68_t * const io)
{
  io->emu68->bus_data = 0
    |(_paula_readW((paula_io68_t *)io, io->emu68->bus_addr+0)<<16)
    |(_paula_readW((paula_io68_t *)io, io->emu68->bus_addr+2)    )
    ;
}

#if 0
/* start DMA on voice n ( DMA disable -> enable ) */
static void start_DMA(paula_t * const paula, const int bit)
{
  int old,chg,cur;

  old = DMACON(paula->dmacon);
  paula->dmacon |= bit;
  cur = DMACON(paula->dmacon);
  chg = cur & ~old;

  if(chg&1) reload(paula->voice+0, paula->map+PAULA_VOICEA, paula->ct_fix);
  if(chg&2) reload(paula->voice+1, paula->map+PAULA_VOICEB, paula->ct_fix);
  if(chg&4) reload(paula->voice+2, paula->map+PAULA_VOICEC, paula->ct_fix);
  if(chg&8) reload(paula->voice+3, paula->map+PAULA_VOICED, paula->ct_fix);
}
#endif

/* stop DMA bit to be stopped */
static inline void stop_DMA(paula_t * const paula, const int bit)
{
  paula->dmacon &= ~bit;
}

/* Reload paula internal register with current value */
static void reload(paulav_t * v, const u8 * p, const int fix)
{
  int len;

  v->start = v->adr = ((p[1]<<16) | (p[2]<<8) | p[3])<<fix;

  len = (p[4]<<8) | p[5];
  len |= (!len) << 16;
  len <<= 1+fix;
  v->end = v->start + len;
}


/* Write INTREQ :
 *
 * - If clearing bits just release the interrupt. Nothing more to do.
 * - If setting bit checks whether the interrupt is denied or not.
 *   When denied it seems that the internal pointer and length register
 *   are reloaded however its is not an official practice.
 */
static void write_intreq(paula_t * const paula, const int intreq)
{

  if ( !(intreq & 0x8000) ) {
    /* Clearing ... */
    paula->intreq &= ~intreq;
    return;
  } else {
    int intdenied;

    /* Master interrupt not set : DENIED */
    intdenied = ~INTENA(paula->intena);
    /* Already requested : DENIED */
    intdenied |= paula->intreq;
    /* Only interrested by requested bits */
    intdenied &= intreq;
    /* Reload for each denied channel */

    /* $$$ May be should not reload if DMA is OFF $$$ */
    
    if(intdenied & (1<< 7))
      reload(paula->voice+0, paula->map+PAULA_VOICEA, paula->ct_fix);
    if(intdenied & (1<< 8))
      reload(paula->voice+1, paula->map+PAULA_VOICEB, paula->ct_fix);
    if(intdenied & (1<< 9))
      reload(paula->voice+2, paula->map+PAULA_VOICEC, paula->ct_fix);
    if(intdenied & (1<<10))
      reload(paula->voice+3, paula->map+PAULA_VOICED, paula->ct_fix);
    
    paula->intreq |= intreq;
  }
}

static void _paula_writeB(paula_io68_t * const paulaio,
			   addr68_t const addr, const int68_t data)
{
  const int i = (u8)addr;
  paula_t * const paula = &paulaio->paula;
  
  paula->map[i] = data;
  switch (i) {

  case PAULA_INTREQL:
    write_intreq(paula,
		 (paula->map[PAULA_INTREQH]<<8)|paula->map[PAULA_INTREQL]);
    break;

  }
}

static void _paula_writeW(paula_io68_t * const paulaio,
			 addr68_t const addr, const int68_t data)
{
  const int i = (u8) addr;
  paula_t * const paula = &paulaio->paula;
  const int v = (u16) data;

  /* Copy into hw-reg */ 
  paula->map[i] = v >> 8;
  paula->map[(u8)(i+1)] = v;

  switch (i) {
  case PAULA_ADKCON: {
    int old_adkcon = paula->adkcon;
    paula->adkcon = clearset(old_adkcon, v);
    if (paula->adkcon & ~old_adkcon & 0xFF) {
      /* Modulation is active !!! */
    }
  } break;

  case PAULA_DMACON: {
    int old_dmacon = paula->dmacon;
    int old_dmaena = DMACON(old_dmacon);
    int new_dmaena;
    int start;
    
    paula->dmacon = clearset(old_dmacon, v);
    new_dmaena = DMACON(paula->dmacon);
    
    start = new_dmaena & ~old_dmaena;
    
    if(start&1) reload(paula->voice+0,paula->map+PAULA_VOICEA,paula->ct_fix);
    if(start&2) reload(paula->voice+1,paula->map+PAULA_VOICEB,paula->ct_fix);
    if(start&4) reload(paula->voice+2,paula->map+PAULA_VOICEC,paula->ct_fix);
    if(start&8) reload(paula->voice+3,paula->map+PAULA_VOICED,paula->ct_fix);
    
  } break;

  case PAULA_INTENA: {
    int old_intena = INTENA(paula->intena), new_intena;
    old_intena=old_intena;
    paula->intena = clearset(paula->intena, v);
    new_intena = INTENA(paula->intena);
    
    if ( new_intena & ~old_intena ) {
      /*Amiga Audio IRQ enabled */
    }
  } break;
  
  case PAULA_INTREQ:
    write_intreq(paula, v);
    break;
    
  default:
    break;
  }
}

static void paulaio_writeB(io68_t * const io)
{
  _paula_writeB((paula_io68_t *)io,
		io->emu68->bus_addr, io->emu68->bus_data);
}

static void paulaio_writeW(io68_t * const io)
{
  _paula_writeW((paula_io68_t *)io,
		io->emu68->bus_addr, io->emu68->bus_data);
}

static void paulaio_writeL(io68_t * const io)
{
  _paula_writeW((paula_io68_t *)io,
		io->emu68->bus_addr+0, io->emu68->bus_data>>16);
  _paula_writeW((paula_io68_t *)io,
		io->emu68->bus_addr+2, io->emu68->bus_data);
}

static interrupt68_t * paulaio_interrupt(io68_t * const io, cycle68_t cycle)
{
  return 0;
}

static cycle68_t paulaio_next_interrupt(io68_t * const io, cycle68_t cycle)
{
  return IO68_NO_INT;
}

static void paulaio_adjust_cycle(io68_t * const io, cycle68_t cycle)
{
}

static int paulaio_reset(io68_t * const io)
{
  return paula_reset(&((paula_io68_t *)io)->paula);
}

static void paulaio_destroy(io68_t * const io)
{
  if (io) {
    paula_cleanup(&((paula_io68_t *)io)->paula);
  }
}

static io68_t paula_io = {
  0,
  "AMIGA Paula",
  0xFFDFF000, 0xFFDFF0DF,
  paulaio_readB,  paulaio_readW,  paulaio_readL,
  paulaio_writeB, paulaio_writeW, paulaio_writeL,
  paulaio_interrupt, paulaio_next_interrupt,
  paulaio_adjust_cycle,
  paulaio_reset,
  paulaio_destroy
};


io68_t * paulaio_create(emu68_t * const emu68, paula_parms_t * const parms)
{
  paula_io68_t * paulaio = 0;

  if (emu68 && emu68->alloc) {
    paulaio = emu68->alloc(sizeof(*paulaio));
    if (paulaio) {
      paula_setup_t setup;
      if (parms) {
	setup.parms = *parms;
      } else {
	setup.parms.emul  = PAULA_EMUL_DEFAULT;
	setup.parms.hz    = 0;
	setup.parms.clock = PAULA_CLOCK_DEFAULT;
      }
      setup.mem     = emu68->mem;
      setup.log2mem = emu68->log2mem;
      paulaio->io = paula_io;
      paula_setup(&paulaio->paula, &setup);
    }
  }
  return &paulaio->io;
}

int paulaio_init(paula_parms_t * const parms)
{
  return paula_init(parms);
}

void paulaio_shutdown(void)
{
  paula_shutdown();
}

uint68_t paulaio_sampling_rate(io68_t * const io, uint68_t sampling_rate)
{
  return io
    ? paula_sampling_rate(&((paula_io68_t*)io)->paula,sampling_rate)
    : sampling_rate
    ;
}

paula_t * paulaio_emulator(io68_t * const io)
{
  return io
    ? &((paula_io68_t*)io)->paula
    : 0
    ;
}