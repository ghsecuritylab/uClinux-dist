/* Conversion of links to local files.
   Copyright (C) 1996, 1997, 2000, 2001 Free Software Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

In addition, as a special exception, the Free Software Foundation
gives permission to link the code of its release of Wget with the
OpenSSL project's "OpenSSL" library (or with modified versions of it
that use the same license as the "OpenSSL" library), and distribute
the linked executables.  You must obey the GNU General Public License
in all respects for all of the code used other than "OpenSSL".  If you
modify this file, you may extend this exception to your version of the
file, but you are not obligated to do so.  If you do not wish to do
so, delete this exception statement from your version.  */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <errno.h>
#include <assert.h>
#include <sys/types.h>

#include "wget.h"
#include "convert.h"
#include "url.h"
#include "recur.h"
#include "utils.h"
#include "hash.h"

static struct hash_table *dl_file_url_map;
struct hash_table *dl_url_file_map;

/* List of HTML files downloaded in this Wget run, used for link
   conversion after Wget is done.  The list and the set contain the
   same information, except the list maintains the order.  Perhaps I
   should get rid of the list, it's there for historical reasons.  */
static slist *downloaded_html_list;
struct hash_table *downloaded_html_set;

static void convert_links PARAMS ((const char *, struct urlpos *));

/* This function is called when the retrieval is done to convert the
   links that have been downloaded.  It has to be called at the end of
   the retrieval, because only then does Wget know conclusively which
   URLs have been downloaded, and which not, so it can tell which
   direction to convert to.

   The "direction" means that the URLs to the files that have been
   downloaded get converted to the relative URL which will point to
   that file.  And the other URLs get converted to the remote URL on
   the server.

   All the downloaded HTMLs are kept in downloaded_html_files, and
   downloaded URLs in urls_downloaded.  All the information is
   extracted from these two lists.  */

void
convert_all_links (void)
{
  slist *html;
  long msecs;
  int file_count = 0;

  struct wget_timer *timer = wtimer_new ();

  /* Destructively reverse downloaded_html_files to get it in the right order.
     recursive_retrieve() used slist_prepend() consistently.  */
  downloaded_html_list = slist_nreverse (downloaded_html_list);

  for (html = downloaded_html_list; html; html = html->next)
    {
      struct urlpos *urls, *cur_url;
      char *url;
      char *file = html->string;

      /* Determine the URL of the HTML file.  get_urls_html will need
	 it.  */
      url = hash_table_get (dl_file_url_map, file);
      if (!url)
	{
	  DEBUGP (("Apparently %s has been removed.\n", file));
	  continue;
	}

      DEBUGP (("Scanning %s (from %s)\n", file, url));

      /* Parse the HTML file...  */
      urls = get_urls_html (file, url, NULL);

      /* We don't respect meta_disallow_follow here because, even if
         the file is not followed, we might still want to convert the
         links that have been followed from other files.  */

      for (cur_url = urls; cur_url; cur_url = cur_url->next)
	{
	  char *local_name;
	  struct url *u = cur_url->url;

	  if (cur_url->link_base_p)
	    {
	      /* Base references have been resolved by our parser, so
		 we turn the base URL into an empty string.  (Perhaps
		 we should remove the tag entirely?)  */
	      cur_url->convert = CO_NULLIFY_BASE;
	      continue;
	    }

	  /* We decide the direction of conversion according to whether
	     a URL was downloaded.  Downloaded URLs will be converted
	     ABS2REL, whereas non-downloaded will be converted REL2ABS.  */
	  local_name = hash_table_get (dl_url_file_map, u->url);

	  /* Decide on the conversion type.  */
	  if (local_name)
	    {
	      /* We've downloaded this URL.  Convert it to relative
                 form.  We do this even if the URL already is in
                 relative form, because our directory structure may
                 not be identical to that on the server (think `-nd',
                 `--cut-dirs', etc.)  */
	      cur_url->convert = CO_CONVERT_TO_RELATIVE;
	      cur_url->local_name = xstrdup (local_name);
	      DEBUGP (("will convert url %s to local %s\n", u->url, local_name));
	    }
	  else
	    {
	      /* We haven't downloaded this URL.  If it's not already
                 complete (including a full host name), convert it to
                 that form, so it can be reached while browsing this
                 HTML locally.  */
	      if (!cur_url->link_complete_p)
		cur_url->convert = CO_CONVERT_TO_COMPLETE;
	      cur_url->local_name = NULL;
	      DEBUGP (("will convert url %s to complete\n", u->url));
	    }
	}

      /* Convert the links in the file.  */
      convert_links (file, urls);
      ++file_count;

      /* Free the data.  */
      free_urlpos (urls);
    }

  msecs = wtimer_elapsed (timer);
  wtimer_delete (timer);
  logprintf (LOG_VERBOSE, _("Converted %d files in %.2f seconds.\n"),
	     file_count, (double)msecs / 1000);
}

