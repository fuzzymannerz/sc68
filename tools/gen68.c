/*
 *          gen68 - "emu68" 68k instruction code generator
 *            Copyright (C) 1999-2009 Ben(jamin) Gerard
 *           <benjihan -4t- users.sourceforge -d0t- net>
 *
 * This  program is  free  software: you  can  redistribute it  and/or
 * modify  it under the  terms of  the GNU  General Public  License as
 * published by the Free Software  Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 *
 * You should have  received a copy of the  GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id$
 *
 */

/* Time-stamp: <2009-06-12 07:20:25 ben> */
static const char modifdate[] = "2009-06-12 07:20:25";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#undef NDEBUG
#include <assert.h>


static int quiet = 0;
static int debug = 0;

static int error(const char * format, ...)
{
  va_list list;
  va_start(list,format);
  vfprintf(stderr,format,list);
  va_end(list);
  return -1;
}

static void msg(const char * format, ...)
{
  if (!quiet) {
    va_list list;
    va_start(list,format);
    vfprintf(stderr,format,list);
    va_end(list);
  }
}

static void dbg(const char * format, ...)
{
  if (debug) {
    va_list list;
    va_start(list,format);
    vfprintf(stderr,format,list);
    va_end(list);
  }
}

static FILE * output;
static void outf(const char * format, ...)
{
  va_list list;
  va_start(list,format);
  vfprintf(output,format,list);
  va_end(list);
}


#define TAB   "  "
#define TAB2  "    "
#define TAB33 "      "
#define OUTF_ASSERT(T,V) outf("%sassert(%s);\n",(T),(V))

static int Usage(void)
{
  printf(
    "Usage: gen68 [-hvqDV] (T|[0-F])*|all [prefix]\n"
    "\n"
    " 'C' code generator for sc68 project.\n"
    "\n"
    " T     Generate function table\n"
    " 0-F   Generate code for given lines\n"
    " all   Generate all (equiv. 0123456789ABCDEFT)\n"
    "\n"
    " If no prefix was given output is stdout.\n"
    "\n"
    " If prefix is given output is done in file(s)\n"
    " - <prefix>line<X>.c for lines 0 to F\n"
    " - <prefix>table.c for function table\n"
    "\n"
    "Copyright (C) 1999-2009 Benjamin Gerard\n"
    );
  return 1;
}

static int Version(void)
{
  printf("gen68 %s\n",modifdate);
  return 1;
}

static const char tsz[] = "bwlw";
static const char *  suffix_name[4] = {  ".B",  ".W",  ".L", 0 };
static const char * isuffix_name[4] = { "I.B", "I.W", "I.L", 0 };
static const char * qsuffix_name[4] = { "Q.B", "Q.W", "Q.L", 0 };
static const char * asuffix_name[4] = { "A.B", "A.W", "A.L", 0 };

static const char *mask_name[] = {
  "BYTE_MSK", "WORD_MSK", "LONG_MSK", "LONG_MSK"
};
static const char *shift_name[] = {
  "BYTE_FIX", "WORD_FIX", "LONG_FIX", "WORD_FIX"
};
static const char *ae_name[8] = {
  "Dn", "An", "(An)", "(An)+", "-(An)", "d(An)", "d(An,Xi)", "<Ae>"
};
static const char *sae_name[8] = {
  "Dx", "Ax", "(Ax)", "(Ax)+", "-(Ax)", "d(Ax)", "d(Ax,Xi)", "<Ae>"
};
static const char *dae_name[8] = {
  "Dy", "Ay", "(Ay)", "(Ay)+", "-(Ay)", "d(Ay)", "d(Ay,Xi)", "<Ae>"
};

static const char thex[16] = {
  '0','1','2','3','4','5','6','7',
  '8','9','A','B','C','D','E','F'
};
  
/*
static char * cc_name[16] = {
  "T ", "F ", "HI", "LS", "CC", "CS", "NE", "EQ",
  "VC", "VS", "PL", "MI", "GE", "LT", "GT", "LE"
};
*/

/* Operation Code Map */
static const char * line_name[16] = {
  /* 0000 */ "Bit Manipulation/MOVEP/Immediate",
  /* 0001 */ "Move Byte",
  /* 0010 */ "Move Long",
  /* 0011 */ "Move Word",
  /* 0100 */ "Miscellaneous",
  /* 0101 */ "ADDQ/SUBQ/Scc/DBcc/TRAPcc",
  /* 0110 */ "Bcc/BSR/BRA",
  /* 0111 */ "MOVEQ",
  /* 1000 */ "OR/DIV/SBCD",
  /* 1001 */ "SUB/SUBX",
  /* 1010 */ "(Unassigned, Reserved)",
  /* 1011 */ "CMP/EOR",
  /* 1100 */ "AND/MUL/ABCD/EXG",
  /* 1101 */ "ADD/ADDX",
  /* 1110 */ "Shift/Rotate/Bit Field",
  /* 1111 */ "Coprocessor Interface/MC68040 and CPU32 Extensions"
};

/* Convert ID (0 .. 1024) -> id-string
 * $$$ static string inside, use one at a time
 */
static char * to_line_id(int i)
{
  static  char s[8];
  int line,num;

  line = i>>6;
  switch(line) {
  case 0xa: case 0xf:
    num=0;
    break;
  default:
    num = i&63;
  }
  sprintf(s,"%X%02X",line,num);
  return s;
}

static void print_fileheader(const char * name)
{
  outf("/* %s - EMU68 generated code by\n"
       " * gen68 %s\n"
       " * Copyright (C) 1998-2009 Benjamin Gerard\n"
       " *\n"
       " * %cId$\n"
       " */\n"
       "\n", name, modifdate, '$');
}

static void gene_table(char *s, int n)
{
  int i;

  outf("#include \"struct68.h\"\n"
       "\n"
       "#ifndef EMU68_MONOLITIC\n"
       "EMU68_EXTERN linefunc68_t");
  for (i=0; i<n; i++) {
    if (!(i&7)) outf("\n"TAB);
    outf("%s%X%02X%c",s,i>>6,i&63,i==n-1?';':',');
  }
  outf("\n"
       "#endif\n");

  outf("\n"
       "\n"
       "linefunc68_t *%s_func[%d] = \n",s,n);
  outf("{");
  for(i=0; i<n; i++) {
    if ((i&7)==0) outf("\n"TAB);
    outf("%s%s,",s,to_line_id(i));
  }
  outf("\n};\n\n");
}



/* ,------------------------------------------------------------------.
 * | n   : opmode+mode                                                |
 * | sz  : 1,2 -> w,l                                                 |
 * '------------------------------------------------------------------'
 */
static void gene_movea(int n, int sz)
{
  int smode = n & 7, dmode = ( n >> 3 ) & 7;
  const char   c = tsz[sz], C = toupper(c);

  assert(n >= 0 &&  n < 0100);
  assert(sz >= 0 && sz < 3);
  assert(dmode == 1);

  if (sz == 0) {
    /* MOVEA.B is ILLEGAL */
    outf(TAB"ILLEGAL; /* MOVEA.B %s,An */\n", sae_name[smode]);
    OUTF_ASSERT(TAB,"EMU68_BREAK");
    return;
  }

  outf(TAB"/* MOVEA.%c %s,An */\n", C, sae_name[smode]);
  outf(TAB"REG68.a[reg9] = %s", (sz == 1) ? "(u32)(s16) " : "");
  if (smode < 2) {
    outf("REG68.%c[reg0];\n", smode["da"]);
  } else {
    outf("read_EA%c(%d,reg0);\n", C, smode);
  }
}

/* ,------------------------------------------------------------------.
 * | n   : opmode+mode                                                |
 * | s   : name                                                       |
 * | sz  : 0,1,2 -> b,w,l                                             |
 * '------------------------------------------------------------------'
 */
static void gene_move(int n, int sz)
{
  int smode = n & 7, dmode = ( n >> 3 ) & 7;
  const char   c = tsz[sz], C = toupper(c);
  const char * mask  =  mask_name[sz];
  const char * shift = shift_name[sz];

  assert(n >= 0 && n < 0100);
  assert(sz >= 0 && sz < 3);
  assert(dmode != 1);

  sz = 1 << sz; /* 1 2 4 */

  /* Generate header */
  outf(TAB"/* MOVE.%c %s,%s */\n", C, sae_name[smode], dae_name[dmode]);

  outf(TAB"const int68_t a = (int68_t) ");
  switch (smode) {
  case 0: case 1:
    outf("REG68.%c[reg0]", smode["da"]);
    break;
  default:
    outf("read_EA%c(%d,reg0)", C, smode);
    break;
  }
  outf(" << %s;\n", shift);
  outf(TAB"MOVE%c(a);\n", C);

  switch (dmode) {
  case 0:
    /* Destination is Dn */
    outf(TAB"REG68.d[reg9] = ");
    if (sz != 4) {
      outf("( REG68.d[reg9] & %s ) + ", mask);
    }
    outf("( (uint68_t) a >> %s );\n", shift);
    break;

  case 1:
    /* Destination is An */
    assert(0);
    break;

  default:
    /* Destination is <Ae> */
    outf(TAB"write_EA%c(%d, reg9, a >> %s);\n", C, dmode, shift);
    break;
  }
}

