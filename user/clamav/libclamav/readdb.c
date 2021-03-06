/*
 *  Copyright (C) 2007-2008 Sourcefire, Inc.
 *
 *  Authors: Tomasz Kojm
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#ifdef _MSC_VER
#include <winsock.h> /* for Sleep() */
#endif

#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef	HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef C_WINDOWS
#include <dirent.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef	HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <fcntl.h>
#include <zlib.h>

#include "clamav.h"
#include "cvd.h"
#ifdef	HAVE_STRINGS_H
#include <strings.h>
#endif
#include "matcher-ac.h"
#include "matcher-bm.h"
#include "matcher.h"
#include "others.h"
#include "str.h"
#include "dconf.h"
#include "filetypes.h"
#include "filetypes_int.h"
#include "readdb.h"

#include "phishcheck.h"
#include "phish_whitelist.h"
#include "phish_domaincheck_db.h"
#include "regex_list.h"
#include "hashtab.h"

#if defined(HAVE_READDIR_R_3) || defined(HAVE_READDIR_R_2)
#include <limits.h>
#include <stddef.h>
#endif

#ifdef CL_THREAD_SAFE
#  include <pthread.h>
static pthread_mutex_t cli_ref_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

struct cli_ignsig {
    char *dbname, *signame;
    unsigned int line;
    struct cli_ignsig *next;
};

struct cli_ignored {
    struct hashset hs;
    struct cli_ignsig *list;
};

/* Prototypes for old public functions just to shut up some gcc warnings;
 * to be removed in 1.0
 */
int cl_loaddb(const char *filename, struct cl_engine **engine, unsigned int *signo);
int cl_loaddbdir(const char *dirname, struct cl_engine **engine, unsigned int *signo);

int cli_parse_add(struct cli_matcher *root, const char *virname, const char *hexsig, uint16_t rtype, uint16_t type, const char *offset, uint8_t target)
{
	struct cli_bm_patt *bm_new;
	char *pt, *hexcpy, *start, *n;
	int ret, virlen, asterisk = 0;
	unsigned int i, j, len, parts = 0;
	int mindist = 0, maxdist = 0, error = 0;


    if(strchr(hexsig, '{')) {

	root->ac_partsigs++;

	if(!(hexcpy = cli_strdup(hexsig)))
	    return CL_EMEM;

	len = strlen(hexsig);
	for(i = 0; i < len; i++)
	    if(hexsig[i] == '{' || hexsig[i] == '*')
		parts++;

	if(parts)
	    parts++;

	start = pt = hexcpy;
	for(i = 1; i <= parts; i++) {

	    if(i != parts) {
		for(j = 0; j < strlen(start); j++) {
		    if(start[j] == '{') {
			asterisk = 0;
			pt = start + j;
			break;
		    }
		    if(start[j] == '*') {
			asterisk = 1;
			pt = start + j;
			break;
		    }
		}
		*pt++ = 0;
	    }

	    if((ret = cli_ac_addsig(root, virname, start, root->ac_partsigs, parts, i, rtype, type, mindist, maxdist, offset, target))) {
		cli_errmsg("cli_parse_add(): Problem adding signature (1).\n");
		error = 1;
		break;
	    }

	    if(i == parts)
		break;

	    mindist = maxdist = 0;

	    if(asterisk) {
		start = pt;
		continue;
	    }

	    if(!(start = strchr(pt, '}'))) {
		error = 1;
		break;
	    }
	    *start++ = 0;

	    if(!pt) {
		error = 1;
		break;
	    }

	    if(!strchr(pt, '-')) {
		if(!cli_isnumber(pt) || (mindist = maxdist = atoi(pt)) < 0) {
		    error = 1;
		    break;
		}
	    } else {
		if((n = cli_strtok(pt, 0, "-"))) {
		    if(!cli_isnumber(n) || (mindist = atoi(n)) < 0) {
			error = 1;
			free(n);
			break;
		    }
		    free(n);
		}

		if((n = cli_strtok(pt, 1, "-"))) {
		    if(!cli_isnumber(n) || (maxdist = atoi(n)) < 0) {
			error = 1;
			free(n);
			break;
		    }
		    free(n);
		}

		if((n = cli_strtok(pt, 2, "-"))) { /* strict check */
		    error = 1;
		    free(n);
		    break;
		}
	    }
	}

	free(hexcpy);
	if(error)
	    return CL_EMALFDB;

    } else if(strchr(hexsig, '*')) {
	root->ac_partsigs++;

	len = strlen(hexsig);
	for(i = 0; i < len; i++)
	    if(hexsig[i] == '*')
		parts++;

	if(parts)
	    parts++;

	for(i = 1; i <= parts; i++) {
	    if((pt = cli_strtok(hexsig, i - 1, "*")) == NULL) {
		cli_errmsg("Can't extract part %d of partial signature.\n", i);
		return CL_EMALFDB;
	    }

	    if((ret = cli_ac_addsig(root, virname, pt, root->ac_partsigs, parts, i, rtype, type, 0, 0, offset, target))) {
		cli_errmsg("cli_parse_add(): Problem adding signature (2).\n");
		free(pt);
		return ret;
	    }

	    free(pt);
	}

    } else if(root->ac_only || strpbrk(hexsig, "?(") || type) {
	if((ret = cli_ac_addsig(root, virname, hexsig, 0, 0, 0, rtype, type, 0, 0, offset, target))) {
	    cli_errmsg("cli_parse_add(): Problem adding signature (3).\n");
	    return ret;
	}

    } else {
	bm_new = (struct cli_bm_patt *) cli_calloc(1, sizeof(struct cli_bm_patt));
	if(!bm_new)
	    return CL_EMEM;

	if(!(bm_new->pattern = (unsigned char *) cli_hex2str(hexsig))) {
	    free(bm_new);
	    return CL_EMALFDB;
	}

	bm_new->length = strlen(hexsig) / 2;

	if((pt = strstr(virname, "(Clam)")))
	    virlen = strlen(virname) - strlen(pt) - 1;
	else
	    virlen = strlen(virname);

	if(virlen <= 0) {
	    free(bm_new->pattern);
	    free(bm_new);
	    return CL_EMALFDB;
	}

	if((bm_new->virname = cli_calloc(virlen + 1, sizeof(char))) == NULL) {
	    free(bm_new->pattern);
	    free(bm_new);
	    return CL_EMEM;
	}

	strncpy(bm_new->virname, virname, virlen);

	if(offset) {
	    bm_new->offset = cli_strdup(offset);
	    if(!bm_new->offset) {
		free(bm_new->pattern);
		free(bm_new->virname);
		free(bm_new);
		return CL_EMEM;
	    }
	}

	bm_new->target = target;

	if(bm_new->length > root->maxpatlen)
	    root->maxpatlen = bm_new->length;

	if((ret = cli_bm_addpatt(root, bm_new))) {
	    cli_errmsg("cli_parse_add(): Problem adding signature (4).\n");
	    free(bm_new->pattern);
	    free(bm_new->virname);
	    free(bm_new);
	    return ret;
	}
    }

    return CL_SUCCESS;
}

