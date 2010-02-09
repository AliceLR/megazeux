/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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
#include "manifest.h"
#include "util.h"

#include "sha256.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define URL_BUF_LEN 256

static bool manifest_parse_sha256(const char *p, uint32_t sha256[8])
{
  int i, j;

  // A SHA256 digest is a fixed length (prefix zeroes)
  if(strlen(p) != 8 * 8)
    return false;

  // Walk the sha256 "registers" and decompose 8byte hex chunks
  for (i = 0, j = 0; i < 8; i++, j += 8)
  {
    char reg_hex[9], *end_ptr;

    // Pinch an 8 byte hex block from source string
    memcpy(reg_hex, p + j, 8);
    reg_hex[8] = 0;

    // Convert the hex string into an integer and check for errors
    sha256[i] = (uint32_t)strtoul(reg_hex, &end_ptr, 16);
    if(end_ptr[0])
      return false;
  }

  return true;
}

static bool manifest_parse_size(char *p, unsigned long *size)
{
  char *endptr;
  *size = strtoul(p, &endptr, 10);
  if(endptr[0])
    return false;
  return true;
}

static void manifest_entry_free(struct manifest_entry *e)
{
  free(e->name);
  free(e);
}

void manifest_list_free(struct manifest_entry **head)
{
  struct manifest_entry *e, *next_e;
  for(e = *head; e; e = next_e)
  {
    next_e = e->next;
    manifest_entry_free(e);
  }
  *head = NULL;
}

static struct manifest_entry *manifest_entry_copy(struct manifest_entry *src)
{
  struct manifest_entry *dest;
  size_t name_len;

  dest = malloc(sizeof(struct manifest_entry));

  name_len = strlen(src->name);
  dest->name = malloc(name_len + 1);
  strncpy(dest->name, src->name, name_len);
  dest->name[name_len] = 0;

  memcpy(dest->sha256, src->sha256, sizeof(uint32_t) * 8);
  dest->size = src->size;
  dest->next = NULL;

  return dest;
}

static struct manifest_entry *manifest_list_copy(struct manifest_entry *src)
{
  struct manifest_entry *src_e, *dest = NULL, *dest_e, *next_dest_e;

  for(src_e = src; src_e; src_e = src_e->next)
  {
    next_dest_e = manifest_entry_copy(src_e);
    if(dest)
    {
      dest_e->next = next_dest_e;
      dest_e = dest_e->next;
    }
    else
      dest = dest_e = next_dest_e;
  }

  return dest;
}

static struct manifest_entry *manifest_list_create(char *buffer,
 unsigned long len)
{
  struct manifest_entry *head = NULL, *e = NULL, *next_e;
  char *m = buffer;

  // Walk the manifest line by line
  while(m[0])
  {
    size_t line_len;
    char *line;

    next_e = calloc(1, sizeof(struct manifest_entry));
    if(head)
    {
      e->next = next_e;
      e = e->next;
    }
    else
      head = e = next_e;

    // Grab the sha256 token
    line = strsep(&m, " ");
    if(!line)
      goto err_invalid_manifest;

    /* Break the 64 character (8x8) hex string down into
     * eight constituent 32bit SHA result registers (32*8=256).
     */
    if(!manifest_parse_sha256(line, e->sha256))
      goto err_invalid_manifest;

    // Grab the size token
    line = strsep(&m, " ");
    if(!line)
      goto err_invalid_manifest;

    // Validate and parse it
    if(!manifest_parse_size(line, &e->size))
      goto err_invalid_manifest;

    // Grab the filename token
    line = strsep(&m, "\n");
    if(!line)
      goto err_invalid_manifest;

    line_len = strlen(line);
    e->name = malloc(line_len + 1);
    strncpy(e->name, line, line_len);
    e->name[line_len] = 0;
  }

  e->next = NULL;
  return head;

err_invalid_manifest:
  warning("Malformed manifest file.\n");
  manifest_list_free(&head);
  return head;
}

