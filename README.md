# Unix Shell in C

A small Unix-style command-line shell written in C for a university systems programming project. It runs external programs, supports a handful of built-in commands, and keeps command history and aliases between sessions.

## Features

- **Run external programs** using `fork`, `execvp` and `wait`
- **Built-in commands:** `cd`, `getpath`, `setpath`, `history`, `alias`, `unalias`, `exit`
- **PATH handling:** view or change `PATH` for the session; the original `PATH` is restored when the shell exits
- **Home directory:** the shell starts in `$HOME`, and `cd` with no argument returns there
- **History:** the last 20 commands are kept, with `!!`, `!n` and `!-n` to re-run them
- **Persistent history:** saved to `~/.hist_list` on exit and reloaded on startup
- **Aliases:** up to 10 aliases, saved to `~/.aliases` and reloaded on startup
- **Alias and history chaining:** aliases and history invocations can expand into each other, with cycle detection to stop infinite loops
- **Error handling:** messages for invalid commands, bad history numbers, missing arguments and failed `cd`

## Build and run

The shell uses POSIX system calls (`fork`, `wait`, `execvp`), so it needs Linux, macOS, or a POSIX layer on Windows such as WSL or MSYS2 (use the plain MSYS2 MSYS terminal, not MINGW).

```
gcc shell.c -pedantic -Wall -o shell && ./shell
```

You will get a `shellOS>` prompt. Type `exit` or press Ctrl-D to quit.

## Usage examples

```
shellOS> ls -l
shellOS> cd /tmp
shellOS> getpath
shellOS> setpath /bin:/usr/bin
shellOS> alias ll ls -l
shellOS> ll
shellOS> history
shellOS> !!
shellOS> !3
shellOS> !-2
shellOS> unalias ll
shellOS> exit
```

## Limits and notes

- Commands are limited to 512 characters and 50 tokens
- History holds 20 commands and there is a maximum of 10 aliases
- Characters such as `|`, `<`, `>`, `&` and `;` are treated as separators when splitting input, so pipes, redirection, background jobs and command chaining are not supported

## Concepts covered

Process creation and synchronisation, command parsing and tokenising, environment variables, file I/O for persistent state, and modular C code.