int cli_initengine(struct cl_engine **engine, unsigned int options)
{
	int ret;


    if(!*engine) {
#ifdef CL_EXPERIMENTAL
	cli_dbgmsg("Initializing the engine ("VERSION"-exp)\n");
#else
	cli_dbgmsg("Initializing the engine ("VERSION")\n");
#endif

	*engine = (struct cl_engine *) cli_calloc(1, sizeof(struct cl_engine));
	if(!*engine) {
	    cli_errmsg("Can't allocate memory for the engine structure!\n");
	    return CL_EMEM;
	}

	(*engine)->refcount = 1;

	(*engine)->root = cli_calloc(CLI_MTARGETS, sizeof(struct cli_matcher *));
	if(!(*engine)->root) {
	    /* no need to free previously allocated memory here */
	    cli_errmsg("Can't allocate memory for roots!\n");
	    return CL_EMEM;
	}

	(*engine)->dconf = cli_dconf_init();
	if(!(*engine)->dconf) {
	    cli_errmsg("Can't initialize dynamic configuration\n");
	    return CL_EMEM;
	}
    }

    if((options & CL_DB_PHISHING_URLS) && (((struct cli_dconf*) (*engine)->dconf)->phishing & PHISHING_CONF_ENGINE))
	if((ret = phishing_init(*engine)))
	    return ret;

    return CL_SUCCESS;
}

static int cli_initroots(struct cl_engine *engine, unsigned int options)
{
	int i, ret;
	struct cli_matcher *root;


    for(i = 0; i < CLI_MTARGETS; i++) {
	if(!engine->root[i]) {
	    cli_dbgmsg("Initializing engine->root[%d]\n", i);
	    root = engine->root[i] = (struct cli_matcher *) cli_calloc(1, sizeof(struct cli_matcher));
	    if(!root) {
		cli_errmsg("cli_initroots: Can't allocate memory for cli_matcher\n");
		return CL_EMEM;
	    }

	    if(cli_mtargets[i].ac_only || (options & CL_DB_ACONLY))
		root->ac_only = 1;

	    cli_dbgmsg("Initialising AC pattern matcher of root[%d]\n", i);
	    if((ret = cli_ac_init(root, cli_ac_mindepth, cli_ac_maxdepth))) {
		/* no need to free previously allocated memory here */
		cli_errmsg("cli_initroots: Can't initialise AC pattern matcher\n");
		return ret;
	    }

	    if(!root->ac_only) {
		cli_dbgmsg("cli_initroots: Initializing BM tables of root[%d]\n", i);
		if((ret = cli_bm_init(root))) {
		    cli_errmsg("cli_initroots: Can't initialise BM pattern matcher\n");
		    return ret;
		}
	    }
	}
    }

    return CL_SUCCESS;
}

char *cli_dbgets(char *buff, unsigned int size, FILE *fs, gzFile *gzs, unsigned int *gzrsize)
{
    if(fs) {
	return fgets(buff, size, fs);

    } else {
	    char *pt;
	    unsigned int bs;

	if(!*gzrsize)
	    return NULL;

	bs = *gzrsize < size ? *gzrsize + 1 : size;
	pt = gzgets(gzs, buff, bs);
	*gzrsize -= strlen(buff);
	if(!pt)
	    cli_errmsg("cli_dbgets: Preliminary end of data\n");
	return pt;
    }
}

static int cli_chkign(const struct cli_ignored *ignored, const char *dbname, unsigned int line, const char *signame)
{
	struct cli_ignsig *pt;

    if(!ignored || !dbname || !signame)
	return 0;

    if(hashset_contains(&ignored->hs, line)) {
	pt = ignored->list;
	while(pt) {
	    if(pt->line == line && !strcmp(pt->dbname, dbname) && !strcmp(pt->signame, signame)) {
		cli_dbgmsg("Skipping signature %s @ %s:%u\n", signame, dbname, line);
		return 1;
	    }
	    pt = pt->next;
	}
    }

    return 0;
}

static int cli_loaddb(FILE *fs, struct cl_engine **engine, unsigned int *signo, unsigned int options, gzFile *gzs, unsigned int gzrsize, const char *dbname)
{
	char buffer[FILEBUFF], *pt, *start;
	unsigned int line = 0, sigs = 0;
	int ret = 0;
	struct cli_matcher *root;


    if((ret = cli_initengine(engine, options))) {
	cl_free(*engine);
	return ret;
    }

    if((ret = cli_initroots(*engine, options))) {
	cl_free(*engine);
	return ret;
    }

    root = (*engine)->root[0];

    while(cli_dbgets(buffer, FILEBUFF, fs, gzs, &gzrsize)) {
	line++;
	cli_chomp(buffer);

	pt = strchr(buffer, '=');
	if(!pt) {
	    cli_errmsg("Malformed pattern line %d\n", line);
	    ret = CL_EMALFDB;
	    break;
	}

	start = buffer;
	*pt++ = 0;

	if((*engine)->ignored && cli_chkign((*engine)->ignored, dbname, line, start))
	    continue;

	if(*pt == '=') continue;

	if((ret = cli_parse_add(root, start, pt, 0, 0, NULL, 0))) {
	    ret = CL_EMALFDB;
	    break;
	}
	sigs++;
    }

    if(!line) {
	cli_errmsg("Empty database file\n");
	cl_free(*engine);
	return CL_EMALFDB;
    }

    if(ret) {
	cli_errmsg("Problem parsing database at line %d\n", line);
	cl_free(*engine);
	return ret;
    }

    if(signo)
	*signo += sigs;

    return CL_SUCCESS;
}

static int cli_loadwdb(FILE *fs, struct cl_engine **engine, unsigned int options, gzFile *gzs, unsigned int gzrsize)
{
	int ret = 0;


    if((ret = cli_initengine(engine, options))) {
	cl_free(*engine);
	return ret;
    }

    if(!(((struct cli_dconf *) (*engine)->dconf)->phishing & PHISHING_CONF_ENGINE))
	return CL_SUCCESS;

    if(!(*engine)->whitelist_matcher) {
	if((ret = init_whitelist(*engine))) {
	    phishing_done(*engine);
	    cl_free(*engine);
	    return ret;
	}
    }

    if((ret = load_regex_matcher((*engine)->whitelist_matcher, fs, options, 1, gzs, gzrsize))) {
	phishing_done(*engine);
	cl_free(*engine);
	return ret;
    }

    return CL_SUCCESS;
}

