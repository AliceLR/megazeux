/* MegaZeux
 *
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "Unit.hpp"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <csignal>
#include <type_traits>
#include <vector>

/* Utility functions that inlined MegaZeux source expects to exist. */

void *check_calloc(size_t nmemb, size_t size, const char *file,
 int line)
{
  void *p = calloc(nmemb, size);
  assert(p);
  return p;
}

void *check_malloc(size_t size, const char *file,
 int line)
{
  void *p = malloc(size);
  assert(p);
  return p;
}

void *check_realloc(void *ptr, size_t size, const char *file,
 int line)
{
  ptr = realloc(ptr, size);
  assert(ptr);
  return ptr;
}

/* Main functions. */

static void sigabrt_handler(int signal)
{
  if(signal == SIGABRT)
  {
    Uerr("Received SIGABRT: ");
  }
  else
    Uerr("Unexpected signal %d received: ", signal);

  Unit::unittestrunner.signal_fail();
}

int main(int argc, char *argv[])
{
  if(argc && argv && argv[0])
  {
    char *fpos = strrchr(argv[0], '/');
    char *bpos = strrchr(argv[0], '\\');
    char tmp;

    if(fpos || bpos)
    {
      fpos = (fpos > bpos) ? fpos : bpos;
      tmp = *fpos;
      *fpos = '\0';
      chdir(argv[0]);
      *fpos = tmp;
    }
  }
  std::signal(SIGABRT, sigabrt_handler);
  return !(Unit::unittestrunner.run());
}


namespace Unit
{
  /************************************************************************
   * Unit::exception functions.
   */

  exception::exception(const exception &e)
  {
    memcpy(this, &e, sizeof(Unit::exception));
    for(size_t i = 0; i < sizeof(allocbuf); i++)
    {
      char *buf = allocbuf[i];
      if(buf)
      {
        size_t len = strlen(buf) + 1;
        allocbuf[i] = new char[len];
        memcpy(allocbuf[i], buf, len);
        if(left == buf)
          left = allocbuf[i];
        if(right == buf)
          right = allocbuf[i];
      }
    }
  }

  void exception::set_reason_fmt(const char *_reason_fmt, va_list vl)
  {
    vsnprintf(reasonbuf, sizeof(reasonbuf), _reason_fmt, vl);
    reason = reasonbuf;
    has_reason = true;
  }

  boolean exception::set_operand(const char *&op, std::nullptr_t _op)
  {
    op = coalesce(_op);
    return false;
  }

  boolean exception::set_operand(const char *&op, const char *_op)
  {
    op = coalesce(_op);
    return true;
  }

  boolean exception::set_operand(const char *&op, const void *_op)
  {
    sprintf(tmpbuf[num], "0x%08zx", reinterpret_cast<size_t>(_op));
    op = tmpbuf[num++];
    return true;
  }

  boolean exception::set_operand(const char *&op, unsigned char _op)
  {
    sprintf(tmpbuf[num], "%u", _op);
    op = tmpbuf[num++];
    return true;
  }

  boolean exception::set_operand(const char *&op, unsigned short _op)
  {
    sprintf(tmpbuf[num], "%u", _op);
    op = tmpbuf[num++];
    return true;
  }

  boolean exception::set_operand(const char *&op, unsigned int _op)
  {
    sprintf(tmpbuf[num], "%d", _op);
    op = tmpbuf[num++];
    return true;
  }

  boolean exception::set_operand(const char *&op, unsigned long _op)
  {
    sprintf(tmpbuf[num], "%lu", _op);
    op = tmpbuf[num++];
    return true;
  }

  boolean exception::set_operand(const char *&op, unsigned long long _op)
  {
    sprintf(tmpbuf[num], "%llu", _op);
    op = tmpbuf[num++];
    return true;
  }

  boolean exception::set_operand(const char *&op, signed char _op)
  {
    sprintf(tmpbuf[num], "%d", _op);
    op = tmpbuf[num++];
    return true;
  }

  boolean exception::set_operand(const char *&op, short _op)
  {
    sprintf(tmpbuf[num], "%d", _op);
    op = tmpbuf[num++];
    return true;
  }

