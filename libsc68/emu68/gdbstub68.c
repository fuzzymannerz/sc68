#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if defined(USE_GDBSTUB68)

#include "gdbstub68.h"
#include "emu68.h"
#include "excep68.h"

#include <string.h>

/* $$$ DEBUG */
#if defined (DEBUG)
#include <stdio.h>
#endif

static const char hexchars[]="0123456789abcdef";


typedef struct emu68_gdbbuf_s emu68_gdbbuf_t;
typedef struct emu68_gdb_s emu68_gdb_t;

struct emu68_gdbbuf_s {
  int  cnt;
  int  max;
  u8  *buf;
};

struct emu68_gdb_s {
  emu68_gdbbuf_t  inp;                  /* input buffer  */
  emu68_gdbbuf_t  out;                  /* output buffer */
  emu68_gdbstub_f access;               /* read/write a single char */
};

int emu68_gdbstub_create(emu68_t * const emu68, emu68_gdbstub_f access)
{
  emu68_gdb_t * gdbstub;
  int bytes;

  const int maxinp = 512;
  const int maxout = 512;

  bytes = sizeof (emu68_gdb_t) + maxinp + maxout;

  gdbstub = emu68_alloc(bytes);
  if (gdbstub) {
    gdbstub->inp.cnt = 0;
    gdbstub->inp.max = maxinp;
    gdbstub->inp.buf = (u8 *)(gdbstub+1);

    gdbstub->out.cnt = 0;
    gdbstub->out.max = maxout;
    gdbstub->out.buf = gdbstub->inp.buf + maxinp;

    gdbstub->access  = access;

    emu68->gdb = gdbstub;
  }

  return -!gdbstub;
}

static int get_char(emu68_t * const emu68) {
  return emu68->gdb->access(emu68, -1);
}

static int put_char(emu68_t * const emu68, int c) {
  return emu68->gdb->access(emu68, (u8)c);
}

/* convert a single hexa digit to its value */
static int hex (int ch)
{
  if ((ch >= 'a') && (ch <= 'f'))
    return (ch - 'a' + 10);
  if ((ch >= '0') && (ch <= '9'))
    return (ch - '0');
  if ((ch >= 'A') && (ch <= 'F'))
    return (ch - 'A' + 10);
  return -1;
}

/* convert the memory pointed to by mem into hex, placing result in
 * buf return a pointer to the last char put in buf (null)
 */
static u8 * mem2hex(const u8 * mem, u8 * buf, int count)
{
  int i, ch;
  for (i = 0; i < count; i++) {
    ch = *mem++;
    *buf++ = hexchars[ch >> 4];
    *buf++ = hexchars[ch & 15];
  }
  *buf = 0;
  return buf;
}

/* convert the hex array pointed to by buf into binary to be placed in
 * mem return a pointer to the character AFTER the last byte
 * written
 */
static u8 * hex2mem(const u8 * buf, u8 * mem, int count)
{
  int i, ch;
  for (i = 0; i < count; i++) {
    ch = hex (*buf++) << 4;
    ch = ch + hex (*buf++);
    *mem++ = ch;
  }
  return mem;
}

/* static unsigned int hex2int(const u8 * buf, int count) */
/* { */
/*   int i; */
/*   unsigned int v; */
/*   for (i=v=0; i<count; i++) { */
/*     v = (v << 4) + hex(*buf++);   /\* $$$ assume hexa digit is valid *\/ */
/*   } */
/*   return v; */
/* } */

static unsigned int hex2val(const u8 * buf, int * count)
{
  int i, d;
  unsigned int v = 0;

  for (i=v=0; (d = hex(buf[i])) >= 0; ++i) {
    v = (v<<4) + d;
  }
  *count = i;
  return v;
}

/**
 * Recieve next packet into input buffer.
 *
 * @retval -1 on error
 * @retval  0 on success
 */
