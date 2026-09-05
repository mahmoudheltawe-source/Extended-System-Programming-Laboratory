#!/usr/bin/env python3
"""Exercise the actual executables against Python's arbitrary-precision ints."""

import os
from pathlib import Path
import random
import re
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]


def run(program="multi", args=(), data=None):
    return subprocess.run(
        [str(ROOT / program), *args],
        input=data,
        text=True,
        capture_output=True,
        timeout=5,
        env={**os.environ, "MALLOC_CHECK_": "3", "MALLOC_PERTURB_": "165"},
    )


class MultiTests(unittest.TestCase):
    def check_addition(self, left, right, ending="\n"):
        result = run(args=["-I"], data=left + ending + right + ending)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        lines = result.stdout.splitlines()
        self.assertEqual(len(lines), 3, result.stdout)
        self.assertTrue(all(re.fullmatch(r"[0-9a-f]+", line) for line in lines))
        self.assertEqual(
            [int(line, 16) for line in lines],
            [int(left, 16), int(right, 16), int(left, 16) + int(right, 16)],
        )

    def test_default_example(self):
        result = run()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertEqual(result.stdout, "4f440201aa\n4f44030201aa\n4f9347040354\n")

    def test_examples_and_carry_boundaries(self):
        for left, right in [
            ("2", "1"), ("f" * 71, "1"), ("0", "0"), ("0", "1"),
            ("ff", "1"), ("ffff", "ffff"), ("10000", "ff"),
            ("ffffff", "ff01"), ("12abcdef", "FE"),
            ("00000000", "0000123"), ("f" * 510, "1"),
            ("f" * 512, "f" * 512), ("f" * 598, "1"),
            ("f" * 599, "f" * 599),
        ]:
            for a, b in [(left, right), (right, left)]:
                with self.subTest(left_length=len(a), right_length=len(b)):
                    self.check_addition(a, b)

    def test_every_permitted_input_length(self):
        generator = random.Random(0xACE1)
        for length in range(1, 600):
            other_length = generator.randrange(1, 600)
            left = "a" + "".join(generator.choices("0123456789abcdef", k=length - 1))
            right = "1" + "".join(generator.choices("0123456789ABCDEF", k=other_length - 1))
            with self.subTest(length=length, other_length=other_length):
                self.check_addition(left, right)

    def test_line_endings(self):
        self.check_addition("aBcD", "F", "\r\n")
        self.check_addition("f" * 599, "1", "\r\n")
        for data, expected in [("abc\nf", 0xACB), ("1\n" + "f" * 599, 16**599)]:
            result = run(args=["-I"], data=data)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(int(result.stdout.splitlines()[-1], 16), expected)

    def test_invalid_input(self):
        for data in [
            "", "1\n", "\n1\n", "1\n\n", "xyz\n1\n", "1\ng\n",
            "-1\n2\n", "0x10\n1\n", "12 34\n1\n", "1\n2!\n",
            "f" * 600 + "\n1\n", "1\n" + "f" * 600 + "\n",
            "f" * 1000 + "\n1\n", "\r\n1\n",
        ]:
            with self.subTest(data=data[:30]):
                result = run(args=["-I"], data=data)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")
                self.assertIn("multi:", result.stderr)

    def test_invalid_arguments(self):
        for args in [[""], ["-"], ["-i"], ["-r"], ["-X"], ["-II"], ["-RR"], ["-I", "-R"]]:
            with self.subTest(args=args):
                result = run(args=args, data="")
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)

    def test_random_mode(self):
        result = run(args=["-R"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        lines = result.stdout.splitlines()
        self.assertEqual(len(lines), 3)
        self.assertTrue(all(re.fullmatch(r"[0-9a-f]+", line) for line in lines))
        self.assertTrue(all(2 <= len(line) <= 510 for line in lines[:2]))
        left, right, total = map(lambda line: int(line, 16), lines)
        self.assertEqual(total, left + right)
        self.assertEqual(run(args=["-R"]).stdout, result.stdout)

    def test_part0(self):
        for args in [[], ["one", "two"], ["", "a b", "-I"]]:
            result = run(program="part0", args=args)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, "")
            self.assertEqual(result.stdout.splitlines(), [str(len(args) + 1), str(ROOT / "part0"), *args])


if __name__ == "__main__":
    unittest.main(verbosity=2)
