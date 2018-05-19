/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2012 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <sys/stat.h>
#include <ctype.h>
#include <time.h>

// Must include after <time.h> (MSVC bug)
#include "util.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "const.h" // for MAX_PATH
#include "error.h"

struct mzx_resource
{
  const char *const base_name;
  char *path;

  /* So mzxrun requires fewer files even in CONFIG_EDITOR=1 build */
  bool optional;
};

/* Using C99 initializers would be nicer here, but MSVC doesn't support
 * them and we try to allow compilation with that compiler.
 *
 * As a result, these must be in exactly the same order as the
 * enum resource_id enumeration defines them.
 */
static struct mzx_resource mzx_res[] = {
#define ASSETS "assets/"
  { CONFFILE,                      NULL, false },
  { ASSETS "default.chr",          NULL, false },
  { ASSETS "edit.chr",             NULL, false },
  { ASSETS "smzx.pal",             NULL, false },
#ifdef CONFIG_EDITOR
  { ASSETS "ascii.chr",            NULL, true },
  { ASSETS "blank.chr",            NULL, true },
  { ASSETS "smzx.chr",             NULL, true },
#endif
#ifdef CONFIG_HELPSYS
  { ASSETS "help.fil",             NULL, true },
#endif
#ifdef CONFIG_RENDER_GL_PROGRAM
#define SHADERS ASSETS "shaders/"
#define SCALERS SHADERS "scalers/"
  { SCALERS,                       NULL, false },
  { SHADERS "scaler.vert",         NULL, false },
  { SCALERS "semisoft.frag",       NULL, false },
  { SHADERS "tilemap.vert",        NULL, false },
  { SHADERS "tilemap.frag",        NULL, false },
  { SHADERS "tilemap.smzx.frag",   NULL, false },
  { SHADERS "mouse.vert",          NULL, false },
  { SHADERS "mouse.frag",          NULL, false },
  { SHADERS "cursor.vert",         NULL, false },
  { SHADERS "cursor.frag",         NULL, false },
#endif
};

#ifdef CONFIG_CHECK_ALLOC

static void out_of_memory_check(void *p, const char *file, int line)
{
  char msgbuf[128];
  if(!p)
  {
    snprintf(msgbuf, sizeof(msgbuf), "Out of memory in %s:%d", file, line);
    msgbuf[sizeof(msgbuf)-1] = '\0';
    error(msgbuf, 2, 4, 0);
  }
}

void *check_calloc(size_t nmemb, size_t size, const char *file, int line)
{
  void *result = calloc(nmemb, size);
  out_of_memory_check(result, file, line);
  return result;
}

void *check_malloc(size_t size, const char *file, int line)
{
  void *result = malloc(size);
  out_of_memory_check(result, file, line);
  return result;
}

void *check_realloc(void *ptr, size_t size, const char *file, int line)
{
  void *result = realloc(ptr, size);
  out_of_memory_check(result, file, line);
  return result;
}

#endif /* CONFIG_CHECK_ALLOC */

