"""Regression checks for the Lab A encoder; run with make test."""
import itertools
from pathlib import Path
import subprocess
import tempfile
import unittest

ENCODER = str(Path(__file__).resolve().with_name('encoder'))


def run(*args, data=b''):
    return subprocess.run([ENCODER, *args], input=data, capture_output=True)


class EncoderTests(unittest.TestCase):
    def check_output(self, args, data, expected):
        result = run(*args, data=data)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, expected)

    def test_assignment_examples(self):
        self.check_output(['+e12345'], b'abcdez\n12#<\n', b'bdfhja\n46#<\n')
        self.check_output(['-e4321'], b'gduqp523\n', b'caspl202\n')

    def test_passthrough_and_empty_input(self):
        self.check_output([], bytes(range(256)), bytes(range(256)))
        self.check_output(['+e123'], b'', b'')

    def test_wrap_and_unchanged_characters(self):
        self.check_output(['+e12'], b'z9 A!\na0', b'a1 A!\nb2')
        self.check_output(['-e9'], b'a0z9Z!', b'r1q0Z!')

    def test_all_bytes_and_long_line(self):
        data = bytes(range(256)) * 1024
        key = [1, 9, 0, 2, 3]
        expected = bytearray()
        for i, c in enumerate(data):
            shift = key[i % len(key)]
            if 97 <= c <= 122:
                c = 97 + (c - 97 + shift) % 26
            elif 48 <= c <= 57:
                c = 48 + (c - 48 + shift) % 10
            expected.append(c)
        self.check_output(['+e19023'], data, bytes(expected))
        self.check_output(['-e19023'], bytes(expected), data)
        self.check_output(['+e1'], b'z' * 100000, b'a' * 100000)

    def test_debug_changes_apply_to_next_argument(self):
        result = run('-D', '+e1', '+D', '-e2')
        self.assertNotEqual(result.returncode, 0)  # Duplicate key, but trace is ordered.
        self.assertEqual(result.stderr.splitlines()[:2], [b'-D', b'-e2'])
        result = run('-D', '+D', '+e1', '-D', data=b'a')
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b'b')
        self.assertEqual(result.stderr, b'-D\n+e1\n-D\n')

    def test_files_and_option_order(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'input file'
            target = Path(directory) / 'output file'
            source.write_bytes(b'abcdez\n12#<\n')
            for args in itertools.permutations(['+e12345', '-I' + str(source),
                                                '-O' + str(target), '-D']):
                self.check_output(args, b'', b'')
                self.assertEqual(target.read_bytes(), b'bdfhja\n46#<\n')
            self.check_output(['-I' + str(source)], b'', source.read_bytes())
            self.check_output(['-O' + str(target)], b'copy\x00\xff', b'')
            self.assertEqual(target.read_bytes(), b'copy\x00\xff')
            result = run('-I' + str(source / 'missing'))
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, b'')
            result = run('-O' + str(Path(directory) / 'missing' / 'out'))
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, b'')

    def test_invalid_arguments(self):
        for args in [('+e',), ('-e',), ('+e1x',), ('-I',), ('-O',),
                     ('unknown',), ('',), ('-',), ('+',), ('-v',),
                     ('+e1', '-e2')]:
            result = run(*args, data=b'abc')
            self.assertNotEqual(result.returncode, 0, args)
            self.assertEqual(result.stdout, b'', args)
            self.assertTrue(result.stderr, args)

    @unittest.skipUnless(Path('/dev/full').exists(), 'requires Linux /dev/full')
    def test_write_failure(self):
        result = run('-O/dev/full', data=b'abc')
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(result.stderr)


if __name__ == '__main__':
    unittest.main()