/* direction */
enum {
  TO_RN = 0,
  TO_AE = 1
};

/* An as source for AE_RN mode */
enum {
  OK_AN = 0,                         /* <An> allowed as source <Ae> */
  NO_AN = 1                          /* <An>  denied as source <Ae> */
};

/* Immediat mode */
enum {
  NOT_IMM = 0,
  REG_IMM = 1,
  SIZ_IMM = 2
};

/* ,------------------------------------------------------------------.
 * | n        : opmode+mode (hacked)                                  |
 * | s        : name                                                  |
 * | no_an    : what with An as source <Ae> operand                   |
 * | hax_size : 0:normal R=D=S=opmode&3 !0:RDS 3bit each              |
 * | immode   : immediat mode                                         |
 * '------------------------------------------------------------------'
 */

static void gene_any_rn(int n, char * s, int no_an, int hax_size, int immode)
{
  const int store = s[0] != 'C'; /* HAXXX: Don't store result for CMP/CHK */
  int aemode = ( n & 007 ) >> 0;
  int opmode = ( n & 070 ) >> 3;
  int opsize, dir, regtyp, dst_is_an, src_xtend;

  const char * suf;                 /* Name suffix ["","Q","I","A"] */
  const char * src;                 /* src name [Dn,#Q,#B,#W,#L     */

  int src_size, dst_size, res_size;

  dbg("GEN: %03o %6s, nan:%d, osz:%04d, imm:%d\n",
      n, s, no_an, hax_size, immode);

  assert(n >= 0 && n < 0100);
  assert(no_an == OK_AN || no_an == NO_AN);
  assert(immode == NOT_IMM || immode == REG_IMM || immode == SIZ_IMM);

  switch (immode) {
  case NOT_IMM:
    switch ( opmode ) {
    case 00:                              /* .B AE_DN */
    case 01:                              /* .W AE_DN */
    case 02:                              /* .L AE_DN */
      opsize = opmode;
      regtyp = 'd';
      suf    = suffix_name[opsize];
      src    = 0;
      dir    = TO_RN;
      break;

    case 03:                              /* .W AE_AN */
      opsize = 1;
      regtyp = 'a';
      suf    = asuffix_name[opsize];
      src    = 0;
      dir    = TO_RN;
      break;

    case 04:                              /* .B DN_AE */
    case 05:                              /* .W DN_AE */
    case 06:                              /* .L DN_AE */
      opsize = opmode - 4;
      regtyp = 'd';
      suf    = suffix_name[opsize];
      src    = "Dn";
      dir    = TO_AE;
      break;

    case 07:                              /* .L AE_AN */
      opsize = 2;
      regtyp = 'a';
      suf    = asuffix_name[opsize];
      src    = "An";
      dir    = TO_RN;
      break;

    default:
      assert(!"INVALID OPMODE FOR NON IMMEDIAT");
    }
    break;

  case REG_IMM:
    switch ( opmode & 3 ) {
    case 00: case 01: case 02:
      opsize = opmode & 3;
      regtyp = 'q';
      suf    = qsuffix_name[opsize];
      src    = "#Q";
      dir    = TO_AE;
      break;
    default:
      assert(!"INVALID OPMODE FOR QUICK IMMEDIAT");
    }
    break;

  case SIZ_IMM:
    switch ( opmode & 3 ) {
    case 00: case 01: case 02:
      opsize = opmode & 3;
      regtyp = 'i';
      suf    = isuffix_name[opsize];
      src    = "#I";
      dir    = TO_AE;
      break;
    default:
      assert(!"INVALID OPMODE FOR IMMEDIAT");
    }
    break;

  default:
    assert(!"INVALID IMMEDIAT MODE");
  }
  assert(opsize >= 0 && opsize <= 2);
  assert(suf);

  src_size = ( hax_size & 0007 ) ? (hax_size & 0003 ) >> 0 : opsize;
  res_size = ( hax_size & 0700 ) ? (hax_size & 0300 ) >> 6 : src_size;
  dst_size = ( hax_size & 0070 ) ? (hax_size & 0030 ) >> 3 : src_size;

  outf(TAB"/* %s%s ", s, suf);
  dbg ("     %s%s ", s, suf);
  if ( dir == TO_RN ) {
    outf("%s,%cn */\n", ae_name[aemode], toupper(regtyp));
    dbg ("%s,%cn\n", ae_name[aemode], toupper(regtyp));
  } else {
    outf("%s,%s */\n", src, ae_name[aemode]);
    dbg ("%s,%s\n", src, ae_name[aemode]);
  }

  dst_is_an =
    ( dir == TO_RN && regtyp == 'a' )
    ||
    ( dir == TO_AE && aemode == 1 )
    ;

  src_xtend = 0;
  if ( dst_is_an ) {
    dst_size = res_size = 2;
    switch (opsize) {
    case 0:
      dbg("GEN : ILLEGAL (.B not allowed)\n");
      outf(TAB"ILLEGAL; /* .B not allowed */\n");
      OUTF_ASSERT(TAB,"EMU68_BREAK");
      return;
    case 1:
      src_xtend = 1;
      break;
    case 2:
      break;
    default:
      assert(!"OPSIZE SHOULD BE VALID");
      break;
    }
  }

  if ( dir == TO_RN && aemode == 1 && no_an == NO_AN ) {
    dbg("GEN : ILLEGAL (source An not allowed)\n");
    outf(TAB"ILLEGAL; /* source An not allowed */\n");
    OUTF_ASSERT(TAB,"EMU68_BREAK");
    return;
  }

  /* Source */
  outf(TAB"const uint68_t s = ( (int68_t) ");
  switch (dir) {

  case TO_RN:
    /* Source is <Ae> */
    switch (aemode) {
    case 0:                             /* Ae is Dn */
      outf("REG68.d[reg0]");
      break;
    case 1:                             /* Ae is An */
      assert(no_an == OK_AN);
      outf("REG68.a[reg0]");
      break;
    default:                            /* Ae is Memory */
      outf("read_EA%c(%d,reg0)", src_size["BWL?"], aemode);
    }
    break;

  case TO_AE:
    switch (regtyp) {
    case 'd': case 'a':
      /* Source is Dn */
      outf("REG68.%c[reg9]", regtyp);
      break;
    case 'q':
      /* Source is Quick */
      outf("( ( ( reg9 - 1 ) & 7 ) + 1 )");
      break;
    case 'i':
      /* Source is immediat */
      outf(TAB"get_next%c()", opsize["wwl?"]);
      break;
    default:
      assert(!"INVALID source regtype");
    }
    break;
  default:
    assert(!"INVALID dir");
  }
  outf(" << %s )", shift_name[opsize]);

  if (src_xtend) {
    dbg("GENERATE SIGN EXTENSION TO LONG\n");
    outf(" >> 16");
    assert(dst_size == 2);
    assert(res_size == 2);
  }
  outf(";\n");

  /* Destination */
  switch (dir) {
  case TO_RN:
    /* Destination is Rn */
    outf(TAB"      uint68_t d = (int68_t) REG68.%c[reg9]", regtyp);
    break;

  case TO_AE:
    /* Destination is Ae */
    switch (aemode) {
    case 0:                             /* Ae is Dn */
      outf(TAB"      uint68_t d = (int68_t) REG68.d[reg0]");
      break;
    case 1:                             /* Ae is An */
      outf(TAB"      uint68_t d = (int68_t) REG68.a[reg0]");
      break;
    default:                            /* Ae is Memory */
      outf(TAB"const addr68_t l = get_EA%c(%d,reg0);\n",
           dst_size["BWL?"], aemode);
      outf(TAB"      uint68_t d = read_%c(l)", dst_size["BWL?"]);
    }
    break;

  default:
    assert(!"INVALID dir");
  }
  outf(" << %s;\n", shift_name[dst_size]);

  /* Operation */
  if (store) {
    outf(TAB"%s%s%c(d,s,d);\n", s, *suf == 'A' ? "A" : "", opsize["BWL?"]);
  } else {
    outf(TAB"%s%s%c(s,d);\n", s, *suf == 'A' ? "A" : "", opsize["BWL?"]);
    return;
  }

  if (dir == TO_RN || aemode < 2) {
    /* Destination is a register */
    static const char * regs[2][2] = {
      { "d[reg0]" , "d[reg9]" },        /* aemode 0 */
      { "a[reg0]" , "a[reg9]" },        /* aemode 1 */
    };
    const char * reg;
    if (dir == TO_RN) {
      assert(regtyp == 'a' || regtyp == 'd');
      reg = regs[regtyp == 'a'][1];
    } else {
      reg = regs[aemode][0];
    }

    /* Can't be An except for immediat; SEE gene_any_an() */
/*     assert(dir == TO_DN || imm != NOT_IMM || aemode != 1); */

    outf(TAB"REG68.%s = ", reg);

    switch (res_size) {
    case 0: case 1:                     /* BYTE or WORD */
      outf("( REG68.%s & %s ) + ", reg, mask_name[res_size] );
    case 2: case 3:                     /* LONG */
      outf("( d >> %s )", shift_name[res_size]);
    }
    outf(";\n");
  } else {
    /* Destnation is Ae */
    assert(aemode >= 2);                /* ... and not a register */
    outf(TAB"write_%c(l, d >> %s);\n", res_size["BWL?"], shift_name[res_size]);
  }
}

