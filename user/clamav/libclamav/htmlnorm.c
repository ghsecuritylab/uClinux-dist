/*
 *  Normalise HTML text.
 *  Decode MS Script Encoder protection. 
 *
 *  Copyright (C) 2007-2008 Sourcefire, Inc.
 *
 *  Authors: Trog
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

#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#include <stdio.h>
#ifdef	HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#if HAVE_MMAP
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#else /* HAVE_SYS_MMAN_H */
#undef HAVE_MMAP
#endif
#endif

#include "others.h"
#include "htmlnorm.h"

typedef enum {
        INVALIDCLASS, BLOBCLASS
} object_type;
#include "blob.h"

#include "entconv.h"

#define HTML_STR_LENGTH 1024
#define MAX_TAG_CONTENTS_LENGTH HTML_STR_LENGTH

typedef enum {
    HTML_BAD_STATE,
    HTML_NORM,
    HTML_COMMENT,
    HTML_CHAR_REF,
    HTML_ENTITY_REF_DECODE,
    HTML_SKIP_WS,
    HTML_TRIM_WS,
    HTML_TAG,
    HTML_TAG_ARG,
    HTML_TAG_ARG_VAL,
    HTML_TAG_ARG_EQUAL,
    HTML_PROCESS_TAG,
    HTML_CHAR_REF_DECODE,
    HTML_SKIP_LENGTH,
    HTML_JSDECODE,
    HTML_JSDECODE_LENGTH,
    HTML_JSDECODE_DECRYPT,
    HTML_SPECIAL_CHAR,
    HTML_RFC2397_TYPE,
    HTML_RFC2397_INIT,
    HTML_RFC2397_DATA,
    HTML_RFC2397_FINISH,
    HTML_RFC2397_ESC,
    HTML_ESCAPE_CHAR
} html_state;

typedef enum {
    SINGLE_QUOTED,
    DOUBLE_QUOTED,
    NOT_QUOTED
} quoted_state;


#define HTML_FILE_BUFF_LEN 8192

typedef struct file_buff_tag {
	int fd;
	unsigned char buffer[HTML_FILE_BUFF_LEN];
	int length;
} file_buff_t;

static const int base64_chars[256] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
};

int table_order[] = {
       00, 02, 01, 00, 02, 01, 02, 01, 01, 02, 01, 02, 00, 01, 02, 01,
       00, 01, 02, 01, 00, 00, 02, 01, 01, 02, 00, 01, 02, 01, 01, 02,
       00, 00, 01, 02, 01, 02, 01, 00, 01, 00, 00, 02, 01, 00, 01, 02,
       00, 01, 02, 01, 00, 00, 02, 01, 01, 00, 00, 02, 01, 00, 01, 02
};

int decrypt_tables[3][128] = {
      {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x57, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
       0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
       0x2E, 0x47, 0x7A, 0x56, 0x42, 0x6A, 0x2F, 0x26, 0x49, 0x41, 0x34, 0x32, 0x5B, 0x76, 0x72, 0x43,
       0x38, 0x39, 0x70, 0x45, 0x68, 0x71, 0x4F, 0x09, 0x62, 0x44, 0x23, 0x75, 0x3C, 0x7E, 0x3E, 0x5E,
       0xFF, 0x77, 0x4A, 0x61, 0x5D, 0x22, 0x4B, 0x6F, 0x4E, 0x3B, 0x4C, 0x50, 0x67, 0x2A, 0x7D, 0x74,
       0x54, 0x2B, 0x2D, 0x2C, 0x30, 0x6E, 0x6B, 0x66, 0x35, 0x25, 0x21, 0x64, 0x4D, 0x52, 0x63, 0x3F,
       0x7B, 0x78, 0x29, 0x28, 0x73, 0x59, 0x33, 0x7F, 0x6D, 0x55, 0x53, 0x7C, 0x3A, 0x5F, 0x65, 0x46,
       0x58, 0x31, 0x69, 0x6C, 0x5A, 0x48, 0x27, 0x5C, 0x3D, 0x24, 0x79, 0x37, 0x60, 0x51, 0x20, 0x36},

      {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x7B, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
       0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
       0x32, 0x30, 0x21, 0x29, 0x5B, 0x38, 0x33, 0x3D, 0x58, 0x3A, 0x35, 0x65, 0x39, 0x5C, 0x56, 0x73,
       0x66, 0x4E, 0x45, 0x6B, 0x62, 0x59, 0x78, 0x5E, 0x7D, 0x4A, 0x6D, 0x71, 0x3C, 0x60, 0x3E, 0x53,
       0xFF, 0x42, 0x27, 0x48, 0x72, 0x75, 0x31, 0x37, 0x4D, 0x52, 0x22, 0x54, 0x6A, 0x47, 0x64, 0x2D,
       0x20, 0x7F, 0x2E, 0x4C, 0x5D, 0x7E, 0x6C, 0x6F, 0x79, 0x74, 0x43, 0x26, 0x76, 0x25, 0x24, 0x2B,
       0x28, 0x23, 0x41, 0x34, 0x09, 0x2A, 0x44, 0x3F, 0x77, 0x3B, 0x55, 0x69, 0x61, 0x63, 0x50, 0x67,
       0x51, 0x49, 0x4F, 0x46, 0x68, 0x7C, 0x36, 0x70, 0x6E, 0x7A, 0x2F, 0x5F, 0x4B, 0x5A, 0x2C, 0x57},

      {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x6E, 0x0A, 0x0B, 0x0C, 0x06, 0x0E, 0x0F,
       0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
       0x2D, 0x75, 0x52, 0x60, 0x71, 0x5E, 0x49, 0x5C, 0x62, 0x7D, 0x29, 0x36, 0x20, 0x7C, 0x7A, 0x7F,
       0x6B, 0x63, 0x33, 0x2B, 0x68, 0x51, 0x66, 0x76, 0x31, 0x64, 0x54, 0x43, 0x3C, 0x3A, 0x3E, 0x7E,
       0xFF, 0x45, 0x2C, 0x2A, 0x74, 0x27, 0x37, 0x44, 0x79, 0x59, 0x2F, 0x6F, 0x26, 0x72, 0x6A, 0x39,
       0x7B, 0x3F, 0x38, 0x77, 0x67, 0x53, 0x47, 0x34, 0x78, 0x5D, 0x30, 0x23, 0x5A, 0x5B, 0x6C, 0x48,
       0x55, 0x70, 0x69, 0x2E, 0x4C, 0x21, 0x24, 0x4E, 0x50, 0x09, 0x56, 0x73, 0x35, 0x61, 0x4B, 0x58,
       0x3B, 0x57, 0x22, 0x6D, 0x4D, 0x25, 0x28, 0x46, 0x4A, 0x32, 0x41, 0x3D, 0x5F, 0x4F, 0x42, 0x65}
};

static inline unsigned int rewind_tospace(const unsigned char* chunk, unsigned int len)
{
	unsigned int count = len;
	while (!isspace(chunk[len - 1]) && (len > 1)) {
		len--;
	}
	if (len == 1) {
		return count;
	}
	return len;
}

/* read at most @max_len of data from @m_area or @stream, skipping NULL chars.
 * This used to be called cli_readline, but we don't stop at end-of-line anymore */
