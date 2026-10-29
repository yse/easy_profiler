"""Tests for scripts/aqt_windows_workaround.py, the Windows Qt resolver.

The resolver walks back from the newest Qt 6 release to the newest whose
package index aqtinstall can read, and must stop rather than walk past a
network failure, because aqt reports both in one breath unless hash checking
is off. The offline tests run it against tests/support/fake_aqt, which answers
with aqtinstall's own messages. With EASY_PROFILER_ONLINE_TESTS=1 and
aqtinstall importable, it also runs against the real aqt and the real servers,
with the TLS failure injected by tests/support/aqt_fault.
Run with: python3 -m unittest tests/test_aqt_windows_workaround.py
"""

import importlib.util
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
RESOLVER = HERE.parent / "scripts" / "aqt_windows_workaround.py"
FAKE_AQT = HERE / "support" / "fake_aqt"
FAULT_INJECTOR = HERE / "support" / "aqt_fault"


def resolve(**variables):
    environment = dict(os.environ, PYTHONPATH=str(FAKE_AQT), **variables)
    return subprocess.run([sys.executable, str(RESOLVER)], env=environment,
                          capture_output=True, text=True)


class FakeAqtTest(unittest.TestCase):
    def setUp(self):
        handle = tempfile.NamedTemporaryFile("w", suffix=".jsonl", delete=False)
        handle.close()
        self.log = handle.name
        self.addCleanup(os.unlink, self.log)

    def probes(self):
        """The versions whose package index the resolver asked about, in order."""
        probed = []
        with open(self.log, encoding="utf-8") as handle:
            for line in handle:
                arguments = json.loads(line)["arguments"]
                if "--arch" in arguments:
                    probed.append(arguments[arguments.index("--arch") + 1])
        return probed

    def test_walks_back_to_the_newest_release_with_a_readable_index(self):
        result = resolve(FAKE_AQT="6.12.0:404 6.11.3:404 6.10.3:ok 6.10.2:ok",
                         FAKE_AQT_LOG=self.log)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "6.10.3\n")
        self.assertEqual(self.probes(), ["6.12.0", "6.11.3", "6.10.3"])
        self.assertIn("skipping 6.12.0", result.stderr)
        self.assertIn("skipping 6.11.3", result.stderr)

    def test_a_network_failure_stops_the_walk_with_aqt_message(self):
        result = resolve(FAKE_AQT="6.12.0:404 6.11.3:ssl 6.10.3:ok", FAKE_AQT_LOG=self.log)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, "")
        self.assertIn("Failure to connect to", result.stderr)
        self.assertIn("qt6_6113/Updates.xml: SSLError", result.stderr)
        self.assertNotIn("skipping 6.11.3", result.stderr)
        self.assertEqual(self.probes(), ["6.12.0", "6.11.3"])

    def test_a_failed_listing_is_relayed(self):
        result = resolve(FAKE_AQT="6.12.0:ok", FAKE_AQT_LISTING="fail", FAKE_AQT_LOG=self.log)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, "")
        self.assertIn("Failure to connect to", result.stderr)
        self.assertEqual(self.probes(), [])

    def test_no_readable_index_at_all_is_an_error(self):
        result = resolve(FAKE_AQT="6.12.0:404 6.11.0:404", FAKE_AQT_LOG=self.log)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, "")
        self.assertIn("no Qt 6 release has a package index", result.stderr)

    def test_every_probe_runs_with_hash_checking_off(self):
        # With hashes on, aqt reports a missing index and a TLS failure in the
        # same words, and the resolver could not tell which it is walking past.
        resolve(FAKE_AQT="6.12.0:404 6.10.3:ok", FAKE_AQT_LOG=self.log)
        with open(self.log, encoding="utf-8") as handle:
            invocations = [json.loads(line) for line in handle]
        self.assertEqual(len(invocations), 3)
        for invocation in invocations:
            self.assertIn("INSECURE_NOT_FOR_PRODUCTION_ignore_hash : True",
                          invocation["settings"])
            self.assertIn("baseurl : https://download.qt.io", invocation["settings"])


@unittest.skipUnless(os.environ.get("EASY_PROFILER_ONLINE_TESTS")
                     and importlib.util.find_spec("aqt"),
                     "needs EASY_PROFILER_ONLINE_TESTS=1, aqtinstall and the network")
class RealAqtTest(unittest.TestCase):
    def resolve(self, **variables):
        environment = dict(os.environ, **variables)
        return subprocess.run([sys.executable, str(RESOLVER)], env=environment,
                              capture_output=True, text=True, timeout=300)

    def test_resolves_an_installable_release(self):
        result = self.resolve()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertRegex(result.stdout, r"^\d+\.\d+\.\d+\n$")

    def test_an_injected_tls_failure_stops_the_walk(self):
        # The injector fails every request whose URL names the release the
        # clean run above resolves, so the walk reaches it and must stop there.
        version = self.resolve().stdout.strip()
        result = self.resolve(PYTHONPATH=str(FAULT_INJECTOR), QT_FAULT="SSLError",
                              QT_FAULT_URL="qt6_" + version.replace(".", ""))
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, "")
        self.assertIn("SSLError", result.stderr)
        self.assertNotIn(f"skipping {version}", result.stderr)


if __name__ == "__main__":
    unittest.main()
