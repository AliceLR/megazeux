# MZX Test Worlds

This folder contains a set of worlds that test various aspects of MZX and
particularly Robotic that can be determined to be correct or incorrect from
within MegaZeux itself. Run the full sequence of tests with `run.sh`, or
use the `make test` rule.

This system currently requires an `mzxrun` executable in the parent directory,
and may not work correctly for all platforms.

## Conventions

### Naming

Test worlds are named in the format

```VVV/XXX [short description].mzx```

where `VVV` is the MegaZeux version associated with the world (e.g. `2.51`)
and XXX is the test number (e.g. `123`).

Example version strings:
* `2.02` for 2.02
* `2.07` for 2.07
* `2.51` for 2.51
* `2.51s1` for 2.51s1
* `2.51s2` for 2.51s2
* `2.51s3` for 2.51s3
* `2.51s3.1` for 2.51s3.1
* `2.51s3.2` for 2.51s3.2
* `2.60` for 2.6
* `2.61` for 2.61
* `2.62` for 2.62
* `2.62b` for 2.62b
* etc...

Letter versions starting from 2.80 onward belong in the folder of their
non-lettered counterpart. Change the first digit of the test number to the
version letter to signify it is a test for a change in that particular
lettered version. Example: a 2.80d test called "Big Test" could be located
at the path `2.80/d01 Big Test.mzx`).

### Dependencies

External files used as dependencies by a test world should be located in the
folder `data`. Test worlds are copied to the testworlds base folder before they
are executed, so they should have access to this folder when running as part
of a test.

### Robotic

Additionally, test worlds must conform to the following conventions:

1) The world must not be encrypted, and must have a properly configured
starting board. A title is helpful but not necessary.

2) The world will perform exactly one test or a group of closely related tests
(e.g. testing various COPY BLOCK uses) and will be created and versioned for the
earliest possible MegaZeux version it is applicable for. If versioned compatibility
behavior exists, create a version of the test for each behavior with similar
filenames and titles using the earliest applicable MZX versions for each.

3) The test must assume it will be operating at MZX_SPEED 1 and with unbounded
COMMANDS, unless the purpose of the test requires a different MZX_SPEED or
COMMANDS value. Setting these defaults explicitly is not necessary.

4) The robot driving the tests must be clearly visible.

5) The first line(s) of the testing robot must be setting the `$title` string
to the title of the test, the `$author` string to your identifier, and the
`$desc` string to a description of the test. This must be wrapped to fit the
robot editor window with `inc "$desc" "[more description]"` as-needed.

6) Upon completion or failure, the counter `result` must be set to one of the
following counters: `PASS`, `WARN`, `FAIL`, `BADF`, or `SKIP`.

7) Extra testing notes may be included in the `$result` string.

8) Counter and string names beginning with two underscores (e.g. `__abc`,
`$__def`) are reserved and should not be used.

9) When the test is finished running, the following snippet of code
MUST be executed:

```
: "exit"
if "__continue" = 1 then "__swap"
end
: "__swap"
swap world "next"
```

### Special Counters

The following counters have special meaning:

* `PASS`: Set `result` to this to indicate that the test passed.
* `FAIL`: Set `result` to this to indicate that the test failed.
* `WARN`: Set `result` to this to indicate that the test had an error condition but otherwise should count as a success.
* `BADF`: Set `result` to this to indicate that the test failed due to a file loading error.
* `SKIP`: Set `result` to this to indicate that the test was skipped.
* `result`: indicates the result of the test. Defaults to `BADF`.
* `$result`: indicates more details about the result of a test.
* `$world`: the filename of the current world. NOTE: This is not necessarily the original filename of the world.
* `$title`: the title of the test.
* `$author`: the author of the test.
* `$desc`: a description of the test.

Worlds from MZX versions 2.62 to 2.70 should use the following compatible strings:

* `$string0`: the filename of the current world. NOTE: This is not necessarily the original filename of the world.
* `$string1`: the title of the test.
* `$string2`: the author of the test.
* `$string3`: a description of the test.
* `$string4`: indicates more details about the result of a test.


### Skipping Tests

To skip a test, set `result` to `SKIP`. The following counters indicate when a
certain test should be skipped:

* `__skip_audio`: all audio in MZX is disabled. All audio tests should check this.
* `__skip_mod`: no module player for the audio system has been enabled.
* `__skip_vorbis`: ogg/vorbis support has been disabled.


### Compatibility Notes

1) DOS worlds are not capable of the string operations described above. Strings
were introduced in MegaZeux 2.62 in a very limited form. In versions 2.62, 2.62b,
and 2.65, strings can be _ONLY **15** CHARACTERS LONG_. In versions 2.68 through
2.70, strings can be up to **63** characters long. While it may be possible to
set longer strings in old worlds using newer versions, it's recommended to stay
within the original bounds and use the compatibility strings listed above.

Worlds from MZX versions before 2.62 don't have access to any strings and need to
provide title, author, and description info in *both* comment form in the main
robot (for reference) and in a separate text file (to be added to the test output).
The text file must have the **exact same file name** as the test world but a .txt
extension instead of a .mzx extension (ex: "mytest.mzx" → "mytest.txt").

Comment form:
```
. "Title: A text"
. "Author: You"
. "Desc: This is a long description of the test."
```

Text file format (the space after "Title:", etc. is required):
```
Title: A test
Author: You
Desc: This is a long description of the test.
```

It is not possible to provide a `$result` equivalent for these worlds currently,
but this functionality may be added in the future.
