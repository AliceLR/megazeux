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

/**
 * Unit test class macros and template. Requires C++11 or higher. Includes a
 * main() implementation, a SIGABRT handler, and custom check_alloc function
 * implementations. To implement a test, simply include this header and write
 * tests as follows:
 *
 * UNITTEST(TestName)
 * {
 *   // test here
 *   // run this once standalone + once for each section
 *
 *   SECTION(SectionName)
 *   {
 *     // only run this once
 *   }
 *
 *   SECTION(SectionName2)
 *   {
 *     // only run this once, but separately from SectionName...
 *   }
 * }
 *
 * The file can contain multiple UNITTEST() {...}. Each must have a unique name.
 * The following macros can be used in tests:
 *
 * [mfmt,...] = optional message to display on failure, passed to vsnprintf.
 *              mfmt must be a string literal.
 *
 * ASSERT(t[,mfmt,...])         t must be non-zero.
 * ASSERTEQ(a,b[,mfmt,...])     a must equal b.
 * ASSERTCMP(a,b[,mfmt,...])    a and b must be null-terminated C strings and must
 *                              be exactly equal (strcmp).
 * ASSERTNCMP(a,b,n[,mfmt,...]) a and b must be null-terminated C strings and must
 *                              be exactly equal for the first n chars (strncmp).
 * ASSERTMEM(a,b,l,[mfmt,...])  the memory pointed to by a and b must be identical
 *                              for l bytes (memcmp).
 * FAIL([mfmt,...])             unconditionally fail the test.
 * SKIP()                       unconditionally skip the test.
 *
 * Additionally, failed assert() assertions will be detected and print error
 * messages (generally, these should only be used in the code being tested).
 *
 * This is not a replacement for the testworlds tests, and likely will be
 * useful only for the subset of MegaZeux algorithm and IO code that can be
 * tested completely standalone from the rest of MegaZeux.
 */

#ifndef UNIT_HPP
#define UNIT_HPP

#ifdef __M_BEGIN_DECLS
#error "Include Unit.hpp first!"
#endif

#define CORE_LIBSPEC
#define EDITOR_LIBSPEC
#define SKIP_SDL
#include "../src/compat.h"
#include "../src/platform_endian.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include <csignal>
#include <iostream>
#include <sstream>
#include <type_traits>
#include <vector>

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

/**
 * Utility templates.
 */

template<class T, class S, int A, int B>
static inline constexpr int samesize(T (&a)[A], S (&b)[B])
{
  static_assert(A == B, "array size mismatch");
  return 0;
}

template<class T, int A>
static inline constexpr int arraysize(T (&a)[A])
{
  return A;
}

template<class T>
static inline const T coalesce(const T var)
{
  return var;
}

static inline unsigned int coalesce(boolean var)
{
  return var;
}

static inline const char *coalesce(const char *var)
{
  return (var ? var : "NULL");
}

static inline constexpr const char *coalesce(std::nullptr_t ignore)
{
  return "NULL";
}

template<class aligntype, size_t A=128>
class alignstr
{
protected:
  union
  {
    char arr[A];
    aligntype ignore;
  } u;

public:
  template<int B>
  alignstr(const char (&str)[B])
  {
    static_assert(A >= B, "alignstr buffer is too small!");
    std::copy(str, str + B - 1, u.arr);
  }

  alignstr(const char * const str)
  {
    size_t slen = strlen(str);
    assert(slen + 1 <= A);
    std::copy(str, str + slen, u.arr);
  }

  constexpr const char *c_str() const
  {
    return u.arr;
  }
};

/**
 * Unit test macros.
 */

#define UNIMPLEMENTED() \
  do\
  {\
    throw Unit::exception(__LINE__, "Test is not yet implemented"); \
  } while(0)