int mzx_res_init(const char *argv0, bool editor)
{
  size_t i, bin_path_len = 0;
  struct stat file_info;
  char *full_path_base;
  char *full_path;
  char *bin_path;
  ssize_t g_ret;
  char *p_dir;
  int ret = 0;

  full_path_base = cmalloc(MAX_PATH);
  bin_path = cmalloc(MAX_PATH);
  p_dir = cmalloc(MAX_PATH);

  g_ret = get_path(argv0, bin_path, MAX_PATH);
  if(g_ret > 0)
  {
    // move and convert to absolute path style
    chdir(bin_path);
    getcwd(bin_path, MAX_PATH);
    bin_path_len = strlen(bin_path);

    // append the trailing '/'
    bin_path[bin_path_len++] = '/';
    bin_path[bin_path_len] = 0;
  }
  else
  {
    free(bin_path);
    bin_path = NULL;
  }

  /* Always try to use SHAREDIR/CONFDIR first, if possible. On some
   * platforms, such as Linux, this will be something other than
   * the current working directory (i.e. '.'). If this fails, try
   * to look up the resources relative to the binary's install
   * location, which should allow the MegaZeux binary to be more
   * "portable".
   */
  for(i = 0; i < END_RESOURCE_ID_T; i++)
  {
    size_t base_name_len = strlen(mzx_res[i].base_name);
    size_t p_dir_len;

    if(i == CONFIG_TXT)
      chdir(CONFDIR);
    else
      chdir(SHAREDIR);

    getcwd(p_dir, MAX_PATH);
    p_dir_len = strlen(p_dir);

    // if we can't add the path we should really fail hard
    if(p_dir_len + base_name_len + 1 >= MAX_PATH)
      continue;

    // append the trailing '/'
    p_dir[p_dir_len++] = '/';
    p_dir[p_dir_len] = 0;

    memcpy(full_path_base, p_dir, p_dir_len);
    memcpy(full_path_base + p_dir_len, mzx_res[i].base_name, base_name_len);
    full_path_base[p_dir_len + base_name_len] = 0;

    full_path = cmalloc(MAX_PATH);
    clean_path_slashes(full_path_base, full_path, MAX_PATH);

    // Attempt to load it from this new path
    if(!stat(full_path, &file_info))
    {
      mzx_res[i].path = full_path;
      continue;
    }

    free(full_path);

    // Try to locate the resource relative to the bin_path
    if(bin_path)
    {
      chdir(bin_path);
      if(!stat(mzx_res[i].base_name, &file_info))
      {
        mzx_res[i].path = cmalloc(bin_path_len + base_name_len + 1);
        memcpy(mzx_res[i].path, bin_path, bin_path_len);
        memcpy(mzx_res[i].path + bin_path_len,
         mzx_res[i].base_name, base_name_len);
        mzx_res[i].path[bin_path_len + base_name_len] = 0;
      }
    }
  }

  for(i = 0; i < END_RESOURCE_ID_T; i++)
  {
    /* Skip non-essential resources */
    if(!editor && mzx_res[i].optional)
      continue;

    if(!mzx_res[i].path)
    {
      warn("Failed to locate critical resource '%s'.\n",
       mzx_res[i].base_name);
      ret = 1;
    }
  }

  free(p_dir);
  free(bin_path);
  free(full_path_base);
  return ret;
}

void mzx_res_free(void)
{
  int i;
  for(i = 0; i < END_RESOURCE_ID_T; i++)
    if(mzx_res[i].path)
      free(mzx_res[i].path);
}

#ifdef USERCONFFILE
#define COPY_BUFFER_SIZE  4096
static unsigned char copy_buffer[COPY_BUFFER_SIZE];
#endif

char *mzx_res_get_by_id(enum resource_id id)
{
  #ifdef USERCONFFILE
  static char userconfpath[MAX_PATH];
  if (id == CONFIG_TXT)
  {
    FILE *fp;

    // Special handling for CONFIG_TXT to allow for user
    // configuration files
    sprintf(userconfpath, "%s/%s", getenv("HOME"), USERCONFFILE);

    // Check if the file can be opened for reading
    fp = fopen_unsafe(userconfpath, "rb");

    if (fp)
    {
      fclose(fp);
      return (char *)userconfpath;
    }
    // Otherwise, try to open the file for writing
    fp = fopen_unsafe(userconfpath, "wb");
    if (fp)
    {
      FILE *original = fopen_unsafe(mzx_res[id].path, "rb");
      if (original)
      {
        size_t bytes_read;
        for (;;)
        {
          bytes_read = fread(copy_buffer, 1, COPY_BUFFER_SIZE, original);
          if (bytes_read)
            fwrite(copy_buffer, 1, bytes_read, fp);
          else
            break;
        }
        fclose(fp);
        fclose(original);
        return (char *)userconfpath;
      }
      fclose(fp);
    }

    // If that's no good, just read the normal config file
  }
  #endif /* USERCONFFILE */
  return mzx_res[id].path;
}

// Get 2 bytes, little endian

int fgetw(FILE *fp)
{
  int a = fgetc(fp), b = fgetc(fp);
  if((a == EOF) || (b == EOF))
    return EOF;

  return (b << 8) | a;
}

// Get 4 bytes, little endian

int fgetd(FILE *fp)
{
  int a = fgetc(fp), b = fgetc(fp), c = fgetc(fp), d = fgetc(fp);
  if((a == EOF) || (b == EOF) || (c == EOF) || (d == EOF))
    return EOF;

  return (d << 24) | (c << 16) | (b << 8) | a;
}

