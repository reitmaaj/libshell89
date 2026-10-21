# 0003 - line-oriented CLI acceptance

## Must exhibit

- A50. `text lower` lowercases ASCII `A`-`Z` and leaves every other byte
  unchanged.
- A51. `text grep --pattern="S"` writes exactly the lines containing literal
  substring `S`, preserving each line's bytes and final-newline state.
- A52. `text grep --ignore-case` folds ASCII case only during matching.
- A53. `text grep` exits 0 when at least one line matches and 1 when none match,
  with no diagnostic for the no-match case.
- A54. An empty grep pattern matches every line.
- A55. `text head --lines="N"` writes exactly the first N lines and drains the
  rest without emitting data.
- A56. `text skip --lines="N"` discards exactly the first N lines and copies the
  remainder byte-for-byte.
- A57. `N = 0`, leading zeros, and the maximum unsigned long are valid line
  counts.
- A58. Line commands work when a line or pattern crosses an internal read
  boundary.

## Must reject (unacceptable behavior)

- B23. An empty, signed, non-decimal, or overflowing `--lines` value exits 2
  with a diagnostic and writes no command data.
- B24. A line longer than 64 KiB in a line-oriented command exits 1 with a
  diagnostic.
- B25. `text head` or `text skip` must not emit or duplicate a newline that was
  absent from the input.
- B26. `text grep` must not treat the pattern as a regular expression or emit
  partial matching lines.
- B27. `text lower` must not fold non-ASCII bytes or alter binary data outside
  `A`-`Z`.
