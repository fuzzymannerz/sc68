/*
 * @file    conf68.c
 * @brief   sc68 config file
 * @author  http://sourceforge.net/users/benjihan
 *
 * Copyright (C) 1998-2014 Benjamin Gerard
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "io68/default.h"
#include "conf68.h"

/* file68 headers */
#include <sc68/file68.h>
#include <sc68/file68_err.h>
#include <sc68/file68_uri.h>
#include <sc68/file68_str.h>
#include <sc68/file68_msg.h>
#include <sc68/file68_opt.h>
#include <sc68/file68_reg.h>

/* standard headers */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef DEBUG_CONFIG68_O
# define DEBUG_CONFIG68_O 0
#endif

static int        config68_cat = msg68_DEFAULT;
static int        config68_use_registry = -1;
static char       config68_def_name[] = "sc68";

static const char cuk_fmt[] = "CUK:Software/sashipa/sc68-%s/";
static const char lmk_str[] = "LMK:Software/sashipa/sc68/config/";
static const char prefix[]  = "sc68-";
static const char optcat[]  = "option";

struct _config68_s {
  int n;
};

enum {
  MAX_TIME = (24 * 60 * 60) - 1
};

/* exported */
int config68_force_file = 0;

static const char * f_asids[] = { "off","on","force" };

static option68_t opts[] = {

  OPT68_IRNG(prefix,"sampling-rate",optcat,
             "sampling rate in Hz",
             SPR_MIN,SPR_MAX,1,0),

  OPT68_ENUM(prefix,"asid",optcat,
             "aSIDfier settings",
             f_asids,sizeof(f_asids)/sizeof(*f_asids),1,0),

#ifdef WITH_FORCE_HW
  OPT68_IRNG(prefix,"force-hw",optcat,"override track hardware {0:off}",
             0,511,0,0),
#endif

#ifdef WITH_FORCE
  OPT68_IRNG(prefix,"force-track",optcat,"override default track {0:off}",
             0,SC68_MAX_TRACK,1,0),

  OPT68_IRNG(prefix,"force-loop",optcat,
             "override default loop {0:off -1:inf}",
             -1,100,1,0),
#endif

  /* OPT68_IRNG(prefix,"skip-time",optcat, */
  /*            "prevent short track from being played (in sec) {0:off}", */
  /*            0,MAX_TIME,1,0), */

  OPT68_IRNG(prefix,"default-time",optcat,
             "default track time (in second)",
             0,MAX_TIME,1,0),
};

static const char config_header[] =
  "# -*- conf-mode -*-\n"
  "#\n"
  "# sc68 config file generated by " PACKAGE_STRING "\n"
  "#\n"
  "# " PACKAGE_URL "\n"
  "#\n";

static int is_symbol_char(int c)
{
  return isalnum(c) || c=='_' || c=='.';
}

static int save_config_entry(vfs68_t * os, const option68_t * opt)
{
  char tmp[256];
  int i, j, c;
  const int max = sizeof(tmp)-1;

  if (opt->org == opt68_UDF || !opt->save)
    return 0;

  /* Save comment on this entry (description and range) */
  i = snprintf(tmp, max, "\n# %s", opt->desc);
  switch (opt->type) {
  case opt68_BOL:
    i += snprintf(tmp+i, max-i, "%s"," [on|off]");
    break;

  case opt68_INT:
    if (!opt->sets) {
      if (opt->min < opt->max)
        i += snprintf(tmp+i, max-i, " [%d..%d]", opt->min, opt->max);
    } else {
      const int * set = (const int *) opt->set;
      i += snprintf(tmp+i, max-i, " %c", '[');
      for (j=0; j<(int)opt->sets; ++j)
        i += snprintf(tmp+i, max-i, "%d%c", set[j], j+1==opt->sets?']':',');
    }
    break;

  case opt68_ENU:
  case opt68_STR:
    if (opt->sets) {
      const char ** set = (const char  **) opt->set;
      i += snprintf(tmp+i, max-i, " %c", '[');
      for (j=0; j<(int)opt->sets; ++j)
        i += snprintf(tmp+i, max-i, "%s%c", set[j], j+1==opt->sets?']':',');
    }
    break;
  }

  if (i < max)
    tmp[i++] = '\n';

  /* transform name */
  for (j = 0; i < max && (c = opt->name[j]); ++j)
    tmp[i++] = (c == '-') ? '_' : c;
  switch (opt->type) {
  case opt68_BOL:
    i += snprintf(tmp+i, max-i, "=%s\n", opt->val.num?"on":"off");
    break;
  case opt68_INT:
    i += snprintf(tmp+i, max-i, "=%d\n", opt->val.num);
    break;
  case opt68_ENU:
    i += snprintf(tmp+i, max-i, "=%s\n", opt->val.num[(char**)opt->set]);
    break;
  case opt68_STR:
    i += snprintf(tmp+i, max-i, "=%s\n", opt->val.str);
    break;
  }
  tmp[i] = 0;
  return - vfs68_puts(os, tmp) < 0;
}