static void write_backup_file PARAMS ((const char *, downloaded_file_t));
static const char *replace_attr PARAMS ((const char *, int, FILE *,
					 const char *));
static const char *replace_attr_refresh_hack PARAMS ((const char *, int, FILE *,
						      const char *, int));
static char *local_quote_string PARAMS ((const char *));
static char *construct_relative PARAMS ((const char *, const char *));

/* Change the links in one HTML file.  LINKS is a list of links in the
   document, along with their positions and the desired direction of
   the conversion.  */
static void
convert_links (const char *file, struct urlpos *links)
{
  struct file_memory *fm;
  FILE *fp;
  const char *p;
  downloaded_file_t downloaded_file_return;

  struct urlpos *link;
  int to_url_count = 0, to_file_count = 0;

  logprintf (LOG_VERBOSE, _("Converting %s... "), file);

  {
    /* First we do a "dry run": go through the list L and see whether
       any URL needs to be converted in the first place.  If not, just
       leave the file alone.  */
    int dry_count = 0;
    struct urlpos *dry = links;
    for (dry = links; dry; dry = dry->next)
      if (dry->convert != CO_NOCONVERT)
	++dry_count;
    if (!dry_count)
      {
	logputs (LOG_VERBOSE, _("nothing to do.\n"));
	return;
      }
  }

  fm = read_file (file);
  if (!fm)
    {
      logprintf (LOG_NOTQUIET, _("Cannot convert links in %s: %s\n"),
		 file, strerror (errno));
      return;
    }

  downloaded_file_return = downloaded_file (CHECK_FOR_FILE, file);
  if (opt.backup_converted && downloaded_file_return)
    write_backup_file (file, downloaded_file_return);

  /* Before opening the file for writing, unlink the file.  This is
     important if the data in FM is mmaped.  In such case, nulling the
     file, which is what fopen() below does, would make us read all
     zeroes from the mmaped region.  */
  if (unlink (file) < 0 && errno != ENOENT)
    {
      logprintf (LOG_NOTQUIET, _("Unable to delete `%s': %s\n"),
		 file, strerror (errno));
      read_file_free (fm);
      return;
    }
  /* Now open the file for writing.  */
  fp = fopen (file, "wb");
  if (!fp)
    {
      logprintf (LOG_NOTQUIET, _("Cannot convert links in %s: %s\n"),
		 file, strerror (errno));
      read_file_free (fm);
      return;
    }

  /* Here we loop through all the URLs in file, replacing those of
     them that are downloaded with relative references.  */
  p = fm->content;
  for (link = links; link; link = link->next)
    {
      char *url_start = fm->content + link->pos;

      if (link->pos >= fm->length)
	{
	  DEBUGP (("Something strange is going on.  Please investigate."));
	  break;
	}
      /* If the URL is not to be converted, skip it.  */
      if (link->convert == CO_NOCONVERT)
	{
	  DEBUGP (("Skipping %s at position %d.\n", link->url->url, link->pos));
	  continue;
	}

      /* Echo the file contents, up to the offending URL's opening
         quote, to the outfile.  */
      fwrite (p, 1, url_start - p, fp);
      p = url_start;

      switch (link->convert)
	{
	case CO_CONVERT_TO_RELATIVE:
	  /* Convert absolute URL to relative. */
	  {
	    char *newname = construct_relative (file, link->local_name);
	    char *quoted_newname = local_quote_string (newname);

	    if (!link->link_refresh_p)
	      p = replace_attr (p, link->size, fp, quoted_newname);
	    else
	      p = replace_attr_refresh_hack (p, link->size, fp, quoted_newname,
					     link->refresh_timeout);

	    DEBUGP (("TO_RELATIVE: %s to %s at position %d in %s.\n",
		     link->url->url, newname, link->pos, file));
	    xfree (newname);
	    xfree (quoted_newname);
	    ++to_file_count;
	    break;
	  }
	case CO_CONVERT_TO_COMPLETE:
	  /* Convert the link to absolute URL. */
	  {
	    char *newlink = link->url->url;
	    char *quoted_newlink = html_quote_string (newlink);

	    if (!link->link_refresh_p)
	      p = replace_attr (p, link->size, fp, quoted_newlink);
	    else
	      p = replace_attr_refresh_hack (p, link->size, fp, quoted_newlink,
					     link->refresh_timeout);

	    DEBUGP (("TO_COMPLETE: <something> to %s at position %d in %s.\n",
		     newlink, link->pos, file));
	    xfree (quoted_newlink);
	    ++to_url_count;
	    break;
	  }
	case CO_NULLIFY_BASE:
	  /* Change the base href to "". */
	  p = replace_attr (p, link->size, fp, "");
	  break;
	case CO_NOCONVERT:
	  abort ();
	  break;
	}
    }

  /* Output the rest of the file. */
  if (p - fm->content < fm->length)
    fwrite (p, 1, fm->length - (p - fm->content), fp);
  fclose (fp);
  read_file_free (fm);

  logprintf (LOG_VERBOSE, "%d-%d\n", to_file_count, to_url_count);
}