// Put 2 bytes, little endian

void fputw(int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc(src >> 8, fp);
}

// Put 4 bytes, little endian

void fputd(int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc((src >> 8) & 0xFF, fp);
  fputc((src >> 16) & 0xFF, fp);
  fputc((src >> 24) & 0xFF, fp);
}

// Determine file size of an open FILE and rewind it

long ftell_and_rewind(FILE *f)
{
  long size;

  fseek(f, 0, SEEK_END);
  size = ftell(f);
  rewind(f);

  return size;
}

// Random function, returns an integer [0-range)

static unsigned long long rng_state;

// Seed the RNG from system time on startup
void rng_seed_init(void)
{
  unsigned long long seed = (((unsigned long long)time(NULL)) << 32) | clock();
  rng_set_seed(seed);
}

unsigned long long rng_get_seed(void)
{
  return rng_state;
}

void rng_set_seed(unsigned long long seed)
{
  rng_state = seed;
}

// xorshift*
// Implementation from https://en.wikipedia.org/wiki/Xorshift
unsigned int Random(unsigned long long range)
{
	unsigned long long x = rng_state;
  if (x == 0) x = 1;
	x ^= x >> 12; // a
	x ^= x << 25; // b
	x ^= x >> 27; // c
	rng_state = x;
	return ((x * 0x2545F4914F6CDD1D) >> 32) * range / 0xFFFFFFFF;
}

// FIXME: This function should probably die. It's unsafe.
void add_ext(char *src, const char *ext)
{
  size_t src_len = strlen(src);
  size_t ext_len = strlen(ext);

  if((src_len < ext_len) || (src[src_len - ext_len] != '.') ||
   strcasecmp(src + src_len - ext_len, ext))
  {
    strcat(src, ext);
  }
}

__utils_maybe_static ssize_t __get_path(const char *file_name, char *dest,
 unsigned int buf_len)
{
  ssize_t c = (ssize_t)strlen(file_name) - 1;

  // no path, or it's too long to store
  if(c == -1 || c > (int)buf_len)
  {
    if(buf_len > 0)
      dest[0] = 0;
    return -1;
  }

  while((file_name[c] != '/') && (file_name[c] != '\\') && c)
    c--;

  if(c > 0)
    memcpy(dest, file_name, c);
  dest[c] = 0;

  return c;
}

ssize_t get_path(const char *file_name, char *dest, unsigned int buf_len)
{
  return __get_path(file_name, dest, buf_len);
}

static int isslash(char n)
{
  return n=='\\' || n=='/';
}

void clean_path_slashes(const char *src, char *dest, size_t buf_size)
{
  unsigned int i = 0;
  unsigned int p = 0;
  size_t src_len = strlen(src);

  while((i < src_len) && (p < buf_size-1))
  {
    if(isslash(src[i]))
    {
      while(isslash(src[i]))
        i++;

      dest[p] = DIR_SEPARATOR_CHAR;
      p++;
    }
    else
    {
      dest[p] = src[i];
      i++;
      p++;
    }
  }
  dest[p] = '\0';

  if((p >= 2) && (dest[p-1] == DIR_SEPARATOR_CHAR) && (dest[p-2] != ':'))
    dest[p-1] = '\0';
}

void split_path_filename(const char *source,
 char *destpath, unsigned int dest_buffer_len,
 char *destfile, unsigned int file_buffer_len)
{
  char temppath[MAX_PATH];
  struct stat path_info;
  int stat_res = stat(source, &path_info);

  // If the entirety of source is a directory
  if((stat_res >= 0) && S_ISDIR(path_info.st_mode))
  {
    if(dest_buffer_len)
      clean_path_slashes(source, destpath, dest_buffer_len);

    if(file_buffer_len)
      strcpy(destfile, "");
  }
  else
  // If source has a directory and a file
  if((source[0] != '\0') && get_path(source, temppath, MAX_PATH))
  {
    // get_path leaves off trailing /, add 1 to offset.
    if(dest_buffer_len)
      clean_path_slashes(temppath, destpath, dest_buffer_len);

    if(file_buffer_len)
      strncpy(destfile, &(source[strlen(temppath) + 1]), file_buffer_len);
  }
  // Source is just a file or blank.
  else
  {
    if(dest_buffer_len)
      strcpy(destpath, "");

    if(file_buffer_len)
      strncpy(destfile, source, file_buffer_len);
  }
}

