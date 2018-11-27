# CS570 Shell

This project was commissioned in my CS570 (Operating Systems) class.

## Purpose

The application mimics the behavior of a real shell (bash, tcsh, etc.) along
with other things added to test our knowledge of Operating Systems procedures
and the C programming language.

### What it does
- Pipes
- Running programs via PATH, `cwd`, or absolute
- Input/Output redirection using `<`, `>`
- Environment variable manipulation
   - `environ NAME value` sets the value of environment variable `$NAME`
   - `environ NAME` prints the value of environment variable `$NAME`
- String/command parsing
- `fork()`, `exec()`
- Changing directory via `cd`
- Background processes via `&`

### What doesn't work
**Hereis:** The hereis (`<<`) function does not work as intended when using the shell as one might in bash/tcsh. Some refactoring/moving code around, or a re-write of certain portions may fix this problem. For the purposes of the class, I felt it unnnecessary.
