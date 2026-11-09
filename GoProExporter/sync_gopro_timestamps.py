#!/usr/bin/env python3
"""Copy filesystem creation and modification times from GoPro .360 files to reframed MP4s.

Usage:
  py sync_gopro_timestamps.py "D:\\Videos\\Galtur2026\\MAX2\\Galtur" "D:\\Videos\\Galtur2026\\MAX2\\Galtur\\rf"
  py sync_gopro_timestamps.py "D:\\Videos\\Galtur2026\\MAX2\\Galtur" "D:\\Videos\\Galtur2026\\MAX2\\Galtur\\rf" --apply

Without --apply, displays the proposed changes without modifying files.
Requires: py -m pip install pywin32
"""
import argparse
from pathlib import Path
import sys

import win32con
import win32file

FILE_WRITE_ATTRIBUTES = 0x0100

def copy_windows_timestamps(source: Path, destination: Path) -> None:
    """Copy Windows creation and last-modified timestamps, leave last-access unchanged."""
    source_handle = win32file.CreateFile(
        str(source), win32con.GENERIC_READ,
        win32con.FILE_SHARE_READ | win32con.FILE_SHARE_WRITE | win32con.FILE_SHARE_DELETE,
        None, win32con.OPEN_EXISTING, win32con.FILE_ATTRIBUTE_NORMAL, None,
    )
    try:
        created, _accessed, modified = win32file.GetFileTime(source_handle)
    finally:
        win32file.CloseHandle(source_handle)

    dest_handle = win32file.CreateFile(
        str(destination), FILE_WRITE_ATTRIBUTES,
        win32con.FILE_SHARE_READ | win32con.FILE_SHARE_WRITE | win32con.FILE_SHARE_DELETE,
        None, win32con.OPEN_EXISTING, win32con.FILE_ATTRIBUTE_NORMAL, None,
    )
    try:
        win32file.SetFileTime(dest_handle, created, None, modified)
    finally:
        win32file.CloseHandle(dest_handle)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('source_dir', type=Path, help='Directory containing original .360 clips')
    parser.add_argument('output_dir', type=Path, help='Directory containing *_reframed.mp4 files')
    parser.add_argument('--apply', action='store_true', help='Actually update MP4 timestamps (default: dry run)')
    args = parser.parse_args()

    if not args.source_dir.is_dir() or not args.output_dir.is_dir():
        parser.error('Both source_dir and output_dir must be existing directories')

    sources = {p.name.casefold(): p for p in args.source_dir.iterdir() if p.is_file() and p.suffix.casefold() == '.360'}
    matched = missing = changed = errors = 0
    for mp4 in sorted(args.output_dir.iterdir(), key=lambda p: p.name.casefold()):
        if not mp4.is_file() or not mp4.name.casefold().endswith('_reframed.mp4'):
            continue
        base = mp4.name[:-len('_reframed.mp4')]
        original = sources.get((base + '.360').casefold())
        if original is None:
            print(f'MISSING original: {mp4.name} (expected {base}.360)')
            missing += 1
            continue
        matched += 1
        if not args.apply:
            print(f'WOULD UPDATE: {mp4.name}  <-  {original.name}')
            continue
        try:
            copy_windows_timestamps(original, mp4)
            print(f'UPDATED: {mp4.name}  <-  {original.name}')
            changed += 1
        except OSError as exc:
            print(f'ERROR updating {mp4.name}: {exc}', file=sys.stderr)
            errors += 1

    print(f'\nMatched: {matched}; missing originals: {missing}; updated: {changed}; errors: {errors}; mode: {"APPLY" if args.apply else "DRY RUN"}')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
