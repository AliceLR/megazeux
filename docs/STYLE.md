# MegaZeux code formatting and style guide

This file is an attempt to summarize the coding style that should be used in code
contributed to MegaZeux.  It is a work-in-progress.  Contributions to the MZX source
code will not be merged unless they comply with this guide.

## License

Include the license at the start of every file in a block comment starting with `MegaZeux` and
followed by copyright notices for all authors that have contributed significantly to the file.
If you contribute significantly to a pre-existing file, you are recommended to add your copyright.

## Margin

Wrap lines at 80 chars.

## Control statements

Are not separated from their parentheses.

```
if(a == b)
```

## Curly braces

Belong on their own lines.

```
void main(int argc, char *argv[])
{
}
```

### if/else

The following spacing/curly brace formats are generally okay:

```
if(cond)
  do_thing();
```

```
if(cond)
  do_thing();

else
  do_other_thing();
```

```
if(cond)
{
  ...
}
```

```
if(cond)
{
  ...
}
else
{
  ...
}
```

```
if(cond)
{
  ...
}
else

if(other_cond)
{
  ...
}

else
{
  ...
}
```

Do not mix curly braces and no curly braces in the same if/else chain.

### Structures/enums

Definition:
```
struct a_struct
{
  ...
};

enum an_enum
{
  ...
};
```

Assignment:
```
static struct a_struct thing =
{
  ...
};
```

## Indentation

Use spaces, not tabs. Exception: shell scripts and Makefiles should use tabs instead.

### Individual lines

Use two *spaces* for each indentation level.

```
{
  {
    {
      // ...
    }
  }
}
```

### Wrapped lines

Use one space after the current indentation level.

```
  if(a == b && ... &&
   a == f)
```

When splitting a function call with many arguments to multiple lines,
this indentation may look better (and helps avoid the margin!):

```
  func([params,]
    params,
    params,
    ...
  );
```


### Macros

`#ifdef` et al should not be indented. Aside from very short blocks, note the original condition
in a commend after `#endif`.

```
    // some code
    ...

#ifdef CONFIG_DEBYTECODE
    ...
#endif /* CONFIG_DEBYTECODE */

    ...
    // more code
```

## Types

### Pointers

Pointers are written in this style:
```
int *an_int_pointer;
```

### Casting
```
  int an_int = (int)not_an_int;
```
```
  int *an_int_ptr = (int *)not_an_int_ptr;
```


## Organization

### C vs. C++

MZX is almost entirely written in C.  Add C++ only where absolutely necessary (e.g. when interfacing
with a library that only has a C++ interface available).

There's a nasty portability problem when using "bool" between C and C++ where the size of
this type will not necessarily be the same. Avoid this type as much as possible.


### Opaqueness and typedefs

Typedefs are appropriate in MZX two cases:
1) Hiding really ugly/long function pointer definitions.
2) Ubiquitous basic types (e.g. boolean) or opaque types (e.g. FILE).
Otherwise, don't use them.

Some subsets of the MZX source (particularly audio, rendering, and networking) heavily practice
hiding structs and implementation-specific functions.  Look at other files in these areas to make
sure your audio handler/renderer/etc follows conventions.


### Headers

General format is:

```
[GPL Copyright notice/license - see LICENSE]

#ifndef __[YOUR FILE HERE]_H
#define __[YOUR FILE HERE]_H

#include "compat.h"

__M_BEGIN_DECLS

[all other contents]

__M_END_DECLS

#endif /* __[YOUR FILE HERE]_H */
```

Notes:
1) The GPL notice must be in every source file. See LICENSE for more information.
2) Each header needs a unique include guard to ensure that it isn't included multiple times.
3) `src/compat.h` defines some required compatibility macros, including `__M_BEGIN_DECLS`
and `__M_END_DECLS`. These macros are used to provide portability when the MZX headers
are included in C++ files.
4) Functions/structs/variables in these MUST be required elsewhere; otherwise, use static and
put it in the C/CPP file!

### Cross-platform threading

For general usage see `src/thread_sdl.h` or `src/thread_pthread.h`.

Use the macro `THREAD_RES` as the return type of the thread function and `THREAD_RETURN;` to
exit the thread function, even if control would otherwise reach the end of the function.
This is necessary due to some platforms expecting a pointer, some expecting an integer,
and some that use no return type at all.
