#!/usr/bin/env python3
"""Read-only, bounded VT subset for offline SeaBIOS/Syslinux serial captures.

This is a screen reconstruction aid, not a terminal or an input transmitter.
Command extraction is deliberately conservative: unknown cells, controls,
horizontal truncation markers, or a missing visible termination prevent a
complete result. The explicit Syslinux .linux editor can end on a short row
when its cursor is at command start/end; ordinary prompts require a blank row.
Even a complete result describes visible text, not the hidden
contents or execution of the bootloader's editor buffer.
"""

import argparse
import json
import re
import sys
import unicodedata
from pathlib import Path


class Screen:
    def __init__(self, columns=80, rows=25):
        if not 1 <= columns <= 512 or not 1 <= rows <= 256:
            raise ValueError("dimensions must be columns=1..512, rows=1..256")
        self.columns, self.rows = columns, rows
        self.cells = [[" "] * columns for _ in range(rows)]
        self.known = [[False] * columns for _ in range(rows)]
        self.written = [[False] * columns for _ in range(rows)]
        self.row = self.column = 0
        # A capture can start mid-screen. Only an explicit CUP/HVP or reset
        # establishes both cursor coordinates; ED2 alone does not.
        self.cursor_known = False
        self.saved = None
        self.wrap_pending = False
        self.autowrap = True
        self.warnings = []
        self.uncertain = False

    def warn(self, message):
        self.warnings.append(message)
        self.uncertain = True

    def erase(self, row, start=0, end=None):
        end = self.columns if end is None else end
        self.cells[row][start:end] = [" "] * (end - start)
        self.known[row][start:end] = [True] * (end - start)
        self.written[row][start:end] = [False] * (end - start)

    def index(self):
        self.wrap_pending = False
        if self.row < self.rows - 1:
            self.row += 1
        else:
            self.cells.pop(0)
            self.known.pop(0)
            self.written.pop(0)
            self.cells.append([" "] * self.columns)
            self.known.append([True] * self.columns)
            self.written.append([False] * self.columns)

    def put(self, char):
        if char == "\ufffd":
            self.warn("replacement character in current screen; possible invalid UTF-8")
        if unicodedata.combining(char) or unicodedata.east_asian_width(char) in "WF":
            self.warn("unsupported combining/wide character " + repr(char))
            return
        if self.wrap_pending:
            self.column = 0
            self.index()
        self.cells[self.row][self.column] = char
        self.known[self.row][self.column] = self.cursor_known
        self.written[self.row][self.column] = True
        if self.column == self.columns - 1:
            self.wrap_pending = self.autowrap
        else:
            self.column += 1

    def csi(self, parameters, final, offset):
        label = f"CSI {parameters}{final} at byte/character {offset}"
        if final == "m" and re.fullmatch(r"[0-9;:]*", parameters):
            return  # Colours do not affect the text grid.
        if parameters == "?25" and final in "hl":
            return  # Cursor visibility only.
        if parameters == "?7" and final in "hl":
            self.autowrap = final == "h"
            self.wrap_pending = False
            return
        if not re.fullmatch(r"[0-9;]*", parameters):
            self.warn("unhandled " + label)
            self.cursor_known = False
            return
        values = [int(x) if x else 0 for x in parameters.split(";")]
        count = values[0] or 1
        self.wrap_pending = False
        if final in "Hf":
            self.row = min((values[0] or 1) - 1, self.rows - 1)
            self.column = min((values[1] if len(values) > 1 else 1) or 1,
                              self.columns) - 1
            self.cursor_known = True
        elif final in "ABCD":
            delta_row = count if final == "B" else -count if final == "A" else 0
            delta_col = count if final == "C" else -count if final == "D" else 0
            self.row = max(0, min(self.rows - 1, self.row + delta_row))
            self.column = max(0, min(self.columns - 1, self.column + delta_col))
        elif final in "G`":
            self.column = min(count - 1, self.columns - 1)
        elif final == "d":
            self.row = min(count - 1, self.rows - 1)
        elif final in "EF":
            self.row = max(0, min(self.rows - 1,
                                  self.row + (count if final == "E" else -count)))
            self.column = 0
        elif final in "JK" and values[0] in (0, 1, 2):
            mode = values[0]
            if final == "K":
                self.erase(self.row, self.column if mode == 0 else 0,
                           self.column + 1 if mode == 1 else self.columns)
            elif mode == 2:
                for row in range(self.rows):
                    self.erase(row)
                # Prior unsupported content no longer affects a cleared screen;
                # an unknown cursor still makes subsequent writes uncertain.
                self.uncertain = False
            elif mode == 0:
                self.erase(self.row, self.column)
                for row in range(self.row + 1, self.rows):
                    self.erase(row)
            else:
                for row in range(self.row):
                    self.erase(row)
                self.erase(self.row, 0, self.column + 1)
        elif final == "X":
            self.erase(self.row, self.column, min(self.columns, self.column + count))
        elif final == "s" and parameters == "":
            self.saved = (self.row, self.column, self.cursor_known)
        elif final == "u" and parameters == "" and self.saved is not None:
            self.row, self.column, self.cursor_known = self.saved
        else:
            self.warn("unhandled " + label)
            self.cursor_known = False

    def feed(self, text):
        position = 0
        while position < len(text):
            char = text[position]
            if char == "\x1b":
                start = position
                if position + 1 >= len(text):
                    self.warn(f"truncated ESC at {start}")
                    break
                next_char = text[position + 1]
                if next_char == "[":
                    position += 2
                    begin = position
                    while position < len(text) and not "@" <= text[position] <= "~":
                        position += 1
                    if position == len(text):
                        self.warn(f"truncated CSI at {start}")
                        break
                    self.csi(text[begin:position], text[position], start)
                elif next_char in "78":
                    if next_char == "7":
                        self.saved = (self.row, self.column, self.cursor_known)
                    elif self.saved is not None:
                        self.row, self.column, self.cursor_known = self.saved
                    else:
                        self.warn(f"restore without saved cursor at {start}")
                    position += 1
                elif next_char == "(" and text[position + 2:position + 3] == "B":
                    position += 2  # ASCII character-set designation.
                elif next_char == "c":
                    # RIS resets the terminal, unlike ED2, including the cursor.
                    warnings = self.warnings
                    self.__init__(self.columns, self.rows)
                    self.warnings = warnings
                    self.cursor_known = True
                    for row in range(self.rows):
                        self.erase(row)
                    position += 1
                elif next_char in "]P^_":
                    end = re.search(r"\x07|\x1b\\", text[position + 2:])
                    self.warn(f"unhandled ESC {next_char!r} string at {start}")
                    if end is None:
                        self.warn(f"truncated control string at {start}")
                        break
                    position += 1 + end.end()
                else:
                    self.warn(f"unhandled ESC {next_char!r} at {start}")
                    self.cursor_known = False
                    position += 1
            elif char == "\r":
                self.column = 0
                self.wrap_pending = False
            elif char in "\n\v\f":
                self.index()
            elif char == "\b":
                self.column = max(0, self.column - 1)
                self.wrap_pending = False
            elif char == "\t":
                self.column = min(self.columns - 1, (self.column // 8 + 1) * 8)
                self.wrap_pending = False
            elif char in "\x00\x07":
                pass  # NUL padding and bell have no screen content.
            elif ord(char) < 32 or ord(char) == 127:
                self.warn(f"unhandled control {ord(char):02x} at {position}")
            else:
                self.put(char)
            position += 1

    def lines(self):
        return ["".join(line) for line in self.cells]

    def written_extent(self, row):
        """Do not mistake a space actually printed at column 80 for padding."""
        return max((column + 1 for column, written in enumerate(self.written[row])
                    if written), default=0)

    def command(self):
        """Join a visible Syslinux kernel editor command, or refuse."""
        lines = self.lines()
        matches = [(row, re.match(r"^\s*>\s*((?:\.linux[ \t]+)?/pmagic/bzImage\b.*)", line))
                   for row, line in enumerate(lines)]
        matches = [(row, match) for row, match in matches if match]
        result = {"complete": False, "text": None, "reason": "no unique kernel prompt"}
        if len(matches) != 1:
            return result
        row, match = matches[0]
        pieces = [match.group(1)]
        explicit_linux = match.group(1).startswith(".linux ")
        short_end = explicit_linux and self.written_extent(row) < self.columns
        end = row + 1
        while not short_end and end < self.rows and lines[end].strip():
            pieces.append(lines[end])
            short_end = explicit_linux and self.written_extent(end) < self.columns
            end += 1
        result["candidate"] = "".join(pieces).rstrip()
        last = end - 1
        cursor_bounds_short_row = self.cursor_known and (
            (self.row, self.column) == (row, match.start(1)) or
            (self.row, self.column) == (last, self.written_extent(last)))
        if self.uncertain:
            result["reason"] = "unhandled or truncated controls affect current screen"
        elif short_end and not cursor_bounds_short_row:
            result["reason"] = "short .linux row lacks a command-start/end cursor boundary"
        elif not short_end and end == self.rows:
            result["reason"] = "command reaches bottom edge without a blank terminator"
        elif not all(all(self.known[n]) for n in range(row, end + (not short_end))):
            result["reason"] = "command or terminating row contains uncaptured cells"
        elif any("<" in line or ">" in line for line in pieces):
            result["reason"] = "possible editor truncation marker in command"
        elif len(pieces) > 1 and self.written_extent(row) < self.columns:
            result["reason"] = "ambiguous short first row before continuation"
        elif any(self.written_extent(n) < self.columns for n in range(row + 1, last)):
            result["reason"] = "ambiguous non-full intermediate continuation row"
        else:
            result.update(complete=True, text=result.pop("candidate"),
                          reason=("visible .linux command ends on a known short row with cursor boundary"
                                  if short_end else
                                  "visible command bounded by a known blank row"))
        return result


def render(data, columns=80, rows=25, start_last_clear=False):
    screen = Screen(columns, rows)
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        text = data.decode("utf-8", errors="replace")
        screen.warn("invalid UTF-8 bytes replaced; inspect original capture")
    if start_last_clear:
        clears = list(re.finditer(r"\x1b(?:\[0*2J|c)", text))
        if clears:
            # Preserve cursor from prefix: ED2 clears text but does not home it.
            cutoff = clears[-1].start()
            screen.feed(text[:cutoff])
            screen.feed(text[cutoff:])
        else:
            screen.warn("--start-last-clear requested but no full clear was captured")
            screen.feed(text)
    else:
        screen.feed(text)
    return screen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--columns", type=int, default=80)
    parser.add_argument("--rows", type=int, default=25)
    parser.add_argument("--start-last-clear", action="store_true")
    parser.add_argument("--join-command", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    try:
        screen = render(args.capture.read_bytes(), args.columns, args.rows,
                        args.start_last_clear)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    command = screen.command()
    if args.json:
        print(json.dumps({"lines": screen.lines(),
                          "cursor": {"row": screen.row + 1, "column": screen.column + 1,
                                     "known": screen.cursor_known},
                          "warnings": screen.warnings, "command": command}, indent=2))
    else:
        for number, line in enumerate(screen.lines(), 1):
            print(f"{number:02d}|{line}|")
        print(f"cursor: {screen.row + 1}:{screen.column + 1}; known={screen.cursor_known}")
        if args.join_command:
            print("COMMAND: " + command["text"] if command["complete"] else
                  "COMMAND REFUSED: " + command["reason"])
        for warning in screen.warnings:
            print("warning: " + warning, file=sys.stderr)
    return 2 if args.join_command and not command["complete"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