static int cli_loadpdb(FILE *fs, struct cl_engine **engine, unsigned int options, gzFile *gzs, unsigned int gzrsize)
{
	int ret = 0;


    if((ret = cli_initengine(engine, options))) {
	cl_free(*engine);
	return ret;
    }

    if(!(((struct cli_dconf *) (*engine)->dconf)->phishing & PHISHING_CONF_ENGINE))
	return CL_SUCCESS;

    if(!(*engine)->domainlist_matcher) {
	if((ret = init_domainlist(*engine))) {
	    phishing_done(*engine);
	    cl_free(*engine);
	    return ret;
	}
    }

    if((ret = load_regex_matcher((*engine)->domainlist_matcher, fs, options, 0, gzs, gzrsize))) {
	phishing_done(*engine);
	cl_free(*engine);
	return ret;
    }

    return CL_SUCCESS;
}

#define NDB_TOKENS 6
static int cli_loadndb(FILE *fs, struct cl_engine **engine, unsigned int *signo, unsigned short sdb, unsigned int options, gzFile *gzs, unsigned int gzrsize, const char *dbname)
{
	const char *tokens[NDB_TOKENS];
	char buffer[FILEBUFF];
	const char *sig, *virname, *offset, *pt;
	struct cli_matcher *root;
	int line = 0, sigs = 0, ret = 0;
	unsigned short target;
	unsigned int phish = options & CL_DB_PHISHING;


    if((ret = cli_initengine(engine, options))) {
	cl_free(*engine);
	return ret;
    }

    if((ret = cli_initroots(*engine, options))) {
	cl_free(*engine);
	return ret;
    }

    while(cli_dbgets(buffer, FILEBUFF, fs, gzs, &gzrsize)) {
	line++;

	if(!strncmp(buffer, "Exploit.JPEG.Comment", 20)) /* temporary */
	    continue;

	if(!phish)
	    if(!strncmp(buffer, "HTML.Phishing", 13) || !strncmp(buffer, "Email.Phishing", 14))
		continue;

	cli_chomp(buffer);

	cli_strtokenize(buffer, ':', NDB_TOKENS, tokens);

	if(!(virname = tokens[0])) {
	    ret = CL_EMALFDB;
	    break;
	}

	if((*engine)->ignored && cli_chkign((*engine)->ignored, dbname, line, virname))
	    continue;

	if((pt = tokens[4])) { /* min version */
	    if(!isdigit(*pt)) {
		ret = CL_EMALFDB;
		break;
	    }

	    if((unsigned int) atoi(pt) > cl_retflevel()) {
		cli_dbgmsg("Signature for %s not loaded (required f-level: %d)\n", virname, atoi(pt));
		continue;
	    }


	    if((pt = tokens[5])) { /* max version */
		if(!isdigit(*pt)) {
		    ret = CL_EMALFDB;
		    break;
		}

		if((unsigned int) atoi(pt) < cl_retflevel()) {
		    continue;
		}

	    }
	}

	if(!(pt = tokens[1]) || !isdigit(*pt)) {
	    ret = CL_EMALFDB;
	    break;
	}
	target = (unsigned short) atoi(pt);

	if(target >= CLI_MTARGETS) {
	    cli_dbgmsg("Not supported target type in signature for %s\n", virname);
	    continue;
	}

	root = (*engine)->root[target];

	if(!(offset = tokens[2])) {
	    ret = CL_EMALFDB;
	    break;
	} else if(!strcmp(offset, "*")) {
	    offset = NULL;
	}

	if(!(sig = tokens[3])) {
	    ret = CL_EMALFDB;
	    break;
	}

	if((ret = cli_parse_add(root, virname, sig, 0, 0, offset, target))) {
	    ret = CL_EMALFDB;
	    break;
	}
	sigs++;
    }

    if(!line) {
	cli_errmsg("Empty database file\n");
	cl_free(*engine);
	return CL_EMALFDB;
    }

    if(ret) {
	cli_errmsg("Problem parsing database at line %d\n", line);
	cl_free(*engine);
	return ret;
    }

    if(signo)
	*signo += sigs;

    if(sdb && sigs && !(*engine)->sdb) {
	(*engine)->sdb = 1;
	cli_dbgmsg("*** Self protection mechanism activated.\n");
    }

    return CL_SUCCESS;
}

#define FTM_TOKENS 8	
static int cli_loadftm(FILE *fs, struct cl_engine **engine, unsigned int options, unsigned int internal, gzFile *gzs, unsigned int gzrsize)
{
	const char *tokens[FTM_TOKENS], *pt;
	char buffer[FILEBUFF];
	unsigned int line = 0, sigs = 0;
	struct cli_ftype *new;
	cli_file_t rtype, type;
	int ret;


    if((ret = cli_initengine(engine, options))) {
	cl_free(*engine);
	return ret;
    }

    if((ret = cli_initroots(*engine, options))) {
	cl_free(*engine);
	return ret;
    }

    while(1) {
	if(internal) {
	    if(!ftypes_int[line])
		break;
	    strncpy(buffer, ftypes_int[line], sizeof(buffer));
	} else {
	    if(!cli_dbgets(buffer, FILEBUFF, fs, gzs, &gzrsize))
		break;
	    cli_chomp(buffer);
	}
	line++;
	cli_strtokenize(buffer, ':', FTM_TOKENS, tokens);

	if(!tokens[0] || !tokens[1] || !tokens[2] || !tokens[3] || !tokens[4] || !tokens[5]) {
	    ret = CL_EMALFDB;
	    break;
	}

	if((pt = tokens[6])) { /* min version */
	    if(!cli_isnumber(pt)) {
		ret = CL_EMALFDB;
		break;
	    }
	    if((unsigned int) atoi(pt) > cl_retflevel()) {
		cli_dbgmsg("cli_loadftm: File type signature for %s not loaded (required f-level: %u)\n", tokens[3], atoi(pt));
		continue;
	    }
	    if((pt = tokens[7])) { /* max version */
		if(!cli_isnumber(pt)) {
		    ret = CL_EMALFDB;
		    break;
		}
		if((unsigned int) atoi(pt) < cl_retflevel())
		    continue;
	    }
	}

	rtype = cli_ftcode(tokens[4]);
	type = cli_ftcode(tokens[5]);
	if(rtype == CL_TYPE_ERROR || type == CL_TYPE_ERROR) {
	    ret = CL_EMALFDB;
	    break;
	}

	if(atoi(tokens[0]) == 1) { /* A-C */
	    if((ret = cli_parse_add((*engine)->root[0], tokens[3], tokens[2], rtype, type, strcmp(tokens[1], "*") ? tokens[1] : NULL, 0)))
		break;

	} else if(atoi(tokens[0]) == 0) { /* memcmp() */
	    new = (struct cli_ftype *) cli_malloc(sizeof(struct cli_ftype));
	    if(!new) {
		ret = CL_EMEM;
		break;
	    }
	    new->type = type;
	    new->offset = atoi(tokens[1]);
	    new->magic = (unsigned char *) cli_hex2str(tokens[2]);
	    if(!new->magic) {
		cli_errmsg("cli_loadftm: Can't decode the hex string\n");
		ret = CL_EMALFDB;
		free(new);
		break;
	    }
	    new->length = strlen(tokens[2]) / 2;
	    new->tname = cli_strdup(tokens[3]);
	    if(!new->tname) {
		free(new->magic);
		free(new);
		ret = CL_EMEM;
		break;
	    }
	    new->next = (*engine)->ftypes;
	    (*engine)->ftypes = new;

	} else {
	    cli_dbgmsg("cli_loadftm: Unsupported mode %u\n", atoi(tokens[0]));
	    continue;
	}
	sigs++;
    }

    if(ret) {
	cli_errmsg("Problem parsing %s filetype database at line %u\n", internal ? "built-in" : "external", line);
	cl_free(*engine);
	return ret;
    }

    if(!sigs) {
	cli_errmsg("Empty %s filetype database\n", internal ? "built-in" : "external");
	cl_free(*engine);
	return CL_EMALFDB;
    }

    cli_dbgmsg("Loaded %u filetype definitions\n", sigs);
    return CL_SUCCESS;
}

