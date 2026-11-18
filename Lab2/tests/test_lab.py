import math
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LabTests(unittest.TestCase):
    def run_case(self, commands, expected_code, expected_values, filename=None):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "result with spaces.txt"
            target = str(output) if filename is None else filename
            process = subprocess.run(
                [str(ROOT / "parent")],
                input=target.encode() + b"\n" + commands,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                cwd=ROOT,
                timeout=10,
            )
            self.assertEqual(process.returncode, expected_code, process.stderr.decode())
            values = [float(x) for x in output.read_text().splitlines()] if output.exists() else []
            self.assertEqual(len(values), len(expected_values), process.stderr.decode())
            for actual, expected in zip(values, expected_values):
                self.assertTrue(math.isclose(actual, expected, rel_tol=2e-6, abs_tol=1e-40))
            match = re.search(rb"child PID=(\d+)", process.stderr)
            if match:
                with self.assertRaises(ProcessLookupError):
                    os.kill(int(match[1]), 0)
            return process

    def test_valid_commands(self):
        self.run_case(b"100 2 5\n7.5 2.5\n-12 -2 3\n0 2\n42\n", 0, [10, 3, 2, 0, 42])

    def test_whitespace_exponents(self):
        self.run_case(b" \t1e2\t2.5  4\r\n", 0, [10])

    def test_no_final_newline(self):
        self.run_case(b"9 2", 0, [4.5])

    def test_float_precision(self):
        self.run_case(b"16777217 1\n", 0, [16777216])

    def test_zero_stops_later_commands(self):
        self.run_case(b"100 2 5\n5 0\n50 2\n", 2, [10])

    def test_negative_zero(self):
        self.run_case(b"5 -0.0\n", 2, [])

    def test_zero_over_zero(self):
        self.run_case(b"0 0\n", 2, [])

    def test_invalid_commands(self):
        for command in [b"\n", b" \t\n", b"abc\n", b"2 3x\n", b"2 3,5\n",
                        b"nan 2\n", b"5 inf\n", b"1e100 2\n", b"1e-100 2\n",
                        b"2 1\x00 3\n", b"3e38 0.01\n"]:
            with self.subTest(command=command):
                self.run_case(command, 1, [])

    def test_previous_results_survive_error(self):
        self.run_case(b"20 2\nabc\n", 1, [10])

    def test_long_line(self):
        self.run_case(b"100 " + b"1 " * 150000 + b"2\n", 0, [50])

    def test_many_commands(self):
        self.run_case(b"10 2\n" * 2000, 0, [5] * 2000)

    def test_no_commands(self):
        self.run_case(b"", 0, [])

    def test_bad_output_path(self):
        self.run_case(b"10 2\n", 1, [], "/nonexistent-lab2-directory/result.txt")

    def test_existing_file_truncated(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "old.txt"
            output.write_text("old contents\n")
            p = subprocess.run([str(ROOT / "parent")], input=f"{output}\n8 2\n".encode(),
                               cwd=ROOT, capture_output=True, timeout=5)
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertEqual(output.read_text(), "4\n")

    def test_missing_child(self):
        with tempfile.TemporaryDirectory() as directory:
            p = subprocess.run([str(ROOT / "parent")], input=b"result.txt\n8 2\n",
                               cwd=directory, capture_output=True, timeout=5)
            self.assertEqual(p.returncode, 1)
            self.assertIn(b"execl", p.stderr)

    def test_missing_filename(self):
        for data in [b"", b"\n", b"a\x00b\n"]:
            with self.subTest(data=data):
                p = subprocess.run([str(ROOT / "parent")], input=data, cwd=ROOT,
                                   capture_output=True, timeout=5)
                self.assertEqual(p.returncode, 1)

    def test_zero_exits_without_more_input_or_eof(self):
        with tempfile.TemporaryDirectory() as directory:
            p = subprocess.Popen([str(ROOT / "parent")], cwd=ROOT, stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                p.stdin.write(f"{directory}/result.txt\n10 0\n".encode())
                p.stdin.flush()
                self.assertEqual(p.wait(timeout=5), 2)
            finally:
                if p.poll() is None:
                    p.kill()
                    p.wait()
                p.stdin.close()
                p.stdout.close()
                p.stderr.close()

    @unittest.skipUnless(Path("/dev/full").exists(), "/dev/full is available on Linux")
    def test_write_failure(self):
        self.run_case(b"10 2\n", 1, [], "/dev/full")


if __name__ == "__main__":
    unittest.main(verbosity=2)
