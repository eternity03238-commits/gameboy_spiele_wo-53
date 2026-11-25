"""Update the ROM embedded in the browser page after a GBDK build."""

import base64
from pathlib import Path
import re


root = Path(__file__).resolve().parent.parent
rom = base64.b64encode((root / "ZEITREISE53.gb").read_bytes()).decode("ascii")
page = root / "index.html"
updated, matches = re.subn(
    r"(?m)^const ROM_B64 = ['\"][A-Za-z0-9+/=]+['\"];?$",
    f"const ROM_B64 = '{rom}';",
    page.read_text(encoding="utf-8"),
)
if matches != 1:
    raise SystemExit("Expected one embedded ROM in index.html.")
page.write_text(updated, encoding="utf-8")