/* Construct and return a malloced copy of the relative link from two
   pieces of information: local name S1 of the referring file and
   local name S2 of the referred file.

   So, if S1 is "jagor.srce.hr/index.html" and S2 is
   "jagor.srce.hr/images/news.gif", the function will return
   "images/news.gif".

   Alternately, if S1 is "fly.cc.fer.hr/ioccc/index.html", and S2 is
   "fly.cc.fer.hr/images/fly.gif", the function will return
   "../images/fly.gif".

   Caveats: S1 should not begin with `/', unless S2 also begins with
   '/'.  S1 should not contain things like ".." and such --
   construct_relative ("fly/ioccc/../index.html",
   "fly/images/fly.gif") will fail.  (A workaround is to call
   something like path_simplify() on S1).  */
static char *
construct_relative (const char *s1, const char *s2)
{
  int i, cnt, sepdirs1;
  char *res;

  if (*s2 == '/')
    return xstrdup (s2);
  /* S1 should *not* be absolute, if S2 wasn't.  */
  assert (*s1 != '/');
  i = cnt = 0;
  /* Skip the directories common to both strings.  */
  while (1)
    {
      while (s1[i] && s2[i]
	     && (s1[i] == s2[i])
	     && (s1[i] != '/')
	     && (s2[i] != '/'))
	++i;
      if (s1[i] == '/' && s2[i] == '/')
	cnt = ++i;
      else
	break;
    }
  for (sepdirs1 = 0; s1[i]; i++)
    if (s1[i] == '/')
      ++sepdirs1;
  /* Now, construct the file as of:
     - ../ repeated sepdirs1 time
     - all the non-mutual directories of S2.  */
  res = (char *)xmalloc (3 * sepdirs1 + strlen (s2 + cnt) + 1);
  for (i = 0; i < sepdirs1; i++)
    memcpy (res + 3 * i, "../", 3);
  strcpy (res + 3 * i, s2 + cnt);
  return res;
}

