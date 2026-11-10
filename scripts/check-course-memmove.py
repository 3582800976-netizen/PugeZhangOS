#!/usr/bin/env python3
"""Compile and check each course memmove directly against snapshot expectations."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--labs', nargs='+', type=int, choices=range(2, 7),
                    default=list(range(2, 7)))
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
compiler = shlex.split(os.environ.get('CC', 'cc'))
failures = 0
with tempfile.TemporaryDirectory(prefix='course-memmove-') as tmp:
    for lab in args.labs:
        course = repo / f'lab{lab}/course'
        obj = Path(tmp) / f'lab{lab}-utils.o'
        program = Path(tmp) / f'lab{lab}-memmove'
        flags = ['-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
                 '-ffreestanding', '-fno-builtin']
        print(f'Lab {lab}: checking actual lib/utils.c', flush=True)
        # Rename the exported symbols only; compile the real source unchanged.
        subprocess.run(compiler + flags + ['-I', str(course),
                       '-Dmemmove=course_memmove', '-Dmemset=course_memset',
                       '-Dstrncmp=course_strncmp', '-c',
                       str(course / 'src/kernel/lib/utils.c'), '-o', str(obj)], check=True)
        subprocess.run(compiler + flags + [str(repo / 'scripts/tests/memmove_host.c'),
                       str(obj), '-o', str(program)], check=True)
        result = subprocess.run([str(program)], timeout=20)
        if result.returncode:
            failures += 1
print(f'course memmove summary: failures={failures}', flush=True)
raise SystemExit(bool(failures))