static unsigned char *cli_readchunk(FILE *stream, m_area_t *m_area, unsigned int max_len)
{
	unsigned char *chunk, *start, *ptr, *end;
	unsigned int chunk_len, count;

	chunk = (unsigned char *) cli_malloc(max_len);
	if (!chunk) {
		return NULL;
	}

	/* Try and use the memory buffer first */
	if (m_area) {
		start = ptr = m_area->buffer + m_area->offset;
		end = m_area->buffer + m_area->length;
		if (start >= end) {
			free(chunk);
			return NULL;
		}
		/* maximum we can copy into the buffer,
		 * we could have less than max_len bytes available */
		chunk_len = MIN(end-start, max_len-1);

		/* look for NULL chars */
		ptr = memchr(start, 0, chunk_len);
	        if(!ptr) {
			/* no NULL chars found, copy all */
			memcpy(chunk, start, chunk_len);
			chunk[chunk_len] = '\0';
			m_area->offset += chunk_len;
			/* point ptr to end of chunk,
			 * so we can check and rewind to a space below */
			ptr = start + chunk_len;
		} else {
			/* copy portion that doesn't contain NULL chars */
			chunk_len = ptr - start;
			if(chunk_len < max_len) {
				memcpy(chunk, start, chunk_len);
			} else {
				chunk_len = 0;
				ptr = start;
			}
			/* we have unknown number of NULL chars,
			 * copy char-by-char and skip them */
			while((ptr < end) && (chunk_len < max_len-1)) {
				const unsigned char c = *ptr++;
				if(c) {
					chunk[chunk_len++] = c;
				}
			}
			chunk[chunk_len] = '\0';
			/* we can't use chunk_len to determine how many bytes we read, since
			 * we skipped chars */
			m_area->offset = ptr - m_area->buffer;
		}
		if(ptr && ptr < end && !isspace(*ptr)) {
			/* we hit max_len, rewind to a space */
			count = rewind_tospace(chunk, chunk_len);
			if(count < chunk_len) {
				chunk[count] = '\0';
				m_area->offset -= chunk_len - count;
			}
		}
	} else {
		if (!stream) {
			cli_dbgmsg("No HTML stream\n");
			free(chunk);
			return NULL;
		}
		chunk_len = fread(chunk, 1, max_len-1, stream);
		if(!chunk_len || chunk_len > max_len-1) {
			/* EOF, or prevent overflow */
			free(chunk);
			return NULL;
		}

		/* Look for NULL chars */
		ptr = memchr(chunk, 0, chunk_len);
		if(ptr) {
			/* NULL char found */
			/* save buffer limits */
		        start = ptr;
			end = chunk + chunk_len;

			/* start of NULL chars, we will copy non-NULL characters
			 * to this position */
			chunk_len = ptr - chunk;

			/* find first non-NULL char */
			while((ptr < end) && !(*ptr)) {
				ptr++;
			}
			/* skip over NULL chars, and move back the rest */
		        while((ptr < end) && (chunk_len < max_len-1)) {
				const unsigned char c = *ptr++;
				if(c) {
					chunk[chunk_len++] = c;
				}
			}
			chunk[chunk_len] = '\0';
		}
		if(chunk_len == max_len - 1) {
			/* rewind to a space (which includes newline) */
			count = rewind_tospace(chunk, chunk_len);
			if(count < chunk_len) {
				chunk[count] = '\0';
				/* seek-back to space */
				fseek(stream, (long)(count - chunk_len), SEEK_CUR);
			}
		}
	}

	return chunk;
}

static void html_output_flush(file_buff_t *fbuff)
{
	if (fbuff && (fbuff->length > 0)) {
		cli_writen(fbuff->fd, fbuff->buffer, fbuff->length);
		fbuff->length = 0;
	}
}

static inline void html_output_c(file_buff_t *fbuff1, unsigned char c)
{
	if (fbuff1) {
		if (fbuff1->length == HTML_FILE_BUFF_LEN) {
			html_output_flush(fbuff1);
		}
		fbuff1->buffer[fbuff1->length++] = c;
	}
}

static void html_output_str(file_buff_t *fbuff, const unsigned char *str, int len)
{
	if (fbuff) {
		if ((fbuff->length + len) >= HTML_FILE_BUFF_LEN) {
			html_output_flush(fbuff);
		}
		if (len >= HTML_FILE_BUFF_LEN) {
			html_output_flush(fbuff);
			cli_writen(fbuff->fd, str, len);
		} else {
			memcpy(fbuff->buffer + fbuff->length, str, len);
			fbuff->length += len;
		}
	}
}

static char *html_tag_arg_value(tag_arguments_t *tags, const char *tag)
{
	int i;
	
	for (i=0; i < tags->count; i++) {
		if (strcmp(tags->tag[i], tag) == 0) {
			return tags->value[i];
		}
	}
	return NULL;
}

static void html_tag_arg_set(tag_arguments_t *tags, const char *tag, const char *value)
{
	int i;
	
	for (i=0; i < tags->count; i++) {
		if (strcmp(tags->tag[i], tag) == 0) {
			free(tags->value[i]);
			tags->value[i] = cli_strdup(value);
			return;
		}
	}
	return;
}
static void html_tag_arg_add(tag_arguments_t *tags,
		const unsigned char *tag, unsigned char *value)
{
	int len, i;
	tags->count++;
	tags->tag = (unsigned char **) cli_realloc2(tags->tag,
				tags->count * sizeof(char *));
	if (!tags->tag) {
		goto abort;
	}
	tags->value = (unsigned char **) cli_realloc2(tags->value,
				tags->count * sizeof(char *));
	if (!tags->value) {
		goto abort;
	}
	if(tags->scanContents) {
		tags->contents= (blob **) cli_realloc2(tags->contents,
				tags->count*sizeof(*tags->contents));
		if(!tags->contents) {
			goto abort;
		}
		tags->contents[tags->count-1]=NULL;
	}
	tags->tag[tags->count-1] = cli_strdup(tag);
	if (value) {
		if (*value == '"') {
			tags->value[tags->count-1] = cli_strdup(value+1);
			len = strlen(value+1);
			if (len > 0) {
				tags->value[tags->count-1][len-1] = '\0';
			}
		} else {
			tags->value[tags->count-1] = cli_strdup(value);
		}
	} else {
		tags->value[tags->count-1] = NULL;
	}
	return;
	
abort:
	/* Bad error - can't do 100% recovery */
	tags->count--;
	for (i=0; i < tags->count; i++) {
		if (tags->tag) {
			free(tags->tag[i]);
		}
		if (tags->value) {
			free(tags->value[i]);
		}
		if(tags->contents) {
			if(tags->contents[i])
				blobDestroy(tags->contents[i]);
		}
	}
	if (tags->tag) {
		free(tags->tag);
	}
	if (tags->value) {
		free(tags->value);
	}
	if (tags->contents)
		free(tags->contents);
	tags->contents=NULL;
	tags->tag = tags->value = NULL;
	tags->count = 0;	
	return;
}

static void html_output_tag(file_buff_t *fbuff, char *tag, tag_arguments_t *tags)
{
	int i, j, len;

	html_output_c(fbuff, '<');
	html_output_str(fbuff, tag, strlen(tag));
	for (i=0; i < tags->count; i++) {
		html_output_c(fbuff, ' ');
		html_output_str(fbuff, tags->tag[i], strlen(tags->tag[i]));
		if (tags->value[i]) {
			html_output_str(fbuff, "=\"", 2);
			len = strlen(tags->value[i]);
			for (j=0 ; j<len ; j++) {
				html_output_c(fbuff, tolower(tags->value[i][j]));
			}
			html_output_c(fbuff, '"');
		}
	}
	html_output_c(fbuff, '>');
}