static struct manifest_entry *manifest_get_local(void)
{
  struct manifest_entry *manifest;
  char *manifest_buffer;
  long manifest_len;
  FILE *f;

  f = fopen("manifest.txt", "rb");
  if(!f)
  {
    warning("Local manifest.txt is missing\n");
    return NULL;
  }

  manifest_len = ftell_and_rewind(f);
  if(manifest_len <= 0)
  {
    warning("Local manifest.txt is too short\n");
    return NULL;
  }

  manifest_buffer = malloc(manifest_len + 1);
  if(fread(manifest_buffer, manifest_len, 1, f) != 1)
  {
    warning("Local manifest.txt could not be read\n");
    free(manifest_buffer);
    fclose(f);
    return NULL;
  }
  manifest_buffer[manifest_len] = 0;
  fclose(f);

  manifest = manifest_list_create(manifest_buffer, manifest_len);
  free(manifest_buffer);
  return manifest;
}

static struct manifest_entry *manifest_get_remote(struct host *h,
 const char *base)
{
  char *manifest_buffer, buf[URL_BUF_LEN];
  struct manifest_entry *manifest = NULL;
  unsigned long manifest_len;

  snprintf(buf, URL_BUF_LEN, "%s/manifest.txt", base);

  manifest_buffer = host_get_file(h, buf, "text/plain", &manifest_len);
  if(manifest_buffer)
  {
    FILE *f = fopen("manifest.txt", "wb");
    if(f)
    {
      if(fwrite(manifest_buffer, manifest_len, 1, f) == 1)
        manifest = manifest_list_create(manifest_buffer, manifest_len);
      else
        warning("Local manifest.txt could not be rewritten\n");
      fclose(f);
    }
    else
      warning("Local manifest.txt could not be opened for writing\n");
    free(manifest_buffer);
  }
  else
    warning("Remote manifest.txt could not be read\n");

  return manifest;
}

static bool manifest_entry_equal(struct manifest_entry *l,
 struct manifest_entry *r)
{
  return strcmp(l->name, r->name) == 0 &&
         l->size == r->size &&
         memcmp(l->sha256, r->sha256, sizeof(uint32_t) * 8) == 0;
}

static void manifest_lists_remove_duplicates(struct manifest_entry **local,
 struct manifest_entry **remote)
{
  struct manifest_entry *l, *prev_l, *next_l;

  for(l = *local, prev_l = NULL; l; l = next_l)
  {
    struct manifest_entry *r, *prev_r, *next_r;

    for(r = *remote, prev_r = NULL; r; r = next_r)
    {
      next_r = r->next;

      if(manifest_entry_equal(l, r))
      {
        if(prev_r)
          prev_r->next = next_r;
        else
          *remote = next_r;
        manifest_entry_free(r);
        break;
      }
      else
        prev_r = r;
    }

    next_l = l->next;

    if(r)
    {
      if(prev_l)
        prev_l->next = next_l;
      else
        *local = next_l;
      manifest_entry_free(l);
    }
    else
      prev_l = l;
  }
}

static bool manifest_entry_check_buffer_validity(struct manifest_entry *e,
 char *buffer, unsigned long buffer_len)
{
  SHA256_ctx ctx;

  if(e->size != buffer_len)
    return false;

  SHA256_init(&ctx);
  SHA256_update(&ctx, buffer, buffer_len);
  SHA256_final(&ctx);

  if(memcmp(ctx.H, e->sha256, sizeof(uint32_t) * 8) != 0)
    return false;

  return true;
}

static bool manifest_entry_check_local_validity(struct manifest_entry *e)
{
  bool valid = false;
  FILE *f;

  /* Local validation poses two additional checks -- whether
   * or not the file can be opened, and whether or not it can be read.
   */
  f = fopen(e->name, "rb");
  if(f)
  {
    unsigned long file_length = ftell_and_rewind(f);
    char *file_data = malloc(file_length);

    if(fread(file_data, file_length, 1, f) == 1)
      valid = manifest_entry_check_buffer_validity(e, file_data, file_length);

    free(file_data);
    fclose(f);
  }

  return valid;
}

