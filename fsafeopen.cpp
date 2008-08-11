/* $Id$
 * MegaZeux
 *
 * Copyright (C) 2004 Alistair Strachan <alistair@devzero.co.uk>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#include "fsafeopen.h"

#ifndef __WIN32__

#include <dirent.h>
//
// convert to lowercase
//

void case1(char *string)
{
  int i, len = strlen(string);

  // lowercase it
  for(i = 0; i < len; i++)
  {
    string[i] = tolower(string[i]);
  }
}

//
// convert to uppercase
//

void case2(char *string)
{
  int i, len = strlen(string);

  // uppercase it
  for(i = 0; i < len; i++)
  {
    string[i] = toupper(string[i]);
  }
}

//
// convert from anything to filename.EXT
//

void case3(char *string)
{
  int i, len = strlen(string);

  // upper case extension
  for(i = len; i > 0; i--)
  {
    string[i] = toupper(string[i]);

    // last separator
    if(string[i] == '.')
      break;
  }

  // lowercase rest
  for(i--; i >= 0; i--)
  {
    string[i] = tolower (string[i]);
  }
}

//
// convert from anything to FILENAME.ext
//

void case4(char *string)
{
  int i, len = strlen(string);

  // lowercase extension
  for(i = len; i > 0; i--)
  {
    string[i] = tolower(string[i]);

    // last separator
    if(string[i] == '.')
      break;
  }

  // uppercase rest
  for(i--; i >= 0; i--)
  {
    string[i] = toupper(string[i]);
  }
}

//
// brute force method; returns -1 if no permutation can be found to work
//

int case5(char *path, char *string)
{
  int i, ret = -1, dirlen = string - path;
  struct dirent *inode;
  char newpath[MAX_PATH];
  DIR *wd;

  // reconstruct wd
  memcpy(newpath, "./", 2);

  // copy everything sans last token
  if(dirlen > 0)
    memcpy(newpath + 2, path, dirlen - 1);

  // open directory
  wd = opendir(newpath);

  if(wd != NULL)
  {
    while(ret != 0)
    {
      // check against inodes in this directory
      inode = readdir(wd);

      // check for null
      if(inode == NULL)
        break;

      if(strcasecmp(inode->d_name, string) == 0)
      {
        // update inode lookup
        strcpy(string, inode->d_name);

        // success
        ret = 0;
      }
    }

    // close working dir
    closedir(wd);
  }

  // return code
  return ret;
}

int match(char *path)
{
  int i, file = 0, pathlen = strlen(path);
  char *oldtoken, *token = NULL;
  struct stat inode;

  //
  // FOUR likely permutations on files, and two likely permutations on
  // directories before we start having to do anything fancy:
  //
  // filename.ext FILENAME.EXT filename.EXT FILENAME.ext
  // directory    DIRECTORY
  //

  while(1)
  {

    if(token == NULL)
    {
      // initial token
      token = strtok(path, "/");
    }
    else
    {
      // subsequent tokens
      token = strtok(NULL, "/");

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
            {
              case1(oldtoken);
              break;
            }

            case 1:
            {
              case2(oldtoken);
              break;
            }

            case 2:
            {
              case3(oldtoken);
              break;
            }

            case 3:
            {
              case4(oldtoken);
              break;
            }

            default:
            {
              // try brute force
              if(case5(path, oldtoken) < 0)
                return -1;
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
          {
            case1(oldtoken);
            break;
          }

          case 1:
          {
            case2(oldtoken);
            break;
          }

          default:
          {
            // try brute force
            if(case5(path, oldtoken) < 0)
              return -1;
          }
        }
      }

      // hack previous token; insert delimeter
      oldtoken[strlen(oldtoken)] = '/';
    }

    // backup last token pointer
    oldtoken = token;
  }

  // all ok
  return 0;
}
#endif // !__WIN32__

int fsafetranslate(const char *path, char *newpath)
{
  struct stat inode;
  int i, pathlen;

  // check for exploit
  if(path == NULL)
    return -3;

  // get length of path
  pathlen = strlen (path);

  if(pathlen < 1)
    return -3;

  // copy string (including \0)
  memcpy(newpath, path, pathlen + 1);

  // convert all path delimeters to UNIX style, obsoleting windows legacy
  for(i = 0; i < pathlen; i++)
  {
    if(newpath[i] == '\\')
      newpath[i] = '/';
  }

  //
  // OK before we do anything, we need to make some security checks. MZX games
  // shouldn't be able to open C:\Windows\Explorer.exe and overwrite it, so
  // we need to filter out any absolute pathnames, or relative pathnames
  // including "..". Do so here.
  //

  // check top-level absolutes, windows and Linux */
  if(newpath[0] == '/')
    return -2;

  if(pathlen > 1)
  {
    // windows drive letters
    if(((newpath[0] >= 'A') && (newpath[0] <= 'Z')) ||
       ((newpath[0] >= 'a') && (newpath[0] <= 'z')))
    {
      if(newpath[1] == ':')
        return -2;
    }

    // reject any pathname including /../
    for(i = 0; i < pathlen; i++)
    {
      if(strncmp(newpath + i, "..", 2) == 0)
      {
        // no need for delimeters -- any .. is invalid
        if((i == 0) || pathlen < 4)
          return -2;

        // check for delimeters (file...ext is valid!)
        if((newpath[i - 1] == '/') && (newpath[i + 2] == '/'))
          return -2;
      }
    }
  }

  // see if file is already there
  if(stat(newpath, &inode) != 0)
  {
#ifndef __WIN32__
    if(match(newpath) < 0)
#endif // __WIN32__
      return -1;
  }

  // all ok
  return 0;
}

FILE *fsafeopen(const char *path, const char *mode)
{
  char newpath[MAX_PATH];
  int i, ret;

  // validate pathname, and optionally retrieve a better name
  ret = fsafetranslate(path, newpath);

  // bad name, or security checks failed
  if(ret < -1)
    return NULL;

  // file NAME could not be translated
  if(ret == -1)
  {
    // if we're reading, any failure to translate the name means we back out
    for(i = 0; i < (int)strlen(mode); i++)
    {
      if(mode[i] == 'r' || mode[i] == '+')
        return NULL;
    }

    // if we're writing, the file may not already exist. if it does, though,
    // we want to use the translated name.
    strcpy(newpath, path);
  }

  // try opening the file
  return fopen(newpath, mode);
}