void html_tag_arg_free(tag_arguments_t *tags)
{
	int i;
	
	for (i=0; i < tags->count; i++) {
		free(tags->tag[i]);
		if (tags->value[i]) {
			free(tags->value[i]);
		}
		if(tags->contents)
			if (tags->contents[i])
				blobDestroy(tags->contents[i]);
	}
	if (tags->tag) {
		free(tags->tag);
	}
	if (tags->value) {
		free(tags->value);
	}
	if(tags->contents)
		free(tags->contents);
	tags->contents = NULL;
	tags->tag = tags->value = NULL;
	tags->count = 0;
}

/**
 * this is used for img, and iframe tags. If they are inside an <a href> tag, then set the contents of the image|iframe to the real URL.
 */
static inline void html_tag_set_inahref(tag_arguments_t *tags,int idx,int in_ahref)
{
	tags->contents[idx-1]=blobCreate();
	blobAddData(tags->contents[idx-1],tags->value[in_ahref-1],strlen(tags->value[in_ahref-1]));
	blobAddData(tags->contents[idx-1], "",1);
	blobClose(tags->contents[idx-1]);
}

/**
 * the displayed text for an <a href> tag
 */
static inline void html_tag_contents_append(tag_arguments_t *tags,int idx,const unsigned char* begin,const unsigned char *end)
{
	if(end && (begin<end)) {
		const size_t blob_len = blobGetDataSize(tags->contents[idx-1]);
		const size_t blob_sizeleft = blob_len <= MAX_TAG_CONTENTS_LENGTH ? (MAX_TAG_CONTENTS_LENGTH - blob_len) : 0;
		const size_t str_len = end - begin;
		if(blob_sizeleft)
			blobAddData(tags->contents[idx-1],begin, blob_sizeleft < str_len ? blob_sizeleft : str_len );
	}
}


static inline void html_tag_contents_done(tag_arguments_t *tags,int idx)
{
	/* append NUL byte */
	blobAddData(tags->contents[idx-1], "", 1);
	blobClose(tags->contents[idx-1]);
}

