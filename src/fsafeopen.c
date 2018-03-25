/* MegaZeux
 *
 * Copyright (C) 2004-2005 Alistair Strachan <alistair@devzero.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#include "fsafeopen.h"
#include "const.h"
#include "util.h"

#ifndef __WIN32__

// convert to lowercase

static void case1(char *string)
{
  int i, len = strlen(string);

  // lowercase it
  for(i = 0; i < len; i++)
    string[i] = tolower((int)string[i]);
}

// convert to uppercase

static void case2(char *string)
{
  int i, len = strlen(string);

  // uppercase it
  for(i = 0; i < len; i++)
    string[i] = toupper((int)string[i]);
}

// convert from anything to filename.EXT

static void case3(char *string)
{
  int i, len = strlen(string);

  // upper case extension
  for(i = len; i > 0; i--)
  {
    string[i] = toupper((int)string[i]);

    // last separator
    if(string[i] == '.')
      break;
  }

  // lowercase rest
  for(i--; i >= 0; i--)
    string[i] = tolower((int)string[i]);
}

// convert from anything to FILENAME.ext

static void case4(char *string)
{
  int i, len = strlen(string);

  // lowercase extension
  for(i = len; i > 0; i--)
  {
    string[i] = tolower((int)string[i]);

    // last separator
    if(string[i] == '.')
      break;
  }

  // uppercase rest
  for(i--; i >= 0; i--)
    string[i] = toupper((int)string[i]);
}

// brute force method; returns -1 if no permutation can be found to work

static int case5(char *path, char *string)
{
  int ret = -FSAFE_BRUTE_FORCE_FAILED;
  int dirlen = string - path;
  struct mzx_dir wd;
  char *newpath;

  newpath = cmalloc(PATH_BUF_LEN);

  // prepend the working directory
  strcpy(newpath, "./");

  // copy everything sans last token
  if(dirlen > 0)
  {
    if(dirlen + 2 >= PATH_BUF_LEN)
      dirlen = PATH_BUF_LEN - 2;
    strncpy(newpath + 2, path, dirlen - 1);
    newpath[dirlen + 2 - 1] = 0;
  }

  if(dir_open(&wd, newpath))
  {
    while(ret != FSAFE_SUCCESS)
    {
      // somebody bad happened, or there's no new entry
      if(!dir_get_next_entry(&wd, newpath))
        break;

      // okay, we got something, but does it match?
      if(strcasecmp(string, newpath) == 0)
      {
        strcpy(string, newpath);
        ret = FSAFE_SUCCESS;
      }
    }

    dir_close(&wd);
  }

  free(newpath);
  return ret;
}

static int match(char *path)
{
  char *oldtoken = NULL, *token = NULL;
  struct stat inode;
  int i;

  if(path == NULL)
    return -FSAFE_MATCH_FAILED;

  /* FOUR likely permutations on files, and two likely permutations on
   * directories before we start having to do anything fancy:
   *
   * filename.ext FILENAME.EXT filename.EXT FILENAME.ext
   * directory    DIRECTORY
   */

  while(1)
  {
    if(token == NULL)
    {
      // initial token
      token = strtok(path, "/\\");
    }
    else
    {
      // subsequent tokens
      token = strtok(NULL, "/\\");

      // this token is the file
      if(token == NULL)
      {
        for(i = 0; i < 5; i++)
        {
          // check file
          if(stat(path, &inode) == 0)
            break;

          // try normal cases, then try brute force
          switch(i)
          {
            case 0:
              case1(oldtoken);
              break;

            case 1:
              case2(oldtoken);
              break;

            case 2:
              case3(oldtoken);
              break;

            case 3:
              case4(oldtoken);
              break;

            default:
              // try brute force
              if(case5(path, oldtoken) < 0)
              {
                debug("%s:%d: file matches for %s failed.\n",
                 __FILE__, __LINE__, path);
                return -FSAFE_MATCH_FAILED;
              }
          }
        }
        break;
      }

      for(i = 0; i < 3; i++)
      {
        // check directory
        if(stat(path, &inode) == 0)
          break;

        // try normal cases, then try brute force
        switch(i)
        {
          case 0:
            case1(oldtoken);
            break;

          case 1:
            case2(oldtoken);
            break;

          default:
            // try brute force
            if(case5(path, oldtoken) < 0)
            {
              debug("%s:%d: directory matches for %s failed.\n",
               __FILE__, __LINE__, path);
              return -FSAFE_MATCH_FAILED;
            }
        }
      }

      /* this "hack" overwrites the token's \0 to re-formulate
       * the string versus strtok(); it has the nice side-affect of
       * also converting windows style path to UNIX ones, so they'll
       * work on everything.
       */
      oldtoken[strlen(oldtoken)] = '/';
    }

    // backup last token pointer
    oldtoken = token;
  }

  return FSAFE_SUCCESS;
}
#endif // !__WIN32__

/* OK before we do anything, we need to make some security checks. MZX games
 * shouldn't be able to open C:\Windows\Explorer.exe and overwrite it, so
 * we need to filter out any absolute pathnames, or relative pathnames
 * including "..". Do so here.
 */