static int cli_loadign(FILE *fs, struct cl_engine **engine, unsigned int options, gzFile *gzs, unsigned int gzrsize)
{
	char buffer[FILEBUFF], *pt;
	unsigned int line = 0;
	struct cli_ignsig *new, *last = NULL;
	struct cli_ignored *ignored;
	int ret;


    if((ret = cli_initengine(engine, options))) {
	cl_free(*engine);
	return ret;
    }

    if(!(ignored = (*engine)->ignored)) {
	ignored = (*engine)->ignored = (struct cli_ignored *) cli_calloc(sizeof(struct cli_ignored), 1);
	if(!ignored || hashset_init(&ignored->hs, 64, 50)) {
	    cl_free(*engine);
	    return CL_EMEM;
	}
    }

    while(cli_dbgets(buffer, FILEBUFF, fs, gzs, &gzrsize)) {
	line++;
	cli_chomp(buffer);

	new = (struct cli_ignsig *) cli_calloc(1, sizeof(struct cli_ignsig));
	if(!new) {
	    ret = CL_EMEM;
	    break;
	}

	if(!(new->dbname = cli_strtok(buffer, 0, ":"))) {
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	}

	if(!(pt = cli_strtok(buffer, 1, ":"))) {
	    free(new->dbname);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	} else {
	    new->line = atoi(pt);
	    free(pt);
	}

	if((ret = hashset_addkey(&ignored->hs, new->line)))
	    break;

	if(!(new->signame = cli_strtok(buffer, 2, ":"))) {
	    free(new->dbname);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	}

	if(!last) {
	    last = ignored->list = new;
	} else {
	    last->next = new;
	    last = new;
	}
    }

    if(ret) {
	cli_errmsg("cli_loadign: Problem parsing database at line %u\n", line);
	cl_free(*engine);
	return ret;
    }

    return CL_SUCCESS;
}

static void cli_freeign(struct cl_engine *engine)
{
	struct cli_ignsig *pt;
	struct cli_ignored *ignored;

    if((ignored = engine->ignored)) {
	while(ignored->list) {
	    pt = ignored->list;
	    ignored->list = ignored->list->next;
	    free(pt->dbname);
	    free(pt->signame);
	    free(pt);
	}
	hashset_destroy(&ignored->hs);
	free(engine->ignored);
	engine->ignored = NULL;
    }
}

static int scomp(const void *a, const void *b)
{
    return *(const uint32_t *)a - *(const uint32_t *)b;
}

#define MD5_HDB	    0
#define MD5_MDB	    1
#define MD5_FP	    2

static int cli_md5db_init(struct cl_engine **engine, unsigned int mode)
{
	struct cli_matcher *bm = NULL;
	int ret;


    if(mode == MD5_HDB) {
	bm = (*engine)->md5_hdb = (struct cli_matcher *) cli_calloc(sizeof(struct cli_matcher), 1);
    } else if(mode == MD5_MDB) {
	bm = (*engine)->md5_mdb = (struct cli_matcher *) cli_calloc(sizeof(struct cli_matcher), 1);
    } else {
	bm = (*engine)->md5_fp = (struct cli_matcher *) cli_calloc(sizeof(struct cli_matcher), 1);
    }

    if(!bm)
	return CL_EMEM;

    if((ret = cli_bm_init(bm))) {
	cli_errmsg("cli_md5db_init: Failed to initialize B-M\n");
	return ret;
    }

    return CL_SUCCESS;
}

#define MD5_DB			    \
    if(mode == MD5_HDB)		    \
	db = (*engine)->md5_hdb;    \
    else if(mode == MD5_MDB)	    \
	db = (*engine)->md5_mdb;    \
    else			    \
	db = (*engine)->md5_fp;