/* static void gene_any_dn(int n, char * s, int sz, int resflag) */
/* { */
/*   gene_any_rn(n,s,sz,resflag,0); */
/* } */

/* static void gene_any_an(int n, char * s, int sz, int resflag) */
/* { */
/*   assert(sz == 1 || sz == 2); */
/*   assert(resflag == 0 || resflag == 2); */
/*   gene_any_rn(n, s, sz, resflag, 1); */
/* } */

/* ,------------------------------------------------------------------.
 * | n   : opmode+mode                                                |
 * | s   : name (ADD,CMP,SUB)                                         |
 * '------------------------------------------------------------------'
 */
  static void gene_any_an(int n, char * s)
{
  const int L = n & 040;
  const int O = L ? 0222 : 0221;
  assert ( (n & 030) == 030 );
#if 1
  gene_any_rn(n, s, OK_AN, O, NOT_IMM);
#else
/* static void gene_any_rn(int n, char * s, int no_an, int opsz, int imm) */

  const int opmode = (n >> 3) & 7;
  const int aemode = n & 7;
  const int sz     = (opmode == 7) + 1; /* .W=1 .L=2 */
  const int c      = tsz[sz], C = toupper(c);
  const int store  = s[0] != 'C';       /* Do not store result for CMP */

  const char * long_shift = shift_name[2];

  assert(n >= 0 && n < 0100);
  assert(opmode == 3 || opmode == 7);
  assert(sz == 1 || sz == 2);

  outf(TAB"/* %sA.%c %s,An */\n", s, C, ae_name[aemode]);

  /* Source */
  outf(TAB"const sint68_t a = ( (sint68_t) ");
  switch (aemode) {
  case 0: case 1:
    /* Source is Rn */
    outf("REG68.%c[reg0] << %s", aemode["da"], shift_name[sz]);
    break;
  default:
    /* Source is <Ae> */
    outf("read_EA%c(%d, reg0) << %s", C, aemode, shift_name[sz]);
    break;
  }
  if (sz == 1) {
    outf(" ) >> 16;\n");                /* Extend Word to Long */
  } else {
    outf(" );\n");
  }
  outf(TAB"%suint68_t b = ( (uint68_t) REG68.a[reg9] << %s );\n",
       store ? "      " : "const ", long_shift);
  outf(TAB"%sA%c(b,a,b);\n", s, C);
  if (store) {
    outf(TAB"REG68.a[reg9] = b >> %s;\n", long_shift);
  }
#endif
}


/* ,------------------------------------------------------------------.
 * |  n       : opmode+mode                                           |
 * |  s       : name                                                  |
 * |  sz      : 0,1,2 -> b,w,l                                        |
 * |  resflag : result is stored                                      |
 * |  regflag : 0:dn 1:an 2:immediat-3-bit 3:imm                      |
 * '------------------------------------------------------------------'
 */
static void gene_any_rn_mem(int n, char * s, int sz, int resflag, int regflag)
{
#if 1
  int imm, nan = NO_AN, osz = 0;

  switch (regflag) {
  case 1:
    assert(0);
  case 0:                               /* Dn / An */
    imm = NOT_IMM;
    nan = NO_AN;
    osz = 0;
    break;

  case 2:                                 /* Quick */
  case 3:                                 /* Immediat */
    n   = ( n & ~070 ) | 040 | (sz << 3); /* Force TO_AE and op size */
    imm = regflag == 2 ? REG_IMM : SIZ_IMM;
    nan = OK_AN;
    osz = 0;
    break;

  default:
    assert(0);
  }
  gene_any_rn(n, s, nan, osz, imm);

#else

  const int  aemode = n & 7;
  const char * m =  mask_name[sz];
  const char * d = shift_name[sz];
  const char
    c = tsz[sz], C = toupper(c),
    r = regflag ? 'a' : 'd',
    q = !(aemode)  ? 'd' : 'a';
  sz = 1<<sz;

  if ((aemode)==1 && sz==1) {
    outf(TAB"ILLEGAL;\n");
    return;
  }

  outf(TAB"/* %s.%c ",s,C);

  /* Destination is memory */
  if ((aemode)>=2) {
    if (regflag>=2) outf("#imm,<Ae> */\n");
    else outf("%cn,<Ae> */\n",r^32);
    outf(TAB"int68_t a,b%s;\n",resflag ? ",s" : "");
    outf(TAB"addr68_t addr;\n");
    /* Source is quick imm */
    if (regflag==2)
      outf(TAB"a = (int68_t)(((reg9-1)&7)+1)<<%s;\n",d);
    /* Source is imm */
    else if (regflag==3)
      outf(TAB"a = get_next%c()<<%s;\n",sz==4 ? 'l' : 'w',d);
    /* Source is register */
    else
      outf(TAB"a = (int68_t)REG68.%c[reg9]<<%s;\n",r,d);

    outf(TAB"addr = get_EA%c(%d,reg0);\n",C,aemode);
    outf(TAB"b = read_%c(addr)<<%s;\n",C,d);
    outf(TAB"%s%c(%sa,b);\n",s,C,resflag ? "s,":"");
    if (resflag) {
      outf(TAB"write_%c(addr,(uint68_t)s>>%s);\n",C,d);
    }
  }
  /* Destination is register */
  else {
    if (regflag>=2) outf("#imm,%cy */\n",q^32);
    else outf("%cx,%cy */\n",r^32,q^32);
    outf(TAB"int68_t a,b%s;\n",resflag ? ",s" : "");

    outf(TAB"a = ( ");
    /* Source is quick imm */
    if (regflag==2) {
      outf("(int68_t) ( ( (reg9 -1 ) & 7 ) + 1 )");
    } else if (regflag==3) {
      outf("get_next%c()",sz==4 ? 'l' : 'w');
    } else {
      /* Source is register */
      outf("(int68_t) REG68.%c[reg9] << %s",r,d);
    }
    outf(" << %s )", d);
    if (q == 'a' && sz == 2) {
      outf(" >> 16");                   /* Sign extend */
    }
    outf(";\n");

    outf(TAB"b = (int68_t)REG68.%c[reg0]",q);

    /* Destination is data register */
    if (q!='a') {
      outf("<<%s;\n",d);
      outf(TAB"%s%c(%sa,b);\n",s,C,resflag ? "s,":"");
    }
    /* Destination is address register */
    else {
      outf(" << LONG_FIX;\n");
      outf(TAB"%sA%c(%sa,b);\n",s,C,resflag ? "s,":"");
    }

    if (resflag) {
      if ( q == 'a' ) {
        outf(TAB"REG68.a[reg0] = (u32) ( s >> LONG_FIX );\n");
      } else {
        outf(TAB"REG68.d[reg0] = (REG68.d[reg0] & %s) + ((uint68_t)s>>%s);\n",
             m,d);
      }
    }
  }

#endif
}

/* ,-----------------------------------------------------------.
 * |                       LINE 9/D                            |
 * `-----------------------------------------------------------'
 */
static void gene_add_sub(int n, int t)
{
  static char s[2][4] = { "ADD","SUB" };
  gene_any_rn(n, s[t], OK_AN, 0, NOT_IMM);
}

static void gene_addx_subx(int n, int t)
{
  static const char s[2][4] = { "ADD","SUB" };
  int   sz = (n>>3)&3;
  const char   c = tsz[sz];
  const char * d = shift_name[sz];
  const char * m =  mask_name[sz];
  sz = 1<<sz;

  outf(TAB"int68_t a,b,s;\n");

  /* addx -(ay),-(ax) */
  if (n&1) {
    outf(TAB"a = read_%c(REG68.a[reg0]-=%d)<<%s;\n",c^32,sz,d);
    outf(TAB"b = read_%c(REG68.a[reg9]-=%d)<<%s;\n",c^32,sz,d);
    outf(TAB"%sX%c(s,a,b);\n",s[t],c^32);
    outf(TAB"write_%c(REG68.a[reg9],(uint68_t)s>>%s);\n",c^32,d);
  }
  /* addx dy,dx */
  else {
    outf(TAB"a = (int68_t)REG68.d[reg0]<<%s;\n",d);
    outf(TAB"b = (int68_t)REG68.d[reg9]<<%s;\n",d);
    outf(TAB"%sX%c(s,a,b);\n",s[t],c^32);
    outf(TAB"REG68.d[reg9] = (REG68.d[reg9] & %s) + ((uint68_t)s>>%s);\n",m,d);
  }
}

