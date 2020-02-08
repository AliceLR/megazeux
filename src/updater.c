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

#include "caption.h"
#include "const.h"
#include "core.h"
#include "error.h"
#include "event.h"
#include "graphics.h"
#include "helpsys.h"
#include "updater.h"
#include "util.h"
#include "window.h"

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

#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

static char executable_dir[MAX_PATH];
static char previous_dir[MAX_PATH];

static long final_size = -1;
static boolean cancel_update;

static boolean updater_was_initialized;

/**
 * A new version has been released and there are updates available on the
 * current host for this platform. Prompt the user to either update to the
 * new version or attempt to update the current version. Returns the version
 * selected by the user, or NULL if canceled by the user.
 */
static const char *ui_new_version_available(context *ctx,
 const char *current_ver, const char *new_ver)
{
  struct world *mzx_world = ctx->world;
  struct element *elements[6];
  struct dialog di;
  size_t buf_len;
  int result;

  buf_len = snprintf(widget_buf, WIDGET_BUF_LEN,
   "A new version is available (%s)", new_ver);
  widget_buf[WIDGET_BUF_LEN - 1] = 0;

  elements[0] = construct_label((55 - buf_len) >> 1, 2, widget_buf);

  elements[1] = construct_label(2, 4,
   "You can continue to receive updates for the version\n"
   "installed (if available), or you can upgrade to the\n"
   "newest version (recommended).");

  elements[2] = construct_label(2, 8,
   "If you do not upgrade, this question will be asked\n"
   "again the next time you run the updater.\n");

  elements[3] = construct_button(9, 11, "Upgrade", 0);
  elements[4] = construct_button(21, 11, "Update Old", 1);
  elements[5] = construct_button(36, 11, "Cancel", 2);

  construct_dialog(&di, "New Version", 11, 6, 55, 14, elements, 6, 3);
  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // User pressed Escape, abort all updates
  if(result < 0 || result == 2)
    return NULL;

  // User pressed Upgrade, use new version.
  if(result == 0)
    return new_ver;

  // Check for updates on the current version.
  return current_ver;
}

/**
 * No changes have been detected between the local manifest and the remote
 * manifest. Prompt the user to either try the next host or to abort. Return
 * true if the user selected to try the next host, otherwise false.
 */
static boolean ui_version_is_current(context *ctx, boolean has_next_host)
{
  struct world *mzx_world = ctx->world;
  struct element *elements[3];
  struct dialog di;
  int result;

  elements[0] = construct_label(2, 2, "This client is already current.");
  elements[1] = construct_button(7, 4, "OK", 0);
  elements[2] = construct_button(13, 4, "Try next host", 1);

  construct_dialog(&di, "No Updates", 22, 9, 35, 6, elements, 3, 1);
  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if((result == 1) && has_next_host)
    return true;

  return false;
}

/**
 * Show the user the list of changes to be applied. Return the number of
 * changes to be applied (or 0 if canceled).
 */
static int ui_confirm_changes(context *ctx, struct manifest_entry *removed,
 struct manifest_entry *replaced, struct manifest_entry *added)
{
  struct manifest_entry *e;
  char **list_entries;
  int list_entry_width = 0;
  int entries = 0;
  int result;
  int i = 0;

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

  clear_screen();
  update_screen();

  if(result >= 0)
    return entries;

  return 0;
}

/**
 * Inform the user that the update process is complete.
 */
static void ui_update_finished(context *ctx)
{
  struct world *mzx_world = ctx->world;
  struct element *elements[2];
  struct dialog di;

  elements[0] = construct_label(2, 2,
   "This client will now attempt to restart itself.");
  elements[1] = construct_button(23, 4, "OK", 0);

  construct_dialog(&di, "Update Successful", 14, 9, 51, 6, elements, 2, 1);
  run_dialog(mzx_world, &di);
  destruct_dialog(&di);
}

/**
 * Clear the screen.
 */
static void display_clear(void)
{
  clear_screen();
  update_screen();
}

/**
 * Indicate that the client is currently connecting to a remote host.
 */