static int cli_loadmd5(FILE *fs, struct cl_engine **engine, unsigned int *signo, unsigned int mode, unsigned int options, gzFile *gzs, unsigned int gzrsize, const char *dbname)
{
	char buffer[FILEBUFF], *pt;
	int ret = CL_SUCCESS;
	unsigned int size_field = 1, md5_field = 0, line = 0, sigs = 0;
	uint32_t size;
	struct cli_bm_patt *new;
	struct cli_matcher *db = NULL;

    if((ret = cli_initengine(engine, options))) {
	cl_free(*engine);
	return ret;
    }

    if(mode == MD5_MDB) {
	size_field = 0;
	md5_field = 1;
    }

    while(cli_dbgets(buffer, FILEBUFF, fs, gzs, &gzrsize)) {
	line++;
	cli_chomp(buffer);

	new = (struct cli_bm_patt *) cli_calloc(1, sizeof(struct cli_bm_patt));
	if(!new) {
	    ret = CL_EMEM;
	    break;
	}

	if(!(pt = cli_strtok(buffer, md5_field, ":"))) {
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	}

	if(strlen(pt) != 32 || !(new->pattern = (unsigned char *) cli_hex2str(pt))) {
	    cli_errmsg("cli_loadmd5: Malformed MD5 string at line %u\n", line);
	    free(pt);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	}
	free(pt);
	new->length = 16;

	if(!(pt = cli_strtok(buffer, size_field, ":"))) {
	    free(new->pattern);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	}
	size = atoi(pt);
	free(pt);

	if(!(new->virname = cli_strtok(buffer, 2, ":"))) {
	    free(new->pattern);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	}

	if((*engine)->ignored && cli_chkign((*engine)->ignored, dbname, line, new->virname)) {
	    free(new->virname);
	    free(new->pattern);
	    free(new);
	    continue;
	}

	MD5_DB;
	if(!db && (ret = cli_md5db_init(engine, mode))) {
	    free(new->pattern);
	    free(new->virname);
	    free(new);
	    break;
	} else {
	    MD5_DB;
	}

	if((ret = cli_bm_addpatt(db, new))) {
	    cli_errmsg("cli_loadmd5: Error adding BM pattern\n");
	    free(new->pattern);
	    free(new->virname);
	    free(new);
	    break;
	}

	if(mode == MD5_MDB) { /* section MD5 */
	    if(!db->md5_sizes_hs.capacity) {
		    hashset_init(&db->md5_sizes_hs, 32768, 80);
	    }
	    hashset_addkey(&db->md5_sizes_hs, size);
	}

	sigs++;
    }

    if(!line) {
	cli_errmsg("cli_loadmd5: Empty database file\n");
	cl_free(*engine);
	return CL_EMALFDB;
    }

    if(ret) {
	cli_errmsg("cli_loadmd5: Problem parsing database at line %u\n", line);
	cl_free(*engine);
	return ret;
    }

    if(signo)
	*signo += sigs;

    return CL_SUCCESS;
}

static int cli_loadmd(FILE *fs, struct cl_engine **engine, unsigned int *signo, int type, unsigned int options, gzFile *gzs, unsigned int gzrsize, const char *dbname)
{
	char buffer[FILEBUFF], *pt;
	unsigned int line = 0, sigs = 0;
	int ret, crc;
	struct cli_meta_node *new;


    if((ret = cli_initengine(engine, options))) {
	cl_free(*engine);
	return ret;
    }

    while(cli_dbgets(buffer, FILEBUFF, fs, gzs, &gzrsize)) {
	line++;
	if(buffer[0] == '#')
	    continue;

	cli_chomp(buffer);

	new = (struct cli_meta_node *) cli_calloc(1, sizeof(struct cli_meta_node));
	if(!new) {
	    ret = CL_EMEM;
	    break;
	}

	if(!(new->virname = cli_strtok(buffer, 0, ":"))) {
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	}

	if((*engine)->ignored && cli_chkign((*engine)->ignored, dbname, line, new->virname)) {
	    free(new->virname);
	    free(new);
	    continue;
	}

	if(!(pt = cli_strtok(buffer, 1, ":"))) {
	    free(new->virname);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	} else {
	    new->encrypted = atoi(pt);
	    free(pt);
	}

	if(!(new->filename = cli_strtok(buffer, 2, ":"))) {
	    free(new->virname);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	} else {
	    if(!strcmp(new->filename, "*")) {
		free(new->filename);
		new->filename = NULL;
	    }
	}

	if(!(pt = cli_strtok(buffer, 3, ":"))) {
	    free(new->filename);
	    free(new->virname);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	} else {
	    if(!strcmp(pt, "*"))
		new->size = -1;
	    else
		new->size = atoi(pt);
	    free(pt);
	}

	if(!(pt = cli_strtok(buffer, 4, ":"))) {
	    free(new->filename);
	    free(new->virname);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	} else {
	    if(!strcmp(pt, "*"))
		new->csize = -1;
	    else
		new->csize = atoi(pt);
	    free(pt);
	}

	if(!(pt = cli_strtok(buffer, 5, ":"))) {
	    free(new->filename);
	    free(new->virname);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	} else {
	    if(!strcmp(pt, "*")) {
		new->crc32 = 0;
	    } else {
		crc = cli_hex2num(pt);
		if(crc == -1) {
		    ret = CL_EMALFDB;
		    break;
		}
		new->crc32 = (unsigned int) crc;
	    }
	    free(pt);
	}

	if(!(pt = cli_strtok(buffer, 6, ":"))) {
	    free(new->filename);
	    free(new->virname);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	} else {
	    if(!strcmp(pt, "*"))
		new->method = -1;
	    else
		new->method = atoi(pt);
	    free(pt);
	}

	if(!(pt = cli_strtok(buffer, 7, ":"))) {
	    free(new->filename);
	    free(new->virname);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	} else {
	    if(!strcmp(pt, "*"))
		new->fileno = 0;
	    else
		new->fileno = atoi(pt);
	    free(pt);
	}

	if(!(pt = cli_strtok(buffer, 8, ":"))) {
	    free(new->filename);
	    free(new->virname);
	    free(new);
	    ret = CL_EMALFDB;
	    break;
	} else {
	    if(!strcmp(pt, "*"))
		new->maxdepth = 0;
	    else
		new->maxdepth = atoi(pt);
	    free(pt);
	}

	if(type == 1) {
	    new->next = (*engine)->zip_mlist;
	    (*engine)->zip_mlist = new;
	} else {
	    new->next = (*engine)->rar_mlist;
	    (*engine)->rar_mlist = new;
	}

	sigs++;
    }

    if(!line) {
	cli_errmsg("Empty database file\n");
	cl_free(*engine);
	return CL_EMALFDB;
    }

    if(ret) {
	cli_errmsg("Problem parsing database at line %d\n", line);
	cl_free(*engine);
	return ret;
    }

    if(signo)
	*signo += sigs;

    return CL_SUCCESS;
}

static int cli_loaddbdir(const char *dirname, struct cl_engine **engine, unsigned int *signo, unsigned int options);

