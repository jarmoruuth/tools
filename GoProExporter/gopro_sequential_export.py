"""Sequential, GUI-driven GoPro Player exports (Windows 11).

Prototype for the supplied GoPro Player screenshots. Opens each
.360 file using its Windows file association, maximizes GoPro Player,
exports, and closes GoPro Player before the next file. Keep display scaling and resolution consistent with the screenshot.

Version 6: optional lock toggles, progress-based export stall detection,
and a pause between fresh GoPro Player instances. --debug-clicks previews
positions without clicking or exporting.

Install: py -m pip install pyautogui pygetwindow pyperclip pywin32
Run:     py gopro_sequential_export.py "D:\\Videos\\Trip\\MAX2" "D:\\Videos\\Exports"

Close GoPro Player before starting this script. Ctrl+C stops between UI steps.
Move mouse to the top-left screen corner for PyAutoGUI emergency stop.
"""

import argparse
import os
import logging
from pathlib import Path
import time

import pyautogui as pg
import pygetwindow as gw
import pyperclip
import win32gui
import win32con

from sync_gopro_timestamps import copy_windows_timestamps

# Measured from user's new MAXIMIZED screenshot, sized 2048x1120.
# Convert to normalized window coordinates, rather than assuming a 1600x900
# app. See --debug-clicks to check them before processing any clip.
REFERENCE_SIZE = (2048, 1120)
POSITIONS = {
    "direction_lock": (2018, 825),
    "horizon_level": (2018, 855),
    "reframe": (1050, 74),
    # Export appears at top-right after selecting Reframe.
    "export": (2002, 87),
}

LOAD_DELAY = 10              # Extra delay after clip title appears
PLAYER_START_TIMEOUT = 90
PLAYER_CLOSE_TIMEOUT = 20
DIALOG_TIMEOUT = 25
DEFAULT_STALL_MINUTES = 15  # No MP4 growth for this long => stop (also covers no output file)
RESTART_PAUSE_SECONDS = 3
STABLE_SECONDS = 15
POLL_SECONDS = 2
# Measured from the center of the 621x253 Export Settings dialog image.
NEXT_FROM_DIALOG_CENTER = (248, 95)

pg.FAILSAFE = True
pg.PAUSE = 0.25
log = logging.getLogger("gopro")

PARTIAL_SUFFIX = ".partial.mp4"


def partial_path(destination):
    """Export target used while GoPro Player writes; renamed when complete."""
    return destination.with_name(destination.stem + PARTIAL_SUFFIX)


def player_windows():
    return [w for w in gw.getAllWindows()
            if "gopro player" in w.title.casefold()]


def player_window():
    matches = player_windows()
    if not matches:
        raise RuntimeError("GoPro Player window not found after opening the .360 file")
    return max(matches, key=lambda w: w.width * w.height)


def wait_for_player(source):
    deadline = time.monotonic() + PLAYER_START_TIMEOUT
    last_title = None
    while time.monotonic() < deadline:
        matches = player_windows()
        if matches:
            w = max(matches, key=lambda v: v.width * v.height)
            last_title = w.title
            if source.stem.casefold() in w.title.casefold():
                return w
        time.sleep(0.5)
    raise TimeoutError(
        f"GoPro Player did not show clip {source.name!r} within "
        f"{PLAYER_START_TIMEOUT}s; last window title: {last_title!r}"
    )


def activate_player():
    w = player_window()
    if w.isMinimized:
        w.restore()
        time.sleep(0.5)
    if not w.isMaximized:
        w.maximize()
        time.sleep(0.9)
    try:
        w.activate()
    except Exception:
        pass
    time.sleep(0.6)
    log.info("GoPro window: %r x=%d y=%d size=%dx%d; desktop=%s",
             w.title, w.left, w.top, w.width, w.height, pg.size())
    return w


def control_position(name):
    # Do not refocus/maximize Player while modal dialogs are displayed.
    w = player_window()
    if name == "next":
        # The Export Settings dialog is centered in the Player window.
        px = w.left + round(w.width / 2 + NEXT_FROM_DIALOG_CENTER[0])
        py = w.top + round(w.height / 2 + NEXT_FROM_DIALOG_CENTER[1])
    else:
        x, y = POSITIONS[name]
        px = w.left + round(x * w.width / REFERENCE_SIZE[0])
        py = w.top + round(y * w.height / REFERENCE_SIZE[1])
    return px, py


def click_control(name):
    px, py = control_position(name)
    log.info("Hover/click %s at (%d, %d)", name, px, py)
    pg.moveTo(px, py, duration=0.25)
    time.sleep(0.9)  # reveal video-overlay controls
    pg.click()
    time.sleep(0.65)


def debug_clicks():
    """Move mouse to positions, but never click or start export."""
    w = activate_player()
    for name in ("direction_lock", "horizon_level", "reframe", "export"):
        x, y = control_position(name)
        log.info("DEBUG %s -> (%d, %d)", name, x, y)
        print(f"Move mouse over {name}: ({x}, {y})")
        pg.moveTo(x, y, duration=0.4)
        time.sleep(2.0)
    print("Debug finished: no clicks made and no export started.")