static u8 * recv_packet(emu68_t * const emu68)
{
  /* u8   *buffer = emu68->gdb.remcomInBuffer; */
  u8   checksum, xmitcsum;
  int  ch, i;

  /* wait around for the start character, ignore all other characters */

resync:
  i = -1;
  do {
    ++i;
    ch = get_char(emu68);
    if (ch < 0)
      return 0;
    if (ch != '$')
      fprintf(stderr, "recv - sync -- get '%c' instead of '$' pos #%d\n",ch,i);
  } while (ch != '$');
  fprintf(stderr, "recv - synced at pos #%d\n",i);

retry:
  checksum = 0;
  xmitcsum = -1;
  emu68->gdb->inp.cnt = 0;

  emu68->gdb->inp.buf[0] = 0;
  emu68->gdb->inp.buf[1] = 0;
  emu68->gdb->inp.buf[2] = 0;
  emu68->gdb->inp.buf[3] = 0;

  do {
    ch = get_char(emu68);
    if (ch < 0)
      return 0;
    if (ch == '$') {
      fprintf(stderr, "recv - got a '$' at pos %d -- retry\n",
              emu68->gdb->inp.cnt);
      goto retry;
    }
    if (ch == '#')
      break;
    checksum = checksum + ch;
    emu68->gdb->inp.buf[emu68->gdb->inp.cnt++] = ch;
  } while (emu68->gdb->inp.cnt < emu68->gdb->inp.max-1);

  emu68->gdb->inp.buf[emu68->gdb->inp.cnt] = 0;
  fprintf(stderr,"recv - got [%s]+'%c'\n",emu68->gdb->inp.buf,ch);

  if (ch != '#')                        /* Overflow ! */
    return 0;

  ch = get_char(emu68);
  if(ch < 0)
    return 0;
  xmitcsum = hex (ch) << 4;
  ch = get_char (emu68);
  if(ch < 0)
    return 0;
  xmitcsum += hex (ch);

  fprintf(stderr,"recv - xmitsum(%02x) (%02x) chksum\n",
          xmitcsum,checksum);

  if (checksum != xmitcsum) {
    /* failed checksum */
    if (put_char(emu68, '-') < 0)
      return 0;
    goto resync;
  }

  /* successful transfer */
  if (put_char(emu68, '+') < 0)
    return 0;

  /* if a sequence char is present, reply the sequence ID */
  if (emu68->gdb->inp.buf[2] == ':') {
    fprintf(stderr, "recv - got SEQ-ID !\n");
    if (put_char (emu68, emu68->gdb->inp.buf[0]) < 0)
      return 0;
    if (put_char (emu68, emu68->gdb->inp.buf[1]) < 0)
      return 0;
    return emu68->gdb->inp.buf + 3;
  }
  return emu68->gdb->inp.buf;
}


/**
 * Send the packet in buffer.
 *
 * @retval -1 on error
 * @retval  0 on success
 */
static int send_packet(emu68_t * const emu68)
{
  int ch;
  u8  checksum, * out;


  fprintf(stderr,"send - [%s]\n", emu68->gdb->out.buf);

  if (!*emu68->gdb->out.buf) {
    /* don't wait for reply on that ! */
    return 0;
  }


  /*  $<packet info>#<checksum>. */
retry:
  if (put_char (emu68, '$') < 0)
    return -1;

  checksum = 0;
  out = emu68->gdb->out.buf;

  while (ch = *out++, ch) {
    checksum += ch;
    if (put_char(emu68, ch) < 0)
      return -1;
  }

  if (0
      || 0 > put_char(emu68, '#')
      || 0 > put_char(emu68, hexchars[checksum >> 4])
      || 0 > put_char(emu68, hexchars[checksum & 15]))
    return -1;

  if (!*emu68->gdb->out.buf) {
    /* don't wait for reply on that ! */
    return 0;
  }

  if (ch = get_char(emu68), ch < 0)
    return -1;

  if (ch == '+') {
    fprintf(stderr,"send - ack - [%s] %02X\n", emu68->gdb->out.buf, checksum);
    return 0;
  }

  goto retry;

  if (ch == '-') {
    fprintf(stderr,"send - try - [%s]\n", emu68->gdb->out.buf);
    goto retry;
  }

  fprintf(stderr,"send - err - [%s] -> '%c'\n", emu68->gdb->out.buf, ch);
  return -1;
}


/* this function takes the 68000 exception number and attempts to
   translate this number into a unix compatible signal value */
static int computeSignal (int exceptionVector)
{

  static const int vector2signal[] = {
    -1, /* 00 reset sp vector */
    -1, /* 01 reset pc vector */
    10, /* 02 bus error       SIGBUS  */
    10, /* 03 address error   SIGBUS  */
    4,  /* 04 illegal         SIGILL  */
    8,  /* 05 divide by zero  SIGFPE  */
    8,  /* 06 chk             SIGFPE  */
    8,  /* 07 trapv           SIGFPE  */
    11, /* 08 privilege       SIGSEGV */
    5,  /* 09 trace trap      SIGTRAP */
    4,  /* 10 line A          SIGILL  */ /* really ? SIGEMT */
    4   /* 11 line F          SIGILL  */ /* see line-A */
  };
  if (exceptionVector < 0)
    return -1;
  if (exceptionVector < sizeof(vector2signal)/sizeof(*vector2signal))
    return vector2signal[exceptionVector];
  if (exceptionVector >= TRAP_VECTOR(0) &&
      exceptionVector <= TRAP_VECTOR(15))
    return 7; /* ??? */
  if (exceptionVector == HWBREAK_VECTOR)
    return 5; /* SIGTRAP */
  if (exceptionVector == HWTRACE_VECTOR)
    return 5; /* SIGTRAP */

  return -1;
}

/*
 * This function does all command procesing for interfacing to gdb.
 */