void join_path_names(char* target, int max_len, const char* path1, const char* path2)
{
  if(path1[strlen(path1)-1] == DIR_SEPARATOR_CHAR)
    snprintf(target, max_len, "%s%s", path1, path2);
  else
    snprintf(target, max_len, "%s%s%s", path1, DIR_SEPARATOR, path2);
}

int create_path_if_not_exists(const char *filename)
{
  struct stat stat_info;
  char parent_directory[MAX_PATH];

  if(!get_path(filename, parent_directory, MAX_PATH))
    return 1;

  if(!stat(parent_directory, &stat_info))
    return 2;

  create_path_if_not_exists(parent_directory);

  if(mkdir(parent_directory, 0755))
    return 3;

  return 0;
}

// Navigate a path name.
int change_dir_name(char *path_name, const char *dest)
{
  struct stat stat_info;
  char path[MAX_PATH];
  const char *current;
  const char *next;
  const char *end;
  char current_char;
  size_t len;

  if(!dest || !dest[0])
    return -1;

  if(!path_name)
    return -1;

  current = dest;
  end = dest + strlen(dest);

  // Destination starts with a root directory.
#if defined(__WIN32__) || defined(CONFIG_WII)

  if(dest[0] == DIR_SEPARATOR_CHAR)
    return -1;

  next = strchr(dest, ':');
  if(next)
  {
    if(next[1] != DIR_SEPARATOR_CHAR && next[1] != 0)
      return -1;

    snprintf(path, MAX_PATH, "%.*s" DIR_SEPARATOR, (int)(next - dest + 1),
     dest);

    if(stat(path, &stat_info) < 0)
      return -1;

    current = next + 1;
    if(current[0] == DIR_SEPARATOR_CHAR)
      current++;
  }
  else
  {
    snprintf(path, MAX_PATH, "%s" DIR_SEPARATOR, path_name);
  }

#else /* !defined(__WIN32__) && !defined(CONFIG_WII) */

  if(dest[0] == DIR_SEPARATOR_CHAR)
  {
    strcpy(path, DIR_SEPARATOR);
    current = dest + 1;
  }
  else
  {
    snprintf(path, MAX_PATH, "%s" DIR_SEPARATOR, path_name);
  }

#endif /* !defined(__WIN32__) && !defined(CONFIG_WII) */

  current_char = current[0];
  len = strlen(path);

  // Apply directory fragments to the path.
  while(current_char != 0)
  {
    // Increment next to skip the separator so it will be copied over.
    next = strchr(current, DIR_SEPARATOR_CHAR);
    if(!next) next = end;
    else      next++;

    // . does nothing, .. goes back one level
    if(current_char == '.')
    {
      if(current[1] == '.')
      {
        // Skip the rightmost separator (current level) and look for the
        // previous separator. If found, truncate the path to it.
        char *pos = path + len - 1;
        do
        {
          pos--;
        }
        while(pos >= path && *pos != DIR_SEPARATOR_CHAR);

        if(pos >= path)
        {
          pos[1] = 0;
          len = strlen(path);
        }
      }
    }
    else
    {
      snprintf(path + len, MAX_PATH - len, "%.*s", (int)(next - current),
       current);
      len = strlen(path);
    }

    current = next;
    current_char = current[0];
  }

  if(stat(path, &stat_info) >= 0)
  {
    clean_path_slashes(path, path_name, MAX_PATH);
    return 0;
  }

  return -1;
}


// Okay I seriously can't be bothered here to figure out
// which platforms actually have this function and which don't
static void *boyer_moore_memrchr(const void *mem, char ch, size_t len)
{
  char *e = (char *)mem + len;
  while(e-- != (char *)mem)
    if(*e == ch)
      return (void *)e;
  return NULL;
}