static void
write_backup_file (const char *file, downloaded_file_t downloaded_file_return)
{
  /* Rather than just writing over the original .html file with the
     converted version, save the former to *.orig.  Note we only do
     this for files we've _successfully_ downloaded, so we don't
     clobber .orig files sitting around from previous invocations. */

  /* Construct the backup filename as the original name plus ".orig". */
  size_t         filename_len = strlen(file);
  char*          filename_plus_orig_suffix;
  boolean        already_wrote_backup_file = FALSE;
  slist*         converted_file_ptr;
  static slist*  converted_files = NULL;

  if (downloaded_file_return == FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED)
    {
      /* Just write "orig" over "html".  We need to do it this way
	 because when we're checking to see if we've downloaded the
	 file before (to see if we can skip downloading it), we don't
	 know if it's a text/html file.  Therefore we don't know yet
	 at that stage that -E is going to cause us to tack on
	 ".html", so we need to compare vs. the original URL plus
	 ".orig", not the original URL plus ".html.orig". */
      filename_plus_orig_suffix = alloca (filename_len + 1);
      strcpy(filename_plus_orig_suffix, file);
      strcpy((filename_plus_orig_suffix + filename_len) - 4, "orig");
    }
  else /* downloaded_file_return == FILE_DOWNLOADED_NORMALLY */
    {
      /* Append ".orig" to the name. */
      filename_plus_orig_suffix = alloca (filename_len + sizeof(".orig"));
      strcpy(filename_plus_orig_suffix, file);
      strcpy(filename_plus_orig_suffix + filename_len, ".orig");
    }

  /* We can get called twice on the same URL thanks to the
     convert_all_links() call in main().  If we write the .orig file
     each time in such a case, it'll end up containing the first-pass
     conversion, not the original file.  So, see if we've already been
     called on this file. */
  converted_file_ptr = converted_files;
  while (converted_file_ptr != NULL)
    if (strcmp(converted_file_ptr->string, file) == 0)
      {
	already_wrote_backup_file = TRUE;
	break;
      }
    else
      converted_file_ptr = converted_file_ptr->next;

  if (!already_wrote_backup_file)
    {
      /* Rename <file> to <file>.orig before former gets written over. */
      if (rename(file, filename_plus_orig_suffix) != 0)
	logprintf (LOG_NOTQUIET, _("Cannot back up %s as %s: %s\n"),
		   file, filename_plus_orig_suffix, strerror (errno));

      /* Remember that we've already written a .orig backup for this file.
	 Note that we never free this memory since we need it till the
	 convert_all_links() call, which is one of the last things the
	 program does before terminating.  BTW, I'm not sure if it would be
	 safe to just set 'converted_file_ptr->string' to 'file' below,
	 rather than making a copy of the string...  Another note is that I
	 thought I could just add a field to the urlpos structure saying
	 that we'd written a .orig file for this URL, but that didn't work,
	 so I had to make this separate list.
	 -- Dan Harkless <wget@harkless.org>

         This [adding a field to the urlpos structure] didn't work
         because convert_file() is called from convert_all_links at
         the end of the retrieval with a freshly built new urlpos
         list.
	 -- Hrvoje Niksic <hniksic@xemacs.org>
      */
      converted_file_ptr = xmalloc(sizeof(*converted_file_ptr));
      converted_file_ptr->string = xstrdup(file);  /* die on out-of-mem. */
      converted_file_ptr->next = converted_files;
      converted_files = converted_file_ptr;
    }
}

static int find_fragment PARAMS ((const char *, int, const char **,
				  const char **));

/* Replace an attribute's original text with NEW_TEXT. */

static const char *
replace_attr (const char *p, int size, FILE *fp, const char *new_text)
{
  int quote_flag = 0;
  char quote_char = '\"';	/* use "..." for quoting, unless the
				   original value is quoted, in which
				   case reuse its quoting char. */
  const char *frag_beg, *frag_end;

  /* Structure of our string is:
       "...old-contents..."
       <---    size    --->  (with quotes)
     OR:
       ...old-contents...
       <---    size   -->    (no quotes)   */

  if (*p == '\"' || *p == '\'')
    {
      quote_char = *p;
      quote_flag = 1;
      ++p;
      size -= 2;		/* disregard opening and closing quote */
    }
  putc (quote_char, fp);
  fputs (new_text, fp);

  /* Look for fragment identifier, if any. */
  if (find_fragment (p, size, &frag_beg, &frag_end))
    fwrite (frag_beg, 1, frag_end - frag_beg, fp);
  p += size;
  if (quote_flag)
    ++p;
  putc (quote_char, fp);

  return p;
}

/* The same as REPLACE_ATTR, but used when replacing
   <meta http-equiv=refresh content="new_text"> because we need to
   append "timeout_value; URL=" before the next_text.  */

static const char *
replace_attr_refresh_hack (const char *p, int size, FILE *fp,
			   const char *new_text, int timeout)
{
  /* "0; URL=..." */
  char *new_with_timeout = (char *)alloca (numdigit (timeout)
					   + 6 /* "; URL=" */
					   + strlen (new_text)
					   + 1);
  sprintf (new_with_timeout, "%d; URL=%s", timeout, new_text);

  return replace_attr (p, size, fp, new_with_timeout);
}