def active_title():
    return win32gui.GetWindowText(win32gui.GetForegroundWindow())


def explorer_folder_windows():
    """Visible File Explorer folder windows: {hwnd: title}.

    Only CabinetWClass windows are folder windows; the taskbar and desktop
    also belong to explorer.exe but have other window classes.
    """
    found = {}

    def callback(hwnd, _):
        if (win32gui.IsWindowVisible(hwnd)
                and win32gui.GetClassName(hwnd) == "CabinetWClass"):
            found[hwnd] = win32gui.GetWindowText(hwnd)
        return True

    win32gui.EnumWindows(callback, None)
    return found


def find_export_explorer(output_dir, existing):
    """Return a newly opened Explorer window showing output_dir, or None."""
    folder = output_dir.name.casefold()
    full = str(output_dir.resolve()).casefold()
    for hwnd, title in explorer_folder_windows().items():
        if hwnd in existing:
            continue
        t = title.casefold()
        # Title is the folder name, or the full path if Explorer is set to
        # show it; Windows 11 may append " - File Explorer".
        if t == folder or t.startswith(folder + " - ") or t.startswith(full):
            return hwnd
    return None


def wait_dialog(contains, timeout=DIALOG_TIMEOUT):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if contains.casefold() in active_title().casefold():
            return
        time.sleep(0.3)
    raise TimeoutError(f"Timed out waiting for {contains!r} dialog; foreground: {active_title()!r}")


def set_filename_in_dialog(path):
    # Standard Windows Open/Save As common dialogs: Alt+N focuses File name.
    pg.hotkey("alt", "n")
    time.sleep(0.3)
    pg.hotkey("ctrl", "a")
    pyperclip.copy(str(path))
    pg.hotkey("ctrl", "v")
    time.sleep(0.3)
    pg.press("enter")


def open_clip(path):
    log.info("Opening using .360 file association: %s", path)
    if player_windows():
        raise RuntimeError(
            "GoPro Player is already open. Close it before running this script "
            "so each clip starts in a fresh Player instance."
        )
    os.startfile(str(path.resolve()))  # Windows ShellExecute via registered .360 association
    wait_for_player(path)
    activate_player()
    time.sleep(LOAD_DELAY)
    log.info("Loaded: %s", player_window().title)


def close_player():
    """Gracefully close the app; do not kill processes or dismiss save prompts."""
    matches = player_windows()
    if not matches:
        return
    log.info("Closing GoPro Player")
    for w in matches:
        try:
            win32gui.PostMessage(w._hWnd, win32con.WM_CLOSE, 0, 0)
        except Exception as exc:
            raise RuntimeError("Could not request GoPro Player to close") from exc
    deadline = time.monotonic() + PLAYER_CLOSE_TIMEOUT
    while time.monotonic() < deadline:
        if not player_windows():
            log.info("GoPro Player closed")
            return
        time.sleep(0.5)
    raise TimeoutError("GoPro Player did not close; check for a confirmation dialog")

def close_export_explorer(hwnd):
    """Close only the Explorer window GoPro Player opened for this export."""
    if hwnd and win32gui.IsWindow(hwnd):
        win32gui.PostMessage(hwnd, win32con.WM_CLOSE, 0, 0)
        time.sleep(0.8)


def dismiss_rating_popup():
    """Dismiss the occasional 'Enjoying GoPro Player?' in-app popup.

    Uses a pixel-exact crop from the provided screenshot; matching can fail
    if Windows display scaling or GoPro Player UI changes.
    """
    template = Path(__file__).with_name("gopro_rating_title.png")
    if not template.exists():
        return
    try:
        match = pg.locateOnScreen(str(template))
    except (pg.ImageNotFoundException, OSError):
        return
    if match:
        # Image crop starts at (12,9) in a 621x280 popup.
        # 'No' button is centered around (468,245) within the popup.
        pg.click(match.left - 12 + 468, match.top - 9 + 245)
        log.info("Dismissed GoPro Player rating popup")
        time.sleep(1)


