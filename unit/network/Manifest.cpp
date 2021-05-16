/* MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <memory>
#include <stdint.h>

#include "../Unit.hpp"

#include "../../src/util.h"
#include "../../src/network/Manifest.hpp"

static const char *DATA_DIR = "../data";
static const char *TEMP_FILE = "DELETE_ME.txt";

class pushd
{
private:
  boolean success;
  char prev[MAX_PATH];

  pushd(const pushd &) {}

public:
  pushd(const char *new_dir)
  {
    trace("--UNIT-- attempting to chdir to %s.\n", new_dir);
    success = false;
    if(getcwd(prev, MAX_PATH))
      if(!chdir(new_dir))
        success = true;
  }

  ~pushd()
  {
    if(success)
    {
      trace("--UNIT-- attempting to chdir back to %s.\n", prev);
      chdir(prev);
    }
  }

  explicit operator bool()
  {
    return success;
  }
};

struct manifestdata
{
  const char *filename;
  const char *value;
  size_t size;
  uint32_t sha256[8];
};

// The lines here should match filedata's contents.
// Intentionally using mixed line ends since either should be supported.
static const char manifest_str[] =
 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad 3 1.txt\n"
 "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1 56 2.txt\r\n";

static const manifestdata filedata[]
{
  {
    "1.txt",
    "abc",
    3,
    {
      0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223,
      0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad
    }
  },
  {
    "2.txt",
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
    56,
    {
      0x248d6a61, 0xd20638b8, 0xe5c02693, 0x0c3e6039,
      0xa33ce459, 0x64ff2167, 0xf6ecedd4, 0x19db06c1
    }
  }
};

static const manifestdata dummy_file =
{
  "ascii.chr",
  nullptr,
  3584,
  {
    0x657ca658, 0x8b6bf729, 0xf0ed71a3, 0xd3c781a4,
    0x65bd30cd, 0xfb11eb5f, 0xc7e27594, 0xc0717ea1
  }
};

UNITTEST(ManifestEntry)
{
  SECTION(ManifestEntry)
  {
    for(int i = 0; i < arraysize(filedata); i++)
    {
      const manifestdata &f = filedata[i];

      ManifestEntry e(f.sha256, f.size, f.filename);
      ASSERTXMEM(e.sha256, f.sha256, sizeof(f.sha256), f.filename);
      ASSERTEQX(e.size, f.size, f.filename);
      ASSERTCMP(e.name, f.filename);

      ManifestEntry e2(e);
      ASSERTXMEM(e2.sha256, f.sha256, sizeof(f.sha256), f.filename);
      ASSERTEQX(e2.size, f.size, f.filename);
      ASSERTCMP(e2.name, f.filename);
    }
  }

  SECTION(operator=)
  {
    for(int i = 0; i < arraysize(filedata); i++)
    {
      const manifestdata &f = filedata[i];

      ManifestEntry e(dummy_file.sha256, dummy_file.size, dummy_file.filename);
      ManifestEntry e2(f.sha256, f.size, f.filename);
      e = e2;

      ASSERTXMEM(e.sha256, f.sha256, sizeof(f.sha256), f.filename);
      ASSERTEQX(e.size, f.size, f.filename);
      ASSERTCMP(e.name, f.filename);
    }
  }

  SECTION(validate)
  {
    pushd ch(DATA_DIR);
    ASSERT(ch);

    for(int i = 0; i < arraysize(filedata); i++)
    {
      const manifestdata &f = filedata[i];

      ManifestEntry e(f.sha256, f.size, f.filename);
      boolean valid = e.validate();
      ASSERTX(valid, f.filename);
    }
  }

  SECTION(validate_filename)
  {
    static const char *valid[] =
    {
      "data/1.txt",
      "sdfjsdlkfjdskljfds/sdfjsdlkfjsdlkjfsdkl/sfdjfklsjdflksd",
      "aaaaaaaaaaa"
    };
    static const char *invalid[] =
    {
      "..\\lol",
      "../lol",
      "totally/normal/path/../../../../../../../../../bootmgr",
      "C:\\Windows\\System32",
      "C:/Windows/System32",
      "/dev",
      "\\dev",
    };

    for(int i = 0; i < arraysize(valid); i++)
      ASSERTX(ManifestEntry::validate_filename(valid[i]), valid[i]);

    for(int i = 0; i < arraysize(invalid); i++)
      ASSERTX(!ManifestEntry::validate_filename(invalid[i]), invalid[i]);
  }

  SECTION(create_from_file)
  {
    pushd ch(DATA_DIR);
    ASSERT(ch);

    for(int i = 0; i < arraysize(filedata); i++)
    {
      const manifestdata &f = filedata[i];

      std::unique_ptr<ManifestEntry> e(ManifestEntry::create_from_file(f.filename));
      ASSERTX(e, f.filename);

      ASSERTXMEM(e->sha256, f.sha256, sizeof(f.sha256), f.filename);
      ASSERTEQX(e->size, f.size, f.filename);
      ASSERTCMP(e->name, f.filename);
    }
  }
}

void test_manifest(const Manifest &m, const char *comment)
{
  char buffer[256];
  const ManifestEntry *e = m.first();
  for(int i = 0; i < arraysize(filedata); i++)
  {
    const manifestdata &f = filedata[i];
    snprintf(buffer, arraysize(buffer), "%s, %s", f.filename, comment);

    ASSERTX(e, buffer);
    ASSERTXMEM(e->sha256, f.sha256, sizeof(f.sha256), buffer);
    ASSERTEQX(e->size, f.size, buffer);
    ASSERTXCMP(e->name, f.filename, buffer);
    ASSERTX(e->validate(), buffer);
    e = e->next;
  }
  ASSERT(!e);
}

UNITTEST(Manifest)
{
  SECTION(create)
  {
    pushd ch(DATA_DIR);
    ASSERT(ch);

    Manifest m;
    m.create("manifest.txt");
    test_manifest(m, "create(filename)");
    m.clear();
    ASSERT(!m.head);

    m.create(manifest_str, arraysize(manifest_str));
    test_manifest(m, "create(buffer, size)");

    Manifest m2;
    m2.create(m);
    test_manifest(m2, "create(const Manifest &)");
  }

  SECTION(append)
  {
    Manifest m1, m2;
    m1.create(manifest_str, arraysize(manifest_str));
    m2.create(manifest_str, arraysize(manifest_str));

    // append(Manifest &) consumes the source Manifest's data.
    m1.append(m2);
    ASSERT(!m2.head);

    ManifestEntry *tmp =
     new ManifestEntry(dummy_file.sha256, dummy_file.size, dummy_file.filename);
    m1.append(tmp);

    const ManifestEntry *e = m1.first();
    for(int j = 0; j < 2; j++)
    {
      for(int i = 0; i < arraysize(filedata); i++)
      {
        const manifestdata &f = filedata[i];
        ASSERTX(e, f.filename);
        ASSERTCMP(e->name, f.filename);
        e = e->next;
      }
    }

    ASSERTX(e, dummy_file.filename);
    ASSERTCMP(e->name, dummy_file.filename);
    e = e->next;
    ASSERT(!e);
  }

  SECTION(write_to_file)
  {
    pushd ch(DATA_DIR);
    ASSERT(ch);

    Manifest m;
    m.create(manifest_str, arraysize(manifest_str));
    m.write_to_file(TEMP_FILE);
    m.clear();
    ASSERT(!m.head);

    m.create(TEMP_FILE);
    test_manifest(m, "create(filename)");

    unlink(TEMP_FILE);
  }

  // NOTE: check_if_remote_exists requires networking.
  // NOTE: get_updates requires networking.
  // NOTE: download_and_replace_entry requires networking.
}