/* Find the first occurrence of '#' in [BEG, BEG+SIZE) that is not
   preceded by '&'.  If the character is not found, return zero.  If
   the character is found, return 1 and set BP and EP to point to the
   beginning and end of the region.

   This is used for finding the fragment indentifiers in URLs.  */

static int
find_fragment (const char *beg, int size, const char **bp, const char **ep)
{
  const char *end = beg + size;
  int saw_amp = 0;
  for (; beg < end; beg++)
    {
      switch (*beg)
	{
	case '&':
	  saw_amp = 1;
	  break;
	case '#':
	  if (!saw_amp)
	    {
	      *bp = beg;
	      *ep = end;
	      return 1;
	    }
	  /* fallthrough */
	default:
	  saw_amp = 0;
	}
    }
  return 0;
}

/* Quote FILE for use as local reference to an HTML file.

   We quote ? as %3F to avoid passing part of the file name as the
   parameter when browsing the converted file through HTTP.  However,
   it is safe to do this only when `--html-extension' is turned on.
   This is because converting "index.html?foo=bar" to
   "index.html%3Ffoo=bar" would break local browsing, as the latter
   isn't even recognized as an HTML file!  However, converting
   "index.html?foo=bar.html" to "index.html%3Ffoo=bar.html" should be
   safe for both local and HTTP-served browsing.  */

static char *
local_quote_string (const char *file)
{
  const char *file_sans_qmark;
  int qm;

  if (!opt.html_extension)
    return html_quote_string (file);

  qm = count_char (file, '?');

  if (qm)
    {
      const char *from = file;
      char *to, *newname;

      /* qm * 2 because we replace each question mark with "%3F",
	 i.e. replace one char with three, hence two more.  */
      int fsqlen = strlen (file) + qm * 2;

      to = newname = (char *)alloca (fsqlen + 1);
      for (; *from; from++)
	{
	  if (*from != '?')
	    *to++ = *from;
	  else
	    {
	      *to++ = '%';
	      *to++ = '3';
	      *to++ = 'F';
	    }
	}
      assert (to - newname == fsqlen);
      *to = '\0';

      file_sans_qmark = newname;
    }
  else
    file_sans_qmark = file;

  return html_quote_string (file_sans_qmark);
}

/* Book-keeping code for dl_file_url_map, dl_url_file_map,
   downloaded_html_list, and downloaded_html_set.  Other code calls
   these functions to let us know that a file has been downloaded.  */

#define ENSURE_TABLES_EXIST do {			\
  if (!dl_file_url_map)					\
    dl_file_url_map = make_string_hash_table (0);	\
  if (!dl_url_file_map)					\
    dl_url_file_map = make_string_hash_table (0);	\
} while (0)

/* Return 1 if S1 and S2 are the same, except for "/index.html".  The
   three cases in which it returns one are (substitute any substring
   for "foo"):

   m("foo/index.html", "foo/")  ==> 1
   m("foo/", "foo/index.html")  ==> 1
   m("foo", "foo/index.html")   ==> 1
   m("foo", "foo/"              ==> 1
   m("foo", "foo")              ==> 1  */

static int
match_except_index (const char *s1, const char *s2)
{
  int i;
  const char *lng;

  /* Skip common substring. */
  for (i = 0; *s1 && *s2 && *s1 == *s2; s1++, s2++, i++)
    ;
  if (i == 0)
    /* Strings differ at the very beginning -- bail out.  We need to
       check this explicitly to avoid `lng - 1' reading outside the
       array.  */
    return 0;

  if (!*s1 && !*s2)
    /* Both strings hit EOF -- strings are equal. */
    return 1;
  else if (*s1 && *s2)
    /* Strings are randomly different, e.g. "/foo/bar" and "/foo/qux". */
    return 0;
  else if (*s1)
    /* S1 is the longer one. */
    lng = s1;
  else
    /* S2 is the longer one. */
    lng = s2;

  /* foo            */            /* foo/           */
  /* foo/index.html */  /* or */  /* foo/index.html */
  /*    ^           */            /*     ^          */

  if (*lng != '/')
    /* The right-hand case. */
    --lng;

  if (*lng == '/' && *(lng + 1) == '\0')
    /* foo  */
    /* foo/ */
    return 1;

  return 0 == strcmp (lng, "/index.html");
}

