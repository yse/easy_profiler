import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_coverage  # noqa: E402


class CoverageGateTests(unittest.TestCase):
    def test_lcov_uses_exact_executable_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "coverage.lcov"
            report.write_text(
                "\n".join(
                    (
                        "SF:/checkout/easy_profiler/easy_profiler_core/example.cpp",
                        "DA:10,0",
                        "DA:12,7",
                        "DA:13,3,checksum",
                        "end_of_record",
                    )
                ),
                encoding="utf-8",
            )

            counts = check_coverage.read_lcov(report)
            covered, coverable = check_coverage.changed_line_coverage(
                counts,
                {"easy_profiler_core/example.cpp": {10, 11, 12, 13, 99}},
            )

        self.assertEqual((covered, coverable), (2, 3))

    def test_lcov_merges_duplicate_records_without_losing_coverage(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "coverage.lcov"
            report.write_text(
                "\n".join(
                    (
                        "SF:/checkout/easy_profiler/easy_profiler_core/example.cpp",
                        "DA:20,0",
                        "end_of_record",
                        "SF:/checkout/easy_profiler/easy_profiler_core/example.cpp",
                        "DA:20,4",
                        "end_of_record",
                    )
                ),
                encoding="utf-8",
            )

            counts = check_coverage.read_lcov(report)

        self.assertEqual(counts["easy_profiler_core/example.cpp"][20], 4)

    def test_path_normalization_does_not_depend_on_checkout_directory(self):
        self.assertEqual(
            check_coverage.normalized("/work/fork/easy_profiler_core/reader.cpp"),
            "easy_profiler_core/reader.cpp",
        )


if __name__ == "__main__":
    unittest.main()
