"""Integration checks using only Python's standard library and Linux utilities."""
import os
from pathlib import Path
import re
import select
import signal
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
SHELL = os.environ.get("LABC_SHELL", str(ROOT / "myshell"))


def run(commands, cwd=ROOT):
    return subprocess.run([SHELL], input=commands, text=True, cwd=cwd,
                          capture_output=True, timeout=8)


class LabTests(unittest.TestCase):
    def test_standalone_pipeline(self):
        result = subprocess.run([ROOT / "mypipeline"], cwd=ROOT,
                                capture_output=True, text=True, timeout=5)
        listing = subprocess.check_output(["ls", "-l"], cwd=ROOT, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "\n".join(listing.splitlines()[-2:]) + "\n")
        self.assertIn("parent_process>exiting", result.stderr)

    def test_path_quotes_pipeline_and_tracking(self):
        result = run('printf "%s\\n" "hello world"|wc -l\nprocs\nprocs\nquit\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertEqual(result.stdout.splitlines()[0].strip(), "1")
        self.assertEqual(result.stdout.count("Terminated"), 2)
        self.assertIn("hello world", result.stdout)
        self.assertIn("wc -l", result.stdout)

    def test_redirection_and_invalid_pipeline(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "in.txt").write_text("one\ntwo\nthree\n")
            result = run('cat<in.txt|tail -n 2>"out file"\n'
                         'printf hello>single\ncat<single\n'
                         'touch forbidden>bad|cat\n'
                         'touch forbidden2|cat<in.txt\n'
                         'touch forbidden3|cat|cat\nquit\n', path)
            self.assertEqual((path / "out file").read_text(), "two\nthree\n")
            self.assertEqual((path / "single").read_text(), "hello")
            self.assertIn("hello", result.stdout)
            for name in ("forbidden", "forbidden2", "forbidden3", "bad"):
                self.assertFalse((path / name).exists())
            self.assertEqual(result.stderr.count("Invalid"), 3)

    def test_large_pipeline_and_foreground_wait(self):
        start = time.monotonic()
        result = run("/bin/sleep 0.2\nseq 1 100000|wc -l\nquit\n")
        self.assertGreaterEqual(time.monotonic() - start, 0.18)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stderr, "")
        self.assertEqual(result.stdout.strip(), "100000")

    def test_history(self):
        result = run('!!\n!1\nprintf original\nhistory\n!!\n!1\n!0\n!21\n!1junk\nquit\n')
        self.assertEqual(result.stdout.count("No such command in history."), 5)
        self.assertEqual(result.stderr, "")
        self.assertIn("1 printf original", result.stdout)
        self.assertEqual(result.stdout.count("original"), 6)
        result = run("".join(f"echo item{i}\n" for i in range(1, 26)) + "history\n!1\nquit\n")
        self.assertIn("1 echo item6\n", result.stdout)
        self.assertIn("20 echo item25\n", result.stdout)
        self.assertTrue(result.stdout.endswith("echo item6\nitem6\n"))

    def test_history_pipeline_and_cd(self):
        with tempfile.TemporaryDirectory() as directory:
            result = run(f'cd {directory}\n!!\npwd\nprintf abc|wc -c>count\n!!\nquit\n')
            self.assertEqual(result.stderr, "")
            self.assertIn(directory, result.stdout)
            self.assertEqual((Path(directory) / "count").read_text().strip(), "3")

    def test_invalid_input_and_exec_failure(self):
        result = run(' \n|\necho "unterminated\necho >\necho & extra\n'
                     'blast 0\nalarm -1\nsleep abc\nblast 9999999999999999999999\n'
                     'not_a_real_labc_command\necho survived\n' + 'x' * 250 + '\necho final\nquit\n')
        self.assertEqual(result.returncode, 0)
        self.assertIn("survived\n", result.stdout)
        self.assertIn("final\n", result.stdout)
        self.assertIn("Command too long", result.stderr)
        self.assertEqual(result.stderr.count("Usage:"), 4)
        self.assertIn("not_a_real_labc_command", result.stderr)

    def test_background_process_signals_and_pipeline(self):
        child = subprocess.Popen([SHELL, "-d"], cwd=ROOT, stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 start_new_session=True)
        output = b""

        def send(line):
            child.stdin.write((line + "\n").encode())
            child.stdin.flush()

        def until(fragment):
            nonlocal output
            deadline = time.monotonic() + 5
            while fragment not in output:
                remaining = deadline - time.monotonic()
                self.assertGreater(remaining, 0, output.decode(errors="replace"))
                ready, _, _ = select.select([child.stdout], [], [], remaining)
                self.assertTrue(ready, output.decode(errors="replace"))
                chunk = os.read(child.stdout.fileno(), 65536)
                self.assertTrue(chunk, output.decode(errors="replace"))
                output += chunk
            data, output = output, b""
            return data.decode()

        try:
            send("./looper&")
            data = until(b"Starting the program")
            pid = int(re.search(r"PID: (\d+) Executing: ./looper", data)[1])
            for stop, wake in (("sleep", "alarm"), ("suspend", "wake")):
                send(f"{stop} {pid}")
                until(b"Looper handling SIGTSTP")
                time.sleep(0.05)
                send("procs")
                send("echo marker")  # Fork flushes built-in output in batch mode.
                self.assertIn("Suspended", until(b"marker\n"))
                send(f"{wake} {pid}")
                until(b"Looper handling SIGCONT")
                time.sleep(0.05)
                send("procs")
                send("echo marker")
                self.assertRegex(until(b"marker\n"), rf"{pid}\s+Running")
            send(f"suspend {pid}")
            until(b"Looper handling SIGTSTP")
            time.sleep(0.05)
            send(f"blast {pid}")
            until(b"Looper handling SIGINT")
            time.sleep(0.05)
            send("procs")
            send("echo marker")
            self.assertRegex(until(b"marker\n"), rf"{pid}\s+Terminated")
            send("procs")
            send("echo marker")
            self.assertNotIn(str(pid), until(b"marker\n"))
            send("/bin/sleep 0.3|cat&")
            send("echo responsive")
            until(b"responsive\n")
            time.sleep(0.4)
            send("procs")
            send("echo marker")
            data = until(b"marker\n")
            self.assertRegex(data, r"Terminated\s+/bin/sleep 0.3")
            self.assertRegex(data, r"Terminated\s+cat")
            send("quit")
            child.wait(timeout=3)
            self.assertEqual(child.returncode, 0)
        finally:
            try:
                os.killpg(child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            child.wait(timeout=3)
            child.stdin.close()
            child.stdout.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