static int cli_html_normalise(int fd, m_area_t *m_area, const char *dirname, tag_arguments_t *hrefs,const struct cli_dconf* dconf)
{
	int fd_tmp, tag_length, tag_arg_length, binary;
	int retval=FALSE, escape, value = 0, hex, tag_val_length=0, table_pos, in_script=FALSE, text_space_written=FALSE;
	FILE *stream_in = NULL;
	html_state state=HTML_NORM, next_state=HTML_BAD_STATE;
	char filename[1024], tag[HTML_STR_LENGTH+1], tag_arg[HTML_STR_LENGTH+1];
	char tag_val[HTML_STR_LENGTH+1], *tmp_file;
	unsigned char *line, *ptr, *arg_value;
	tag_arguments_t tag_args;
	quoted_state quoted;
	unsigned long length;
	file_buff_t *file_buff_o2, *file_buff_text;
	file_buff_t *file_tmp_o1;
	int in_ahref=0;/* index of <a> tag, whose contents we are parsing. Indexing starts from 1, 0 means outside of <a>*/
	unsigned char* href_contents_begin=NULL;/*beginning of the next portion of <a> contents*/
	unsigned char* ptrend=NULL;/*end of <a> contents*/
	unsigned char* in_form_action = NULL;/* the action URL of the current <form> tag, if any*/

	struct entity_conv conv;
	int rc;
	unsigned char entity_val[HTML_STR_LENGTH+1];
	size_t entity_val_length = 0;
	const int dconf_entconv = dconf && dconf->phishing&PHISHING_CONF_ENTCONV;
	/* dconf for phishing engine sets scanContents, so no need for a flag here */


	tag_args.scanContents=0;/* do we need to store the contents of <a></a>?*/
	if (!m_area) {
		if (fd < 0) {
			cli_dbgmsg("Invalid HTML fd\n");
			return FALSE;
		}
		lseek(fd, 0, SEEK_SET);	
		fd_tmp = dup(fd);
		if (fd_tmp < 0) {
			return FALSE;
		}
		stream_in = fdopen(fd_tmp, "r");
		if (!stream_in) {
			close(fd_tmp);
			return FALSE;
		}
	}

	tag_args.count = 0;
	tag_args.tag = NULL;
	tag_args.value = NULL;
	tag_args.contents = NULL;
	if (dirname) {
		snprintf(filename, 1024, "%s/rfc2397", dirname);
		if (mkdir(filename, 0700) && errno != EEXIST) {
			file_buff_o2 = file_buff_text = NULL;
			goto abort;
		}

		file_buff_o2 = (file_buff_t *) cli_malloc(sizeof(file_buff_t));
		if (!file_buff_o2) {
			file_buff_o2 = file_buff_text = NULL;
			goto abort;
		}

		/* this will still contains scripts that are inside comments */
		snprintf(filename, 1024, "%s/nocomment.html", dirname);
		file_buff_o2->fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR|S_IRUSR);
		if (!file_buff_o2->fd) {
			cli_dbgmsg("open failed: %s\n", filename);
			free(file_buff_o2);
			file_buff_o2 = file_buff_text = NULL;
			goto abort;
		}

		file_buff_text = (file_buff_t *) cli_malloc(sizeof(file_buff_t));
		if(!file_buff_text) {
			free(file_buff_o2);
			file_buff_o2 = file_buff_text = NULL;
			goto abort;
		}

		snprintf(filename, 1024, "%s/notags.html", dirname);
		file_buff_text->fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR|S_IRUSR);
		if(!file_buff_text->fd) {
			cli_dbgmsg("open failed: %s\n", filename);
			close(file_buff_o2->fd);
			free(file_buff_o2);
			free(file_buff_text);
			file_buff_o2 = file_buff_text = NULL;
		}
		file_buff_o2->length = 0;
		file_buff_text->length = 0;
	} else {
		file_buff_o2 = NULL;
		file_buff_text = NULL;
	}

	binary = FALSE;

	ptr = line = cli_readchunk(stream_in, m_area, 8192);

	while (line) {
		if(href_contents_begin)
			href_contents_begin=ptr;/*start of a new line, last line already appended to contents see below*/
		while (*ptr && isspace(*ptr)) {
			ptr++;
		}
		while (*ptr) {
			if (!binary && *ptr == '\n') {
				/* Convert it to a space and re-process */
				*ptr = ' ';
				continue;
			}
			if (!binary && *ptr == '\r') {
				ptr++;
				continue;
			}
			switch (state) {
			case HTML_SPECIAL_CHAR:
				cli_dbgmsg("Impossible, special_char can't occur here\n");
				break;
			case HTML_BAD_STATE:
				/* An engine error has occurred */
				cli_dbgmsg("HTML Engine Error\n");
				goto abort;
			case HTML_SKIP_LENGTH:
				length--;
				ptr++;
				if (!length) {
					state = next_state;
				}
				break;
			case HTML_SKIP_WS:
				if (isspace(*ptr)) {
					ptr++;
				} else {
					state = next_state;
					next_state = HTML_BAD_STATE;
				}
				break;
			case HTML_TRIM_WS:
				if (isspace(*ptr)) {
					ptr++;
				} else {
					if(!in_script)
						html_output_c(file_buff_o2, ' ');
					state = next_state;
					next_state = HTML_BAD_STATE;
				}
				break;
			case HTML_NORM:
				if (*ptr == '<') {
					ptrend=ptr; /* for use by scanContents */
					html_output_c(file_buff_o2, '<');
					if (!in_script && !text_space_written) {
						html_output_c(file_buff_text, ' ');
						text_space_written = TRUE;
					}
					if(hrefs && hrefs->scanContents && in_ahref && href_contents_begin) {
						/*append this text portion to the contents of <a>*/
						html_tag_contents_append(hrefs,in_ahref,href_contents_begin,ptr);
						href_contents_begin=NULL;/*We just encountered another tag inside <a>, so skip it*/
					}
					ptr++;
					state = HTML_SKIP_WS;
					tag_length=0;
					next_state = HTML_TAG;
				} else if (isspace(*ptr)) {
					if(!text_space_written && !in_script) {
						html_output_c(file_buff_text, ' ');
						text_space_written = TRUE;
					}
					state = HTML_TRIM_WS;
					next_state = HTML_NORM;
				} else if (*ptr == '&') {
					if(!text_space_written && !in_script) {
						html_output_c(file_buff_text, ' ');
						text_space_written = TRUE;
					}
					state = HTML_CHAR_REF;
					next_state = HTML_NORM;
					ptr++;
				} else {
					unsigned char c = tolower(*ptr);
					/* normalize ' to " for scripts */
					if(in_script && c == '\'') c = '"';
					html_output_c(file_buff_o2, c);
					if (!in_script) {
						if(*ptr < 0x20) {
							if(!text_space_written) {
								html_output_c(file_buff_text, ' ');
								text_space_written = TRUE;
							}
						} else {
							html_output_c(file_buff_text, c);
							text_space_written = FALSE;
						}
					}
					ptr++;
				}
				break;
			case HTML_TAG:
				if ((tag_length == 0) && (*ptr == '!')) {
					/* Comment */
					if (in_script) {
						/* we still write scripts to nocomment.html */
						html_output_c(file_buff_o2, '!');
					} else {
						/* Need to rewind in the no-comment output stream */
						if (file_buff_o2 && (file_buff_o2->length > 0)) {
							file_buff_o2->length--;
						}
					}
					state = HTML_COMMENT;
					next_state = HTML_BAD_STATE;
					ptr++;
				} else if (*ptr == '>') {
					html_output_c(file_buff_o2, '>');
					ptr++;
					tag[tag_length] = '\0';
					state = HTML_SKIP_WS;
					next_state = HTML_PROCESS_TAG;
				} else if (!isspace(*ptr)) {
					html_output_c(file_buff_o2, tolower(*ptr));
					/* if we're inside a script we only care for </script>.*/
					if(in_script && tag_length==0 && *ptr != '/') {
						state = HTML_NORM;
					}
					if (tag_length < HTML_STR_LENGTH) {
						tag[tag_length++] = tolower(*ptr);
					}
					ptr++;
				}  else {
					tag[tag_length] = '\0';
					state = HTML_SKIP_WS;
					tag_arg_length = 0;
					/* if we'd go to HTML_TAG_ARG whitespace would be inconsistently normalized for in_script*/
					next_state = !in_script ? HTML_TAG_ARG : HTML_PROCESS_TAG;
				}
				break;
			case HTML_TAG_ARG:
				if (*ptr == '=') {
					html_output_c(file_buff_o2, '=');
					tag_arg[tag_arg_length] = '\0';
					ptr++;
					state = HTML_SKIP_WS;
					escape = FALSE;
					quoted = NOT_QUOTED;
					tag_val_length = 0;
					next_state = HTML_TAG_ARG_VAL;
				} else if (isspace(*ptr)) {
					ptr++;
					tag_arg[tag_arg_length] = '\0';
					state = HTML_SKIP_WS;
					next_state = HTML_TAG_ARG_EQUAL;
				} else if (*ptr == '>') {
					html_output_c(file_buff_o2, '>');
					if (tag_arg_length > 0) {
						tag_arg[tag_arg_length] = '\0';
						html_tag_arg_add(&tag_args, tag_arg, NULL);
					}
					ptr++;
					state = HTML_PROCESS_TAG;
					next_state = HTML_BAD_STATE;
				} else {
					if (tag_arg_length == 0) {
						/* Start of new tag - add space */
						html_output_c(file_buff_o2,' ');
					}
					html_output_c(file_buff_o2, tolower(*ptr));
					if (tag_arg_length < HTML_STR_LENGTH) {
						tag_arg[tag_arg_length++] = tolower(*ptr);
					}
					ptr++;
				}
				break;
			case HTML_TAG_ARG_EQUAL:
				if (*ptr == '=') {
					html_output_c(file_buff_o2, '=');
					ptr++;
					state = HTML_SKIP_WS;
					escape = FALSE;
					quoted = NOT_QUOTED;
					tag_val_length = 0;
					next_state = HTML_TAG_ARG_VAL;
				} else {
					if (tag_arg_length > 0) {
						tag_arg[tag_arg_length] = '\0';
						html_tag_arg_add(&tag_args, tag_arg, NULL);
					}
					tag_arg_length=0;
					state = HTML_TAG_ARG;
					next_state = HTML_BAD_STATE;
				}
				break;
			case HTML_TAG_ARG_VAL:
				if ((tag_val_length == 5) && (strncmp(tag_val, "data:", 5) == 0)) {
					/* RFC2397 inline data */

					/* Rewind one byte so we don't recursuive */
					if (file_buff_o2 && (file_buff_o2->length > 0)) {
						file_buff_o2->length--;
					}

					if (quoted != NOT_QUOTED) {
						html_output_c(file_buff_o2, '"');
					}
					tag_val_length = 0;
					state = HTML_RFC2397_TYPE;
					next_state = HTML_TAG_ARG;
				} else if ((tag_val_length == 6) && (strncmp(tag_val, "\"data:", 6) == 0)) {
					/* RFC2397 inline data */

					/* Rewind one byte so we don't recursuive */
					if (file_buff_o2 && (file_buff_o2->length > 0)) {
						file_buff_o2->length--;
					}

					if (quoted != NOT_QUOTED) {
						html_output_c(file_buff_o2, '"');
					}

					tag_val_length = 0;
					state = HTML_RFC2397_TYPE;
					next_state = HTML_TAG_ARG;
				} else if (*ptr == '&') {
					state = HTML_CHAR_REF;
					next_state = HTML_TAG_ARG_VAL;
					ptr++;
				} else if (*ptr == '\'') {
					if (tag_val_length == 0) {
						quoted = SINGLE_QUOTED;
						html_output_c(file_buff_o2, '"');
						if (tag_val_length < HTML_STR_LENGTH) {
							tag_val[tag_val_length++] = '"';
						}
						ptr++;
					} else {
						if (!escape && (quoted==SINGLE_QUOTED)) {
							html_output_c(file_buff_o2, '"');
							if (tag_val_length < HTML_STR_LENGTH) {
								tag_val[tag_val_length++] = '"';
							}
							tag_val[tag_val_length] = '\0';
							html_tag_arg_add(&tag_args, tag_arg, tag_val);
							ptr++;
							state = HTML_SKIP_WS;
							tag_arg_length=0;
							next_state = HTML_TAG_ARG;
						} else {
							html_output_c(file_buff_o2, '"');
							if (tag_val_length < HTML_STR_LENGTH) {
								tag_val[tag_val_length++] = '"';
							}
							ptr++;
						}
					}
				} else if (*ptr == '"') {
					if (tag_val_length == 0) {
						quoted = DOUBLE_QUOTED;
						html_output_c(file_buff_o2, '"');
						if (tag_val_length < HTML_STR_LENGTH) {
							tag_val[tag_val_length++] = '"';
						}
						ptr++;
					} else {
						if (!escape && (quoted==DOUBLE_QUOTED)) {
							html_output_c(file_buff_o2, '"');
							if (tag_val_length < HTML_STR_LENGTH) {
								tag_val[tag_val_length++] = '"';
							}
							tag_val[tag_val_length] = '\0';
							html_tag_arg_add(&tag_args, tag_arg, tag_val);
							ptr++;
							state = HTML_SKIP_WS;
							tag_arg_length=0;
							next_state = HTML_TAG_ARG;
						} else {
							html_output_c(file_buff_o2, '"');
							if (tag_val_length < HTML_STR_LENGTH) {
								tag_val[tag_val_length++] = '"';
							}
							ptr++;
						}
					}
				} else if (isspace(*ptr) || (*ptr == '>')) {
					if (quoted == NOT_QUOTED) {
						tag_val[tag_val_length] = '\0';
						html_tag_arg_add(&tag_args, tag_arg, tag_val);
						state = HTML_SKIP_WS;
						tag_arg_length=0;
						next_state = HTML_TAG_ARG;
					} else {
						html_output_c(file_buff_o2, *ptr);
						if (tag_val_length < HTML_STR_LENGTH) {
							if (isspace(*ptr)) {
								tag_val[tag_val_length++] = ' ';
							} else {
								tag_val[tag_val_length++] = '>';
							}
						}
						state = HTML_SKIP_WS;
						escape = FALSE;
						quoted = NOT_QUOTED;
						next_state = HTML_TAG_ARG_VAL;
						ptr++;
					}
				} else {
					html_output_c(file_buff_o2, tolower(*ptr));
					if (tag_val_length < HTML_STR_LENGTH) {
						tag_val[tag_val_length++] = *ptr;
					}
					ptr++;
				}

				if (*ptr == '\\') {
					escape = TRUE;
				} else {
					escape = FALSE;
				}
				break;
			case HTML_COMMENT:
				if (in_script && !isspace(*ptr)) {
					unsigned char c = tolower(*ptr);
					/* dump script to nocomment.html, since we no longer have
					 * comment.html/script.html */
					if(c == '\'') c = '"';
					html_output_c(file_buff_o2, c);
				}
				if (*ptr == '>') {
					state = HTML_SKIP_WS;
					next_state = HTML_NORM;
				}
				ptr++;
				break;
			case HTML_PROCESS_TAG:

				/* Default to no action for this tag */
				state = HTML_SKIP_WS;
				next_state = HTML_NORM;
				if (tag[0] == '/') {
					/* End tag */
					state = HTML_SKIP_WS;
					next_state = HTML_NORM;
					if (strcmp(tag, "/script") == 0) {
						in_script=FALSE;
						/*don't output newlines in nocomment.html
						 * html_output_c(file_buff_o2, '\n');*/
					}
					if (hrefs && hrefs->scanContents && in_ahref) {
						if(strcmp(tag,"/a") == 0) {
							html_tag_contents_done(hrefs,in_ahref);
							in_ahref=0;/* we are no longer inside an <a href>
							nesting <a> tags not supported, and shouldn't be supported*/
						}
						href_contents_begin=ptr;
					}
					if (strcmp(tag, "/form") == 0)  {
						if (in_form_action)
							free(in_form_action);
						in_form_action = NULL;
					}
				} else if (strcmp(tag, "script") == 0) {
					arg_value = html_tag_arg_value(&tag_args, "language");
					/* TODO: maybe we can output all tags only via html_output_tag */
					if (arg_value && (strcasecmp(arg_value, "jscript.encode") == 0)) {
						html_tag_arg_set(&tag_args, "language", "javascript");
						state = HTML_SKIP_WS;
						next_state = HTML_JSDECODE;
						/* we already output the old tag, output the new tag now */
						html_output_tag(file_buff_o2, tag, &tag_args);
					} else if (arg_value && (strcasecmp(arg_value, "vbscript.encode") == 0)) {
						html_tag_arg_set(&tag_args, "language", "vbscript");
						state = HTML_SKIP_WS;
						next_state = HTML_JSDECODE;
						/* we already output the old tag, output the new tag now */
						html_output_tag(file_buff_o2, tag, &tag_args);
					} else {
						in_script = TRUE;
					}
				} else if (hrefs) {
					if(in_ahref && !href_contents_begin)
						href_contents_begin=ptr;
					if (strcmp(tag, "a") == 0) {
						arg_value = html_tag_arg_value(&tag_args, "href");
						if (arg_value && strlen(arg_value) > 0) {
							if (hrefs->scanContents) {
								unsigned char* arg_value_title = html_tag_arg_value(&tag_args,"title");
								/*beginning of an <a> tag*/
								if (in_ahref)
									/*we encountered nested <a> tags, pretend previous closed*/
									if (href_contents_begin) {
										html_tag_contents_append(hrefs,in_ahref,
											href_contents_begin,ptrend);
										/*add pending contents between tags*/
										html_tag_contents_done(hrefs,in_ahref);
										in_ahref=0;
										}
								if (arg_value_title) {
									/* title is a 'displayed link'*/
									html_tag_arg_add(hrefs,"href_title",arg_value_title);
									hrefs->contents[hrefs->count-1]=blobCreate();
									html_tag_contents_append(hrefs,hrefs->count,arg_value,
										arg_value+strlen(arg_value));
									html_tag_contents_done(hrefs,hrefs->count);
								}
								if (in_form_action) {
									/* form action is the real URL, and href is the 'displayed' */
									html_tag_arg_add(hrefs,"form",arg_value);
									hrefs->contents[hrefs->count-1] =  blobCreate();
									html_tag_contents_append(hrefs, hrefs->count, in_form_action,
											in_form_action + strlen(in_form_action));
									html_tag_contents_done(hrefs,hrefs->count);
								}
							}
							html_tag_arg_add(hrefs, "href", arg_value);
							if (hrefs->scanContents) {
								in_ahref=hrefs->count; /* index of this tag (counted from 1) */
								href_contents_begin=ptr;/* contents begin after <a ..> ends */
								hrefs->contents[hrefs->count-1]=blobCreate();
							}
						}
					} else if (strcmp(tag,"form") == 0 && hrefs->scanContents) {
						const unsigned char* arg_action_value = html_tag_arg_value(&tag_args,"action");
						if (arg_action_value) {
							if(in_form_action)
								free(in_form_action);
							in_form_action = cli_strdup(arg_action_value);
						}
					} else if (strcmp(tag, "img") == 0) {
						arg_value = html_tag_arg_value(&tag_args, "src");
						if (arg_value && strlen(arg_value) > 0) {
							html_tag_arg_add(hrefs, "src", arg_value);
							if(hrefs->scanContents && in_ahref)
								/* "contents" of an img tag, is the URL of its parent <a> tag */
								html_tag_set_inahref(hrefs,hrefs->count,in_ahref);
							if (in_form_action) {
								/* form action is the real URL, and href is the 'displayed' */
								html_tag_arg_add(hrefs,"form",arg_value);
								hrefs->contents[hrefs->count-1] =  blobCreate();
								html_tag_contents_append(hrefs, hrefs->count, in_form_action,
										in_form_action + strlen(in_form_action));
								html_tag_contents_done(hrefs,hrefs->count);
							}
						}
						arg_value = html_tag_arg_value(&tag_args, "dynsrc");
						if (arg_value && strlen(arg_value) > 0) {
							html_tag_arg_add(hrefs, "dynsrc", arg_value);
							if(hrefs->scanContents && in_ahref)
								/* see above */
								html_tag_set_inahref(hrefs,hrefs->count,in_ahref);
							if (in_form_action) {
								/* form action is the real URL, and href is the 'displayed' */
								html_tag_arg_add(hrefs,"form",arg_value);
								hrefs->contents[hrefs->count-1] =  blobCreate();
								html_tag_contents_append(hrefs, hrefs->count, in_form_action,
										in_form_action + strlen(in_form_action));
								html_tag_contents_done(hrefs,hrefs->count);
							}
						}
					} else if (strcmp(tag, "iframe") == 0) {
						arg_value = html_tag_arg_value(&tag_args, "src");
						if (arg_value && strlen(arg_value) > 0) {
							html_tag_arg_add(hrefs, "iframe", arg_value);
							if(hrefs->scanContents && in_ahref)
								/* see above */
								html_tag_set_inahref(hrefs,hrefs->count,in_ahref);
							if (in_form_action) {
								/* form action is the real URL, and href is the 'displayed' */
								html_tag_arg_add(hrefs,"form",arg_value);
								hrefs->contents[hrefs->count-1] =  blobCreate();
								html_tag_contents_append(hrefs, hrefs->count, in_form_action,
										in_form_action + strlen(in_form_action));
								html_tag_contents_done(hrefs,hrefs->count);
							}
						}
					} else if (strcmp(tag,"area") == 0) {
						arg_value = html_tag_arg_value(&tag_args,"href");
						if (arg_value && strlen(arg_value) > 0) {
							html_tag_arg_add(hrefs, "area", arg_value);
							if(hrefs->scanContents && in_ahref)
								/* see above */
								html_tag_set_inahref(hrefs,hrefs->count,in_ahref);
							if (in_form_action) {
								/* form action is the real URL, and href is the 'displayed' */
								html_tag_arg_add(hrefs,"form",arg_value);
								hrefs->contents[hrefs->count-1] =  blobCreate();
								html_tag_contents_append(hrefs, hrefs->count, in_form_action,
									in_form_action + strlen(in_form_action));
								html_tag_contents_done(hrefs,hrefs->count);
							}
						}
					}
					/* TODO:imagemaps can have urls too */
				} else if (strcmp(tag, "a") == 0) {
					/* a/img tags for buff_text can be processed only if we're not processing hrefs */
					arg_value = html_tag_arg_value(&tag_args, "href");
					if(arg_value && arg_value[0]) {
						html_output_str(file_buff_text, arg_value, strlen(arg_value));
						html_output_c(file_buff_text, ' ');
						text_space_written = TRUE;
					}
				} else if (strcmp(tag, "img") == 0) {
					arg_value = html_tag_arg_value(&tag_args, "src");
					if(arg_value && arg_value[0]) {
						html_output_str(file_buff_text, arg_value, strlen(arg_value));
						html_output_c(file_buff_text, ' ');
						text_space_written = TRUE;
					}
				}
				html_tag_arg_free(&tag_args);
				break;
			case HTML_CHAR_REF:
				if (*ptr == '#') {
					value = 0;
					hex = FALSE;
					state = HTML_CHAR_REF_DECODE;
					ptr++;
				} else {
					if(dconf_entconv)
						state = HTML_ENTITY_REF_DECODE;
					else {
						if(next_state == HTML_TAG_ARG_VAL && tag_val_length < HTML_STR_LENGTH) {
							tag_val[tag_val_length++] = '&';
						}
						html_output_c(file_buff_o2, '&');

						state = next_state;
						next_state = HTML_BAD_STATE;
					}
				}
				break;
			case HTML_ENTITY_REF_DECODE:
				if(*ptr == ';') {
					size_t i;
					const char* normalized;
					entity_val[entity_val_length] = '\0';
					normalized = entity_norm(&conv, entity_val);
					if(normalized) {
						for(i=0; i < strlen(normalized); i++) {
							const unsigned char c = normalized[i]&0xff;
							html_output_c(file_buff_o2, c);
							if (next_state == HTML_TAG_ARG_VAL && tag_val_length < HTML_STR_LENGTH) {
								tag_val[tag_val_length++] = c;
							}
						}
					}
					else {
						html_output_c(file_buff_o2, '&');
						if (next_state == HTML_TAG_ARG_VAL && tag_val_length < HTML_STR_LENGTH) {
								tag_val[tag_val_length++] = '&';
						}
						for(i=0; i < entity_val_length; i++) {
							const char c = tolower(entity_val[i]);
							html_output_c(file_buff_o2, c);
							if (next_state == HTML_TAG_ARG_VAL && tag_val_length < HTML_STR_LENGTH) {
								tag_val[tag_val_length++] = c;
							}
						}
						if (next_state == HTML_TAG_ARG_VAL && tag_val_length < HTML_STR_LENGTH) {
							tag_val[tag_val_length++] = ';';
						}
						html_output_c(file_buff_o2, ';');
					}
					entity_val_length = 0;
					state = next_state;
					next_state = HTML_BAD_STATE;
					ptr++;
				}
				else if ( (isalnum(*ptr) || *ptr=='_' || *ptr==':' || (*ptr=='-')) && entity_val_length < HTML_STR_LENGTH) {
					entity_val[entity_val_length++] = *ptr++;
				}
				else {
						/* entity too long, or not valid, dump it */
						size_t i;
						if (next_state==HTML_TAG_ARG_VAL && tag_val_length < HTML_STR_LENGTH) {
								tag_val[tag_val_length++] = '&';
						}
						html_output_c(file_buff_o2, '&');
						for(i=0; i < entity_val_length; i++) {
							const char c = tolower(entity_val[i]);
							html_output_c(file_buff_o2, c);
							if (next_state==HTML_TAG_ARG_VAL && tag_val_length < HTML_STR_LENGTH) {
								tag_val[tag_val_length++] = c;
							}
						}

						state = next_state;
						next_state = HTML_BAD_STATE;
						entity_val_length = 0;
				}
				break;
			case HTML_CHAR_REF_DECODE:
				if ((value==0) && ((*ptr == 'x') || (*ptr == 'X'))) {
					hex=TRUE;
					ptr++;
				} else if (*ptr == ';') {
					if (next_state==HTML_TAG_ARG_VAL && tag_val_length < HTML_STR_LENGTH) {
							tag_val[tag_val_length++] = value; /* store encoded values too */
					}
					if(dconf_entconv) {

						if(value < 0x80)
							html_output_c(file_buff_o2, tolower(value));
						else {
							unsigned char buff[10];
							unsigned char* out = u16_normalize_tobuffer(value, buff, 10);
							if(out && out>buff) {
								html_output_str(file_buff_o2, buff, out-buff-1);
							}
						}
					} else
							html_output_c(file_buff_o2, tolower(value&0xff));
					state = next_state;
					next_state = HTML_BAD_STATE;
					ptr++;
				} else if (isdigit(*ptr) || (hex && isxdigit(*ptr))) {
					if (hex) {
						value *= 16;
					} else {
						value *= 10;
					}
					if (isdigit(*ptr)) {
						value += (*ptr - '0');
					} else {
						value += (tolower(*ptr) - 'a' + 10);
					}
					ptr++;
				} else {
					html_output_c(file_buff_o2, value);
					state = next_state;
					next_state = HTML_BAD_STATE;
				}
				break;
			case HTML_JSDECODE:
				/* Check for start marker */
				if (strncmp(ptr, "#@~^", 4) == 0) {
					ptr += 4;
					state = HTML_JSDECODE_LENGTH;
					next_state = HTML_BAD_STATE;
				} else {
					html_output_c(file_buff_o2, tolower(*ptr));
					ptr++;
				}
				break;
			case HTML_JSDECODE_LENGTH:
				if (strlen(ptr) < 8) {
					state = HTML_NORM;
					next_state = HTML_BAD_STATE;
					break;
				}
				length = base64_chars[ptr[0]] << 2;
				length += base64_chars[ptr[1]] >> 4;
				length += (base64_chars[ptr[1]] & 0x0f) << 12;
				length += (base64_chars[ptr[2]] >> 2) << 8;
				length += (base64_chars[ptr[2]] & 0x03) << 22;
				length += base64_chars[ptr[3]] << 16;
				length += (base64_chars[ptr[4]] << 2) << 24;
				length += (base64_chars[ptr[5]] >> 4) << 24;
				table_pos = 0;
				state = HTML_JSDECODE_DECRYPT;
				next_state = HTML_BAD_STATE;
				ptr += 8;
				break;
			case HTML_JSDECODE_DECRYPT:
				if (length == 0) {
					html_output_str(file_buff_o2, "</script>\n", 10);
					length = 12;
					state = HTML_SKIP_LENGTH;
					next_state = HTML_NORM;
					break;
				}
				if (*ptr < 0x80) {
					value = decrypt_tables[table_order[table_pos]][*ptr];
					if (value == 0xFF) { /* special character */
						ptr++;
						length--;
						switch (*ptr) {
						case '\0':
							/* Fixup for end of line */
							ptr--;
							break;
						case 0x21:
							html_output_c(file_buff_o2, 0x3c);
							break;
							/*
						case 0x23:
							html_output_c(file_buff_o2, 0x0d);
							break;
							we strip whitespace
							*/
						case 0x24:
							html_output_c(file_buff_o2, 0x40);
							break;
							/*
						case 0x26:
							html_output_c(file_buff_o2, 0x0a);
							break;
							we strip whitespace 
							*/
						case 0x2a:
							html_output_c(file_buff_o2, 0x3e);
							break;
						}
					} else if(!isspace(value&0xff)) {
						html_output_c(file_buff_o2, tolower(value&0xff));
					}
				}
				table_pos = (table_pos + 1) % 64;
				ptr++;
				length--;
				break;

			case HTML_RFC2397_TYPE:
				if (*ptr == '\'') {
					if (!escape && (quoted==SINGLE_QUOTED)) {
						/* Early end of data detected. Error */
						ptr++;
						state = HTML_SKIP_WS;
						tag_arg_length=0;
						next_state = HTML_TAG_ARG;
					} else {
						if (tag_val_length < HTML_STR_LENGTH) {
							tag_val[tag_val_length++] = '"';
						}
						ptr++;
					}
				} else if (*ptr == '"') {
					if (!escape && (quoted==DOUBLE_QUOTED)) {
						/* Early end of data detected. Error */
						ptr++;
						state = HTML_SKIP_WS;
						tag_arg_length=0;
						next_state = HTML_TAG_ARG;
					} else {
						if (tag_val_length < HTML_STR_LENGTH) {
							tag_val[tag_val_length++] = '"';
						}
						ptr++;
					}
				} else if (isspace(*ptr) || (*ptr == '>')) {
					if (quoted == NOT_QUOTED) {
						/* Early end of data detected. Error */
						state = HTML_SKIP_WS;
						tag_arg_length=0;
						next_state = HTML_TAG_ARG;
					} else {
						if (tag_val_length < HTML_STR_LENGTH) {
							if (isspace(*ptr)) {
								tag_val[tag_val_length++] = ' ';
							} else {
								tag_val[tag_val_length++] = '>';
							}
						}
						state = HTML_SKIP_WS;
						escape = FALSE;
						quoted = NOT_QUOTED;
						next_state = HTML_RFC2397_TYPE;
						ptr++;
					}
				} else if (*ptr == ',') {
					/* Beginning of data */
					tag_val[tag_val_length] = '\0';
					state = HTML_RFC2397_INIT;
					escape = FALSE;
					next_state = HTML_BAD_STATE;
					ptr++;

				} else {
					if (tag_val_length < HTML_STR_LENGTH) {
						tag_val[tag_val_length++] = tolower(*ptr);
					}
					ptr++;
				}
				if (*ptr == '\\') {
					escape = TRUE;
				} else {
					escape = FALSE;
				}
				break;
			case HTML_RFC2397_INIT:
				if (dirname) {
					file_tmp_o1 = (file_buff_t *) cli_malloc(sizeof(file_buff_t));
					if (!file_tmp_o1) {
						goto abort;
					}
					snprintf(filename, 1024, "%s/rfc2397", dirname);
					tmp_file = cli_gentemp(filename);
					if(!tmp_file) {
						free(file_tmp_o1);
						goto abort;
					}
					cli_dbgmsg("RFC2397 data file: %s\n", tmp_file);
					file_tmp_o1->fd = open(tmp_file, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR|S_IRUSR);
					free(tmp_file);
					if (!file_tmp_o1->fd) {
						cli_dbgmsg("open failed: %s\n", filename);
						free(file_tmp_o1);
						goto abort;
					}
					file_tmp_o1->length = 0;

					html_output_str(file_tmp_o1, "From html-normalise\n", 20);
					html_output_str(file_tmp_o1, "Content-type: ", 14);
					if ((tag_val_length == 0) && (*tag_val == ';')) {
						html_output_str(file_tmp_o1, "text/plain\n", 11);
					}
					html_output_str(file_tmp_o1, tag_val, tag_val_length);
					html_output_c(file_tmp_o1, '\n');
					if (strstr(tag_val, ";base64") != NULL) {
						html_output_str(file_tmp_o1, "Content-transfer-encoding: base64\n", 34);
					}
					html_output_c(file_tmp_o1, '\n');
				} else {
					file_tmp_o1 = NULL;
				}
				state = HTML_RFC2397_DATA;
				binary = TRUE;
				break;
			case HTML_RFC2397_DATA:
				if (*ptr == '&') {
					state = HTML_CHAR_REF;
					next_state = HTML_RFC2397_DATA;
					ptr++;
				} else if (*ptr == '%') {
					length = 0;
					value = 0;
					state = HTML_ESCAPE_CHAR;
					next_state = HTML_RFC2397_ESC;
					ptr++;
				} else if (*ptr == '\'') {
					if (!escape && (quoted==SINGLE_QUOTED)) {
						state = HTML_RFC2397_FINISH;
						ptr++;
					} else {
						html_output_c(file_tmp_o1, *ptr);
						ptr++;
					}
				} else if (*ptr == '\"') {
					if (!escape && (quoted==DOUBLE_QUOTED)) {
						state = HTML_RFC2397_FINISH;
						ptr++;
					} else {
						html_output_c(file_tmp_o1, *ptr);
						ptr++;
					}
				} else if (isspace(*ptr) || (*ptr == '>')) {
					if (quoted == NOT_QUOTED) {
						state = HTML_RFC2397_FINISH;
						ptr++;
					} else {
						html_output_c(file_tmp_o1, *ptr);
						ptr++;
					}
				} else {
					html_output_c(file_tmp_o1, *ptr);
					ptr++;
				}
				if (*ptr == '\\') {
					escape = TRUE;
				} else {
					escape = FALSE;
				}
				break;
			case HTML_RFC2397_FINISH:
				if(file_tmp_o1) {
					html_output_flush(file_tmp_o1);
					close(file_tmp_o1->fd);
					free(file_tmp_o1);
				}
				state = HTML_SKIP_WS;
				escape = FALSE;
				quoted = NOT_QUOTED;
				next_state = HTML_TAG_ARG;
				binary = FALSE;
				break;
			case HTML_RFC2397_ESC:
				if (length == 2) {
					html_output_c(file_tmp_o1, value);
				} else if (length == 1) {
					html_output_c(file_tmp_o1, '%');
					html_output_c(file_tmp_o1, value+'0');
				} else {
					html_output_c(file_tmp_o1, '%');
				}
				state = HTML_RFC2397_DATA;
				break;
			case HTML_ESCAPE_CHAR:
				value *= 16;
				length++;
				if (isxdigit(*ptr)) {
					if (isdigit(*ptr)) {
						value += (*ptr - '0');
					} else {
						value += (tolower(*ptr) - 'a' + 10);
					}
				} else {
					state = next_state;
				}
				if (length == 2) {
					state = next_state;
				}
				ptr++;
				break;
			}
		}
		if(hrefs && hrefs->scanContents && in_ahref && href_contents_begin)
			/* end of line, append contents now, resume on next line */
			html_tag_contents_append(hrefs,in_ahref,href_contents_begin,ptr);
		ptrend = NULL;
		free(line);
		ptr = line = cli_readchunk(stream_in, m_area, 8192);
	}

	if(dconf_entconv) {
		/* handle "unfinished" entitites */
		size_t i;
		const char* normalized;
		entity_val[entity_val_length] = '\0';
		normalized = entity_norm(&conv, entity_val);
		if(normalized) {
			for(i=0; i < strlen(normalized); i++)
				html_output_c(file_buff_o2, normalized[i]&0xff);
		}
		else {
			if(entity_val_length) {
				html_output_c(file_buff_o2, '&');
				for(i=0; i < entity_val_length; i++)
					html_output_c(file_buff_o2, tolower(entity_val[i]));
			}
		}
	}
	retval = TRUE;