#define ASSERT(test, ...) \
  do\
  {\
    if(!(test)) \
    { \
      throw Unit::exception(__LINE__, #test, "" __VA_ARGS__); \
    } \
  } while(0)

#define ASSERTEQ(a, b, ...) \
  do\
  {\
    if(!((a) == (b)))\
    { \
      throw Unit::exception(__LINE__, #a " == " #b, (a), (b), "" __VA_ARGS__); \
    } \
  } while(0)

#define ASSERTCMP(a, b, ...) \
  do\
  {\
    if(strcmp(a,b)) \
    {\
      throw Unit::exception(__LINE__, "strcmp(" #a ", " #b ")", (a), (b), "" __VA_ARGS__); \
    }\
  } while(0)

#define ASSERTNCMP(a, b, n, ...) \
  do\
  {\
    if(strncmp(a,b,n)) \
    {\
      throw Unit::exception(__LINE__, "strncmp(" #a ", " #b ", " #n ")", (a), (b), "" __VA_ARGS__); \
    }\
  } while(0)

#define ASSERTMEM(a, b, l, ...) \
  do\
  {\
    if(memcmp(a,b,l)) \
    {\
      throw Unit::exception(__LINE__, "memcmp(" #a ", " #b ", " #l ")", (a), (b), (l), "" __VA_ARGS__); \
    }\
  } while(0)

#define FAIL(...) \
  do\
  {\
    throw Unit::exception(__LINE__, nullptr, "" __VA_ARGS__); \
  } while(0)

#define SKIP() \
  do\
  {\
    throw Unit::skip(); \
  } while(0)

#define SECTION(sectionname) \
  this->section_name = #sectionname; \
  if((++this->count_sections) == this->expected_section)

#define UNITTEST(testname) \
class testname ## _unittest_impl final : public Unit::unittest \
{\
  void run_impl() override;\
public:\
  testname ## _unittest_impl(): Unit::unittest(__FILE__, #testname) {}\
}\
static testname ## _unittest_inst;\
\
inline void testname ## _unittest_impl::run_impl(void)

namespace Unit
{
  class unittest;

  class skip final: std::exception {};

  class exception final: std::exception
  {
  public:
    int line;
    std::string test;
    std::string reason;
    bool has_reason;
    std::string left;
    std::string right;
    bool has_values;

    exception(int _line, const char *_test):
     line(_line), test(coalesce(_test)), reason(""), has_reason(false),
     left(coalesce(nullptr)), right(coalesce(nullptr)), has_values(false) {}

    exception(int _line, const char *_test, const char *_reason_fmt, ...):
     line(_line), test(coalesce(_test)), has_reason(false),
     left(coalesce(nullptr)), right(coalesce(nullptr)), has_values(false)
    {
      if(_reason_fmt && _reason_fmt[0])
      {
        va_list vl;
        va_start(vl, _reason_fmt);
        set_reason_fmt(_reason_fmt, vl);
        va_end(vl);
      }
      else
        reason = coalesce(nullptr);
    }

    template<class T, class S>
    exception(int _line, const char *_test, T _left, S _right, const char *_reason_fmt, ...):
     line(_line), test(coalesce(_test)), has_reason(false), has_values(false)
    {
      if(_reason_fmt && _reason_fmt[0])
      {
        va_list vl;
        va_start(vl, _reason_fmt);
        set_reason_fmt(_reason_fmt, vl);
        va_end(vl);
      }
      else
        reason = coalesce(nullptr);

      set_operand(left, _left);
      set_operand(right, _right);
    }

    template<class T, class E = std::enable_if<std::is_integral<T>::value>>
    exception(int _line, const char *_test, const T *_left, const T *_right,
     size_t length, const char *_reason_fmt, ...):
     line(_line), test(coalesce(_test)), has_reason(false)
    {
      if(_reason_fmt && _reason_fmt[0])
      {
        va_list vl;
        va_start(vl, _reason_fmt);
        set_reason_fmt(_reason_fmt, vl);
        va_end(vl);
      }
      else
        reason = coalesce(nullptr);

      has_values = (_left || _right);
      length /= sizeof(T);

      set_operand_integral_ptr(left, _left, length);
      set_operand_integral_ptr(right, _right, length);
    }

    // Print non-integral memcmp types bytewise.
    exception(int _line, const char *_test, const void *_left, const void *_right,
     size_t length, const char *_reason):
     exception(_line, _test, (const uint8_t *)_left, (const uint8_t *)_right,
      length, _reason) {}

  private:

    void set_reason_fmt(const char *_reason_fmt, va_list vl)
    {
      char reasonbuf[1024];

      vsnprintf(reasonbuf, sizeof(reasonbuf), _reason_fmt, vl);

      reason = reasonbuf;
      has_reason = true;
    }

    template<class T>
    void set_operand(std::string &op, T _op)
    {
      std::stringstream st;
      st << coalesce(_op);
      op = st.str();
      has_values |= !std::is_same<T, std::nullptr_t>::value;
    }

    template<class T>
    void set_operand_integral_ptr(std::string &op, const T *_op, size_t length)
    {
      std::stringstream st;

      if(_op)
      {
        st << std::hex;

        for(size_t i = 0; i < length; i++)
          st << static_cast<uint64_t>(_op[i]) << ' ';
      }
      else
        st << coalesce(_op);

      op = st.str();
    }
  };

  class unittestrunner_cls final
  {
  private:

    std::vector<unittest *> tests;
    unittest *current_test;
    unsigned int count;
    unsigned int passed;
    unsigned int failed;
    unsigned int skipped;
    unsigned int total;

    void print_status()
    {
      if(!total)
      {
        std::cerr << "ERROR: no tests defined!\n\n";
        return;
      }

      if(total == count && total == passed && !failed && !skipped)
      {
        // Print a shorter summary for the general case...
        std::cerr << "Passed " << total << " "
         << (total > 1 ? "tests" : "test") << ".\n";
        std::cerr << std::endl;
        return;
      }

      std::cerr << "\n"
        "Summary:\n"
        "  Tests total: " << total << "\n";

      if(passed)  std::cerr << "  Tests passed: " << passed << "\n";
      if(failed)  std::cerr << "  Tests failed: " << failed << "\n";
      if(skipped) std::cerr << "  Tests skipped: " << skipped << "\n";
      std::cerr << std::endl;
    }

  public:
    bool run(void);
    void signal_fail();

    void addtest(unittest *t)
    {
      tests.push_back(t);
    }
  }
  static unittestrunner;

  class unittest
  {
    bool has_run = false;
    bool has_failed_main = false;
    unsigned int last_failed_section = 0;
    bool printed_failed = false;

  protected:

    const char *file_name;
    const char *test_name;
    const char *section_name = nullptr;

    unsigned int num_sections;
    unsigned int count_sections;
    unsigned int expected_section;
    unsigned int failed_sections;
    unsigned int skipped_sections;

  public:

    bool entire_test_skipped;

    unittest(const char *_file, const char *_test_name):
     file_name(_file), test_name(_test_name)
    {
      unittestrunner.addtest(this);
    };

    ~unittest() {};

    inline void run_section(void)
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

    inline bool run(void)
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
        std::cerr << "  Failed " << this->failed_sections << " section(s)";

        if(this->skipped_sections)
        {
          std::cerr << " (passed " << passed << ", skipped " <<
           this->skipped_sections << ").\n";
        }
        else
          std::cerr << " (passed " << passed << ").\n";
        return false;
      }

      print_test_success();
      return true;
    }

    inline int passed_sections(void)
    {
      return this->count_sections - this->failed_sections - this->skipped_sections;
    }

    inline void print_test_success(void)
    {
      const char *_test_name = coalesce(test_name);
      unsigned int passed = this->passed_sections();

      std::cerr << "Passed test '" << file_name << "::" << _test_name << "'";

      if(num_sections > 0)
      {
        std::cerr << " (" << passed << (passed > 1 ? " sections" : " section");

        if(this->skipped_sections)
          std::cerr << ", " << this->skipped_sections << " skipped)\n";
        else
          std::cerr << ")\n";
      }
      else
        std::cerr << "\n";
    }

    inline void print_test_failed(void)
    {
      if(!printed_failed)
      {
        const char *_test_name = coalesce(test_name);

        std::cerr << "Failed test '" << file_name << "::" << _test_name << "'\n";
        printed_failed = true;
      }
    }

    inline void print_test_skipped(void)
    {
      const char *_test_name = coalesce(test_name);
      std::cerr << "Skipping test '" << file_name << "::" << _test_name << "'";

      if(this->skipped_sections)
      {
        std::cerr << " (" << this->skipped_sections <<
         (this->skipped_sections > 0 ? " section)\n" : " sections)\n");
      }
      else
        std::cerr << "\n";
    }

    inline void signal_fail()
    {
      const char *_test_name = coalesce(this->test_name);
      const char *_section_name = coalesce(this->section_name);

      if(this->expected_section)
      {
        std::cerr << "Test '" << file_name << "::"
          << _test_name << "' aborted in section '"
          << _section_name <<  "' (#" << this->expected_section
          << " out of " << this->num_sections << ")\n";
      }
      else
        std::cerr << "Test '" << file_name << "::" << _test_name << "' aborted\n";
    }

    inline void print_exception(const Unit::exception &e)
    {
      const char *_section_name = coalesce(this->section_name);

      print_test_failed();

      if(this->expected_section)
      {
        if(this->last_failed_section < this->expected_section)
        {
          this->failed_sections++;
          std::cerr << "  In section '" << _section_name << "': \n";
          this->last_failed_section = this->expected_section;
        }
        std::cerr <<
         "    Assert failed at line " << e.line << ": " << e.test;
      }
      else
      {
        std::cerr << "  Assert failed at line " << e.line << ": " << e.test;
        this->has_failed_main = true;
      }

      if(e.has_reason)
        std::cerr << " (" << e.reason << ")\n";
      else
        std::cerr << "\n";

      if(e.has_values)
      {
        std::cerr
          << "    Left:  " << e.left << "\n"
          << "    Right: " << e.right << "\n";
      }
    }

    void skip()
    {
      if(!this->expected_section)
      {
        this->entire_test_skipped = true;
      }
      else
        this->skipped_sections++;
    }

    virtual void run_impl(void) {};
  };

  bool unittestrunner_cls::run(void)
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
      std::cerr << "ERROR: NULL test!\n";

    failed++;
    skipped += total - count;
    print_status();
  }
}

void sigabrt_handler(int signal)
{
  if(signal == SIGABRT)
  {
    std::cerr << "Received SIGABRT: ";
  }
  else
    std::cerr << "Unexpected signal " << signal << " received: ";

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

#endif /* UNIT_HPP */
