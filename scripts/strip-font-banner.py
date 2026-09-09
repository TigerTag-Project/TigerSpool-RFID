#!/usr/bin/env python3
"""Reduce absolute paths in a generated font's banner to bare file names.

lv_font_conv writes the full path of whatever it was handed into the comment at
the top of the file. That path differs between machines, so a face regenerated
on another bench would differ from the committed one in its banner alone - and
check-generated.py would call it out of date every time.
"""
import pathlib, re, sys

p = pathlib.Path(sys.argv[1])
s = p.read_text()
s = re.sub(r"--font \S*/([^/\s]+)", r"--font \1", s)
s = re.sub(r"-o \S*/([^/\s]+)", r"-o \1", s)
p.write_text(s)