abort:
	if (in_form_action)
		free(in_form_action);
	if (in_ahref) /* tag not closed, force closing */
		html_tag_contents_done(hrefs,in_ahref);

	html_tag_arg_free(&tag_args);
	if (!m_area) {
		fclose(stream_in);
	}
	if (file_buff_o2) {
		html_output_flush(file_buff_o2);
		close(file_buff_o2->fd);
		free(file_buff_o2);
	}
	if(file_buff_text) {
		html_output_flush(file_buff_text);
		close(file_buff_text->fd);
		free(file_buff_text);
	}
	return retval;
}

int html_normalise_mem(unsigned char *in_buff, off_t in_size, const char *dirname, tag_arguments_t *hrefs,const struct cli_dconf* dconf)
{
	m_area_t m_area;

	m_area.buffer = in_buff;
	m_area.length = in_size;
	m_area.offset = 0;

	return cli_html_normalise(-1, &m_area, dirname, hrefs, dconf);
}

int html_normalise_fd(int fd, const char *dirname, tag_arguments_t *hrefs,const struct cli_dconf* dconf)
{
#if HAVE_MMAP
	int retval=FALSE;
	m_area_t m_area;
	struct stat statbuf;

	if (fstat(fd, &statbuf) == 0) {
		m_area.length = statbuf.st_size;
		m_area.buffer = (unsigned char *) mmap(NULL, m_area.length, PROT_READ, MAP_PRIVATE, fd, 0);
		m_area.offset = 0;
		if (m_area.buffer == MAP_FAILED) {
			cli_dbgmsg("mmap HTML failed\n");
			retval = cli_html_normalise(fd, NULL, dirname, hrefs, dconf);
		} else {
			cli_dbgmsg("mmap'ed file\n");
			retval = cli_html_normalise(-1, &m_area, dirname, hrefs, dconf);
			munmap(m_area.buffer, m_area.length);
		}
	} else {
		cli_dbgmsg("fstat HTML failed\n");
		retval = cli_html_normalise(fd, NULL, dirname, hrefs, dconf);
	}
	return retval;
#else
	return cli_html_normalise(fd, NULL, dirname, hrefs, dconf);
#endif
}