  boolean exception::set_operand(const char *&op, int _op)
  {
    sprintf(tmpbuf[num], "%d", _op);
    op = tmpbuf[num++];
    return true;
  }

  boolean exception::set_operand(const char *&op, long _op)
  {
    sprintf(tmpbuf[num], "%ld", _op);
    op = tmpbuf[num++];
    return true;
  }

  boolean exception::set_operand(const char *&op, long long _op)
  {
    sprintf(tmpbuf[num], "%lld", _op);
    op = tmpbuf[num++];
    return true;
  }

  template<class T, class E = std::enable_if<std::is_integral<T>::value>>
  char *_set_operand_integral_ptr(const T *_op, size_t length)
  {
    if(_op)
    {
      char *buf;
      char *pos;
      size_t allocleft = length * (sizeof(T) * 2 + 1);

      buf = new char[allocleft];
      pos = buf;

      for(size_t i = 0; i < length; i++)
      {
        size_t n = snprintf(pos, allocleft, "%0*lx ", (int)sizeof(T) * 2, (unsigned long)_op[i]);
        if(n >= allocleft)
          break;

        pos += n;
        allocleft -= n;
      }

      return buf;
    }
    return nullptr;
  }

  void exception::set_operand_alloc(const char *&op, char *buf)
  {
    if(buf)
    {
      allocbuf[num++] = buf;
      op = buf;
    }
  }

  void exception::set_operand_integral_ptr(const char *&op, const char *_op, size_t length)
  {
    char *buf = _set_operand_integral_ptr(_op, length);
    set_operand_alloc(op, buf);
  }

  void exception::set_operand_integral_ptr(const char *&op, const unsigned char *_op, size_t length)
  {
    char *buf = _set_operand_integral_ptr(_op, length);
    set_operand_alloc(op, buf);
  }

  void exception::set_operand_integral_ptr(const char *&op, const unsigned short *_op, size_t length)
  {
    char *buf = _set_operand_integral_ptr(_op, length);
    set_operand_alloc(op, buf);
  }

  void exception::set_operand_integral_ptr(const char *&op, const unsigned int *_op, size_t length)
  {
    char *buf = _set_operand_integral_ptr(_op, length);
    set_operand_alloc(op, buf);
  }


  /************************************************************************
   * Unit::unittest functions.
   */

  void unittest::run_section(void)
  {
    try
    {
      run_impl();
    }
    catch(const Unit::skip &e)
    {
      this->skip();
    }
    catch(const Unit::exception &e)
    {
      this->print_exception(e);
    }
  }

  bool unittest::run(void)
  {
    assert(!has_run);
    has_run = true;

    // NOTE-- first time doesn't run any section if sections are present.
    this->failed_sections = 0;
    this->count_sections = 0;
    this->expected_section = 0;
    this->skipped_sections = 0;
    this->entire_test_skipped = false;
    run_section();

    if(has_failed_main)
      return false;

    if(this->entire_test_skipped)
    {
      print_test_skipped();
      return true;
    }

    this->num_sections = this->count_sections;

    while(this->expected_section < this->num_sections)
    {
      this->count_sections = 0;
      this->expected_section++;
      run_section();
    }

    if(this->entire_test_skipped ||
      (this->num_sections && this->skipped_sections == this->num_sections))
    {
      print_test_skipped();
      this->entire_test_skipped = true;
      return true;
    }

    if(this->last_failed_section)
    {
      unsigned int passed = this->passed_sections();
      Uerr("  Failed %u section(s)", this->failed_sections);

      if(this->skipped_sections)
      {
        Uerr(" (passed %u, skipped %u).\n", passed, this->skipped_sections);
      }
      else
        Uerr(" (passed %u).\n", passed);

      UerrFlush();
      return false;
    }

    print_test_success();
    return true;
  }