static void gene_adda_suba(int n, int t)
{
  char * s = t ? "SUB" : "ADD";
  gene_any_an(n, s);
}

static void gene_line9_D(int n, int t)
{
  outf("DECL_LINE68(line%c%02X)\n{\n",t ? '9' : 'D',n);
  if ((n&030)==030) gene_adda_suba(n,t);
  else if ((n&046)==040 ) gene_addx_subx(n,t);
  else gene_add_sub(n,t);

  outf("}\n\n");
}

/* ,-----------------------------------------------------------.
 * |                        LINE E                             |
 * `-----------------------------------------------------------'
 */

static void gene_tbl_lsl_mem(void)
{
  static char shf_str[][4] = { "AS", "LS", "ROX", "RO" };
  int d;
  int n;
  char c;

  for (d=0; d<2; d++) {
    c = d ? 'L' : 'R';
    for (n=0; n<4; n++) {
      outf("static void %s%c_mem"
           "(emu68_t * const emu68, int reg, int mode)\n{\n", shf_str[n],c);
      outf(TAB"/* %s%c.W <Ae> */\n", shf_str[n], c);
      outf(TAB"const addr68_t l = get_EAW(mode,reg);\n");
      outf(TAB"       int68_t a = read_W(l)<<WORD_FIX;\n");
      outf(TAB"%s%cW(a,a,1);\n",shf_str[n],c);
      outf(TAB"write_W(l,a>>WORD_FIX);\n");
      outf("}\n\n");
    }
  }

  for (d=0; d<2; d++) {
    c = "RL"[d];
    outf("static void (*const lslmem%c_fc[4])"
         "(emu68_t *const,int,int)=\n{\n", c);
    for(n=0; n<4; n++) {
      outf(TAB"%s%c_mem,",shf_str[n], c);
    }
    outf("\n};\n\n");
  }
}

static void gene_any_lsl_mem(int n)
{
  outf(TAB"lslmem%c_fc[reg9&3](emu68,reg0,%d);\n", "RL"[(n>>5)&1], n & 7);
}

static void gene_any_lsl_reg(int n)
{
  static char shf_str[][4] = { "AS", "LS", "ROX", "RO" };
  int sz = (n >> 3) & 3;
  const char   c = tsz[sz];
  const char * m =  mask_name[sz];
  const char * d = shift_name[sz];
  sz = 1 << sz;

  if (n&4) {
    outf(TAB"/* %s%c.%c Dn,Dn */\n", shf_str[n&3], "RL"[(n>>5)&1], c^32);
    outf(TAB"const int d = REG68.d[reg9];\n");
  } else {
    outf(TAB"/* %s%c.%c #d,Dn */\n", shf_str[n&3], "RL"[(n>>5)&1], c^32);
    outf(TAB"const int d = ((reg9-1)&7)+1;\n");
  }
  outf(TAB" uint68_t a = (uint68_t)REG68.d[reg0]<<%s;\n",d);
  outf(TAB"%s%c%c(a,a,d);\n", shf_str[n&3], "RL"[(n>>5)&1], c^32);
  outf(TAB"REG68.d[reg0] = (REG68.d[reg0] & %s) + (a>>%s);\n", m, d);
}

static void gene_lineE(int n)
{
  int sz;

  if (!n) gene_tbl_lsl_mem();

  sz=(n>>3)&3;
  outf("DECL_LINE68(lineE%02X)\n{\n",n);
  if (sz==3) gene_any_lsl_mem(n);
  else gene_any_lsl_reg(n);
  outf("}\n\n");
}

/* ,-----------------------------------------------------------.
 * |                        LINE B                             |
 * `-----------------------------------------------------------'
 */

static void gene_cmp_eor(int n)
{
  if ( n & 040 ) {
    /* EOR */
    gene_any_rn(n, "EOR", NO_AN, 0, NOT_IMM);
  } else {
    /* CMP */
    gene_any_rn(n, "CMP", OK_AN, 0, NOT_IMM);
  }
/*   int sz = (n>>3)&3; */
/*   if (t) */
/*     gene_any_dn_mem(n,s[t],sz,t); */
/*   else */
/*     gene_any_dn(n,s[t],sz,t); */
}

static void gene_cmp_mem(int n)
{
  int sz =  ( n >> 3 ) & 3;
  char C = tsz[sz] ^ 32;
  const char * shift = shift_name[sz];
  sz = 1<<sz;

  assert( (n & 047) == 041 );

  outf(
    TAB"/* CMPM.%c (Ay)+,(Ax)+ */\n"
    TAB"int68_t y0, x9; addr68_t l;\n", C);
  outf(
    TAB"l = (s32) REG68.a[reg0];\n"
    TAB"REG68.a[reg0] = (u32) ( REG68.a[reg0] + %d );\n"
    TAB"y0 = read_%c(l) << %s;\n", sz, C, shift);
  outf(
    TAB"l = (s32) REG68.a[reg9];\n"
    TAB"REG68.a[reg9] = (u32) ( REG68.a[reg9] + %d );\n"
    TAB"x9 = read_%c(l) << %s;\n", sz, C, shift);
  outf(
    TAB"CMP%c(y0,x9);\n", C);
}

static void gene_cmpa(int n)
{
  gene_any_an(n, "CMP");
}

static void gene_lineB(int n)
{
  outf("DECL_LINE68(lineB%02X)\n{\n",n);
  if ( (n & 030) == 030 ) {
    gene_cmpa(n);
  } else if ( ( n & 047 ) == 041 ) {
    gene_cmp_mem(n);
  } else {
    gene_cmp_eor(n);
  }
  outf("}\n\n");
}


/* ,-----------------------------------------------------------.
 * |                       LINE 8&C                            |
 * `-----------------------------------------------------------'
 */

static void gene_abcd_sbcd(int n, char * s)
{
  assert ( (n & 076) == 040 );

  if ( n & 1 ) {
    outf(TAB"/* %s -(Ay),-(Ax) */\n",s);
    outf(TAB"const addr68_t l0 = REG68.a[reg0] = "
         "(u32) ( REG68.a[reg0] - 1 );\n");
    outf(TAB"const addr68_t l9 = REG68.a[reg9] = "
         "(u32) ( REG68.a[reg9] - 1 );\n");
    outf(TAB"int s = read_B(l0);\n");
    outf(TAB"int d = read_B(l9);\n");
    outf(TAB"%sB(d,s,d);\n", s);
    outf(TAB"write_B(l9,d);\n");
  } else {
    outf(TAB"/* %s Dy,Dx */\n",s);
    outf(TAB"int s = (u8) REG68.d[reg0];\n");
    outf(TAB"int d = (u8) REG68.d[reg9];\n");
    outf(TAB"%sB(d,s,d);\n", s);
    outf(TAB"REG68.d[reg9] = (REG68.d[reg9] & 0xFFFFFF00) | d;\n");
  }
}


static void gene_mul_div(int n, char * s)
{
  char name[8];
  int aemode = ( n & 007 );
/*   int opmode = ( n & 070 ); */

  sprintf(name, "%s%c", s, "US"[ ( n >> 5 ) & 1 ]);

  assert( ( n & 030 ) == 030 );

  if (*s == 'M') {
    /* MUL */
    gene_any_rn(aemode | 010, name, NO_AN, 0211, NOT_IMM);
  } else {
    /* DIV */
    gene_any_rn(aemode | 010, name, NO_AN, 0221, NOT_IMM);
  }
}

static void gene_exg(int n)
{
  assert( (n & 040) == 040 );

  switch (n &= 037) {
  case 010:
    outf(TAB"/* EXG Dx,Dy */\n");
    outf(TAB"EXG(reg9,reg0);\n");
    break;
  case 011:
    outf(TAB"/* EXG Ax,Ay */\n");
    outf(TAB"EXG(reg9+8,reg0+8);\n");
    break;
  case 021:
    outf(TAB"/* EXG Dx,Ay */\n");
    outf(TAB"EXG(reg9,reg0+8);\n");
    break;
  default:
    outf(TAB"ILLEGAL; /* EXG op:%03o */\n", n);
    OUTF_ASSERT(TAB,"EMU68_BREAK");
    assert(0);
    break;
  }
}

/* t := [ 0:AND 1:ORR ] */
static void gene_line8_C(int n, int t)
{
  outf("DECL_LINE68(line%c%02X)\n{\n", t["C8"], n);

  switch ( n ) {
  case 050: case 051: case 061:
    if (!t) {
      gene_exg(n);
    } else {
      outf(TAB"ILLEGAL; /* EXG on other line (op:%03o) */\n", n);
    }
    break;
  case 040: case 041:
    gene_abcd_sbcd(n, t ? "SBCD" : "ABCD");
    break;

  default:

    if ( ( n & 030 ) == 030 ) {
      gene_mul_div(n, t ? "DIV" : "MUL");
    } else {
      gene_any_rn(n, t ? "ORR" : "AND", NO_AN, 0, NOT_IMM);
    }
  }
  outf("}\n\n");
}

