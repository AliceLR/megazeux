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
#include "updater.h"
#include "const.h"
#include "event.h"
#include "util.h"
#include "game.h"
#include "graphics.h"
#include "window.h"
#include "error.h"

#include "editor/window.h"

#include "network/manifest.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <string.h>
#include <errno.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifndef PLATFORM
#error Must define a valid "friendly" platform name!
#endif

#define MAX_RETRIES   3

#define OUTBOUND_PORT 80
#define LINE_BUF_LEN  256
#define BLOCK_SIZE    4096

#define UPDATES_TXT   "updates.txt"
#define DELETE_TXT    "delete.txt"

#define WIDGET_BUF_LEN 80

static struct manifest_entry *delete_list, *delete_p;

static char widget_buf[WIDGET_BUF_LEN];

static char previous_dir[MAX_PATH];

static long final_size = -1;
static bool cancel_update;

static char **process_argv;
static int process_argc;

static char **rewrite_argv_for_execv(int argc, char **argv)
{
  char **new_argv = cmalloc((argc+1) * sizeof(char *));
  char *arg;
  int length;
  int pos;
  int i;
  int i2;

  // Due to a bug in execv, args with spaces present are treated as multiple
  // args in the new process. Each arg in argv must be wrapped in double quotes
  // to work properly. Because of this, " and \ must also be escaped.

  for(i = 0; i < argc; i++)
  {
    length = strlen(argv[i]);
    arg = cmalloc(length * 2 + 2);
    arg[0] = '"';

    for(i2 = 0, pos = 1; i2 < length; i2++, pos++)
    {
      switch(argv[i][i2])
      {
        case '"':
        case '\\':
          arg[pos] = '\\';
          pos++;
          break;
      }
      arg[pos] = argv[i][i2];
    }

    arg[pos] = '"';
    arg[pos + 1] = '\0';

    new_argv[i] = arg;
  }

  new_argv[argc] = NULL;

  return new_argv;
}

static bool check_prune_basedir(const char *file)
{
  static char path[MAX_PATH];
  ssize_t ret;

  ret = get_path(file, path, MAX_PATH);
  if(ret < 0)
  {
    error("Failed to prune directories (path too long)", 1, 8, 0);
    return false;
  }

  // This file has no base directory
  if(ret == 0)
    return true;

  // At the head of the recursion we remove the directory
  rmdir(path);

  // Recursion; remove any parent directory
  return check_prune_basedir(path);
}

/* FIXME: The allocation of MAX_PATH on the stack in a recursive
 *        function WILL cause problems, eventually!
 */
static bool check_create_basedir(const char *file)
{
  static struct stat s;
  char path[MAX_PATH];
  ssize_t ret;

  ret = get_path(file, path, MAX_PATH);
  if(ret < 0)
  {
    error("Failed to create directories (path too long)", 1, 8, 0);
    return false;
  }

  // This file has no base directory
  if(ret == 0)
    return true;

  if(stat(path, &s) < 0)
  {
    // Every other kind of error is fatal
    if(errno != ENOENT)
    {
      error("Unknown stat() error occurred", 1, 8, 0);
      return false;
    }

    // Recursion; create any parent directory
    if(!check_create_basedir(path))
      return false;

    // At the tail of the recursion we create the directory
    mkdir(path, 0777);
    return true;
  }

  if(!S_ISDIR(s.st_mode))
  {
    snprintf(widget_buf, WIDGET_BUF_LEN,
     "File \"%s\" prevents creation of directory by same name", path);
    widget_buf[WIDGET_BUF_LEN - 1] = 0;
    error(widget_buf, 1, 8, 0);
    return false;
  }

  return true;
}

static void check_cancel_update(void)
{
  update_event_status();
  if(get_key(keycode_internal) == IKEY_ESCAPE
   || get_exit_status())
    cancel_update = true;
}

static void recv_cb(long offset)
{
  check_cancel_update();

  if(final_size > 0 && offset > final_size)
  {
    error("Transferred more than expected uncompressed size.", 1, 8, 0);
    cancel_update = true;
    return;
  }

  meter_interior(offset, final_size);
  update_screen();
}

static bool cancel_cb(void)
{
  return cancel_update;
}