int cli_load(const char *filename, struct cl_engine **engine, unsigned int *signo, unsigned int options, gzFile *gzs, unsigned int gzrsize)
{
	FILE *fs = NULL;
	int ret = CL_SUCCESS;
	uint8_t skipped = 0;
	const char *dbname;


    if(!gzs && (fs = fopen(filename, "rb")) == NULL) {
	cli_errmsg("cli_load(): Can't open file %s\n", filename);
	return CL_EOPEN;
    }

/*
#ifdef C_WINDOWS
    if((dbname = strrchr(filename, '\\')))
#else
*/
    if((dbname = strrchr(filename, '/')))
/*#endif */
	dbname++;
    else
	dbname = filename;

    if(cli_strbcasestr(dbname, ".db")) {
	ret = cli_loaddb(fs, engine, signo, options, gzs, gzrsize, dbname);

    } else if(cli_strbcasestr(dbname, ".cvd")) {
	    int warn = 0;

	if(!strcmp(dbname, "daily.cvd"))
	    warn = 1;

	ret = cli_cvdload(fs, engine, signo, warn, options, 0);

    } else if(cli_strbcasestr(dbname, ".cld")) {
	    int warn = 0;

	if(!strcmp(dbname, "daily.cld"))
	    warn = 1;

	ret = cli_cvdload(fs, engine, signo, warn, options | CL_DB_CVDNOTMP, 1);

    } else if(cli_strbcasestr(dbname, ".hdb")) {
	ret = cli_loadmd5(fs, engine, signo, MD5_HDB, options, gzs, gzrsize, dbname);

    } else if(cli_strbcasestr(dbname, ".hdu")) {
	if(options & CL_DB_PUA)
	    ret = cli_loadmd5(fs, engine, signo, MD5_HDB, options, gzs, gzrsize, dbname);
	else
	    skipped = 1;

    } else if(cli_strbcasestr(dbname, ".fp")) {
	ret = cli_loadmd5(fs, engine, signo, MD5_FP, options, gzs, gzrsize, dbname);

    } else if(cli_strbcasestr(dbname, ".mdb")) {
	ret = cli_loadmd5(fs, engine, signo, MD5_MDB, options, gzs, gzrsize, dbname);

    } else if(cli_strbcasestr(dbname, ".mdu")) {
	if(options & CL_DB_PUA)
	    ret = cli_loadmd5(fs, engine, signo, MD5_MDB, options, gzs, gzrsize, dbname);
	else
	    skipped = 1;

    } else if(cli_strbcasestr(dbname, ".ndb")) {
	ret = cli_loadndb(fs, engine, signo, 0, options, gzs, gzrsize, dbname);

    } else if(cli_strbcasestr(dbname, ".ndu")) {
	if(!(options & CL_DB_PUA))
	    skipped = 1;
	else
	    ret = cli_loadndb(fs, engine, signo, 0, options, gzs, gzrsize, dbname);

    } else if(cli_strbcasestr(dbname, ".sdb")) {
	ret = cli_loadndb(fs, engine, signo, 1, options, gzs, gzrsize, dbname);

    } else if(cli_strbcasestr(dbname, ".zmd")) {
	ret = cli_loadmd(fs, engine, signo, 1, options, gzs, gzrsize, dbname);

    } else if(cli_strbcasestr(dbname, ".rmd")) {
	ret = cli_loadmd(fs, engine, signo, 2, options, gzs, gzrsize, dbname);

    } else if(cli_strbcasestr(dbname, ".cfg")) {
	ret = cli_dconf_load(fs, engine, options, gzs, gzrsize);

    } else if(cli_strbcasestr(dbname, ".wdb")) {
	if(options & CL_DB_PHISHING_URLS) {
	    ret = cli_loadwdb(fs, engine, options, gzs, gzrsize);
	} else
	    skipped = 1;
    } else if(cli_strbcasestr(dbname, ".pdb")) {
	if(options & CL_DB_PHISHING_URLS) {
	    ret = cli_loadpdb(fs, engine, options, gzs, gzrsize);
	} else
	    skipped = 1;
    } else if(cli_strbcasestr(dbname, ".ftm")) {
	ret = cli_loadftm(fs, engine, options, 0, gzs, gzrsize);

    } else if(cli_strbcasestr(dbname, ".ign")) {
	ret = cli_loadign(fs, engine, options, gzs, gzrsize);

    } else {
	cli_dbgmsg("cli_load: unknown extension - assuming old database format\n");
	ret = cli_loaddb(fs, engine, signo, options, gzs, gzrsize, dbname);
    }

    if(ret) {
	cli_errmsg("Can't load %s: %s\n", filename, cl_strerror(ret));
    } else  {
	if(skipped)
	    cli_dbgmsg("%s skipped\n", filename);
	else
	    cli_dbgmsg("%s loaded\n", filename);
    }

    if(fs)
	fclose(fs);

    return ret;
}

int cl_loaddb(const char *filename, struct cl_engine **engine, unsigned int *signo) {
    return cli_load(filename, engine, signo, CL_DB_STDOPT, NULL, 0);
}