/* ,-----------------------------------------------------------.
 * |                        LINE 5                             |
 * `-----------------------------------------------------------'
 */

static void gene_dbcc(int n)
{
  const int cclsb = ( n >> 5 ) & 1;     /* LSB of code condition */

  outf(TAB"/* DBcc Dn,<addr> */\n");
  outf(TAB"DBCC(reg0,(reg9<<1)+%d);\n", cclsb);
}

static void gene_scc(int n)
{
  const int cclsb = ( n >> 5 ) & 1;     /* LSB of code condition */

  outf(TAB"/* Scc %s */\n", ae_name[n&7]);
  outf(TAB"const int r = SCC((reg9<<1)+%d);\n", cclsb);
  if ( (n&7) >= 2 )
    outf(TAB"write_EAB(%d,reg0,r);\n", n&7);
  else
    outf(TAB"REG68.d[reg0] = (REG68.d[reg0]&0xFFFFFF00)+r;\n");
}

static void gene_line5(int n)
{
  int sz=(n>>3)&3;
  outf("DECL_LINE68(line5%02X)\n{\n",n);
  if (sz!=3)
    gene_any_rn_mem(n, (n&(1<<5)) ? "SUB" : "ADD", sz, 1, 2);
  else if ((n&7)==1)
    gene_dbcc(n);
  else if ( (n&030) == 030 )
    gene_scc(n);
  else {
    error("internal: line 5 opmode=0%o\n",n);
    outf("#error \"internal: line 5 opmode=0%o\"\n",n);
    outf(TAB"ILLEGAL /* LINE5 op:%03o */\n",n);
  }
  outf("}\n\n");
}

/* ,-----------------------------------------------------------.
 * |                        LINE 0                             |
 * `-----------------------------------------------------------'
 */
static void gene_movep(int n)
{
  int cycle, i;
  int sz   = 2 << ( (n >> 3) & 1 );
  int sens = n & (1 << 4);

  assert( ( n & 047 ) == 041 );

  cycle = 4 * sz; /* 8 or 16 */
  outf(TAB"/* MOVEP.%c %s */\n","WL"[sz>>2],
       !sens ? "d(An),Dn" : "Dn,d(An)");

  outf(TAB"const addr68_t l = REG68.a[reg0] + get_nextw();\n");

  /* Dn -> Mem */
  if (sens) {
    outf(TAB"const uint68_t a = REG68.d[reg9];\n");
    for(i=0; i<sz; i++) {
      outf(TAB"write_B( l + %d, a >> %d);\n", i*2, (sz-i-1)*8);
    }
  }
  /* Mem -> Dn */
  else {
    outf(TAB"      uint68_t a;\n");
    for(i=0; i<sz; i++) {
      outf(TAB"a %c= read_B( l + %d ) << %d;\n",
           i ? '+' : ' ', i*2, (sz-i-1)*8);
    }
    if (sz==2) {
      outf(TAB"REG68.d[reg9] = ( REG68.d[reg9] & ~0xFFFF ) + a;\n");
    } else {
      outf(TAB"REG68.d[reg9] = a;\n");
    }
  }
  outf(TAB"ADDCYCLE(%d);\n",cycle);
}

static void gene_bitop_dynamic(int n)
{
  static char s[4][5] = { "BTST", "BCHG", "BCLR", "BSET" };
  static int cycler[4] = { 2, 4, 6, 4 };
  static int cyclem[4] = { 0, 4, 4, 4 };
  const int t = (n>>3) & 3;             /* type */
  const int dstmode = n & 7;            /* <Ae>+reg0 */
  const int dynamic = (n>>5) & 1;       /* bit is Dn[reg9] */

  assert(n >= 0 && n < 0100);
  assert(dstmode != 1);
  assert(dynamic == 1);

  if (dstmode == 0) {
    outf(TAB"/* %s.L Dx,Dy */\n",s[t]);
    outf(TAB"int68_t   y = REG68.d[reg0];\n");
    outf(TAB"const int x = REG68.d[reg9];\n");
    outf(TAB"%sL(y,y,x);\n",s[t]);
    if (t) {
      outf(TAB"REG68.d[reg0] = (u32) y;\n");
    }
    outf(TAB"ADDCYCLE(%d);\n",cycler[t]);
  } else {
    outf(TAB"/* %s.B Dn,%s */\n",s[t],ae_name[dstmode]);
    outf(TAB"const addr68_t l = get_EAB(%d,reg0);\n",dstmode);
    outf(TAB"      int68_t  y = read_B(l);\n");
    outf(TAB"const int      x = REG68.d[reg9];\n");
    outf(TAB"%sB(y,y,x);\n", s[t]);
    if (t) {
      outf(TAB"write_B(l,y);\n");
    }
    outf(TAB"ADDCYCLE(%d);\n",cyclem[t]);
  }
}

static void gene_bxxx_mem(int t)
{
  static char s[4][5]  = { "BTST", "BCHG", "BCLR", "BSET" };
  static int cyclem[4] = { 0, 4, 4, 4 };

  assert(t>=0 && t<4);

  outf("static inline\nvoid %s_mem"
       "(emu68_t * const emu68, const int bit, int mode, int reg0)\n{\n",
       s[t]);
  outf(TAB"/* %s.B #b,<Ae> */\n",s[t]);
  outf(TAB"addr68_t addr = get_eab68[mode](emu68,reg0);\n");
  outf(TAB"int68_t a = read_B(addr);\n");
  outf(TAB"%sB(a,a,bit);\n",s[t]);
  if (t) outf(TAB"write_B(addr,a);\n"); /* $$$ should write ???  */
  outf(TAB"ADDCYCLE(%d);\n",cyclem[t]);
  outf("}\n\n");
}

static void gene_bxxx_reg(int t)
{
  static char s[4][5] = { "BTST", "BCHG", "BCLR", "BSET" };
  static int cycler[4] = { 2, 4, 6, 4 };

  assert(t>=0 && t<4);

  outf("static inline\nvoid %s_reg"
       "(emu68_t * const emu68, const int bit, int reg0)\n{\n",s[t]);
  outf(TAB"/* %s.L #b,Dn */\n",s[t]);
  outf(TAB"int68_t a = REG68.d[reg0];\n");
  outf(TAB"%sL(a,a,bit);\n",s[t]);
  if (t) outf(TAB"REG68.d[reg0] = (u32) a;\n");
  outf(TAB"ADDCYCLE(%d);\n",cycler[t]);
  outf("}\n\n");
}

static void gene_line0_mix(int n)
{
  static char s[4][5] = { "BTST", "BCHG", "BCLR", "BSET" };
  const int dstmode = n & 7;
  const int t       = ( n >> 3 ) & 3;   /* type            */
  const int dynamic = (n>>5) & 1;       /* bit is Dn[reg9] */

  assert(n >= 0 && n < 0100 && (n&63) < 32);
  assert(dynamic == 0);

  outf(TAB"if (reg9 == 4) {\n");
  switch (dstmode) {
  case 0:
    outf(TAB2"const int bit = get_nextw();\n");
    outf(TAB2"%s_reg(emu68, bit, reg0);\n",s[t]);
    break;
  case 1:
    outf(TAB2"ILLEGAL; /* %s #xx,An (op:%032o) */\n", s[t], n);
    OUTF_ASSERT(TAB2,"EMU68_BREAK");
    break;
  default:
    outf(TAB2"const int bit = get_nextw();\n");
    outf(TAB2"%s_mem(emu68, bit, %d, reg0);\n", s[t], dstmode);
    break;
  }
  outf(TAB"} else {\n");
  outf(TAB2"line0_imm[reg9][%d](emu68,reg0);\n", n & 63);
  outf(TAB"}\n");
}

static void gene_imm_sr(int n, int sz)
{
/*  outf("static void l0_%s%s(int reg0)\n{\n",s,t ? "CCR" : "SR" );*/

  static const char * iname [] = { "ORR", "AND", "EOR", "???" };
  static const char * idest [] = { "CCR", "SR" };


  outf(TAB"if (reg0==4) { /* %s TO %s */\n",iname[n&3], idest[!!sz]);
  outf(TAB2"uint68_t a;\n");

  switch(n)
  {
  case 0: /* ORR */
    if (!sz) outf(TAB2"a = get_nextw()&255;\n");
    else     outf(TAB2"a = get_nextw();\n");
    outf(TAB2"REG68.sr |= a;\n");
    break;

  case 1: /* AND */
    if (!sz) outf(TAB2"a = get_nextw()|0xFF00;\n");
    else     outf(TAB2"a = get_nextw();\n");
    outf(TAB2"REG68.sr &= a;\n");
    break;

  case 2: /*EOR */
    if (!sz) outf(TAB2"a = get_nextw()&255;\n");
    else     outf(TAB2"a = get_nextw();\n");
    outf(TAB2"REG68.sr ^= a;\n");
    break;
  }
  outf(TAB"} else {\n");

  switch(n)
  {
  case 0:
    gene_any_rn_mem(7,"ORR",sz,1,3);
    break;
  case 1:
    gene_any_rn_mem(7,"AND",sz,1,3);
    break;
  case 2:
    gene_any_rn_mem(7,"EOR",sz,1,3);
    break;
  }
  outf(TAB"}\n");
}