int html_screnc_decode(int fd, const char *dirname)
{
	int fd_tmp, table_pos=0, result, count, state, retval=FALSE;
	unsigned char *line, tmpstr[6];
	unsigned long length;
	unsigned char *ptr, filename[1024];
	FILE *stream_in;
	file_buff_t file_buff;
	
	lseek(fd, 0, SEEK_SET);	
	fd_tmp = dup(fd);
	if (fd_tmp < 0) {
		return FALSE;
	}
	stream_in = fdopen(fd_tmp, "r");
	if (!stream_in) {
		close(fd_tmp);
		return FALSE;
	}
	
	snprintf(filename, 1024, "%s/screnc.html", dirname);
	file_buff.fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR|S_IRUSR);
	file_buff.length = 0;
	
	if (!file_buff.fd) {
		cli_dbgmsg("open failed: %s\n", filename);
		fclose(stream_in);
		return FALSE;
	}
	
	while ((line = cli_readchunk(stream_in, NULL, 8192)) != NULL) {
		ptr = strstr(line, "#@~^");
		if (ptr) {
			break;
		}
		free(line);
        }
	if (!line) {
		goto abort;
	}
	
	/* Calculate the length of the encoded string */
	ptr += 4;
	count = 0;
	do {
		if (! *ptr) {
			free(line);
			ptr = line = cli_readchunk(stream_in, NULL, 8192);
			if (!line) {
				goto abort;
			}
		}
		tmpstr[count++] = *ptr;
		ptr++;
	} while (count < 6);
	
	length = base64_chars[tmpstr[0]] << 2;
	length += base64_chars[tmpstr[1]] >> 4;
	length += (base64_chars[tmpstr[1]] & 0x0f) << 12;
	length += (base64_chars[tmpstr[2]] >> 2) << 8;
	length += (base64_chars[tmpstr[2]] & 0x03) << 22;
	length += base64_chars[tmpstr[3]] << 16;
	length += (base64_chars[tmpstr[4]] << 2) << 24;
	length += (base64_chars[tmpstr[5]] >> 4) << 24;

	/* Move forward 2 bytes */
	count = 2;
	state = HTML_SKIP_LENGTH;

	while (length && line) {
		while (length && *ptr) {
			if ((*ptr == '\n') || (*ptr == '\r')) {
				ptr++;
				continue;
			}
			switch (state) {
			case HTML_SKIP_LENGTH:
				ptr++;
				count--;
				if (count == 0) {
					state = HTML_NORM;
				}
				break;
			case HTML_SPECIAL_CHAR:
				switch (*ptr) {
				case 0x21:
					html_output_c(&file_buff, 0x3c);
					break;
				/*case 0x23:
					html_output_c(&file_buff, 0x0d);
					break;
					we strip whitespace
					*/
				case 0x24:
					html_output_c(&file_buff, 0x40);
					break;
				/*case 0x26:
					html_output_c(&file_buff, 0x0a);
					break;
					we strip whitespace
					*/
				case 0x2a:
					html_output_c(&file_buff, 0x3e);
					break;
				}
				ptr++;
				length--;
				state = HTML_NORM;
				break;
			case HTML_NORM:	
				if (*ptr < 0x80) {
					result = decrypt_tables[table_order[table_pos]][*ptr];
					if (result == 0xFF) { /* special character */
						state = HTML_SPECIAL_CHAR;
					} else if(!isspace(result&0xff)) {
						html_output_c(&file_buff, tolower(result&0xff));
					}
				}
				ptr++;
				length--;
				table_pos = (table_pos + 1) % 64;
				break;
			}
		}
		free(line);
		if (length) {
			ptr = line = cli_readchunk(stream_in, NULL, 8192);
		}
	}
	retval = TRUE;
						
abort:
	fclose(stream_in);
	html_output_flush(&file_buff);
	close(file_buff.fd);
	return retval;
}