static int
dissociate_urls_from_file_mapper (void *key, void *value, void *arg)
{
  char *mapping_url = (char *)key;
  char *mapping_file = (char *)value;
  char *file = (char *)arg;

  if (0 == strcmp (mapping_file, file))
    {
      hash_table_remove (dl_url_file_map, mapping_url);
      xfree (mapping_url);
      xfree (mapping_file);
    }

  /* Continue mapping. */
  return 0;
}

/* Remove all associations from various URLs to FILE from dl_url_file_map. */

static void
dissociate_urls_from_file (const char *file)
{
  hash_table_map (dl_url_file_map, dissociate_urls_from_file_mapper,
		  (char *)file);
}

/* Register that URL has been successfully downloaded to FILE.  This
   is used by the link conversion code to convert references to URLs
   to references to local files.  It is also being used to check if a
   URL has already been downloaded.  */

void
register_download (const char *url, const char *file)
{
  char *old_file, *old_url;

  ENSURE_TABLES_EXIST;

  /* With some forms of retrieval, it is possible, although not likely
     or particularly desirable.  If both are downloaded, the second
     download will override the first one.  When that happens,
     dissociate the old file name from the URL.  */

  if (hash_table_get_pair (dl_file_url_map, file, &old_file, &old_url))
    {
      if (0 == strcmp (url, old_url))
	/* We have somehow managed to download the same URL twice.
	   Nothing to do.  */
	return;

      if (match_except_index (url, old_url)
	  && !hash_table_contains (dl_url_file_map, url))
	/* The two URLs differ only in the "index.html" ending.  For
	   example, one is "http://www.server.com/", and the other is
	   "http://www.server.com/index.html".  Don't remove the old
	   one, just add the new one as a non-canonical entry.  */
	goto url_only;

      hash_table_remove (dl_file_url_map, file);
      xfree (old_file);
      xfree (old_url);

      /* Remove all the URLs that point to this file.  Yes, there can
	 be more than one such URL, because we store redirections as
	 multiple entries in dl_url_file_map.  For example, if URL1
	 redirects to URL2 which gets downloaded to FILE, we map both
	 URL1 and URL2 to FILE in dl_url_file_map.  (dl_file_url_map
	 only points to URL2.)  When another URL gets loaded to FILE,
	 we want both URL1 and URL2 dissociated from it.

         This is a relatively expensive operation because it performs
         a linear search of the whole hash table, but it should be
         called very rarely, only when two URLs resolve to the same
         file name, *and* the "<file>.1" extensions are turned off.
         In other words, almost never.  */
      dissociate_urls_from_file (file);
    }

  hash_table_put (dl_file_url_map, xstrdup (file), xstrdup (url));

 url_only:
  /* A URL->FILE mapping is not possible without a FILE->URL mapping.
     If the latter were present, it should have been removed by the
     above `if'.  So we could write:

         assert (!hash_table_contains (dl_url_file_map, url));

     The above is correct when running in recursive mode where the
     same URL always resolves to the same file.  But if you do
     something like:

         wget URL URL

     then the first URL will resolve to "FILE", and the other to
     "FILE.1".  In that case, FILE.1 will not be found in
     dl_file_url_map, but URL will still point to FILE in
     dl_url_file_map.  */
  if (hash_table_get_pair (dl_url_file_map, url, &old_url, &old_file))
    {
      hash_table_remove (dl_url_file_map, url);
      xfree (old_url);
      xfree (old_file);
    }

  hash_table_put (dl_url_file_map, xstrdup (url), xstrdup (file));
}

/* Register that FROM has been redirected to TO.  This assumes that TO
   is successfully downloaded and already registered using
   register_download() above.  */

void
register_redirection (const char *from, const char *to)
{
  char *file;

  ENSURE_TABLES_EXIST;

  file = hash_table_get (dl_url_file_map, to);
  assert (file != NULL);
  if (!hash_table_contains (dl_url_file_map, from))
    hash_table_put (dl_url_file_map, xstrdup (from), xstrdup (file));
}

/* Register that the file has been deleted. */

void
register_delete_file (const char *file)
{
  char *old_url, *old_file;

  ENSURE_TABLES_EXIST;

  if (!hash_table_get_pair (dl_file_url_map, file, &old_file, &old_url))
    return;

  hash_table_remove (dl_file_url_map, file);
  xfree (old_file);
  xfree (old_url);
  dissociate_urls_from_file (file);
}