def wait_for_finished_export(output, existing_explorers,
                             stall_minutes=DEFAULT_STALL_MINUTES):
    """Wait for Explorer after export plus a stable, nonempty MP4.

    GoPro Player opens an Explorer window on the output folder when the
    export completes; only a window that did not exist before the export
    (existing_explorers) and shows the output folder counts.

    No fixed maximum export duration. Instead, abort if the target MP4 has
    not appeared or increased in size for stall_minutes minutes. This is a
    heuristic: GoPro could be busy writing to a temporary file, so adjust
    --stall-minutes for unusual workloads.
    """
    stall_seconds = stall_minutes * 60
    last_progress = time.monotonic()
    explorer_hwnd = None
    last_size = 0
    stable_since = None
    while True:
        dismiss_rating_popup()
        if explorer_hwnd is None:
            explorer_hwnd = find_export_explorer(output.parent, existing_explorers)
            if explorer_hwnd:
                log.info("Export folder window opened")
        explorer_seen = explorer_hwnd is not None
        try:
            size = output.stat().st_size
        except FileNotFoundError:
            size = 0
        now = time.monotonic()
        if size > last_size:
            last_progress = now
            stable_since = None
        elif size > 0 and size == last_size:
            if stable_since is None:
                stable_since = now
        else:
            stable_since = None
        last_size = size
        if explorer_seen and stable_since is not None and now - stable_since >= STABLE_SECONDS:
            log.info("Export finished: %s (%d bytes)", output, size)
            close_export_explorer(explorer_hwnd)
            return
        # Don't mistake a completed export for a stall if Explorer appeared
        # slightly before the stability window ends.
        if now - last_progress >= stall_seconds:
            raise TimeoutError(
                f"No export-file growth for {stall_minutes:g} minutes: {output} "
                f"({size} bytes; Explorer seen: {explorer_seen}). "
                "Check GoPro Player before retrying; the partial file is "
                "deleted automatically on the next run."
            )
        time.sleep(POLL_SECONDS)


def export_clip(source, destination, args):
    # Export to a .partial.mp4 name and rename only after a verified finish,
    # so an interrupted export is never mistaken for a completed one.
    partial = partial_path(destination)
    if partial.exists():
        log.info("Deleting leftover partial export: %s", partial)
        partial.unlink()
    open_clip(source)
    # The user confirmed both locks start enabled when a new clip is opened.
    if not args.keep_direction_lock:
        click_control("direction_lock")
    else:
        log.info("Keeping Direction Lock enabled")
    if not args.keep_horizon_level:
        click_control("horizon_level")
    else:
        log.info("Keeping Horizon Level enabled")
    click_control("reframe")
    existing_explorers = set(explorer_folder_windows())
    click_control("export")
    click_control("next")
    wait_dialog("Save As")
    set_filename_in_dialog(partial)
    wait_for_finished_export(partial, existing_explorers,
                             stall_minutes=args.stall_minutes)
    close_player()
    partial.rename(destination)
    log.info("Saved %s", destination)
    try:
        copy_windows_timestamps(source, destination)
    except OSError as exc:
        log.warning("Could not copy timestamps to %s: %s "
                    "(fix later with sync_gopro_timestamps.py)", destination, exc)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_dir", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--debug-clicks", action="store_true", help="Open first clip and hover over control positions without clicking")
    parser.add_argument("--all", action="store_true",
                        help="Process every .360 file; default processes only first pending file for safety")
    parser.add_argument("--keep-direction-lock", action="store_true",
                        help="Leave Direction Lock enabled (skip its click)")
    parser.add_argument("--keep-horizon-level", action="store_true",
                        help="Leave Horizon Level enabled (skip its click)")
    parser.add_argument("--stall-minutes", type=float, default=DEFAULT_STALL_MINUTES,
                        help="Stop if the output MP4 does not appear/grow for this many minutes (default: 15)")
    parser.add_argument("--restart-pause", type=float, default=RESTART_PAUSE_SECONDS,
                        help="Seconds to pause after Player closes, before next clip (default: 3)")
    args = parser.parse_args()
    if args.stall_minutes <= 0:
        parser.error("--stall-minutes must be greater than 0")
    if args.restart_pause < 0:
        parser.error("--restart-pause cannot be negative")
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    if not args.input_dir.is_dir():
        parser.error("Input directory not found")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    sources = sorted(p for p in args.input_dir.iterdir() if p.is_file() and p.suffix.lower() == ".360")
    pending = [(p, args.output_dir / (p.stem + "_reframed.mp4")) for p in sources]
    pending = [(src, dst) for src, dst in pending if not dst.exists()]
    if not args.all:
        pending = pending[:1]
    if not pending:
        log.info("Nothing to export")
        return
    log.info("Found %d pending clip(s). Do not touch mouse/keyboard during export.", len(pending))
    if player_windows():
        parser.error("Close GoPro Player before starting; files will be opened with Windows file association")
    if args.debug_clicks:
        open_clip(pending[0][0])
        debug_clicks()
        return  # Leave the Player open for visual inspection.
    for i, (src, dst) in enumerate(pending, 1):
        log.info("[%d/%d] %s", i, len(pending), src.name)
        try:
            export_clip(src, dst, args)
            if i < len(pending) and args.restart_pause:
                log.info("Pause %.1fs before next clip", args.restart_pause)
                time.sleep(args.restart_pause)
        except Exception:
            screenshot = Path(__file__).with_name("gopro_export_error.png")
            try:
                pg.screenshot().save(screenshot)
                log.error("Screenshot at failure: %s", screenshot)
            except Exception:
                pass
            log.exception("Stopped on %s; inspect GoPro Player before retrying", src)
            raise
    log.info("Finished %d clip(s)", len(pending))


if __name__ == "__main__":
    main()
