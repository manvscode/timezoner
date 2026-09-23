#!/usr/bin/env python3
"""Regression checks for invalid timezone configuration entries."""

import pathlib
import subprocess
import tempfile
import unittest


BINARY = pathlib.Path(__file__).resolve().parents[1] / "bin" / "timezoner"


class InvalidTimezoneTests(unittest.TestCase):
    def run_with_config(self, content, *options):
        with tempfile.TemporaryDirectory() as directory:
            config = pathlib.Path(directory) / "contacts.cfg"
            config.write_text(content, encoding="utf-8")
            return subprocess.run(
                [str(BINARY), "-m", *options, "-f", str(config)],
                capture_output=True,
                text=True,
                check=False,
            )

    def test_invalid_zone_fails_without_crashing_in_both_groupings(self):
        content = (
            'America/New_York "valid@example.com" "Valid" "n/a" "n/a"\n'
            'No/Such_Zone "invalid@example.com" "Invalid" "n/a" "n/a"\n'
        )
        for options in ((), ("-U",)):
            with self.subTest(options=options):
                result = self.run_with_config(content, *options)
                self.assertEqual(result.returncode, 1)
                self.assertIn("Invalid timezone 'No/Such_Zone' on line 2", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_valid_zone_still_displays(self):
        result = self.run_with_config(
            'America/New_York "valid@example.com" "Valid" "n/a" "n/a"\n'
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Valid", result.stdout)


if __name__ == "__main__":
    unittest.main()