static void gene_imm_op(int n, char *s, int sz, int op)
{
  if ((n&7)==1) return;

  outf("static void l0_%s%c%d(emu68_t * const emu68, int reg0)\n{\n",
       s,tsz[sz],n);
  if ((n&7)!=7 || op>2 || sz==2)
    gene_any_rn_mem(n,s,sz,op!=5,3);
  else
    gene_imm_sr(op,sz);

  outf("}\n\n");
}


static void gene_imm_illegal(void)
{
  outf("static void l0_ill(emu68_t * const emu68, int reg0)\n{\n");
  outf(TAB"reg0 = reg0;\n");
  outf(TAB"ILLEGAL;\n");
  outf("}\n\n");
}


static void gene_line0(int n)
{
  int i;
  static char ts[8][5] = {"ORR","AND","SUB","ADD","???","EOR","CMP","???"};

  if (!n) {
    for(i=0; i<4; i++) {
      gene_bxxx_reg(i);
      gene_bxxx_mem(i);
    }

    gene_imm_illegal();

    for (i=0; i<8; i++) {
      int j;
      for (j=0; j<3; j++) {
        gene_imm_op(i, "ORR", j, 0);
        gene_imm_op(i, "AND", j, 1);
        gene_imm_op(i, "EOR", j, 2);
        gene_imm_op(i, "ADD", j, 3);
        gene_imm_op(i, "SUB", j, 4);
        gene_imm_op(i, "CMP", j, 5);
      }
    }

    outf("static void (*const line0_imm[8][32])"
         "(emu68_t * const emu68, int) =\n{\n");
    for (i=0; i<8; i++) {
      int j;
      char *s;
      s = ts[i];
      outf("/* %s */\n  {\n",s);
      for (j=0;j<32; j++)
      {
        char c;

        if ((j&7)==0) outf(TAB);
        c = tsz[j>>3];
        outf("l0_");
        switch(i)
        {

        case 0: /* ORR */
        case 1: /* AND */
        case 5: /* EOR */
          /*if (j==007)
            outf("%sCCR,",s);
            else if (j==017)
            outf("%sSR,",s);
            else */if ((j&030)==030 || (j&7)==1)
            outf("ill,");
          else
            outf("%s%c%d,",s,c,j&7);
          break;

        case 2: /* SUB */
        case 3: /* ADD */
        case 6: /* CMP */
          if ((j&030)==030 || (j&7)==1)
            outf("ill,");
          else
            outf("%s%c%d,",s,c,j&7);
          break;

        default:
          outf("ill,");
          break;
        }
        if ((j&7)==7) outf("\n");
      }
      outf("\n  },\n");
    }
    outf("};\n\n");
  }
  outf("DECL_LINE68(line0%02X)\n{\n",n);
  if ( (n & 047) == 041 )
    gene_movep(n);
  else if ( (n & 040) == 040 )
    gene_bitop_dynamic(n);
  else
    gene_line0_mix(n);
  outf("}\n\n");
}

/* ,-----------------------------------------------------------.
 * |                      LINE 1/2/3                           |
 * `-----------------------------------------------------------'
 */


static void gene_line1_2_3(int n, int sz, int line)
{
  const int dmode = ( n >> 3 ) & 7;

  outf("DECL_LINE68(line%d%02X)\n{\n", line, n);

  switch (dmode) {
  case 1:
    gene_movea(n, sz); break;
  default:
    gene_move(n, sz);
  }
  outf("}\n\n");
}

/* ,-----------------------------------------------------------.
 * |                        LINE 6                             |
 * `-----------------------------------------------------------'
 */
static void gene_bcc(int n)
{
  const int hdep  = (signed char)((n&31)<<3); /* MSB of displacement   */
  const int cclsb = (n>>5)&1;                 /* LSB of code condition */

  outf(TAB"/* Bcc or BSR */\n");
  outf(TAB"uint68_t pc = REG68.pc;\n");

  /* Possible jmp word */
  if (!hdep) {
    outf(TAB"if (!reg0)\n");
    outf(TAB2"pc += get_nextw(); /* .W */\n");
    outf(TAB"else\n");
    outf(TAB2"pc += reg0;        /* .B */\n");
  }
  /* only jmp short */
  else {
    outf(TAB"pc += reg0%+4d;      /* .B */\n", hdep);
  }
  outf(TAB"BCC(pc,(reg9<<1)+%d);\n", cclsb);
}

static void gene_line6( int n )
{
  outf("DECL_LINE68(line6%02X)\n{\n",n);
  gene_bcc(n);
  outf("}\n\n");
}

/* ,-----------------------------------------------------------.
 * |                        LINE 7                             |
 * `-----------------------------------------------------------'
 */
static void gene_any_moveq(int n)
{
  int v = (n&31) << 3;
  assert(n >= 0 && n < 0100 && ( ( n>>5 ) & 1 ) == 0);

  outf(TAB"/* MOVEQ #N,Dn [%d..%d] */\n", (signed char)v, (signed char)(v+7));
  outf(TAB"int68_t a = reg0%+5d;\n", (signed char)v);
  outf(TAB"REG68.d[reg9] = (u32)a;\n");
  outf(TAB"MOVEL(a);\n");
}

static void gene_line7( int n )
{
  outf("DECL_LINE68(line7%02X)\n{\n",n);
  switch ( (n >> 5) & 1 ) {
  case 0:
    gene_any_moveq(n);
    break;
  case 1:
    outf(TAB"ILLEGAL; /* op:%03o */\n",n);
    OUTF_ASSERT(TAB,"EMU68_BREAK");
    break;
  }
  outf("}\n\n");
}

/* ,-----------------------------------------------------------.
 * |                       LINE A/F                            |
 * `-----------------------------------------------------------'
 */

/* t=1 -> lineA  t=0 -> lineF  */
static void gene_lineA_F(int n, int t)
{
  if (!n) {
    outf("DECL_LINE68(line%c%02X)\n{\n", "FA"[t], n);
    outf(TAB"LINE%c;\n", "FA"[t]);
    outf("}\n\n");
  }
}

/* ,-----------------------------------------------------------.
 * |                        LINE 4                             |
 * `-----------------------------------------------------------'
 */
static void gene_chk(int n)
{
  int opmode = ( n & 070 );
  int aemode = ( n & 007 );

  assert ( opmode == 060 || opmode == 040 );

  if ( opmode == 040 ) {
    /* CHK.L */
    outf("#ifndef EMU68_68020\n");
    outf(TAB"ILLEGAL; /* CHK.L op:%03o */\n", n);
    outf("#else\n");
    gene_any_rn(aemode|020, "CHK", NO_AN, 0, NOT_IMM);
    outf("#endif\n");
  } else {
    gene_any_rn(aemode|010, "CHK", NO_AN, 0, NOT_IMM);
  }
}

static void gene_lea(int n)
{
  int mode = n & 7;

  outf(TAB"/* LEA %s,An */\n", ae_name[mode]);

  switch (mode) {
  case 2:
    outf(TAB"REG68.a[reg9] = (u32) REG68.a[reg0];\n");
    break;
  case 5:
    outf(TAB"REG68.a[reg9] = (u32) ( REG68.a[reg0] + get_nextw() );\n");
    break;
  case 6: case 7:
    outf(TAB"REG68.a[reg9] = (u32) get_eal68[%d](emu68,reg0);\n",mode);
    break;
  default:
    outf(TAB"ILLEGAL; /* LEA %s,An */\n", ae_name[mode]);
    OUTF_ASSERT(TAB,"EMU68_BREAK");
    return;
  }
}

static void gene_movefromsr(void)
{
  outf(TAB"/* MOVE FROM SR */\n");
  outf(TAB"if (mode)\n");
  outf(TAB2"write_W(get_eaw68[mode](emu68,reg0),REG68.sr);\n");
  outf(TAB"else\n");
  outf(TAB2"REG68.d[reg0] = (REG68.d[reg0]&0xFFFF0000) + (u16)REG68.sr;\n");
}

static void gene_movetosr(void)
{
  outf(TAB"/* MOVE TO SR */\n");
  outf(TAB"if (mode)\n");
  outf(TAB2"REG68.sr = read_W(get_eaw68[mode](emu68,reg0));\n");
  outf(TAB"else\n");
  outf(TAB2"REG68.sr = (u16)REG68.d[reg0];\n");
}