  void unittest::print_test_success(void)
  {
    const char *_test_name = coalesce(test_name);
    unsigned int passed = this->passed_sections();

    Uerr("Passed test '%s::%s'", file_name, _test_name);

    if(num_sections > 0)
    {
      Uerr(" (%u section%s", passed, (passed > 1) ? "s" : "");

      if(this->skipped_sections)
        Uerr(", %u skipped)\n", this->skipped_sections);
      else
        Uerr(")\n");
    }
    else
      Uerr("\n");

    UerrFlush();
  }

  void unittest::print_test_failed(void)
  {
    if(!printed_failed)
    {
      const char *_test_name = coalesce(test_name);

      Uerr("Failed test '%s::%s'\n", file_name, _test_name);
      printed_failed = true;
    }
  }

  void unittest::print_test_skipped(void)
  {
    const char *_test_name = coalesce(test_name);
    Uerr("Skipping test '%s::%s'", file_name, _test_name);

    if(this->skipped_sections)
    {
      Uerr(" (%u section%s)\n", this->skipped_sections,
        (this->skipped_sections > 0) ? "s" : "");
    }
    else
      Uerr("\n");

    UerrFlush();
  }

  void unittest::signal_fail()
  {
    const char *_test_name = coalesce(this->test_name);
    const char *_section_name = coalesce(this->section_name);

    if(this->expected_section)
    {
      Uerr("Test '%s::%s' aborted in section '%s' (#%u out of %u)\n",
        file_name, _test_name, _section_name, this->expected_section, this->num_sections);
    }
    else
      Uerr("Test '%s::%s' aborted\n", file_name, _test_name);
  }

  void unittest::print_exception(const Unit::exception &e)
  {
    const char *_section_name = coalesce(this->section_name);

    print_test_failed();

    if(this->expected_section)
    {
      if(this->last_failed_section < this->expected_section)
      {
        this->failed_sections++;
        Uerr("  In section '%s': \n", _section_name);
        this->last_failed_section = this->expected_section;
      }
      Uerr("    Assert failed at line %d: %s", e.line, e.test);
    }
    else
    {
      Uerr("  Assert failed at line %d: %s", e.line, e.test);
      this->has_failed_main = true;
    }

    if(e.has_reason)
      Uerr(" (%s)\n", e.reason);
    else
      Uerr("\n");

    if(e.has_values)
    {
      Uerr("    Left:  %s\n", e.left);
      Uerr("    Right: %s\n", e.right);
    }
    UerrFlush();
  }

  void unittest::skip()
  {
    if(!this->expected_section)
    {
      this->entire_test_skipped = true;
    }
    else
      this->skipped_sections++;
  }


  /************************************************************************
   * Unit::unittestrunner_cls and support functions.
   */

  static std::vector<unittest *> tests;
  unittestrunner_cls unittestrunner;

  void unittestrunner_cls::print_status()
  {
    if(!total)
    {
      Uerr("ERROR: no tests defined!\n\n");
      failed = 1;
      return;
    }

    if(total == count && total == passed && !failed && !skipped)
    {
      // Print a shorter summary for the general case...
      Uerr("Passed %u test%s.\n\n", total, (total > 1) ? "s" : "");
      UerrFlush();
      return;
    }

    Uerr("\nSummary:\n  Tests total: %u\n", total);

    if(passed)  Uerr("  Tests passed: %u\n", passed);
    if(failed)  Uerr("  Tests failed: %u\n", failed);
    if(skipped) Uerr("  Tests skipped: %u\n", skipped);

    Uerr("\n");
    UerrFlush();
  }

  bool unittestrunner_cls::run()
  {
    count = 0;
    passed = 0;
    failed = 0;
    skipped = 0;
    total = tests.size();

    for(unittest *t : tests)
    {
      count++;
      current_test = t;
      bool result = t->run();

      if(result)
      {
        if(!t->entire_test_skipped)
          passed++;
        else
          skipped++;
      }
      else
        failed++;
    }

    print_status();
    return !failed;
  }

  void unittestrunner_cls::signal_fail()
  {
    if(current_test)
      current_test->signal_fail();
    else
      Uerr("ERROR: NULL test!\n");

    failed++;
    skipped += total - count;
    print_status();
  }

  void unittestrunner_cls::addtest(unittest *t)
  {
    tests.push_back(t);
  }
}
