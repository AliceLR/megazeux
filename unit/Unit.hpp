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
 * ASSERT(t)          t must be non-zero.
 * ASSERTX(t,m)       t must be non-zero. (m = message to display on failure)
 * ASSERTEQ(a,b)      a must equal b.
 * ASSERTEQX(a,b,m)   a must equal b. (m = message to display on failure)
 * ASSERTCMP(a,b)     a and b must be null-terminated C strings and must be
 *                    exactly equal.
 * ASSERTXCMP(a,b,m)  a and b must be null-terminated C strings and must be
 *                    exactly equal. (m = message to display on failure)
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

#include <assert.h>
#include <csignal>
#include <iostream>
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

template<class aligntype, size_t A=128>
class alignstr
{
protected:
  union
  {
    char arr[A];
    aligntype ignore;
  } u;
  size_t len;

public:
  template<int B>
  alignstr(const char (&str)[B]): len(B)
  {
    static_assert(A >= B, "alignstr buffer is too small!");
    std::copy(str, str + B - 1, u.arr);
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
    this->assert_fail(__LINE__, "Test is not yet implemented"); \
    return; \
  } while(0)

#define ASSERT(test) \
  do\
  {\
    if(!(test)) \
    { \
      this->assert_fail(__LINE__, #test); \
      return; \
    } \
  } while(0)

#define ASSERTX(test, reason) \
  do\
  {\
    if(!(test)) \
    {\
      this->assert_fail(__LINE__, #test, reason); \
      return; \
    }\
  } while(0)

#define ASSERTEQ(a, b) \
  do\
  {\
    if(!((a) == (b)))\
    { \
      this->assert_fail(__LINE__, #a " == " #b, (a), (b), nullptr); \
      return; \
    } \
  } while(0)

#define ASSERTEQX(a, b, reason) \
  do\
  {\
    if(!((a) == (b)))\
    {\
      this->assert_fail(__LINE__, #a " == " #b, (a), (b), reason); \
      return; \
    }\
  } while(0)

#define ASSERTCMP(a, b) \
  do\
  {\
    if(strcmp(a,b)) \
    {\
      this->assert_fail(__LINE__, "strcmp(" #a ", " #b ")", (a), (b), nullptr); \
      return; \
    }\
  } while(0)


#define ASSERTXCMP(a, b, reason) \
  do\
  {\
    if(strcmp(a,b)) \
    {\
      this->assert_fail(__LINE__, "strcmp(" #a ", " #b ")", (a), (b), reason); \
      return; \
    }\
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

      std::cerr << "\n"
        "Summary:\n"
        "  Tests total: " << total << "\n"
        "  Tests run: " << count << "\n";

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
    bool has_failed_section = false;

  protected:

    const char *file_name;
    const char *test_name;
    const char *section_name = nullptr;

    unsigned int num_sections;
    unsigned int count_sections;
    unsigned int expected_section;
    unsigned int failed_sections;

  public:

    unittest(const char *_file, const char *_test_name):
     file_name(_file), test_name(_test_name)
    {
      unittestrunner.addtest(this);
    };

    ~unittest() {};

    inline bool run(void)
    {
      assert(!has_run);
      has_run = true;

      // NOTE-- first time doesn't run any section if sections are present.
      this->failed_sections = 0;
      this->count_sections = 0;
      this->expected_section = 0;
      run_impl();

      if(has_failed_main)
      {
        print_test_failed();
        return false;
      }

      this->num_sections = this->count_sections;

      while(this->expected_section < this->num_sections)
      {
        this->count_sections = 0;
        this->expected_section++;
        run_impl();
      }

      if(this->has_failed_section)
      {
        print_test_failed();
        return false;
      }

      print_test_success();
      return true;
    }

    template<class T>
    inline T coalesce(const T var)
    { return var; }

    inline const char *coalesce(const char *var)
    { return (var ? var : "NULL"); }

    inline const char *coalesce(std::nullptr_t ignore)
    { return "NULL"; }

    inline void print_test_success(void)
    {
      const char *_test_name = coalesce(test_name);

      std::cerr << "Passed test '" << file_name << "::" << _test_name << "'";

      if(num_sections > 0)
      {
        std::cerr << " (" << num_sections
          << (num_sections > 1 ? " sections)\n" : " section)\n");
      }
      else
        std::cerr << "\n";
    }

    inline void print_test_failed(void)
    {
      const char *_test_name = coalesce(test_name);

      std::cerr << "Failed test '" << file_name << "::" << _test_name << "'\n";
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

    inline void assert_fail(int line, const char *test)
    { assert_fail(line, test, nullptr, nullptr, nullptr); }

    inline void assert_fail(int line, const char *test, const char *reason)
    { assert_fail(line, test, nullptr, nullptr, reason); }

    template<class T, class S>
    inline void assert_fail(int line, const char *test,
     const T left, const S right, const char *reason)
    {
      const char *_test_name = coalesce(this->test_name);
      const char *_section_name = coalesce(this->section_name);

      if(this->expected_section)
      {
        if(!this->has_failed_section)
        {
          std::cerr << "In test '" << file_name << "::" << _test_name << ": \n";
          this->has_failed_section = true;
        }
        std::cerr <<
         "  Assert failed for section '" << _section_name <<
           "' at line " << line << ": " << test;
      }
      else
      {
        std::cerr <<
         " Assert failed for test '" << file_name << "::" << _test_name <<
         "' at line " << line << ": " << test;

        this->has_failed_main = true;
      }

      if(reason) std::cerr << " (" << reason << ")\n";
      else       std::cerr << "\n";

      if(!std::is_same<T, std::nullptr_t>::value ||
       !std::is_same<S, std::nullptr_t>::value)
      {
        std::cerr
          << "    Left:  " << coalesce(left) << "\n"
          << "    Right: " << coalesce(right) << "\n";
      }
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
      t->run() ? passed++ : failed++;
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
    Unit::unittestrunner.signal_fail();
  }
  else
    std::cerr << "Unexpected signal received\n";
}

int main(int argc, char *argv[])
{
  std::signal(SIGABRT, sigabrt_handler);
  return !(Unit::unittestrunner.run());
}

#endif /* UNIT_HPP */
