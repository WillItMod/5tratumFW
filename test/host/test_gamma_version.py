#!/usr/bin/env python3
"""Run the production WWW version generator without changing built assets."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class GammaVersionTests(unittest.TestCase):
    def test_current_application_marker_and_generated_web_pair_match(self):
        version = (ROOT / "version.txt").read_text().strip()
        self.assertEqual(version, "5tratumFW-0.1.0-beta.8")
        self.assertLessEqual(len(version.encode()), 31)
        with tempfile.TemporaryDirectory(prefix="gamma-version-") as temporary:
            directory = Path(temporary)
            web = directory / "main/http_server/axe-os"
            dist = web / "dist/axe-os"
            dist.mkdir(parents=True)
            shutil.copy2(ROOT / "version.txt", directory / "version.txt")
            shutil.copy2(ROOT / "main/http_server/axe-os/generate-version.js", web / "generate-version.js")
            subprocess.run(["node", "generate-version.js"], cwd=web, check=True, capture_output=True)
            self.assertEqual((dist / "version.txt").read_text(), version)
            provenance = json.loads((dist / "build-info.json").read_text())
            self.assertEqual(provenance["product"], "5tratumFW")
            self.assertEqual(provenance["version"], version)
            self.assertEqual(provenance["nodeVersion"], "24.14.0")
            self.assertEqual(provenance["upstream"], "v2.14.2")


if __name__ == "__main__":
    unittest.main(verbosity=2)
