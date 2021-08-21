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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static inline constexpr const char *coalesce(decltype(nullptr) ignore)
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
    memcpy(u.arr, str, B);
  }

/*
  alignstr(const char * const str)
  {
    size_t slen = strlen(str);
    assert(slen + 1 <= A);
    memcpy(u.arr, str, slen);
  }
*/

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

#define Uerr(...) do{ fprintf(stderr, "" __VA_ARGS__); }while(0)
#define UerrFlush() do{ fflush(stderr); }while(0)

namespace Unit
{
  class unittest;

  class skip final {};

  /**
   * Thrown on test failure (see ASSERT macros above).
   */
  class exception final
  {
  public:
    int line;
    const char *test      = coalesce(nullptr);
    const char *reason    = coalesce(nullptr);
    bool has_reason       = false;
    const char *left      = coalesce(nullptr);
    const char *right     = coalesce(nullptr);
    bool has_values       = false;

    /* A copy constructor is required to be throwable. */
    exception(const exception &e);

    exception(int _line, const char *_test):
     line(_line), test(coalesce(_test)), reason("") {}

    exception(int _line, const char *_test, const char *_reason_fmt, ...):
     line(_line), test(coalesce(_test))
    {
      if(_reason_fmt && _reason_fmt[0])
      {
        va_list vl;
        va_start(vl, _reason_fmt);
        set_reason_fmt(_reason_fmt, vl);
        va_end(vl);
      }
    }

    template<class T, class S>
    exception(int _line, const char *_test, T _left, S _right, const char *_reason_fmt, ...):
     line(_line), test(coalesce(_test))
    {
      if(_reason_fmt && _reason_fmt[0])
      {
        va_list vl;
        va_start(vl, _reason_fmt);
        set_reason_fmt(_reason_fmt, vl);
        va_end(vl);
      }

      has_values |= set_operand(left, _left);
      has_values |= set_operand(right, _right);
    }

    template<class T>
    exception(int _line, const char *_test, const T *_left, const T *_right,
     size_t length, const char *_reason_fmt, ...):
     line(_line), test(coalesce(_test))
    {
      if(_reason_fmt && _reason_fmt[0])
      {
        va_list vl;
        va_start(vl, _reason_fmt);
        set_reason_fmt(_reason_fmt, vl);
        va_end(vl);
      }

      has_values = (_left || _right);
      length /= sizeof(T);

      set_operand_integral_ptr(left, _left, length);
      set_operand_integral_ptr(right, _right, length);
    }

    // Print non-integral memcmp types bytewise.
    exception(int _line, const char *_test, const void *_left, const void *_right,
     size_t length, const char *_reason):
     exception(_line, _test, (const unsigned char *)_left, (const unsigned char *)_right,
      length, _reason) {}

    ~exception()
    {
      delete[] allocbuf[0];
      delete[] allocbuf[1];
    }

  private:

    static constexpr int BUF_SIZE = 24;
    char reasonbuf[1024];
    char tmpbuf[2][BUF_SIZE];
    char *allocbuf[2] = { nullptr, nullptr };
    int num = 0;

    void set_reason_fmt(const char *_reason_fmt, va_list vl);
    boolean set_operand(const char *&op, decltype(nullptr) _op);
    boolean set_operand(const char *&op, const char *_op);
    boolean set_operand(const char *&op, const void *_op);
    boolean set_operand(const char *&op, unsigned char _op);
    boolean set_operand(const char *&op, unsigned short _op);
    boolean set_operand(const char *&op, unsigned int _op);
    boolean set_operand(const char *&op, unsigned long _op);
    boolean set_operand(const char *&op, unsigned long long _op);
    boolean set_operand(const char *&op, signed char _op);
    boolean set_operand(const char *&op, short _op);
    boolean set_operand(const char *&op, int _op);
    boolean set_operand(const char *&op, long _op);
    boolean set_operand(const char *&op, long long _op);

    void set_operand_alloc(const char *&op, char *buf);
    void set_operand_integral_ptr(const char *&op, const char *_op, size_t length);
    void set_operand_integral_ptr(const char *&op, const unsigned char *_op, size_t length);
    void set_operand_integral_ptr(const char *&op, const unsigned short *_op, size_t length);
    void set_operand_integral_ptr(const char *&op, const unsigned int *_op, size_t length);
  };

  /**
   * Class for running all unit tests in the current set of unit tests.
   */
  class unittestrunner final
  {
  private:

    unittest *current_test;
    unsigned int count;
    unsigned int passed;
    unsigned int failed;
    unsigned int skipped;
    unsigned int total;

    void print_status();

  public:
    static unittestrunner &get();
    bool run(void);
    void signal_fail();
    void addtest(unittest *t);
  };

  /**
   * Class for an individual unit test. Don't use directly; create individual
   * test subclasses using the UNITTEST() macro.
   */
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
      unittestrunner::get().addtest(this);
    };

    ~unittest() {};

    inline int passed_sections(void)
    {
      return this->count_sections - this->failed_sections - this->skipped_sections;
    }

    bool run(void);
    void signal_fail();

  private:
    void run_section(void);
    void print_test_success(void);
    void print_test_failed(void);
    void print_test_skipped(void);
    void print_exception(const Unit::exception &e);
    void skip();

    virtual void run_impl(void) = 0;
  };
}

#endif /* UNIT_HPP */
