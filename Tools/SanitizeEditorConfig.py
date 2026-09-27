"""Remove machine-generated Editor credentials from tracked project config.

The Unreal Editor may append Android File Server settings, including a local
security token, to DefaultEngine.ini. Run this before staging project changes.
"""

import re
from pathlib import Path


project = Path(__file__).resolve().parents[1]
config = project / "Config" / "DefaultEngine.ini"
original = config.read_text(encoding="utf-8-sig")
sections = re.split(r"(?=^\[)", original, flags=re.MULTILINE)
cleaned = "".join(
    part for part in sections
    if not part.startswith("[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]")
)
cleaned, replacements = re.subn(
    r"^EditorStartupMap=.*$",
    "EditorStartupMap=/Game/SpacePatriot/Maps/L_KellenReachWalk.L_KellenReachWalk",
    cleaned,
    count=1,
    flags=re.MULTILINE,
)
if replacements != 1:
    raise RuntimeError("Expected exactly one EditorStartupMap setting")
if "SecurityToken" in cleaned:
    raise RuntimeError("A generated security token remains in project config")
cleaned = cleaned.rstrip() + "\n"
config.write_text(cleaned, encoding="utf-8", newline="\n")
print("Sanitized project config and set the walk map as Editor startup map")