static void delete_hook(const char *file)
{
  struct manifest_entry *new_entry;
  struct SHA256_ctx ctx;
  bool ret;
  FILE *f;

  new_entry = ccalloc(1, sizeof(struct manifest_entry));
  if(!new_entry)
    goto err_out;

  if(delete_p)
  {
    delete_p->next = new_entry;
    delete_p = delete_p->next;
  }
  else
    delete_list = delete_p = new_entry;

  delete_p->name = cmalloc(strlen(file) + 1);
  if(!delete_p->name)
    goto err_delete_p;
  strcpy(delete_p->name, file);

  f = fopen_unsafe(file, "rb");
  if(!f)
    goto err_delete_p_name;
  delete_p->size = (unsigned long)ftell_and_rewind(f);

  ret = manifest_compute_sha256(&ctx, f, delete_p->size);
  fclose(f);
  if(!ret)
    goto err_delete_p_name;

  memcpy(delete_p->sha256, ctx.H, sizeof(Uint32) * 8);
  return;

err_delete_p_name:
  free(delete_p->name);
err_delete_p:
  free(delete_p);
  delete_p = NULL;
err_out:
  return;
}

static bool swivel_current_dir(bool have_video)
{
  bool ret = false;
  char *base_path;
  int g_ret;

  // Store the user's current directory, so we can get back to it
  getcwd(previous_dir, MAX_PATH);

  base_path = cmalloc(MAX_PATH);

  // Find and change into the base path for this MZX binary
  g_ret = get_path(process_argv[0], base_path, MAX_PATH);
  if(g_ret <= 0)
    goto err_free_base_path;

  if(chdir(base_path))
  {
    if(have_video)
      error("Failed to change into install directory.", 1, 8, 0);
    else
      warn("Failed to change into install directory.\n");
    goto err_free_base_path;
  }

  ret = true;
err_free_base_path:
  free(base_path);
  return ret;
}

static bool swivel_current_dir_back(bool have_video)
{
  if(chdir(previous_dir))
  {
    if(have_video)
      error("Failed to change back to user directory.", 1, 8, 0);
    else
      warn("Failed to change back to user directory.\n");
    return false;
  }

  return true;
}

static bool backup_original_manifest(void)
{
  unsigned int len, pos = 0;
  char block[BLOCK_SIZE];
  FILE *input, *output;
  bool ret = false;
  struct stat s;

  // No existing manifest; this is non-fatal
  if(stat(MANIFEST_TXT, &s))
  {
    ret = true;
    goto err_out;
  }

  input = fopen_unsafe(MANIFEST_TXT, "rb");
  if(!input)
    goto err_out;

  output = fopen_unsafe(MANIFEST_TXT "~", "wb");
  if(!output)
    goto err_close_input;

  len = (unsigned int)ftell_and_rewind(input);

  while(pos < len)
  {
    unsigned int block_size = MIN(BLOCK_SIZE, len - pos);

    if(fread(block, block_size, 1, input) != 1)
      goto err_close_output;
    if(fwrite(block, block_size, 1, output) != 1)
      goto err_close_output;

    pos += block_size;
  }

  ret = true;
err_close_output:
  fclose(output);
err_close_input:
  fclose(input);
err_out:
  return ret;
}

static bool restore_original_manifest(bool ret)
{
  struct stat s;

  if(ret)
  {
    /* The update was successful, so we try to remove the backup manifest.
     * Ignore any errors that might occur as a result of this file not
     * existing (it won't if we never backed a manifest up).
     */
    unlink(MANIFEST_TXT "~");
    return true;
  }

  // Try to remove original manifest before restoration
  if(unlink(MANIFEST_TXT))
  {
    error("Failed to remove " MANIFEST_TXT ". Check permissions.", 1, 8, 0);
    return false;
  }

  // No manifest to restore; consider this successful
  if(stat(MANIFEST_TXT "~", &s))
    return true;

  // Try to restore backup manifest
  if(rename(MANIFEST_TXT "~", MANIFEST_TXT))
  {
    error("Failed to roll back manifest. Check permissions.", 1, 8, 0);
    return false;
  }

  return true;
}

static bool write_delete_list(void)
{
  struct manifest_entry *e;
  FILE *f;

  if(delete_list)
  {
    f = fopen_unsafe(DELETE_TXT, "ab");
    if(!f)
    {
      error("Failed to create \"" DELETE_TXT "\". Check permissions.", 1, 8, 0);
      return false;
    }

    for(e = delete_list; e; e = e->next)
    {
      fprintf(f, "%08x%08x%08x%08x%08x%08x%08x%08x %lu %s\n",
       e->sha256[0], e->sha256[1], e->sha256[2], e->sha256[3],
       e->sha256[4], e->sha256[5], e->sha256[6], e->sha256[7],
       e->size, e->name);
    }

    fclose(f);
  }
  return true;
}