static void display_connecting(const char *host_name)
{
  size_t buf_len;

  buf_len = snprintf(widget_buf, WIDGET_BUF_LEN,
   "Connecting to \"%s\". Please wait..", host_name);
  widget_buf[WIDGET_BUF_LEN - 1] = 0;

  m_hide();
  draw_window_box(3, 11, 76, 13, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  write_string(widget_buf, (WIDGET_BUF_LEN - buf_len) >> 1, 12, DI_TEXT, 0);
  update_screen();
  m_show();
}

/**
 * Indicate that the manifest is currently being processed.
 */
static void display_computing_manifest(void)
{
  m_hide();
  draw_window_box(3, 11, 76, 13, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  write_string("Computing manifest deltas (added, replaced, deleted)..",
   13, 12, DI_TEXT, 0);
  update_screen();
  m_show();
}

/**
 * Indicate the current download.
 */
static void display_download_init(const char *filename, int cur, int total)
{
  char name[72];

  m_hide();
  snprintf(name, 72, "%s (%ldb) [%u/%u]", filename, final_size, cur, total);
  meter(name, 0, final_size);
  update_screen();
  m_show();
}

/**
 * Update the download progress bar.
 */
static void display_download_update(long progress)
{
  m_hide();
  meter_interior(progress, final_size);
  update_screen();
  m_show();
}

static boolean check_prune_basedir(const char *file)
{
  static char path[MAX_PATH];
  ssize_t ret;

  ret = get_path(file, path, MAX_PATH);
  if(ret < 0)
  {
    error_message(E_UPDATE, 0, "Failed to prune directories (path too long)");
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
static boolean check_create_basedir(const char *file)
{
  static struct stat s;
  char path[MAX_PATH];
  ssize_t ret;

  ret = get_path(file, path, MAX_PATH);
  if(ret < 0)
  {
    error_message(E_UPDATE, 1, "Failed to create directories (path too long)");
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
      error_message(E_UPDATE, 2, "Unknown stat() error occurred");
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
    error_message(E_UPDATE, 3, widget_buf);
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
    error_message(E_UPDATE, 4,
     "Transferred more than expected uncompressed size.");
    cancel_update = true;
    return;
  }

  display_download_update(offset);
}

static boolean cancel_cb(void)
{
  return cancel_update;
}

static void delete_hook(const char *file)
{
  struct manifest_entry *new_entry;
  struct SHA256_ctx ctx;
  boolean ret;
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

// Determine the executable dir. This is required for the updater.
static boolean find_executable_dir(int argc, char **argv)
{
#ifdef __WIN32__
  {
    // Windows may not reliably give a full path in argv[0]. Fortunately,
    // there's a Windows solution for this. TODO make UTF-friendly
    char filename[MAX_PATH];
    HMODULE module = GetModuleHandle(NULL);
    DWORD ret = GetModuleFileNameA(module, filename, MAX_PATH);

    if(ret > 0 && ret < MAX_PATH)
      if(get_path(filename, executable_dir, MAX_PATH) > 0)
        return true;

    warn("--MAIN-- Failed to get executable from Win32: %s\n", filename);
  }
#endif

  if(argc >= 1 && argv)
  {
    if(get_path(argv[0], executable_dir, MAX_PATH) > 0)
      return true;

    else
    {
      if(argv[0] && argv[0][0])
        warn("--MAIN-- Failed to get executable from argv[0]: %s\n", argv[0]);
      else
        warn("--MAIN-- Failed to get executable from argv[0]: (null)\n");
    }
  }
  else
    warn("--MAIN-- Failed to get executable from argv: argc < 1\n");

  // Nope. Oh well.
  executable_dir[0] = 0;
  return false;
}

static boolean swivel_current_dir(boolean have_video)
{
  char base_path[MAX_PATH];

  if(!executable_dir[0])
  {
    if(have_video)
      error_message(E_UPDATE, 25,
       "Updater: couldn't determine install directory.");
    else
      warn("--UPDATER-- Couldn't determine install directory.\n");
    return false;
  }

  // Store the user's current directory, so we can get back to it
  getcwd(previous_dir, MAX_PATH);

  if(chdir(executable_dir))
  {
    info("--UPDATER-- getcwd(): %s\n", previous_dir);
    info("--UPDATER-- attempted chdir() to: %s\n", base_path);
    if(have_video)
      error_message(E_UPDATE, 5,
       "Updater: failed to change into install directory.");
    else
      warn("--UPDATER-- Failed to change into install directory.\n");
    return false;
  }
  return true;
}

static boolean swivel_current_dir_back(boolean have_video)
{
  if(chdir(previous_dir))
  {
    if(have_video)
      error_message(E_UPDATE, 6,
       "Updater: failed to change back to user directory.");
    else
      warn("--UPDATER-- Failed to change back to user directory.\n");
    return false;
  }

  return true;
}

static boolean backup_original_manifest(void)
{
  unsigned int len, pos = 0;
  char block[BLOCK_SIZE];
  FILE *input, *output;
  boolean ret = false;
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

static boolean restore_original_manifest(boolean ret)
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
    error_message(E_UPDATE, 7,
     "Failed to remove " MANIFEST_TXT ". Check permissions.");
    return false;
  }

  // No manifest to restore; consider this successful
  if(stat(MANIFEST_TXT "~", &s))
    return true;

  // Try to restore backup manifest
  if(rename(MANIFEST_TXT "~", MANIFEST_TXT))
  {
    error_message(E_UPDATE, 8,
     "Failed to roll back manifest. Check permissions.");
    return false;
  }

  return true;
}

static boolean write_delete_list(void)
{
  struct manifest_entry *e;
  FILE *f;

  if(delete_list)
  {
    f = fopen_unsafe(DELETE_TXT, "ab");
    if(!f)
    {
      error_message(E_UPDATE, 9,
       "Failed to create \"" DELETE_TXT "\". Check permissions.");
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
  struct manifest_entry *e_prev = NULL;
  struct manifest_entry *e;
  int retry_times = 0;
  struct stat s;
  boolean files_failed = false;
  boolean is_valid;
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

      is_valid = manifest_entry_check_validity(e, f);
      fclose(f);

      if(is_valid)
      {
        if(unlink(e->name))
          goto err_delete_failed;

        /* Obtain the path for this file. If the file isn't at the top
         * level, and the directory is empty (rmdir ensures this)
         * the directory will be pruned.
         */
        check_prune_basedir(e->name);
        info("--UPDATER-- Deleted '%s'\n", e->name);
      }
      else
        info("--UPDATER-- Skipping invalid entry '%s'\n", e->name);
    }

    // File was removed, doesn't exist, or is non-applicable; remove from list
    if(delete_list == e)
      delete_list = e_next;

    // Keep the link on the last failed file up-to-date.
    if(e_prev)
      e_prev->next = e_next;

    manifest_entry_free(e);
    continue;

err_delete_failed:
    {
      int errval;

      warn("--UPDATER-- Failed to delete '%s'\n", e->name);

      switch(retry_times)
      {
        case 0:
        {
          if(!strcmp(e->name, "mzx_help.fil") ||
           !strcmp(e->name, "assets/help.fil"))
          {
            // HACK: Older MZX versions do not properly close these files
            // because that would have been too easy. Silently skip them for
            // now; they'll be deleted the next time MZX is started.
            errval = ERROR_OPT_FAIL;
            break;
          }

          // 1st failure: delay a little bit and then retry automatically.
          delay(200);
          errval = ERROR_OPT_RETRY;
          break;
        }

        case 1:
        {
          // 2nd failure: give user the option to either retry or fail.
          char buf[72];
          snprintf(buf, 72, "Failed to delete \"%.30s\". Retry?", e->name);
          buf[71] = 0;

          errval = error_message(E_UPDATE_RETRY, 10, buf);
          break;
        }

        default:
        {
          // 3rd failure: auto fail so we're not here all day. Also, display
          // an error message when this loop is finished.
          errval = ERROR_OPT_FAIL;
          files_failed = true;
          break;
        }
      }

      if(errval == ERROR_OPT_RETRY)
      {
        info("--UPDATER-- Retrying '%s'...\n", e->name);
        retry_times++;

        // Set the next file to this file to try again...
        e_next = e;
      }
      else
      {
        info("--UPDATER-- Skipping '%s'...\n", e->name);
        retry_times = 0;

        // Track this file so its link can be updated.
        e_prev = e;
      }
      continue;
    }
  }

  if(files_failed)
    error_message(E_UPDATE, 24,
     "Failed to delete files; check permissions and restart MegaZeux");
}

static boolean reissue_connection(struct config_info *conf, struct host **h,
 char *host_name, int is_automatic)
{
  boolean ret = false;

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
      error_message(E_UPDATE, 11, "Failed to create TCP client socket.");
    goto err_out;
  }

  if(is_automatic)
    host_set_timeout_ms(*h, 1000);

  display_connecting(host_name);

  if(!host_connect(*h, host_name, OUTBOUND_PORT))
  {
    if(!is_automatic)
    {
      snprintf(widget_buf, WIDGET_BUF_LEN,
       "Connection to \"%s\" failed.", host_name);
      widget_buf[WIDGET_BUF_LEN - 1] = 0;
      error_message(E_UPDATE, 12, widget_buf);
    }
  }
  else
    ret = true;

  display_clear();

err_out:
  return ret;
}

/**
 * Run a synchronous update check.
 * @param ctx           Current context
 * @param is_automatic  Disable more annoying UI displays for automated checks.
 * @return              true if the update completed, otherwise false.
 */
static boolean __check_for_updates(context *ctx, boolean is_automatic)
{
  struct config_info *conf = get_config();
  int cur_host;
  char *update_host;
  boolean try_next_host = true;
  boolean ret = false;

  set_context(CTX_UPDATER);
  set_error_suppression(E_UPDATE, false);

  if(!updater_was_initialized)
  {
    error_message(E_UPDATE, 13,
     "Updater couldn't be initialized; check folder permissions");
    goto err_out;
  }

  if(conf->update_host_count < 1)
  {
    error_message(E_UPDATE, 14,
     "No updater hosts defined! Aborting.");
    goto err_out;
  }

  if(!swivel_current_dir(true))
    goto err_out;

  for(cur_host = 0; (cur_host < conf->update_host_count) && try_next_host; cur_host++)
  {
    char buffer[LINE_BUF_LEN], *url_base, *value;
    struct manifest_entry *removed, *replaced, *added, *e;
    int i = 0, entries = 0;
    char update_branch[LINE_BUF_LEN];
    const char *version = VERSION;
    enum host_status status;
    struct host *h = NULL;
    struct http_info req;
    unsigned int retries;
    FILE *f;

    // Acid test: Can we write to this directory?
    f = fopen_unsafe(UPDATES_TXT, "w+b");
    if(!f)
    {
      error_message(E_UPDATE, 15,
       "Failed to create \"" UPDATES_TXT "\". Check permissions.");
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
        error_message(E_UPDATE, 16, widget_buf);
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
        error_message(E_UPDATE, 17,
         "Failed to identify applicable update version.");
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
      // Notify the user that updates are available.
      caption_set_updates_available(true);

      // If this is an auto check and silent mode is enabled, we can stop here.
      if(is_automatic && conf->update_auto_check == UPDATE_AUTO_CHECK_SILENT)
      {
        try_next_host = false;
        goto err_host_destroy;
      }

      version = ui_new_version_available(ctx, version, value);

      // Abort if no version was selected.
      if(version == NULL)
      {
        try_next_host = false;
        goto err_host_destroy;
      }
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
      error_message(E_UPDATE, 18,
       "Failed to back up manifest. Check permissions.");
      try_next_host = false;
      goto err_free_url_base;
    }

    for(retries = 0; retries < MAX_RETRIES; retries++)
    {
      display_computing_manifest();

      status = manifest_get_updates(h, url_base, &removed, &replaced, &added);

      display_clear();

      if(status == HOST_SUCCESS)
        break;

      // Unsupported platform.
      if(-status == HOST_HTTP_REDIRECT || -status == HOST_HTTP_CLIENT_ERROR)
      {
        error_message(E_UPDATE, 19, "No updates available for this platform.");
        goto err_roll_back_manifest;
      }

      if(!reissue_connection(conf, &h, update_host, 0))
        goto err_roll_back_manifest;
    }

    if(retries == MAX_RETRIES)
    {
      error_message(E_UPDATE, 20, "Failed to compute update manifests");
      goto err_roll_back_manifest;
    }

    // At this point, we have a successful manifest, so we won't need another host
    try_next_host = false;

    if(!removed && !replaced && !added)
    {
      boolean has_next_host = (cur_host < conf->update_host_count);

      if(is_automatic)
        goto err_free_update_manifests;

      // The user may want to attempt an update from the next host.
      try_next_host = ui_version_is_current(ctx, has_next_host);
      goto err_free_update_manifests;
    }

    // Set the updates available notification if it hasn't been set already.
    caption_set_updates_available(true);

    // Switch back to the normal checking timeout for the rest of the process.
    if(is_automatic)
    {
      if(conf->update_auto_check == UPDATE_AUTO_CHECK_SILENT)
        goto err_free_update_manifests;

      host_set_timeout_ms(h, HOST_TIMEOUT_DEFAULT);
      is_automatic = 0;
    }

    /* Show the user the list of changes to perform and prompt the user to
     * confirm or cancel.
     */
    entries = ui_confirm_changes(ctx, removed, replaced, added);

    if(entries <= 0)
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
        boolean m_ret;

        if(!check_create_basedir(e->name))
          goto err_free_delete_list;

        final_size = (long)e->size;
        display_download_init(e->name, i, entries);

        m_ret = manifest_entry_download_replace(h, url_base, e, delete_hook);

        display_clear();

        if(m_ret)
          break;

        if(cancel_update)
        {
          error_message(E_UPDATE, 21, "Download was cancelled; update aborted.");
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
        error_message(E_UPDATE, 22, widget_buf);
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

  if(ret)
  {
    // Inform the user the update was successful.
    ui_update_finished(ctx);

    // Signal core to exit and restart MZX.
    core_full_restart(ctx);
    return true;
  }
  return false;
}

boolean updater_init(int argc, char *argv[])
{
  FILE *f;

  check_for_updates = __check_for_updates;
  updater_was_initialized = false;

  if(!find_executable_dir(argc, argv))
    return false;

  if(!swivel_current_dir(false))
    return false;

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
  }

err_swivel_back:
  swivel_current_dir_back(false);
  updater_was_initialized = true;
  return true;
}

boolean is_updater(void)
{
  return true;
}