static void manifest_add_list_validate_augment(struct manifest_entry *local,
 struct manifest_entry **added)
{
  struct manifest_entry *e, *a = *added;

  // Scan along to penultimate `added' (if we have any)
  if(a)
    while(a->next)
      a = a->next;

  for(e = local; e; e = e->next)
  {
    struct manifest_entry *new_added;

    if(manifest_entry_check_local_validity(e))
     continue;

    warning("Local file '%s' failed manifest validation\n", e->name);

    new_added = manifest_entry_copy(e);
    if(*added)
    {
      a->next = new_added;
      a = a->next;
    }
    else
      a = *added = new_added;
  }
}

bool manifest_get_updates(struct host *h, const char *basedir,
 struct manifest_entry **removed, struct manifest_entry **replaced,
 struct manifest_entry **added)
{
  struct manifest_entry *local, *remote;

  local = manifest_get_local();

  remote = manifest_get_remote(h, basedir);
  if(!remote)
    return false;

  *replaced = NULL;
  *removed = NULL;
  *added = NULL;

  if(local)
  {
    struct manifest_entry *added_copy;

    // Copy remote -> added because the list is modified by the next call
    *added = manifest_list_copy(remote);
    *removed = local;

    /* The "removed" list is simply the local list; both lists are modified
     * in place to filter any duplicate entries.
     */
    manifest_lists_remove_duplicates(removed, added);

    /* This hack removes the "added" list entries from the remote list, to
     * give us a list containing only the files that remained the same.
     */
    added_copy = manifest_list_copy(*added);
    manifest_lists_remove_duplicates(&remote, &added_copy);
    assert(added_copy == NULL);
  }

  /* Now we've decided what files should be added/removed, we use the remote
   * subset manifest to produce a replacement list. Any file that doesn't match
   * the remote manifest is added to this third list.
   */
  manifest_add_list_validate_augment(remote, replaced);
  manifest_list_free(&remote);
  return true;
}

bool manifest_entry_download_replace(struct host *h, const char *basedir,
 struct manifest_entry *e)
{
  char *file_data, buf[URL_BUF_LEN];
  unsigned long file_size;
  bool ret = false;
  FILE *f;

  snprintf(buf, URL_BUF_LEN, "%s/%08x%08x%08x%08x%08x%08x%08x%08x", basedir,
    e->sha256[0], e->sha256[1], e->sha256[2], e->sha256[3],
    e->sha256[4], e->sha256[5], e->sha256[6], e->sha256[7]);

  file_data = host_get_file(h, buf, "application/octet-stream", &file_size);
  if(!file_data)
  {
    warning("File '%s' could not be downloaded\n", e->name);
    goto err_out;
  }

  if(!manifest_entry_check_buffer_validity(e, file_data, file_size))
  {
    warning("File '%s' failed validation\n", e->name);
    goto err_free_file_data;
  }

  f = fopen(e->name, "wb");
  if(!f)
  {
    // Since directly overwriting failed, try moving it out of the way
    snprintf(buf, URL_BUF_LEN, "%s~", e->name);
    if(rename(e->name, buf))
    {
      warning("Failed to rename in-use file '%s' to '%s'\n", e->name, buf);
      goto err_close_free_file_data;
    }

    f = fopen(e->name, "wb");
    if(!f)
    {
      warning("Unable to open file '%s' for writing\n", e->name);
      goto err_close_free_file_data;
    }
  }

  if(fwrite(file_data, file_size, 1, f) != 1)
  {
    warning("Failed to write out updated file '%s'\n", e->name);
    goto err_close_free_file_data;
  }

  ret = true;
err_close_free_file_data:
  fclose(f);
err_free_file_data:
  free(file_data);
err_out:
  return ret;
}
