# MegaZeux code formatting style guide

This file is an attempt to summarize the coding style that should be used in code
contributed to MegaZeux.  It is a work-in-progress.

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
  if(a == b && ...
   && a == f)
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