static int cli_loaddbdir(const char *dirname, struct cl_engine **engine, unsigned int *signo, unsigned int options)
{
	DIR *dd;
	struct dirent *dent;
#if defined(HAVE_READDIR_R_3) || defined(HAVE_READDIR_R_2)
	union {
	    struct dirent d;
	    char b[offsetof(struct dirent, d_name) + NAME_MAX + 1];
	} result;
#endif
	char *dbfile;
	int ret = CL_ESUPPORT;


    cli_dbgmsg("Loading databases from %s\n", dirname);
    dbfile = (char *) cli_malloc(strlen(dirname) + 20);
    if(!dbfile)
	return CL_EMEM;

    /* try to load local.ign and daily.cvd/daily.ign first */
    sprintf(dbfile, "%s/local.ign", dirname);
    if(!access(dbfile, R_OK) && (ret = cli_load(dbfile, engine, signo, options, NULL, 0))) {
	free(dbfile);
	return ret;
    }

    sprintf(dbfile, "%s/daily.cld", dirname);
    if(access(dbfile, R_OK))
	sprintf(dbfile, "%s/daily.cvd", dirname);
    if(!access(dbfile, R_OK) && (ret = cli_load(dbfile, engine, signo, options, NULL, 0))) {
	free(dbfile);
	return ret;
    }

    sprintf(dbfile, "%s/daily.ign", dirname);
    if(!access(dbfile, R_OK) && (ret = cli_load(dbfile, engine, signo, options, NULL, 0))) {
	free(dbfile);
	return ret;
    }

    /* check for and load daily.cfg */
    sprintf(dbfile, "%s/daily.cfg", dirname);
    if(!access(dbfile, R_OK) && (ret = cli_load(dbfile, engine, signo, options, NULL, 0))) {
	free(dbfile);
	return ret;
    }
    free(dbfile);

    if((dd = opendir(dirname)) == NULL) {
        cli_errmsg("cli_loaddbdir(): Can't open directory %s\n", dirname);
        return CL_EOPEN;
    }

#ifdef HAVE_READDIR_R_3
    while(!readdir_r(dd, &result.d, &dent) && dent) {
#elif defined(HAVE_READDIR_R_2)
    while((dent = (struct dirent *) readdir_r(dd, &result.d))) {
#else
    while((dent = readdir(dd))) {
#endif
#if	(!defined(C_INTERIX)) && (!defined(C_WINDOWS)) && (!defined(C_CYGWIN))
	if(dent->d_ino)
#endif
	{
	    if(strcmp(dent->d_name, ".") && strcmp(dent->d_name, "..") && strcmp(dent->d_name, "daily.cvd") && strcmp(dent->d_name, "daily.cld") && strcmp(dent->d_name, "daily.ign") && strcmp(dent->d_name, "daily.cfg") && strcmp(dent->d_name, "local.ign") && CLI_DBEXT(dent->d_name)) {

		dbfile = (char *) cli_malloc(strlen(dent->d_name) + strlen(dirname) + 2);

		if(!dbfile) {
		    cli_dbgmsg("cli_loaddbdir(): dbfile == NULL\n");
		    closedir(dd);
		    return CL_EMEM;
		}
		sprintf(dbfile, "%s/%s", dirname, dent->d_name);
		ret = cli_load(dbfile, engine, signo, options, NULL, 0);

		if(ret) {
		    cli_dbgmsg("cli_loaddbdir(): error loading database %s\n", dbfile);
		    free(dbfile);
		    closedir(dd);
		    return ret;
		}
		free(dbfile);
	    }
	}
    }

    closedir(dd);
    if(ret == CL_ESUPPORT)
	cli_errmsg("cli_loaddb(): No supported database files found in %s\n", dirname);

    return ret;
}

int cl_loaddbdir(const char *dirname, struct cl_engine **engine, unsigned int *signo) {
    return cli_loaddbdir(dirname, engine, signo, CL_DB_STDOPT);
}

int cl_load(const char *path, struct cl_engine **engine, unsigned int *signo, unsigned int options)
{
	struct stat sb;
	int ret;


    if(stat(path, &sb) == -1) {
        cli_errmsg("cl_loaddbdir(): Can't get status of %s\n", path);
        return CL_EIO;
    }

    if((ret = cli_initengine(engine, options))) {
	cl_free(*engine);
	return ret;
    }

    (*engine)->dboptions = options;

    switch(sb.st_mode & S_IFMT) {
	case S_IFREG: 
	    ret = cli_load(path, engine, signo, options, NULL, 0);
	    break;

	case S_IFDIR:
	    ret = cli_loaddbdir(path, engine, signo, options);
	    break;

	default:
	    cli_errmsg("cl_load(%s): Not supported database file type\n", path);
	    return CL_EOPEN;
    }

    return ret;
}

const char *cl_retdbdir(void)
{
    return DATADIR;
}

int cl_statinidir(const char *dirname, struct cl_stat *dbstat)
{
	DIR *dd;
	const struct dirent *dent;
#if defined(HAVE_READDIR_R_3) || defined(HAVE_READDIR_R_2)
	union {
	    struct dirent d;
	    char b[offsetof(struct dirent, d_name) + NAME_MAX + 1];
	} result;
#endif
        char *fname;


    if(dbstat) {
	dbstat->entries = 0;
	dbstat->stattab = NULL;
	dbstat->statdname = NULL;
	dbstat->dir = cli_strdup(dirname);
    } else {
        cli_errmsg("cl_statdbdir(): Null argument passed.\n");
	return CL_ENULLARG;
    }

    if((dd = opendir(dirname)) == NULL) {
        cli_errmsg("cl_statdbdir(): Can't open directory %s\n", dirname);
	cl_statfree(dbstat);
        return CL_EOPEN;
    }

    cli_dbgmsg("Stat()ing files in %s\n", dirname);

#ifdef HAVE_READDIR_R_3
    while(!readdir_r(dd, &result.d, &dent) && dent) {
#elif defined(HAVE_READDIR_R_2)
    while((dent = (struct dirent *) readdir_r(dd, &result.d))) {
#else
    while((dent = readdir(dd))) {
#endif
#if	(!defined(C_INTERIX)) && (!defined(C_WINDOWS)) && (!defined(C_CYGWIN))
	if(dent->d_ino)
#endif
	{
	    if(strcmp(dent->d_name, ".") && strcmp(dent->d_name, "..") && CLI_DBEXT(dent->d_name)) {
		dbstat->entries++;
		dbstat->stattab = (struct stat *) cli_realloc2(dbstat->stattab, dbstat->entries * sizeof(struct stat));
		if(!dbstat->stattab) {
		    cl_statfree(dbstat);
		    closedir(dd);
		    return CL_EMEM;
		}

#if defined(C_INTERIX) || defined(C_OS2)
		dbstat->statdname = (char **) cli_realloc2(dbstat->statdname, dbstat->entries * sizeof(char *));
		if(!dbstat->statdname) {
		    cl_statfree(dbstat);
		    closedir(dd);
		    return CL_EMEM;
		}
#endif

                fname = cli_malloc(strlen(dirname) + strlen(dent->d_name) + 32);
		if(!fname) {
		    cl_statfree(dbstat);
		    closedir(dd);
		    return CL_EMEM;
		}
		sprintf(fname, "%s/%s", dirname, dent->d_name);
#if defined(C_INTERIX) || defined(C_OS2)
		dbstat->statdname[dbstat->entries - 1] = (char *) cli_malloc(strlen(dent->d_name) + 1);
		if(!dbstat->statdname[dbstat->entries - 1]) {
		    cl_statfree(dbstat);
		    closedir(dd);
		    return CL_EMEM;
		}

		strcpy(dbstat->statdname[dbstat->entries - 1], dent->d_name);
#endif
		stat(fname, &dbstat->stattab[dbstat->entries - 1]);
		free(fname);
	    }
	}
    }

    closedir(dd);
    return CL_SUCCESS;
}

int cl_statchkdir(const struct cl_stat *dbstat)
{
	DIR *dd;
	struct dirent *dent;
#if defined(HAVE_READDIR_R_3) || defined(HAVE_READDIR_R_2)
	union {
	    struct dirent d;
	    char b[offsetof(struct dirent, d_name) + NAME_MAX + 1];
	} result;
#endif
	struct stat sb;
	unsigned int i, found;
	char *fname;


    if(!dbstat || !dbstat->dir) {
        cli_errmsg("cl_statdbdir(): Null argument passed.\n");
	return CL_ENULLARG;
    }

    if((dd = opendir(dbstat->dir)) == NULL) {
        cli_errmsg("cl_statdbdir(): Can't open directory %s\n", dbstat->dir);
        return CL_EOPEN;
    }

    cli_dbgmsg("Stat()ing files in %s\n", dbstat->dir);

#ifdef HAVE_READDIR_R_3
    while(!readdir_r(dd, &result.d, &dent) && dent) {
#elif defined(HAVE_READDIR_R_2)
    while((dent = (struct dirent *) readdir_r(dd, &result.d))) {
#else
    while((dent = readdir(dd))) {
#endif
#if	(!defined(C_INTERIX)) && (!defined(C_WINDOWS)) && (!defined(C_CYGWIN))
	if(dent->d_ino)
#endif
	{
	    if(strcmp(dent->d_name, ".") && strcmp(dent->d_name, "..") && CLI_DBEXT(dent->d_name)) {
                fname = cli_malloc(strlen(dbstat->dir) + strlen(dent->d_name) + 32);
		if(!fname) {
		    closedir(dd);
		    return CL_EMEM;
		}

		sprintf(fname, "%s/%s", dbstat->dir, dent->d_name);
		stat(fname, &sb);
		free(fname);

		found = 0;
		for(i = 0; i < dbstat->entries; i++)
#if defined(C_INTERIX) || defined(C_OS2)
		    if(!strcmp(dbstat->statdname[i], dent->d_name)) {
#else
		    if(dbstat->stattab[i].st_ino == sb.st_ino) {
#endif
			found = 1;
			if(dbstat->stattab[i].st_mtime != sb.st_mtime) {
			    closedir(dd);
			    return 1;
			}
		    }

		if(!found) {
		    closedir(dd);
		    return 1;
		}
	    }
	}
    }

    closedir(dd);
    return CL_SUCCESS;
}

int cl_statfree(struct cl_stat *dbstat)
{

    if(dbstat) {

#if defined(C_INTERIX) || defined(C_OS2)
	    int i;

	if(dbstat->statdname) {
	    for(i = 0; i < dbstat->entries; i++) {
		if(dbstat->statdname[i])
		    free(dbstat->statdname[i]);
		dbstat->statdname[i] = NULL;
	    }
	    free(dbstat->statdname);
	    dbstat->statdname = NULL;
	}
#endif

	if(dbstat->stattab) {
	    free(dbstat->stattab);
	    dbstat->stattab = NULL;
	}
	dbstat->entries = 0;

	if(dbstat->dir) {
	    free(dbstat->dir);
	    dbstat->dir = NULL;
	}
    } else {
        cli_errmsg("cl_statfree(): Null argument passed\n");
	return CL_ENULLARG;
    }

    return CL_SUCCESS;
}

void cl_free(struct cl_engine *engine)
{
	int i;
	struct cli_meta_node *metapt, *metah;
	struct cli_matcher *root;


    if(!engine) {
	cli_errmsg("cl_free: engine == NULL\n");
	return;
    }

#ifdef CL_THREAD_SAFE
    pthread_mutex_lock(&cli_ref_mutex);
#endif

    engine->refcount--;
    if(engine->refcount) {
#ifdef CL_THREAD_SAFE
	pthread_mutex_unlock(&cli_ref_mutex);
#endif
	return;
    }

#ifdef CL_THREAD_SAFE
    pthread_mutex_unlock(&cli_ref_mutex);
#endif

    if(engine->root) {
	for(i = 0; i < CLI_MTARGETS; i++) {
	    if((root = engine->root[i])) {
		if(!root->ac_only)
		    cli_bm_free(root);
		cli_ac_free(root);
		free(root);
	    }
	}
	free(engine->root);
    }

    if((root = engine->md5_hdb)) {
	cli_bm_free(root);
	free(root);
    }

    if((root = engine->md5_mdb)) {
	cli_bm_free(root);
	free(root->soff);
	if(root->md5_sizes_hs.capacity) {
		hashset_destroy(&root->md5_sizes_hs);
	}
	free(root);
    }

    if((root = engine->md5_fp)) {
	cli_bm_free(root);
	free(root);
    }

    metapt = engine->zip_mlist;
    while(metapt) {
	metah = metapt;
	metapt = metapt->next;
	free(metah->virname);
	if(metah->filename)
	    free(metah->filename);
	free(metah);
    }

    metapt = engine->rar_mlist;
    while(metapt) {
	metah = metapt;
	metapt = metapt->next;
	free(metah->virname);
	if(metah->filename)
	    free(metah->filename);
	free(metah);
    }

    if(((struct cli_dconf *) engine->dconf)->phishing & PHISHING_CONF_ENGINE)
	phishing_done(engine);

    if(engine->dconf)
	free(engine->dconf);

    cli_ftfree(engine->ftypes);
    cli_freeign(engine);
    free(engine);
}

static void cli_md5db_build(struct cli_matcher* root)
{
	if(root && root->md5_sizes_hs.capacity) {
		/* TODO: use hashset directly, instead of the array when matching*/
		cli_dbgmsg("Converting hashset to array: %lu entries\n", root->md5_sizes_hs.count);
		root->soff_len = hashset_toarray(&root->md5_sizes_hs, &root->soff);
		hashset_destroy(&root->md5_sizes_hs);
		qsort(root->soff, root->soff_len, sizeof(uint32_t), scomp);
	}
}


int cl_build(struct cl_engine *engine)
{
	unsigned int i;
	int ret;
	struct cli_matcher *root;


    if(!engine)
	return CL_ENULLARG;

    if(!engine->ftypes)
	if((ret = cli_loadftm(NULL, &engine, 0, 1, NULL, 0)))
	    return ret;

    for(i = 0; i < CLI_MTARGETS; i++) {
	if((root = engine->root[i])) {
	    if((ret = cli_ac_buildtrie(root)))
		return ret;
	    cli_dbgmsg("matcher[%u]: %s: AC sigs: %u BM sigs: %u %s\n", i, cli_mtargets[i].name, root->ac_patterns, root->bm_patterns, root->ac_only ? "(ac_only mode)" : "");
	}
    }

    cli_md5db_build(engine->md5_mdb);
    cli_freeign(engine);
    cli_dconf_print(engine->dconf);

    return CL_SUCCESS;
}

struct cl_engine *cl_dup(struct cl_engine *engine)
{
    if(!engine) {
	cli_errmsg("cl_dup: engine == NULL\n");
	return NULL;
    }

#ifdef CL_THREAD_SAFE
    pthread_mutex_lock(&cli_ref_mutex);
#endif

    engine->refcount++;

#ifdef CL_THREAD_SAFE
    pthread_mutex_unlock(&cli_ref_mutex);
#endif

    return engine;
}