// Index must be an array of 256 ints
void boyer_moore_index(void *B, size_t b_len,
 int *index, bool ignore_case)
{
  char *b = (char *)B;
  int i;

  char *s = b;
  char *last = b + b_len - 1;
  char *c1, *c2;

  while(s < last)
  {
    if(!ignore_case)
    {
      c1 = boyer_moore_memrchr(b, *s, b_len);
      if(c1)
        index[(int)*s] = (last - c1);
    }
    else
    {
      c1 = boyer_moore_memrchr(b, tolower((int)*s), b_len);
      c2 = boyer_moore_memrchr(b, toupper((int)*s), b_len);
      if(c1 && c1 > c2)
        index[tolower((int)*s)] = (last - c1);
      else
      if(c2)
        index[tolower((int)*s)] = (last - c2);
    }
    s++;
  }
  for(i = 0; i < 256; i++)
    if(index[i] <= 0 || index[i] > (int)b_len)
      index[i] = b_len;
}

// Search for substring B in haystack A. The index greatly increases the
// search speed, especially for large needles. This is actually a reduced
// Boyer-Moore search, as the original version uses two separate indexes.
void *boyer_moore_search(void *A, size_t a_len, void *B, size_t b_len,
 int *index, bool ignore_case)
{
  unsigned char *a = (unsigned char *)A;
  unsigned char *b = (unsigned char *)B;
  size_t i = b_len - 1;
  int j;
  if(!ignore_case) {
    while(i < a_len) {
      j = b_len - 1;
      while(j >= 0 && a[i] == b[j])
        j--, i--;
      if(j == -1)
        return (void *)(a + i);
      i += MAX(1, index[(int)a[i]]) + (b_len - j - 1);
    }
  }
  else {
    while(i < a_len) {
      j = b_len - 1;
      while(j >= 0 && tolower((int)a[i]) == tolower((int)b[j]))
        j--, i--;
      if(j == -1)
        return (void *)(a + i);
      i += MAX(1, index[tolower((int)a[i])]) + (b_len - j - 1);
    }
  }
  return NULL;
}


int mem_getc(const unsigned char **ptr)
{
  int val = (*ptr)[0];
  *ptr += 1;
  return val;
}

int mem_getw(const unsigned char **ptr)
{
  int val = (*ptr)[0] | ((*ptr)[1] << 8);
  *ptr += 2;
  return val;
}

int mem_getd(const unsigned char **ptr)
{
  int val = (*ptr)[0] | ((*ptr)[1] << 8) | ((*ptr)[2] << 16) | ((int)((*ptr)[3]) << 24);
  *ptr += 4;
  return val;
}

void mem_putc(int src, unsigned char **ptr)
{
  (*ptr)[0] = src;
  *ptr += 1;
}

void mem_putw(int src, unsigned char **ptr)
{
  (*ptr)[0] = src & 0xFF;
  (*ptr)[1] = (src >> 8) & 0xFF;
  *ptr += 2;
}

void mem_putd(int src, unsigned char **ptr)
{
  (*ptr)[0] = src & 0xFF;
  (*ptr)[1] = (src >> 8) & 0xFF;
  (*ptr)[2] = (src >> 16) & 0xFF;
  (*ptr)[3] = (src >> 24) & 0xFF;
  *ptr += 4;
}


// like fsafegets, except from memory.
int memsafegets(char *dest, int size, char **src, char *end)
{
  char *pos = dest;
  char *next = *src;
  char *stop = MIN(end, next+size);
  char ch;

  // Return 0 if this is the end of the memory block
  if(next == NULL)
    return 0;

  // Copy the memory until the end, the bound, or a newline
  while(next<stop && (ch = *next)!='\n')
  {
    *pos = ch;
    pos++;
    next++;
  }
  *pos = 0;

  // Place the counter where the next line begins
  if(next < end)
  {
    *src = next + 1;
  }

  // Mark that this is the end
  else
  {
    *src = NULL;
  }

  // Length at least 1 -- get rid of \r and \n
  if(pos > dest)
    if(pos[-1] == '\r' || pos[-1] == '\n')
      pos[-1] = 0;

  // Length at least 2 -- get rid of \r and \n
  if(pos - 1 > dest)
    if(pos[-2] == '\r' || pos[-2] == '\n')
      pos[-2] = 0;

  return 1;
}


#if defined(__WIN32__) && defined(__STRICT_ANSI__)

/* On WIN32 with C99 defining __STRICT_ANSI__ these POSIX.1-2001 functions
 * are not available. The stricmp/strnicmp functions are available, but not
 * to C99 programs.
 *
 * Redefine them here; these are copied from glibc, and should be optimal.
 */