static void gene_moveccr(void)
{
  outf(TAB"/* MOVE TO CCR */\n");
  outf(TAB"if (mode)\n");
  outf(TAB2"SET_CCR(REG68.sr,read_W(get_eaw68[mode](emu68,reg0)));\n");
  outf(TAB"else\n");
  outf(TAB2"SET_CCR(REG68.sr,REG68.d[reg0]);\n");
}

static void gene_pea_swap(void)
{
  outf(TAB"if (!mode) {\n");
  outf(TAB2"/* SWAP */\n");
  outf(TAB2"SWAP(reg0);\n");
  outf(TAB"} else {\n");
  outf(TAB2"/* PEA */\n");
  outf(TAB2"pushl(get_eal68[mode](emu68,reg0));\n");
  outf(TAB"}\n");
}

static void gene_movemmem_ext(int sz)
{
  sz = "WL"[sz];
  outf(TAB"if (!mode) {\n");
  outf(TAB2"/* EXT.%c Dn */\n", sz);
  if (sz == 'W') {
    outf(TAB2"const int68_t d = (int68_t) (s8) REG68.d[reg0] << WORD_FIX;\n");
    outf(TAB2"EXTW(d);\n");
    outf(TAB2"REG68.d[reg0] &= 0xFFFF0000;\n");
    outf(TAB2"REG68.d[reg0] |= (uint68_t) d >> WORD_FIX;\n");
  } else {
    outf(TAB2"const int68_t d = (int68_t) (s16) REG68.d[reg0] << LONG_FIX;\n");
    outf(TAB2"EXTL(d);\n");
    outf(TAB2"REG68.d[reg0] = d>>LONG_FIX;\n", sz);
  }
  outf(TAB"} else {\n");
  outf(TAB2"/* MOVEM.%c REGS,<AE> */\n", sz);
  outf(TAB2"movemmem%c(emu68, mode, reg0);\n", sz^32);
  outf(TAB"}\n");
}

static void gene_movemreg( int sz )
{
  sz = "WL"[sz];
  outf(TAB"/* MOVEM.%c <AE>,REGS */\n", sz);
  outf(TAB"movemreg%c(emu68,mode,reg0);\n", sz^32);
}

static void gene_movemregfunc( int sz )
{
  char c = "wl"[sz];
  sz = ( sz + 1 ) * 2;
  outf("static void movemreg%c"
       "(emu68_t * const emu68, int mode, int reg0)\n",c);
  outf("{\n");
  outf(TAB"uint68_t m = (u16) get_nextw(), addr;\n");
  outf(TAB"s32 * r = REG68.d;\n");
  outf(TAB"addr = get_ea%c68[mode](emu68,reg0);\n",c);
  outf(TAB"for(; m; r++, m>>=1)\n");
  outf(TAB2"if ( m & 1 ){ *r = read_%c(addr); addr += %d; }\n", c^32, sz);
  outf(TAB"if ( mode == 3 ) REG68.a[reg0] = addr;\n");
  outf("}\n\n");
}

static void gene_movemmemfunc( int sz )
{
  char c = "wl"[sz];
  sz = ( sz + 1 ) * 2;
  outf("static void movemmem%c"
       "(emu68_t * const emu68, int mode, int reg0)\n",c);
  outf("{\n");
  outf(TAB"uint68_t m = (u16)get_nextw(), addr;\n");
  outf(TAB"if (mode==4) {\n");
  outf(TAB2"s32 * r = REG68.a+7;\n");
  outf(TAB2"addr = get_ea%c68[3](emu68,reg0);\n",c);
  outf(TAB2"for(; m; r--, m>>=1)\n");
  outf(TAB33"if (m&1) write_%c(addr-=%d,*r);\n",c^32,sz);
  outf(TAB2"REG68.a[reg0] = addr;\n");
  outf(TAB"} else {\n");
  outf(TAB2"s32 * r = REG68.d;\n");
  outf(TAB2"addr = get_ea%c68[mode](emu68,reg0);\n",c);
  outf(TAB2"for(; m; r++, m>>=1)\n");
  outf(TAB33"if (m&1) { write_%c(addr,*r); addr+=%d; }\n",c^32,sz);
  outf(TAB"}\n");
  outf("}\n\n");
}

static void gene_tas_illegal(void)
{
  outf(TAB"if (mode<2) {\n");
  outf(TAB2"/* TAS.B Dn */\n");
  outf(TAB2"int68_t a = (int68_t) REG68.d[reg0];\n");
  outf(TAB2"TASB(a,a);\n");
  outf(TAB2"REG68.d[reg0] = a;\n");
  outf(TAB"} else {\n");
  outf(TAB2"if ( mode == 7 && reg0 > 1 ) {\n");
  outf(TAB33"ILLEGAL;\n");
  outf(TAB2"} else {\n");
  outf(TAB33"/* TAS.B <Ae> */\n");
  outf(TAB33"const addr68_t l = get_eab68[mode](emu68,reg0);\n");
  outf(TAB33"       int68_t a = read_B(l);\n");
  outf(TAB33"TASB(a,a);\n");
  outf(TAB33"write_B(l,a);\n");
  outf(TAB2"}\n");
  outf(TAB"}\n");
}

static void gene_jmp(int s)
{
  outf(TAB"/* J%s <Ae> */\n", s ? "MP" : "SR");
  outf(TAB"const addr68_t pc = get_eal68[mode](emu68,reg0);\n");
  outf(TAB"J%s(pc);\n", s ? "MP" : "SR");
}

/* flags : bit 0:no read bit 1:no write */
static void gene_implicit(char * s, int sz, int flags)
{
  const char   c = tsz[sz], C = toupper(c);
  const char * d = shift_name[sz];
  const char * m =  mask_name[sz];

  assert(sz >= 0 && sz < 3);
  assert(flags >= 0 && flags < 3);

  sz = 1 << sz;

  outf(TAB"uint68_t b;\n\n");
  outf(TAB"if ( !mode ) {\n"); {
    outf(TAB2"/* %s.%c Dn */\n", s, C);
    if ( ! (flags & 1) ) {
      outf(TAB2"b = (uint68_t) REG68.d[reg0] << %s;\n", d);
    }
    outf(TAB2"%s%c(b,b);\n", s, C);
    if ( ! (flags & 2) ) {
      outf(TAB2"REG68.d[reg0] = ");
      if (sz != 4) {
        outf("( REG68.d[reg0] & %s ) + ", m);
      }
      outf("( b >> %s );\n",  d);
    }
  } outf(TAB"} else {\n"); {
    OUTF_ASSERT(TAB2,"mode != 1");
    outf(TAB2"/* %s.%c <Ae> */\n", s, C);
    outf(TAB2"const addr68_t addr = get_ea%c68[mode](emu68,reg0);\n", c);
    if ( ! (flags & 1) ) {
      outf(TAB2"b = read_%c(addr) << %s;\n", C, d);
    }
    outf(TAB2"%s%c(b,b);\n", s, C);
    if ( ! (flags & 2) ) {
      outf(TAB2"write_%c(addr, b >> %s);\n", C, d);
    }
  }
  outf(TAB"}\n");
}

static void gene_funky4_mode6( void)
{
  int reg0;
  for(reg0=0; reg0<8;reg0++)
  {
    outf("static void funky4_m6_%d(emu68_t * const emu68)\n"
         "{\n", reg0);
    switch(reg0)
    {
    case 0: outf(TAB"RESET;\n");   break;
    case 1: outf(TAB"NOP;\n");     break;
    case 2: outf(TAB"STOP;\n");    break;
    case 3: outf(TAB"RTE;\n");     break;
    case 4: outf(TAB"ILLEGAL;\n"); break;
    case 5: outf(TAB"RTS;\n");     break;
    case 6: outf(TAB"TRAPV;\n");   break;
    case 7: outf(TAB"RTR;\n");     break;
    }
    outf("}\n\n");
  }
  outf("\n"
       "static void (*const funky4_m6_func[8])(emu68_t * const) = {");
  for ( reg0 = 0; reg0 < 8; ++reg0) {
    if ( ! (reg0 % 4) ) outf("\n"TAB);
    outf("funky4_m6_%d,",reg0);
  }
  outf("\n};\n\n");
}

