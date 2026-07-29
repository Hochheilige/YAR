import os
import sys

# ANSI SGR codes. These are blanked out below when the terminal cannot render
# them, so call sites can interpolate them unconditionally.
RESET = "\033[0m"
BOLD = "\033[1m"
RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
CYAN = "\033[36m"
GREY = "\033[90m"


def _enable_windows_vt():
    # cmd.exe and conhost print escape sequences literally until
    # ENABLE_VIRTUAL_TERMINAL_PROCESSING (0x4) is set on the output handle.
    # Windows Terminal enables it already; setting it again is harmless.
    # Fails when stdout is a pipe or a file, which is not a console at all.
    import ctypes

    kernel32 = ctypes.windll.kernel32
    handle = kernel32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
    mode = ctypes.c_uint32()
    if not kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
        return False
    return bool(kernel32.SetConsoleMode(handle, mode.value | 0x4))


def _supports_color():
    # Honour the de-facto standard opt-out, and stay plain when the output is
    # redirected to a file or piped into another tool.
    if os.environ.get("NO_COLOR"):
        return False

    forced = bool(os.environ.get("FORCE_COLOR"))
    if not forced and not sys.stdout.isatty():
        return False

    if sys.platform == "win32" and not _enable_windows_vt():
        return forced

    return True


COLOR_ENABLED = _supports_color()

# tqdm draws its own bar, so it takes a colour name rather than an escape code.
BAR_COLOR = "cyan" if COLOR_ENABLED else None

if not COLOR_ENABLED:
    RESET = BOLD = RED = GREEN = YELLOW = CYAN = GREY = ""


def step(message):
    """A stage is starting."""
    print(f"{CYAN}{BOLD}==>{RESET} {message}")


def success(message):
    print(f"{GREEN}  ok{RESET} {message}")


def warn(message):
    print(f"{YELLOW} warn{RESET} {message}")


def error(message):
    print(f"{RED}{BOLD}fail{RESET} {RED}{message}{RESET}")


def detail(message):
    """Secondary output: command logs, paths, skip notices."""
    print(f"{GREY}{message}{RESET}")