int emu68_gdbstub_handle(emu68_t * const emu68, int exceptionVector)
{
  int status = EMU68_NRM;
  int sigval;
  u8  *inp, *out;

  /* reply to host that an exception has occurred */
  sigval = computeSignal(exceptionVector);
  if (sigval == -1)
    sigval = 4;

  out = emu68->gdb->out.buf;
  *out++ = 'S';
  *out++ = hexchars[sigval >> 4];
  *out++ = hexchars[sigval &  15];
  *out  = 0;

  if (-1 == send_packet(emu68))
    return EMU68_ERR;                   /* $$$ Or may be not ? */

  /* emu68->gdb->stepping = 0; */

  for (;;) {
    emu68->gdb->out.buf[0] = 0;
    out = emu68->gdb->out.buf;
    inp = recv_packet(emu68);
    if (!inp)
      return EMU68_ERR;

    switch (*inp++) {

    case '?':
      /* ? -- Get last signal */
      *out++ = 'S';
      *out++ = hexchars[sigval >> 4];
      *out++ = hexchars[sigval % 16];
      *out   = 0;
      break;

    case 'd':
      /* d -- Toggle debug flag */
      /* emu68->gdb->rdebug ^= 1; */
      break;

    case 'g': {
      /* g -- Return the value of the CPU registers. */
      u8 regs[8+8+1+1][4];
      int i;

      /* convert emu68 native endian to m68k big endian */
      for (i = 0; i < 8; ++i) {
        s32 *ptr = emu68->reg.d;
        regs[i][0] = ptr[i] >> 24;
        regs[i][1] = ptr[i] >> 16;
        regs[i][2] = ptr[i] >>  8;
        regs[i][3] = ptr[i];
      }
      /* next is PS (SR) */
      regs[i][0] = 0;
      regs[i][1] = 0;
      regs[i][2] = emu68->reg.sr >> 8;
      regs[i][3] = emu68->reg.sr;
      ++i;
      /* next is PC */
      regs[i][0] = emu68->reg.pc >> 24;
      regs[i][1] = emu68->reg.pc >> 16;
      regs[i][2] = emu68->reg.pc >>  8;
      regs[i][3] = emu68->reg.pc;
      ++i;

      out = mem2hex(&regs[0][0], out, i*4);
    } break;

    case 'G': {
      /* G -- Set the value of the CPU registers. */
      int i;
      for (i = 0; i < 18; ++i) {
        int j, v;
        for (j=v=0; j<8; ++j, v<<=4)
          v += hex(*inp++);
        if (i<16)
          emu68->reg.d[i] = v;
        else if (i == 17)
          emu68->reg.sr = v;
        else
          emu68->reg.pc = v;
      }
      *out++ = 'O';
      *out++ = 'K';
      *out   = 0;
    } break;

    case 'm': case 'M': {
      /* mAA..AA,LLLL -- Read LLLL bytes at address AA..AA */
      /* MAA..AA,LLLL -- Write LLLL bytes at address AA.AA return OK */
      int cnt;
      unsigned int adr, len;
      u8 * mem;
      u8 mode = inp[-1];

      adr = hex2val(inp, &cnt);
      if (cnt && inp[cnt] == ',')
        len = hex2val(inp += cnt + 1, &cnt);
      else
        cnt = 0;
      if (!cnt) {
        *out++ = 'E'; *out++ = '0'; *out++ = '1'; *out = 0;
        break;
      }
      inp += cnt;

      mem = emu68_memptr(emu68, adr, len);
      if (mem) {
        if (mode == 'm')
          out = mem2hex (mem, out, len);
        else {
          hex2mem(inp,mem,len);
          *out++ = 'O'; *out++ = 'K'; *out = 0;
        }
      } else {
        *out++ = 'E'; *out++ = '0'; *out++ = '3'; *out = 0;
        break;
      }
    } break;

    case 's':
      /* sAA..AA   Step one instruction from AA..AA(optional) */
      /* emu68->gdb->stepping = 1; */
      status = EMU68_BRK;
    case 'c': {
      /* cAA..AA    Continue at address AA..AA(optional) */
      int cnt;
      unsigned int adr;

      adr = hex2val(inp, &cnt);
      if (cnt)
        emu68->reg.pc = adr;

      /* emu68->reg.sr &= ~SR_T; */
      /* if (stepping) */
      /*   emu68->reg.sr |= SR_T; */

    } break;

    case 'H': {
      /* H op thread-id : set thread operation*/
      int op;
      const u8 * id;

      op = *inp++;
      id = inp;
      switch (op) {
      case 'c':
        /* continue */
        *out++ = 'O'; *out++ = 'K'; *out = 0;
        break;
      default:
        *out++ = 'E'; *out++ = '0'; *out++ = '1'; *out = 0;
      }
    } break;

    case 'q': {
      /* q name params : general query */

      fprintf(stderr, "Query -- '%s'\n",inp);
      if (!strcmp((char*)inp,"C")) {
        /* current thread-id */
        *out++ = 'Q'; *out++ = 'C'; *out++ = '1'; *out = 0;
        break;
      } else if (!strcmp((char*)inp,"Offsets")) {
        strcpy((char*)out, "Text=0;Data=0");
        break;
      }
      *out++ = 'E'; *out++ = '1'; *out++ = '1'; *out = 0;
    } break;


    case 'k':
      /* k -- kill the program */
      status = EMU68_STP;
      break;
    }

    /* reply to the request */
    /* if (out > emu68->gdb->out.buf) */
    if (send_packet(emu68) < 0)
      return EMU68_ERR;
  }

  return status;
}

#endif /* defined(USE_GDBSTUB68) */
