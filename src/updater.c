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

#include <errno.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifndef PLATFORM
#error Must define a valid "friendly" platform name!
#endif

#define OUTBOUND_PORT 80
#define LINE_BUF_LEN  256
#define UPDATES_TXT   "updates.txt"

#define UPDATE_HOST   "updates.digitalmzx.net"

#ifdef DEBUG
#define UPDATE_BRANCH "Current-Unstable"
#else
#define UPDATE_BRANCH "Current-Stable"
#endif

static char **process_argv;

static bool check_prune_basedir(const char *file)
{
  char path[MAX_PATH];
  int ret;

  ret = get_path(file, path, MAX_PATH);
  if(ret < 0)
  {
    warn("Path too long\n");
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

static bool check_create_basedir(const char *file)
{
  char path[MAX_PATH];
  struct stat s;
  int ret;

  ret = get_path(file, path, MAX_PATH);
  if(ret < 0)
  {
    warn("Path too long\n");
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
      perror("stat");
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
    warn("Path '%s' is getting in the way (must be a directory)\n", path);
    return false;
  }

  return true;
}

static long final_size = -1;
static bool cancel_update;

static void check_cancel_update(void)
{
  update_event_status();
  if(get_key(keycode_internal) == IKEY_ESCAPE)
    cancel_update = true;
}

static void recv_cb(long offset)
{
  check_cancel_update();

  if(final_size > 0 && offset > final_size)
  {
    warn("Transferred more than expected uncompressed size.\n");
    warn("It is probable that corruption occurred.\n");
    cancel_update = true;
    return;
  }

  meter_interior(offset, final_size);
  update_screen();
}

static bool cancel_cb(void)
{
  if(cancel_update)
  {
    cancel_update = false;
    return true;
  }

  return false;
}

#define WIDGET_BUF_LEN 80

static void __check_for_updates(void)
{
  char *base_path, buffer[LINE_BUF_LEN], *url_base, *value;
  struct manifest_entry *removed, *replaced, *added, *e;
  char **list_entries, widget_buf[WIDGET_BUF_LEN];
  int i = 0, entries = 0, buf_len;
  const char *version = "2.82";
  size_t list_entry_width = 0;
  host_status_t status;
  bool ret = false;
  struct host *h;
  FILE *f;

  m_hide();

  // Store the user's current directory, so we can get back to it
  getcwd(current_dir, MAX_PATH);

  // Find and change into the base path for this MZX binary
  base_path = malloc(MAX_PATH);
  get_path(process_argv[0], base_path, MAX_PATH);
  if(chdir(base_path))
  {
    error("Failed to change into install directory.", 1, 8, 0);
    goto err_free_base_path;
  }

  // Acid test: Can we write to this directory?
  f = fopen(UPDATES_TXT, "w+b");
  if(!f)
  {
    error("Failed to create \"" UPDATES_TXT "\". Check permissions.",
     1, 8, 0);
    goto err_chdir;
  }

  if(!host_layer_init())
  {
    error("Failed to initialize network layer.", 1, 8, 0);
    goto err_chdir;
  }

  h = host_create(HOST_TYPE_TCP, HOST_FAMILY_IPV4);
  if(!h)
  {
    error("Failed to create TCP client socket.", 1, 8, 0);
    goto err_layer_exit;
  }

  buf_len = snprintf(widget_buf, WIDGET_BUF_LEN, "Connecting to \""
   UPDATE_HOST "\" to receive version list..");
  widget_buf[WIDGET_BUF_LEN - 1] = 0;

  draw_window_box(3, 11, 76, 13, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  write_string(widget_buf, (WIDGET_BUF_LEN - buf_len) >> 1, 12, DI_TEXT, 0);
  update_screen();

  if(!host_connect(h, UPDATE_HOST, OUTBOUND_PORT))
  {
    error("Connection to \"" UPDATE_HOST "\" failed.", 1, 8, 0);
    goto err_clear_screen;
  }

  clear_screen(32, 7);
  update_screen();

  // Grab the file containing the names of the current Stable and Unstable
  status = host_recv_file(h, "/" UPDATES_TXT, f, "text/plain");
  if(status != HOST_SUCCESS)
  {
    snprintf(widget_buf, WIDGET_BUF_LEN, "Failed to download \""
     UPDATES_TXT "\" (err=%d).\n", status);
    widget_buf[WIDGET_BUF_LEN - 1] = 0;
    error(widget_buf, 1, 8, 0);
    goto err_host_destroy;
  }

  // Walk this list (of two, hopefully)
  rewind(f);
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

    if(strcmp(key, UPDATE_BRANCH) == 0)
      break;
  }

  fclose(f);
  unlink(UPDATES_TXT);

  /* There was no "Current-XXX: Version" found; we cannot proceed with the
   * update because we cannot compute an update URL below.
   */
  if(!value)
  {
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
    element *elements[5];
    int result;
    dialog di;

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

    elements[3] = construct_button(14, 11, "Upgrade", 0);
    elements[4] = construct_button(29, 11, "Update Old", 1);

    construct_dialog(&di, "New Major Version", 11, 6, 55, 14, elements, 5, 3);
    result = run_dialog(NULL, &di);
    destruct_dialog(&di);

    // User pressed Escape, abort all updates
    if(result < 0)
      goto err_host_destroy;

    // User pressed Upgrade, use new major
    if(result == 0)
      version = value;
  }

  /* We can now compute a unique URL base for the updater. This will
   * be composed of a user-selected version and a static platform-archicture
   * name.
   */
  url_base = malloc(LINE_BUF_LEN);
  snprintf(url_base, LINE_BUF_LEN, "/%s/" PLATFORM, version);
  debug("Update base URL: %s\n", url_base);

  draw_window_box(3, 11, 76, 13, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  write_string("Computing manifest deltas (added, replaced, deleted)..",
   13, 12, DI_TEXT, 0);
  update_screen();

  if(!manifest_get_updates(h, url_base, &removed, &replaced, &added))
  {
    error("Failed to compute update manifests", 1, 8, 0);
    goto err_free_url_base;
  }

  clear_screen(32, 7);
  update_screen();

  if(!removed && !replaced && !added)
  {
    element *elements[2];
    int result;
    dialog di;

    elements[0] = construct_label(2, 2, "This client is already current.");
    elements[1] = construct_button(15, 4, "OK", 0);

    construct_dialog(&di, "No Updates", 22, 9, 35, 6, elements, 2, 1);
    result = run_dialog(NULL, &di);
    destruct_dialog(&di);

    goto err_free_update_manifests;
  }

  for(e = removed; e; e = e->next, entries++)
    list_entry_width = MAX(list_entry_width, 2 + strlen(e->name) + 1 + 1);
  for(e = replaced; e; e = e->next, entries++)
    list_entry_width = MAX(list_entry_width, 2 + strlen(e->name) + 1 + 1);
  for(e = added; e; e = e->next, entries++)
    list_entry_width = MAX(list_entry_width, 2 + strlen(e->name) + 1 + 1);

  // We don't want the listbox to be too wide
  list_entry_width = MIN(list_entry_width, 60);

  list_entries = malloc(entries * sizeof(char *));

  for(e = removed; e; e = e->next, i++)
  {
    list_entries[i] = malloc(list_entry_width);
    snprintf(list_entries[i], list_entry_width, "- %s", e->name);
    list_entries[i][list_entry_width - 1] = 0;
  }

  for(e = replaced; e; e = e->next, i++)
  {
    list_entries[i] = malloc(list_entry_width);
    snprintf(list_entries[i], list_entry_width, "* %s", e->name);
    list_entries[i][list_entry_width - 1] = 0;
  }

  for(e = added; e; e = e->next, i++)
  {
    list_entries[i] = malloc(list_entry_width);
    snprintf(list_entries[i], list_entry_width, "+ %s", e->name);
    list_entries[i][list_entry_width - 1] = 0;
  }

  list_menu((const char **)list_entries, list_entry_width,
   "Deltas", 0, entries, (80 - (list_entry_width + 9)) >> 1);

  for(i = 0; i < entries; i++)
    free(list_entries[i]);
  free(list_entries);

  host_set_callbacks(h, NULL, recv_cb, cancel_cb);

  for(e = removed; e; e = e->next)
  {
    if(unlink(e->name))
    {
      perror("unlink");
      goto err_free_update_manifests;
    }

    /* Obtain the path for this file. If the file isn't at the top
     * level, and the directory is empty (rmdir ensures this)
     * the directory will be pruned.
     */
    check_prune_basedir(e->name);
  }

  for(e = replaced; e; e = e->next)
  {
    char name[72];

    if(!check_create_basedir(e->name))
      goto err_free_update_manifests;

    final_size = (long)e->size;

    snprintf(name, 72, "%s (%ldb)", e->name, final_size);
    meter(name, 0, final_size);
    update_screen();

    if(!manifest_entry_download_replace(h, url_base, e))
      goto err_free_update_manifests;
  }

  for(e = added; e; e = e->next)
  {
    char name[72];

    if(!check_create_basedir(e->name))
      goto err_free_update_manifests;

    final_size = (long)e->size;

    snprintf(name, 72, "%s (%ldb)", e->name, final_size);
    meter(name, 0, final_size);
    update_screen();

    if(!manifest_entry_download_replace(h, url_base, e))
      goto err_free_update_manifests;
  }

  ret = true;
err_free_update_manifests:
  manifest_list_free(&removed);
  manifest_list_free(&replaced);
  manifest_list_free(&added);
err_free_url_base:
  free(url_base);
err_host_destroy:
  host_destroy(h);
err_clear_screen:
  clear_screen(32, 7);
  update_screen();
  m_show();
err_layer_exit:
  host_layer_exit();
err_chdir:
  chdir(current_dir);
err_free_base_path:
  free(base_path);

  /* At this point we found updates and we successfully updated
   * to them. Reload the program with the original argv.
   */
  if(ret)
  {
    const void *argv = process_argv;
    execv(process_argv[0], argv);
  }
}

void updater_init(char *argv[])
{
  check_for_updates = __check_for_updates;
  process_argv = argv;
}
