Import("env") # type: ignore
from pathlib import Path

pw_file = Path("ota_password.txt")
if not pw_file.exists():
    print("ERROR: ota_password.txt fehlt")
    Exit(1) # type: ignore

password = pw_file.read_text(encoding="utf-8").strip()
if not password:
    print("ERROR: ota_password.txt ist leer")
    Exit(1) # type: ignore

env.Append(UPLOAD_FLAGS=[f"-a{password}"]) # type: ignore


print(f"[get_ota_pass] OTA password loaded ({len(password)} chars)")