/* Register that FILE is an HTML file that has been downloaded. */

void
register_html (const char *url, const char *file)
{
  if (!downloaded_html_set)
    downloaded_html_set = make_string_hash_table (0);
  else if (hash_table_contains (downloaded_html_set, file))
    return;

  /* The set and the list should use the same copy of FILE, but the
     slist interface insists on strduping the string it gets.  Oh
     well. */
  string_set_add (downloaded_html_set, file);
  downloaded_html_list = slist_prepend (downloaded_html_list, file);
}

/* Cleanup the data structures associated with recursive retrieving
   (the variables above).  */
void
convert_cleanup (void)
{
  if (dl_file_url_map)
    {
      free_keys_and_values (dl_file_url_map);
      hash_table_destroy (dl_file_url_map);
      dl_file_url_map = NULL;
    }
  if (dl_url_file_map)
    {
      free_keys_and_values (dl_url_file_map);
      hash_table_destroy (dl_url_file_map);
      dl_url_file_map = NULL;
    }
  if (downloaded_html_set)
    string_set_free (downloaded_html_set);
  slist_free (downloaded_html_list);
  downloaded_html_list = NULL;
}

/* Book-keeping code for downloaded files that enables extension
   hacks.  */

/* This table should really be merged with dl_file_url_map and
   downloaded_html_files.  This was originally a list, but I changed
   it to a hash table beause it was actually taking a lot of time to
   find things in it.  */

static struct hash_table *downloaded_files_hash;

/* We're storing "modes" of type downloaded_file_t in the hash table.
   However, our hash tables only accept pointers for keys and values.
   So when we need a pointer, we use the address of a
   downloaded_file_t variable of static storage.  */
   
static downloaded_file_t *
downloaded_mode_to_ptr (downloaded_file_t mode)
{
  static downloaded_file_t
    v1 = FILE_NOT_ALREADY_DOWNLOADED,
    v2 = FILE_DOWNLOADED_NORMALLY,
    v3 = FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED,
    v4 = CHECK_FOR_FILE;

  switch (mode)
    {
    case FILE_NOT_ALREADY_DOWNLOADED:
      return &v1;
    case FILE_DOWNLOADED_NORMALLY:
      return &v2;
    case FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED:
      return &v3;
    case CHECK_FOR_FILE:
      return &v4;
    }
  return NULL;
}

/* Remembers which files have been downloaded.  In the standard case,
   should be called with mode == FILE_DOWNLOADED_NORMALLY for each
   file we actually download successfully (i.e. not for ones we have
   failures on or that we skip due to -N).

   When we've downloaded a file and tacked on a ".html" extension due
   to -E, call this function with
   FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED rather than
   FILE_DOWNLOADED_NORMALLY.

   If you just want to check if a file has been previously added
   without adding it, call with mode == CHECK_FOR_FILE.  Please be
   sure to call this function with local filenames, not remote
   URLs.  */

downloaded_file_t
downloaded_file (downloaded_file_t mode, const char *file)
{
  downloaded_file_t *ptr;

  if (mode == CHECK_FOR_FILE)
    {
      if (!downloaded_files_hash)
	return FILE_NOT_ALREADY_DOWNLOADED;
      ptr = hash_table_get (downloaded_files_hash, file);
      if (!ptr)
	return FILE_NOT_ALREADY_DOWNLOADED;
      return *ptr;
    }

  if (!downloaded_files_hash)
    downloaded_files_hash = make_string_hash_table (0);

  ptr = hash_table_get (downloaded_files_hash, file);
  if (ptr)
    return *ptr;

  ptr = downloaded_mode_to_ptr (mode);
  hash_table_put (downloaded_files_hash, xstrdup (file), &ptr);

  return FILE_NOT_ALREADY_DOWNLOADED;
}

static int
df_free_mapper (void *key, void *value, void *ignored)
{
  xfree (key);
  return 0;
}

void
downloaded_files_free (void)
{
  if (downloaded_files_hash)
    {
      hash_table_map (downloaded_files_hash, df_free_mapper, NULL);
      hash_table_destroy (downloaded_files_hash);
      downloaded_files_hash = NULL;
    }
}