static void apply_delete_list(void)
{
  struct manifest_entry *e_next = delete_list;
  struct manifest_entry *e;
  struct stat s;
  bool ret;
  FILE *f;

  while(e_next)
  {
    e = e_next;
    e_next = e->next;

    if(!stat(e->name, &s))
    {
      f = fopen_unsafe(e->name, "rb");
      if(!f)
        goto err_delete_failed;

      ret = manifest_entry_check_validity(e, f);
      fclose(f);

      if(ret)
      {
        if(unlink(e->name))
          goto err_delete_failed;

        /* Obtain the path for this file. If the file isn't at the top
         * level, and the directory is empty (rmdir ensures this)
         * the directory will be pruned.
         */
        check_prune_basedir(e->name);
      }
    }

    // File was removed, doesn't exist, or is non-applicable; remove from list
    if(delete_list == e)
      delete_list = e_next;

    manifest_entry_free(e);
    continue;

err_delete_failed:
    {
      char buf[72];
      snprintf(buf, 72, "Failed to delete \"%.30s\". Check permissions.",
       e->name);
      buf[71] = 0;

      error(buf, 1, 8, 0);

      if(e_next)
        e->next = e_next->next;

      continue;
    }
  }
}

static bool reissue_connection(struct config_info *conf, struct host **h,
 char *host_name, int is_automatic)
{
  bool ret = false;
  int buf_len;

  assert(h != NULL);

  /* We might be passed an existing socket. If we have been, close
   * and destroy the associated host, and create a new one.
   */
  if(*h)
    host_destroy(*h);

  *h = host_create(HOST_TYPE_TCP, HOST_FAMILY_IPV4);
  if(!*h)
  {
    if(!is_automatic)
      error("Failed to create TCP client socket.", 1, 8, 0);
    goto err_out;
  }

  if(is_automatic)
    host_set_timeout_ms(*h, 1000);

  m_hide();

  buf_len = snprintf(widget_buf, WIDGET_BUF_LEN,
   "Connecting to \"%s\". Please wait..", host_name);
  widget_buf[WIDGET_BUF_LEN - 1] = 0;

  draw_window_box(3, 11, 76, 13, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  write_string(widget_buf, (WIDGET_BUF_LEN - buf_len) >> 1, 12, DI_TEXT, 0);
  update_screen();

  if(!host_connect(*h, host_name, OUTBOUND_PORT))
  {
    if(!is_automatic)
    {
      buf_len = snprintf(widget_buf, WIDGET_BUF_LEN,
       "Connection to \"%s\" failed.", host_name);
      widget_buf[WIDGET_BUF_LEN - 1] = 0;
      error(widget_buf, 1, 8, 0);
    }
  }
  else
    ret = true;

  clear_screen(32, 7);
  m_show();
  update_screen();

err_out:
  return ret;
}

static void __check_for_updates(struct world *mzx_world, struct config_info *conf,
 int is_automatic)
{
  int cur_host;
  char *update_host;
  bool try_next_host = true;
  bool ret = false;

  set_context(CTX_UPDATER);

  if(conf->update_host_count < 1)
  {
    error("No updater hosts defined! Aborting.", 1, 8, 0);
    goto err_out;
  }

  if(!swivel_current_dir(true))
    goto err_out;

  for(cur_host = 0; (cur_host < conf->update_host_count) && try_next_host; cur_host++)
  {
    char **list_entries, buffer[LINE_BUF_LEN], *url_base, *value;
    struct manifest_entry *removed, *replaced, *added, *e;
    int i = 0, entries = 0, buf_len, result;
    char update_branch[LINE_BUF_LEN];
    const char *version = VERSION;
    int list_entry_width = 0;
    enum host_status status;
    struct host *h = NULL;
    struct http_info req;
    unsigned int retries;
    FILE *f;

    // Acid test: Can we write to this directory?
    f = fopen_unsafe(UPDATES_TXT, "w+b");
    if(!f)
    {
      error("Failed to create \"" UPDATES_TXT "\". Check permissions.", 1, 8, 0);
      goto err_chdir;
    }

    update_host = conf->update_hosts[cur_host];

    if(!reissue_connection(conf, &h, update_host, is_automatic))
      goto err_host_destroy;

    for(retries = 0; retries < MAX_RETRIES; retries++)
    {
      // Grab the file containing the names of the current Stable and Unstable
      strcpy(req.url, "/" UPDATES_TXT);
      strcpy(req.expected_type, "text/plain");

      status = host_recv_file(h, &req, f);
      rewind(f);

      if(status == HOST_SUCCESS)
        break;

      // Stop early on redirect and client error codes
      if(-status == HOST_HTTP_REDIRECT || -status == HOST_HTTP_CLIENT_ERROR)
      {
        retries = MAX_RETRIES;
        break;
      }

      if(!reissue_connection(conf, &h, update_host, is_automatic))
        goto err_host_destroy;
    }

    if(retries == MAX_RETRIES)
    {
      if(!is_automatic)
      {
        snprintf(widget_buf, WIDGET_BUF_LEN, "Failed to download \""
         UPDATES_TXT "\" (%d/%d).\n", req.status_code, status);
        widget_buf[WIDGET_BUF_LEN - 1] = 0;
        error(widget_buf, 1, 8, 0);
      }
      goto err_host_destroy;
    }

    snprintf(update_branch, LINE_BUF_LEN, "Current-%.240s",
     conf->update_branch_pin);

    // Walk this list (of two, hopefully)
    while(true)
    {
      char *m = buffer, *key;
      value = NULL;

      // Grab a single line from the manifest
      if(!fgets(buffer, LINE_BUF_LEN, f))
        break;

      key = strsep(&m, ":\n");
      if(!key)
        break;

      value = strsep(&m, ":\n");
      if(!value)
        break;

      if(strcmp(key, update_branch) == 0)
        break;
    }

    fclose(f);
    unlink(UPDATES_TXT);

    /* There was no "Current-XXX: Version" found; we cannot proceed with the
     * update because we cannot compute an update URL below.
     */
    if(!value)
    {
      if(!is_automatic)
        error("Failed to identify applicable update version.", 1, 8, 0);
      goto err_host_destroy;
    }

    /* There's likely to be a space prepended to the version number.
     * Skip it here.
     */
    if(value[0] == ' ')
      value++;

    /* We found the latest update version, but we should check to see if that
     * matches the version we're already using. The user may choose to receive
     * "stability" updates for their current major version, or upgrade to the
     * newest one.
     */
    if(strcmp(value, version) != 0)
    {
      struct element *elements[6];
      struct dialog di;

      conf->update_available = 1;

      // If this is an auto check and silent mode is enabled, we can stop here.
      if(is_automatic && conf->update_auto_check == UPDATE_AUTO_CHECK_SILENT)
      {
        try_next_host = false;
        goto err_host_destroy;
      }

      buf_len = snprintf(widget_buf, WIDGET_BUF_LEN,
       "A new major version is available (%s)", value);
      widget_buf[WIDGET_BUF_LEN - 1] = 0;

      elements[0] = construct_label((55 - buf_len) >> 1, 2, widget_buf);

      elements[1] = construct_label(2, 4,
       "You can continue to receive updates for the version\n"
       "installed (if available), or you can upgrade to the\n"
       "newest major version (recommended).");

      elements[2] = construct_label(2, 8,
       "If you do not upgrade, this question will be asked\n"
       "again the next time you run the updater.\n");

      elements[3] = construct_button(9, 11, "Upgrade", 0);
      elements[4] = construct_button(21, 11, "Update Old", 1);
      elements[5] = construct_button(36, 11, "Cancel", 2);

      construct_dialog(&di, "New Major Version", 11, 6, 55, 14, elements, 6, 3);
      result = run_dialog(mzx_world, &di);
      destruct_dialog(&di);

      // User pressed Escape, abort all updates
      if(result < 0 || result == 2)
      {
        try_next_host = false;
        goto err_host_destroy;
      }

      // User pressed Upgrade, use new major
      if(result == 0)
        version = value;
    }

    /* We can now compute a unique URL base for the updater. This will
     * be composed of a user-selected version and a static platform-archicture
     * name.
     */
    url_base = cmalloc(LINE_BUF_LEN);
    snprintf(url_base, LINE_BUF_LEN, "/%s/" PLATFORM, version);
    debug("Update base URL: %s\n", url_base);

    /* The call to manifest_get_updates() destroys any existing manifest
     * file in this directory. Since we still allow user to abort after
     * this call, and downloading the updates may fail, we copy the
     * old manifest to a backup location and optionally restore it later.
     */
    if(!backup_original_manifest())
    {
      error("Failed to back up manifest. Check permissions.", 1, 8, 0);
      try_next_host = false;
      goto err_free_url_base;
    }

    for(retries = 0; retries < MAX_RETRIES; retries++)
    {
      m_hide();

      draw_window_box(3, 11, 76, 13, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
      write_string("Computing manifest deltas (added, replaced, deleted)..",
       13, 12, DI_TEXT, 0);
      update_screen();

      status = manifest_get_updates(h, url_base, &removed, &replaced, &added);

      clear_screen(32, 7);
      m_show();
      update_screen();

      if(status == HOST_SUCCESS)
        break;

      // Unsupported platform.
      if(-status == HOST_HTTP_REDIRECT || -status == HOST_HTTP_CLIENT_ERROR)
      {
        error("No updates available for this platform.", 1, 8, 0);
        goto err_roll_back_manifest;
      }

      if(!reissue_connection(conf, &h, update_host, 0))
        goto err_roll_back_manifest;
    }

    if(retries == MAX_RETRIES)
    {
      error("Failed to compute update manifests", 1, 8, 0);
      goto err_roll_back_manifest;
    }

    // At this point, we have a successful manifest, so we won't need another host
    try_next_host = false;

    if(!removed && !replaced && !added)
    {
      struct element *elements[3];
      struct dialog di;

      if(is_automatic)
        goto err_free_update_manifests;

      elements[0] = construct_label(2, 2, "This client is already current.");
      elements[1] = construct_button(7, 4, "OK", 0);
      elements[2] = construct_button(13, 4, "Try next host", 1);

      construct_dialog(&di, "No Updates", 22, 9, 35, 6, elements, 3, 1);
      result = run_dialog(mzx_world, &di);
      destruct_dialog(&di);

      if((result == 1) && (cur_host < conf->update_host_count))
        try_next_host = true;

      goto err_free_update_manifests;
    }

    // Switch back to the normal checking timeout for the rest of the process.
    if(is_automatic)
    {
      if(conf->update_auto_check == UPDATE_AUTO_CHECK_SILENT)
        goto err_free_update_manifests;

      host_set_timeout_ms(h, HOST_TIMEOUT_DEFAULT);
      is_automatic = 0;
    }

    for(e = removed; e; e = e->next, entries++)
      list_entry_width = MAX(list_entry_width, 2 + (int)strlen(e->name)+1+1);
    for(e = replaced; e; e = e->next, entries++)
      list_entry_width = MAX(list_entry_width, 2 + (int)strlen(e->name)+1+1);
    for(e = added; e; e = e->next, entries++)
      list_entry_width = MAX(list_entry_width, 2 + (int)strlen(e->name)+1+1);

    // We don't want the listbox to be too wide
    list_entry_width = MIN(list_entry_width, 60);

    list_entries = cmalloc(entries * sizeof(char *));

    for(e = removed; e; e = e->next, i++)
    {
      list_entries[i] = cmalloc(list_entry_width);
      snprintf(list_entries[i], list_entry_width, "- %s", e->name);
      list_entries[i][list_entry_width - 1] = 0;
    }

    for(e = replaced; e; e = e->next, i++)
    {
      list_entries[i] = cmalloc(list_entry_width);
      snprintf(list_entries[i], list_entry_width, "* %s", e->name);
      list_entries[i][list_entry_width - 1] = 0;
    }

    for(e = added; e; e = e->next, i++)
    {
      list_entries[i] = cmalloc(list_entry_width);
      snprintf(list_entries[i], list_entry_width, "+ %s", e->name);
      list_entries[i][list_entry_width - 1] = 0;
    }

    draw_window_box(19, 1, 59, 4, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
    write_string(" Task Summary ", 33, 1, DI_TITLE, 0);
    write_string("ESC   - Cancel   [+] Add   [-] Delete", 21, 2, DI_TEXT, 0);
    write_string("ENTER - Proceed  [*] Replace  ", 21, 3, DI_TEXT, 0);

    result = list_menu((const char **)list_entries, list_entry_width,
     NULL, 0, entries, ((80 - (list_entry_width + 9)) >> 1) + 1, 4);

    for(i = 0; i < entries; i++)
      free(list_entries[i]);
    free(list_entries);

    clear_screen(32, 7);
    update_screen();

    if(result < 0)
      goto err_free_update_manifests;

    /* Defer deletions until we restart; any of these files may still be
     * in use by this (old) process. Reduce the number of entries by the
     * number of removed items for the progress meter below.
     */
    for(e = removed; e; e = e->next, entries--)
      delete_hook(e->name);

    /* Since the operations for adding and replacing a file are identical,
     * we modify the replaced list and tack on the added list to the end.
     *
     * Either list may be NULL; in the case that `replaced' is NULL, simply
     * re-assign the `added' pointer. `added' being NULL has no effect.
     *
     * Later, we need only free the replaced list (see below).
     */
    if(replaced)
    {
      for(e = replaced; e->next; e = e->next)
        ;
      e->next = added;
    }
    else
      replaced = added;

    cancel_update = false;
    host_set_callbacks(h, NULL, recv_cb, cancel_cb);

    i = 1;
    for(e = replaced; e; e = e->next, i++)
    {
      for(retries = 0; retries < MAX_RETRIES; retries++)
      {
        char name[72];
        bool m_ret;

        if(!check_create_basedir(e->name))
          goto err_free_delete_list;

        final_size = (long)e->size;

        m_hide();
        snprintf(name, 72, "%s (%ldb) [%u/%u]", e->name, final_size, i, entries);
        meter(name, 0, final_size);
        update_screen();

        m_ret = manifest_entry_download_replace(h, url_base, e, delete_hook);

        clear_screen(32, 7);
        m_show();
        update_screen();

        if(m_ret)
          break;

        if(cancel_update)
        {
          error("Download was cancelled; update aborted.", 1, 8, 0);
          goto err_free_delete_list;
        }

        if(!reissue_connection(conf, &h, update_host, 0))
          goto err_free_delete_list;
        host_set_callbacks(h, NULL, recv_cb, cancel_cb);
      }

      if(retries == MAX_RETRIES)
      {
        snprintf(widget_buf, WIDGET_BUF_LEN,
         "Failed to download \"%s\" (after %d attempts).", e->name, retries);
        widget_buf[WIDGET_BUF_LEN - 1] = 0;
        error(widget_buf, 1, 8, 0);
        goto err_free_delete_list;
      }
    }

    if(!write_delete_list())
      goto err_free_delete_list;

    try_next_host = false;
    ret = true;
err_free_delete_list:
    manifest_list_free(&delete_list);
    delete_list = delete_p = NULL;
err_free_update_manifests:
    manifest_list_free(&removed);
    manifest_list_free(&replaced);
err_roll_back_manifest:
    restore_original_manifest(ret);
err_free_url_base:
    free(url_base);
err_host_destroy:
    host_destroy(h);

    pop_context();
  } //end host for loop

err_chdir:
  swivel_current_dir_back(true);
err_out:

  /* At this point we found updates and we successfully updated
   * to them. Reload the program with the original argv.
   */
  if(ret)
  {
    char **new_argv;
    struct element *elements[2];
    struct dialog di;

    elements[0] = construct_label(2, 2,
     "This client will now attempt to restart itself.");
    elements[1] = construct_button(23, 4, "OK", 0);

    construct_dialog(&di, "Update Successful", 14, 9, 51, 6, elements, 2, 1);
    run_dialog(mzx_world, &di);
    destruct_dialog(&di);

    new_argv = rewrite_argv_for_execv(process_argc, process_argv);
    execv(process_argv[0], (const void *)new_argv);
    perror("execv");

    error("Attempt to invoke self failed!", 1, 8, 0);
    return;
  }
}

bool updater_init(int argc, char *argv[])
{
  FILE *f;

  process_argc = argc;
  process_argv = argv;

  if(!swivel_current_dir(false))
    return false;

  check_for_updates = __check_for_updates;

  f = fopen_unsafe(DELETE_TXT, "rb");
  if(!f)
    goto err_swivel_back;

  delete_list = manifest_list_create(f);
  fclose(f);

  apply_delete_list();
  unlink(DELETE_TXT);

  if(delete_list)
  {
    write_delete_list();
    manifest_list_free(&delete_list);
    error("Failed to delete files; check permissions and restart MegaZeux",
     1, 8, 0);
  }

err_swivel_back:
  swivel_current_dir_back(false);
  return true;
}

bool is_updater(void)
{
  return true;
}