int strcasecmp(const char *s1, const char *s2)
{
  while(*s1 != '\0' && tolower(*s1) == tolower(*s2))
  {
    s1++;
    s2++;
  }

  return tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n)
{
  if(n == 0)
    return 0;

  while(n-- != 0 && tolower(*s1) == tolower(*s2))
  {
    if(n == 0 || *s1 == '\0' || *s2 == '\0')
      break;
    s1++;
    s2++;
  }

  return tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2);
}

#endif // __WIN32__ && __STRICT_ANSI__

#if defined(__WIN32__) || defined(__amigaos__)

// strsep() stolen from glibc (GPL)

char *strsep(char **stringp, const char *delim)
{
  char *begin, *end;

  begin = *stringp;
  if(!begin)
    return NULL;

  if(delim[0] == '\0' || delim[1] == '\0')
  {
    char ch = delim[0];
    if(ch == '\0')
      end = NULL;
    else
    {
      if(*begin == ch)
        end = begin;
      else if(*begin == '\0')
        end = NULL;
      else
        end = strchr(begin + 1, ch);
    }
  }
  else
    end = strpbrk(begin, delim);

  if(end)
  {
    *end++ = '\0';
    *stringp = end;
  }
  else
    *stringp = NULL;

  return begin;
}

#endif // __WIN32__ || __amigaos__

long dir_tell(struct mzx_dir *dir)
{
  return dir->pos;
}

bool dir_open(struct mzx_dir *dir, const char *path)
{
  dir->d = opendir(path);
  if(!dir->d)
    return false;

  dir->entries = 0;
  while(readdir(dir->d) != NULL)
    dir->entries++;

#if defined(CONFIG_PSP) || defined(CONFIG_3DS)
  strncpy(dir->path, path, PATH_BUF_LEN);
  dir->path[PATH_BUF_LEN - 1] = 0;
  closedir(dir->d);
  dir->d = opendir(path);
#else
  rewinddir(dir->d);
#endif

  dir->pos = 0;
  return true;
}

void dir_close(struct mzx_dir *dir)
{
  if(dir->d)
  {
    closedir(dir->d);
    dir->d = NULL;
    dir->entries = 0;
    dir->pos = 0;
  }
}

void dir_seek(struct mzx_dir *dir, long offset)
{
  long i;

  if(!dir->d)
    return;

  dir->pos = CLAMP(offset, 0L, dir->entries);

#if defined(CONFIG_PSP) || defined(CONFIG_3DS)
  closedir(dir->d);
  dir->d = opendir(dir->path);
  if(!dir->d)
    return;
#else
  rewinddir(dir->d);
#endif

  for(i = 0; i < dir->pos; i++)
    readdir(dir->d);
}

bool dir_get_next_entry(struct mzx_dir *dir, char *entry)
{
  struct dirent *inode;

  if(!dir->d)
    return false;

  dir->pos = MIN(dir->pos + 1, dir->entries);

  inode = readdir(dir->d);
  if(!inode)
  {
    entry[0] = 0;
    return false;
  }

  snprintf(entry, PATH_BUF_LEN, "%s", inode->d_name);
  return true;
}

#if defined(CONFIG_AUDIO) || defined(CONFIG_EDITOR)

/* It would be nice if this could be static, but we can't put it in either
 * edit.c or audio.c, because one (but NOT the other) might be disabled.
 *
 * In this case, the code wouldn't be compiled (noticed by the PSP port).
 */
const char *const mod_gdm_ext[] =
{
  ".xm", ".s3m", ".mod", ".med", ".mtm", ".stm", ".it", ".669", ".ult",
  ".wav", ".dsm", ".far", ".okt", ".amf", ".ogg", ".gdm", NULL
};

#endif // CONFIG_AUDIO || CONFIG_EDITOR

#if defined(__amigaos__)

long __stack_chk_guard[8];

void __stack_chk_fail(void)
{
  warn("Stack overflow detected; terminated");
  exit(0);
}

#endif // __amigaos__

#if defined(CONFIG_PSP) || defined(CONFIG_NDS)
FILE *popen(const char *command, const char *type) {
	return NULL;
}

int pclose(FILE *stream) {
	return 0;
}
#endif // CONFIG_PSP || CONFIG_NDS
