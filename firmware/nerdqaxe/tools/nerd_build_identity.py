"""Exact public build identities; source profiles are not PCB qualification."""
from dataclasses import dataclass


@dataclass(frozen=True)
class BuildIdentity:
    board: str
    model: str
    asic_count: int
    asset_model: str
    family: str
    tag_prefix: str
    version_file: str
    web_version_file: str
    board_source: str
    build_helper: str
    package_helper: str
    build_directory: str

    @property
    def app_name(self) -> str:
        return f"esp-miner-{self.asset_model}.bin"


QA = BuildIdentity("NERDQAXEPLUS2", "NerdQAxe++", 4, "NerdQAxe++", "qa", "qaxe-v",
                   "version.txt", "main/http_server/axe-os/src/app/firmware-web-version.ts",
                   "main/boards/nerdqaxeplus2.cpp", "tools/build_5tratumfw_qa.sh",
                   "tools/package_5tratumfw_qa.py", "build/qa-public")
OCT = BuildIdentity("NERDOCTAXEGAMMA", "NerdOCTAXE-γ", 8, "NerdOCTAXE-Gamma", "oct", "octaxe-v",
                    "version-oct.txt", "main/http_server/axe-os/src/app/firmware-web-version.oct.ts",
                    "main/boards/nerdoctaxegamma.cpp", "tools/build_5tratumfw_oct.sh",
                    "tools/package_5tratumfw_oct.py", "build/oct-public")


def checked_identity(identity: BuildIdentity) -> BuildIdentity:
    if identity not in (QA, OCT):
        raise ValueError("Unknown or modified public Nerd build identity")
    return identity