static void gene_funky4_mode(void)
{
  int mode;

  for ( mode = 0; mode < 8; ++mode )
  {
    outf("static void funky4_m%d(emu68_t * const emu68, int reg0)\n{\n",
         mode);
    switch(mode)
    {
    case 0: case 1:
      outf(TAB"const int a = ( %d << 3 ) + reg0;\n",mode);
      outf(TAB"TRAP(a);\n");
      break;
    case 2:
      outf(TAB"LINK(reg0);\n");
      break;
    case 3:
      outf(TAB"UNLK(reg0);\n");
      break;
    case 4:
      outf(TAB"REG68.usp = REG68.a[reg0];\n");
      break;
    case 5:
      outf(TAB"REG68.a[reg0] = REG68.usp;\n");
      break;
    case 6:
      outf(TAB"funky4_m6_func[reg0](emu68);\n");
      break;
    case 7:
      outf(TAB"ILLEGAL;\n");
    }
    outf("}\n\n");
  }

  outf("static void (* const funky4_func[8])(emu68_t * const, int) = {");
  for ( mode = 0; mode < 8; ++mode) {
    if ( ! (mode % 4) ) outf("\n"TAB);
    outf("funky4_m%d,",mode);
  }
  outf("\n};\n\n");
}

static void gene_funky4(void)
{
  outf(TAB"funky4_func[mode](emu68,reg0);\n");
}

static void gene_line4(int n)
{
  if ( ! n ) {
    /* */
    int i,j,r,s;

    gene_funky4_mode6();
    gene_funky4_mode();

    gene_movemregfunc( 0 );
    gene_movemregfunc( 1 );
    gene_movemmemfunc( 0 );
    gene_movemmemfunc( 1 );

    for ( r = 0; r < 8; r++ ) {
      for ( s = 0; s < 4; s++ ) {
        outf("static void line4_r%d_s%d"
             "(emu68_t * const emu68, int mode, int reg0)\n{\n", r, s);
        switch (r) {
        case 0:
          if ( s == 3 )
            gene_movefromsr();
          else
            gene_implicit("NEGX", s, 0);
          break;

        case 1:
          if ( s == 3 )
            outf(TAB"ILLEGAL;\n");
          else
            gene_implicit("CLR", s, 1);
          break;

        case 2:
          if ( s == 3 )
            gene_moveccr();
          else
            gene_implicit("NEG", s, 0);
          break;

        case 3:
          if ( s == 3 )
            gene_movetosr();
          else
            gene_implicit("NOT",s,0);
          break;

        case 4:
          if ( s == 0 )
            gene_implicit("NBCD",s,0);
          else if ( s == 1 )
            gene_pea_swap();
          else
            gene_movemmem_ext( s - 2 );
          break;

        case 5:
          if ( s == 3 )
            gene_tas_illegal();
          else
            gene_implicit("TST",s,2);
          break;

        case 6:
          if ( s >= 2 )
            gene_movemreg(s-2);
          else
            outf(TAB"ILLEGAL;\n");
          break;

        case 7:
          if ( s == 0 )
            outf(TAB"ILLEGAL;\n");
          else if ( s == 1 )
            gene_funky4();
          else
            gene_jmp( s - 2);
          break;
        }
        outf("}\n\n");
      }
    }

    for ( i = 0; i < 4; ++i ) {
      outf("DECL_STATIC_LINE68((* const line4_%d_func[8])) = {", i);
      for ( j = 0; j < 8; ++j ) {
        if ( ! (j % 4) ) outf("\n"TAB);
        outf("line4_r%d_s%d,", j, i);
      }
      outf("\n};\n\n");
    }
  }

  outf("DECL_LINE68(line4%02X)\n{\n", n);
  if ( n & 040 ) {
    switch ( n & 070 ) {
    case 040:                           /* CHK.L */
    case 060:                           /* CHK.W */
      gene_chk(n);
      break;
    case 070:                           /* LEA   */
      gene_lea(n);
      break;
    case 050:
      outf(TAB"ILLEGAL; /* line:4 op:050 */\n");
      OUTF_ASSERT(TAB,"EMU68_BREAK");
      break;
    default:
      assert(!"UNEXPECTED ERROR");
    }
  } else {
    outf(TAB"line4_%d_func[reg9](emu68,%d,reg0);\n",(n&070)>>3,n&7);
  }
  outf("}\n\n");
}

/*  Create a filename from prefix + name,
 *  @warning return a static string
 */
static char * output_fname(char *prefix, char *name)
{
  static char fname[4096];
  snprintf(fname,sizeof(fname),"%s%s",prefix,name);
  fname[sizeof(fname)-1] = 0;
  return fname;
}

static int outfile(char * prefix, char * name)
{
  char * outname;

  if (!prefix) {
    output = stdout;
    print_fileheader(name);
  } else if (!name) {
    error("internal error: null pointer\n");
    return -1;
  } else {
    outname = output_fname(prefix,name);
    if (output = fopen(outname,"wt"), !output) {
      perror(outname);
      return -1;
    }
    print_fileheader(name);
  }
  return 0;
}

int main(int na, char **a)
{
  int l, i, linetogen  = 0;
  char * prefix     = NULL;
  FILE * savestdout = stdout;

  if ( na < 2 ) {
    return Usage();
  }

  for ( i=1; i<na && a[i][0] == '-'; i++ ) {
    if (a[i][1] != '-') {
      /* Short Options */
      int j;
      for (j=1; a[i][j]; ++j) {
        switch (a[i][j]) {
        case 'h':
          return Usage();
        case 'q':
          quiet = 1; break;
        case 'v':
          quiet = 0; break;
        case 'D':
          debug = 1; break;
        case 'V':
          return Version();
        default:
          return error("gen68: invalid option `-%c'\n", a[i][j]);
        }
      }
    } else {
      if ( ! a[i][2] ) {
        /* `--' */
        ++i; break;
      } else if ( !strcmp("--help",a[i]) || !strcmp("--usage",a[i]) ) {
        return Usage();
      } else if ( !strcmp("--version",a[i]) ) {
        return Version();
      } else if ( !strcmp("--verbose",a[i]) ) {
        quiet = 0;
      } else if ( !strcmp("--quiet",a[i]) ) {
        quiet = 1;
      } else if ( !strcmp("--debug",a[i]) ) {
        debug = 1;
      } else
        return error("gen68: invalid option `%s'\n", a[i]);
    }
  }

  /* What to generate */
  if ( i < na ) {
    if ( !strcmp(a[i], "all") ) {
      linetogen = 0x1FFFF;
    } else {
      char * s;
      for ( s = a[i]; *s; s++ ) {
        if ( ( *s>='0' && *s<='9' ) )  linetogen |= 1<<(*s-'0');
        else if ((*s>='a' && *s<='f')) linetogen |= 1<<(*s-'a'+10);
        else if ((*s>='A' && *s<='F')) linetogen |= 1<<(*s-'A'+10);
        else if ((*s=='T' || *s=='t')) linetogen |= 1<<16;
        else return
               error("gen68: parameter `%s'; xdigit or 't' expected\n", a[i]);
      }
    }
    ++i;
  }

  /* Prefix */
  if ( i < na ) {
    prefix = a[i++];
  }

  if ( !linetogen ) {
    msg("nothing to generate.\n");
    return 0;
  }
  if ( prefix ) {
    msg("prefix is `%s'\n", prefix);
  } else {
    msg("output to stdout\n");
  }

  for ( l = 0; l < 17; ++l ) {
    static int first = 1;
    static char fline[] = "lineX.c";
    char * fname   = 0;

    if ( ! ( linetogen & ( 1 << l ) ) ) continue;

    if ( l == 16 ) {
      msg("Generating instruction table ...\n");
      fname = "table.c";
    } else {
      msg("Generating line %X ...\n", l);
      fname = fline;
      fname[4] = thex[l];
    }

    if ( outfile( prefix, fname ) )
      return 10+l;

    if ( first && !prefix ) {
      int i;
      outf("/*\n");
      for ( i = 0; i < 16; ++i )
        if ( linetogen & ( 1 << i ) )
          outf(" * Line %X: %s\n", i, line_name[i]);
      if ( linetogen & ( 1 << i ) )
        outf(" * Table\n");
      outf(" */\n\n");
      first = 0;
    }

    outf("/* Line %X: %s */\n\n", l, line_name[l]);

    for ( i=0; i<64; i++ ) {
      switch (l) {
      case 0x0: gene_line0(i);         break;
      case 0x1: gene_line1_2_3(i,0,1); break;
      case 0x2: gene_line1_2_3(i,2,2); break;
      case 0x3: gene_line1_2_3(i,1,3); break;
      case 0x4: gene_line4(i);         break;
      case 0x5: gene_line5(i);         break;
      case 0x6: gene_line6(i);         break;
      case 0x7: gene_line7(i);         break;
      case 0x8: gene_line8_C(i,1);     break;
      case 0x9: gene_line9_D(i,1);     break;
      case 0xA: gene_lineA_F(i,1);     break;
      case 0xB: gene_lineB(i);         break;
      case 0xC: gene_line8_C(i,0);     break;
      case 0xD: gene_line9_D(i,0);     break;
      case 0xE: gene_lineE(i);         break;
      case 0xF: gene_lineA_F(i,0);     break;
      default:
        if (!i) gene_table("line",64*16);
      }
      fflush(output);
    }

    if ( output != savestdout ) {
      fclose(output);
      output = 0;
    }
  }

  return 0;
}