int config68_save(const char * confname)
{
  int err = 0;
  char tmp[128];
  option68_t * opt;

  confname = confname ? confname : config68_def_name;

  if (!config68_use_registry) {
    /* Save into file */
    vfs68_t * os=0;
    const int sizeof_config_hd = sizeof(config_header)-1;

    strncpy(tmp, "sc68://config/", sizeof(tmp));
    strncat(tmp, confname, sizeof(tmp));
    os = uri68_vfs(tmp, 2, 0);
    err = vfs68_open(os);
    if (!err) {
      TRACE68(config68_cat,"conf68: save into \"%s\"\n",
              vfs68_filename(os));
      err =
        - (vfs68_write(os, config_header, sizeof_config_hd)
           != sizeof_config_hd);
      for (opt = option68_enum(0); opt; opt=opt->next)
        err |= save_config_entry(os, opt);
    }
    vfs68_close(os);
    vfs68_destroy(os);
  } else {
    /* Save into registry */
    int l = snprintf(tmp, sizeof(tmp), cuk_fmt, confname);
    char * s = tmp + l;
    l = sizeof(tmp) - l;

    for (opt = option68_enum(0); opt; opt=opt->next) {
      if (opt->org == opt68_UDF || !opt->save)
        continue;
      strncpy(s,opt->name,l);
      switch (opt->type) {
      case opt68_INT: case opt68_BOL:
        err |= registry68_puti(0, tmp, opt->val.num);
        break;
      case opt68_ENU:
        err |= registry68_puts(0, tmp, opt->val.num[(char**)opt->set]);
        break;
      case opt68_STR:
        err |= registry68_puts(0, tmp, opt->val.str);
        break;
      }
    }

  }

  return err;
}

/* Load config from registry */
static int load_from_registry(const char * confname)
{
  option68_t * opt;
  int err = 0;
  char paths[2][64];

  snprintf(paths[0], sizeof(paths[0]), cuk_fmt, confname);
  strncpy(paths[1], lmk_str, sizeof(paths[1]));

  for (opt = option68_enum(0); opt; opt = opt->next) {
    char path[128], str[512];
    int  k, val;
    if (!opt->save)
      continue;

    for (k=0; k<2; ++k) {
      strncpy(path, paths[k], sizeof(path));
      strncat(path, opt->name, sizeof(path));

      TRACE68(config68_cat, "conf68: trying -- '%s'\n", path);
      switch (opt->type) {
      case opt68_BOL: case opt68_INT:
        err = registry68_geti(0, path, &val);
        if (!err)
          err = option68_iset(opt, val, opt68_PRIO, opt68_CFG);
        if (!err)
          TRACE68(config68_cat,
                  "conf68: load '%s' <- %d\n", path, val);
        break;
      case opt68_ENU: case opt68_STR:
        err = registry68_gets(0, path, str, sizeof(str));
        if (!err)
          err = option68_set(opt, str, opt68_PRIO, opt68_CFG);
        TRACE68(config68_cat,
                "conf68: load '%s' <- '%s'\n", path, str);
        break;
      default:
        assert(!"invalid option type");
        err = -1;
        break;
      }
    }
  }

  return 0;
}

/* Load config from file */
static int load_from_file(const char * confname)
{
  vfs68_t * is = 0;
  char s[256], * word;
  int err;
  option68_t * opt;

  strcpy(s, "sc68://config/");
  strcat(s, confname);
  is = uri68_vfs(s, 1, 0);
  err = vfs68_open(is);
  if (err)
    goto error;

  for(;;) {
    char * name;
    int i, len, c = 0;

    len = vfs68_gets(is, s, sizeof(s));
    if (len == -1) {
      err = -1;
      break;
    }
    if (len == 0) {
      break;
    }

    i = 0;

    /* Skip space */
    while (i < len && (c=s[i++], isspace(c)))
      ;

    if (!is_symbol_char(c)) {
      continue;
    }

    /* Get symbol name. */
    name = s+i-1;
    while (i < len && is_symbol_char(c = s[i++]))
      if (c == '_') s[i-1] = c = '-';
    s[i-1] = 0;

    /* TRACE68(config68_cat,"conf68: load get key name='%s\n", name); */

    /* Skip space */
    while (i < len && isspace(c))
      c=s[i++];

    /* Must have '=' */
    if (c != '=') {
      continue;
    }
    c=s[i++];

    /* Skip space */
    while (i < len && isspace(c))
      c=s[i++];

    word = s + i - 1;
    while (i < len && (c = s[i++]) && c != '\n');
    s[i-1] = 0;

    opt = option68_get(name, opt68_ALWAYS);
    if (!opt) {
      TRACE68(config68_cat, "conf68: unknown config key '%s'='%s'\n",
              name, word);
      continue;
    }
    if (!opt->save) {
      TRACE68(config68_cat, "conf68: config key '%s'='%s' not for save\n",
              name, word);
    }

    TRACE68(config68_cat, "conf68: set name='%s'='%s'\n",
            name, word);
    option68_set(opt, word, opt68_PRIO, opt68_CFG);
  }

error:
  vfs68_destroy(is);
  TRACE68(config68_cat, "conf68: loaded => [%s]\n",strok68(err));
  return err;

}


/* Load config */
int config68_load(const char * appname)
{
  appname = appname ? appname : config68_def_name;
  return config68_use_registry
    ? load_from_registry(appname)
    : load_from_file(appname)
    ;
}

int config68_init(int argc, char * argv[])
{
  config68_cat = msg68_cat("conf","config file", DEBUG_CONFIG68_O);
  option68_append(opts,sizeof(opts)/sizeof(*opts));
  argc = option68_parse(argc,argv);
  config68_use_registry = !config68_force_file && registry68_support();
  TRACE68(config68_cat,
          "conf68: will use %s\n",
          config68_use_registry?"registry":"config file");
  return argc;
}

void config68_shutdown(void)
{
  msg68_cat_free(config68_cat);
  config68_cat = msg68_DEFAULT;
}