static int fsafetest(const char *path, char *newpath)
{
  size_t i, pathlen;

  // we don't accept it if it's NULL or too short
  if((path == NULL) || (path[0] == 0))
    return -FSAFE_INVALID_ARGUMENT;

  pathlen = strlen (path);
  strcpy(newpath, path);

  // convert the slashes
  for(i = 0; i < pathlen; i++)
  {
    if(newpath[i] == '\\')
      newpath[i] = '/';
  }

  // check root specifiers
  if(newpath[0] == '/')
    return -FSAFE_ABSOLUTE_PATH_ERROR;

  // otherwise it's too short to contain anything hazardous
  if(pathlen == 1)
    return FSAFE_SUCCESS;

  // windows drive letters
  if(((newpath[0] >= 'A') && (newpath[0] <= 'Z')) ||
     ((newpath[0] >= 'a') && (newpath[0] <= 'z')))
  {
    if(newpath[1] == ':')
      return -FSAFE_WINDOWS_DRIVE_LETTER_ERROR;
  }

  // reject any pathname including /../
  for(i = 0; i < pathlen; i++)
  {
    // not a match for ".."
    if(strncmp(newpath + i, "..", 2) != 0)
      continue;

    // no need for delimeters -- any .. is invalid
    if((i == 0) || (pathlen < 4))
      return -FSAFE_PARENT_DIRECTORY_ERROR;

    /* Here we just make the assumption that path/name\dir can't happen;
     * it's a fairly decent assumption because msys doesn't like it, and
     * linux doesn't even recognise \ as a directory delimeter!
     */
    if((newpath[i - 1] == '/')  && (newpath[i + 2] == '/'))
      return -FSAFE_PARENT_DIRECTORY_ERROR;
  }

  return FSAFE_SUCCESS;
}

int fsafetranslate(const char *path, char *newpath)
{
  struct stat file_info;
  int ret;

  // try to pass the basic security tests
  ret = fsafetest(path, newpath);
  if(ret == FSAFE_SUCCESS)
  {
    // see if file is already there
    if(stat(newpath, &file_info) != 0)
    {
#ifndef __WIN32__
      // it isn't, so try harder..
      ret = match(newpath);
      if(ret == FSAFE_SUCCESS)
      {
        // ..and update the stat information for the new path
        if(stat(newpath, &file_info) != 0)
          ret = -FSAFE_MATCH_FAILED;
      }
      else
      {
        // Replace the failed match with the original user-supplied path
        fsafetest(path, newpath);
      }
#else
      // on WIN32 we can't, so fail hard
      ret = -FSAFE_MATCH_FAILED;
#endif
    }

    if(ret == FSAFE_SUCCESS)
    {
      // most callers don't want directories
      if(S_ISDIR(file_info.st_mode))
        ret = -FSAFE_MATCHED_DIRECTORY;
    }
  }

#if !defined(__WIN32__)
  if(ret == -FSAFE_SUCCESS || ret == -FSAFE_MATCHED_DIRECTORY)
  {
    debug("%s:%d: translated %s to %s%s.\n", __FILE__, __LINE__,
     path, newpath, (ret == -FSAFE_MATCHED_DIRECTORY) ? "/" : "");
  }
  else
  {
    debug("%s:%d: failed to translate %s (err %d).\n",
     __FILE__, __LINE__, path, ret);
  }
#endif

  return ret;
}

FILE *fsafeopen(const char *path, const char *mode)
{
  char *newpath;
  int i, ret;
  FILE *f;

  newpath = cmalloc(MAX_PATH);

  // validate pathname, and optionally retrieve a better name
  ret = fsafetranslate(path, newpath);

  // filename couldn't be "found", but there were no security issues
  if(ret == -FSAFE_MATCH_FAILED)
  {
    // if we're reading, any failure to translate the name means we back out
    for(i = 0; i < (int)strlen(mode); i++)
    {
      if(mode[i] == 'r' || mode[i] == '+')
      {
        free(newpath);
        return NULL;
      }
    }
  }
  else if(ret < 0)
  {
    // bad name, or security checks failed
    free(newpath);
    return NULL;
  }

  // _TRY_ opening the file
  f = fopen_unsafe(newpath, mode);
  free(newpath);
  return f;
}

/* It's conceivable that on some platforms (like Linux, or Macintosh classic),
 * fgets may return a string that still contains "EOL" characters considered
 * by another platform. For example, if a file is written out by Windows,
 * and a Linux user reads it, the buffers will not remove the \r character.
 * 
 * This function provides a "safe" wrapper that removes all kinds of line
 * endings from the buffer, and should work at least until somebody invents
 * a new three byte string terminator ;-(
 */
char *fsafegets(char *s, int size, FILE *stream)
{
  char *ret = fgets(s, size, stream);

  if(ret)
  {
    size_t len = strlen(ret);

    if(len > 0)
      if(s[len - 1] == '\r' || s[len - 1] == '\n')
        s[len - 1] = '\0';

    if(len > 1)
      if(s[len - 2] == '\r' || s[len - 2] == '\n')
        s[len - 2] = '\0';
  }

  return ret;
}

