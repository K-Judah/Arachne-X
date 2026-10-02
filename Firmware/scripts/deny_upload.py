"""This phase supports compilation only. Do not open a serial port or flash."""
Import("env")  # noqa: F821 - provided by PlatformIO/SCons

from SCons.Script import COMMAND_LINE_TARGETS

if any("upload" in target.lower() or target.lower() == "program"
       for target in COMMAND_LINE_TARGETS):
    raise RuntimeError("Arachne-X foundation: hardware upload is intentionally disabled")
