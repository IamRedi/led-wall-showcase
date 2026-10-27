import contextlib
import io
import os
from pathlib import Path
import stat
import struct
import sys
import tempfile
import unittest
from argparse import Namespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools import nvs_provision as nvs


def _set_active(page, slot):
    offset = nvs.PAGE_HEADER_SIZE + slot // 4
    shift = (slot % 4) * 2
    page[offset] = (page[offset] & ~(0b11 << shift)) | (nvs.BIT_ACTIVE << shift)


def _make_image():
    page = bytearray(b"\xff" * nvs.PAGE_SIZE)
    struct.pack_into("<I", page, 0, nvs.PAGE_STATE_ACTIVE)
    page[nvs.PAGE_HEADER_SIZE:nvs.ENTRIES_OFFSET] = b"\x00" * 32

    namespace = bytearray(nvs.ENTRY_SIZE)
    namespace[0] = 0
    namespace[1] = nvs.TYPE_U8
    namespace[2] = 1
    namespace[3] = 0xFF
    namespace[8:15] = b"ledwall"
    namespace[24] = 1
    struct.pack_into("<I", namespace, 4,
                     nvs.crc32_nvs(bytes(namespace[:4]) + bytes(namespace[8:])))
    page[nvs.ENTRIES_OFFSET:nvs.ENTRIES_OFFSET + nvs.ENTRY_SIZE] = namespace
    _set_active(page, 0)

    for slot, key, value in ((1, "tail.length", "137"),
                             (3, "wifi.password", "test-secret-never-print")):
        entry, blob = nvs.make_entry(1, key, value)
        offset = nvs.ENTRIES_OFFSET + slot * nvs.ENTRY_SIZE
        page[offset:offset + nvs.ENTRY_SIZE] = entry
        page[offset + nvs.ENTRY_SIZE:offset + nvs.ENTRY_SIZE + len(blob)] = blob
        for used_slot in range(slot, slot + entry[2]):
            _set_active(page, used_slot)
    return bytes(page)


class NvsProvisionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.image = self.root / "nvs.bin"
        self.original = _make_image()
        self.image.write_bytes(self.original)

    def tearDown(self):
        self.temp.cleanup()

    def add(self, backup):
        args = Namespace(image=str(self.image), key="tail.enabled", value="1",
                         backup=str(backup))
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return nvs.cmd_add(args)

    def test_add_keeps_private_exact_prechange_backup(self):
        backup = self.root / "nvs.before.bin"
        self.assertEqual(self.add(backup), 0)
        self.assertEqual(backup.read_bytes(), self.original)
        self.assertNotEqual(self.image.read_bytes(), self.original)
        self.assertEqual(stat.S_IMODE(backup.stat().st_mode), 0o600)

    def test_existing_backup_is_never_overwritten_or_used_to_mutate_image(self):
        backup = self.root / "nvs.before.bin"
        backup.write_bytes(b"owner data")
        self.assertNotEqual(self.add(backup), 0)
        self.assertEqual(backup.read_bytes(), b"owner data")
        self.assertEqual(self.image.read_bytes(), self.original)

    def test_list_never_reveals_wifi_secret(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            result = nvs.cmd_list(Namespace(image=str(self.image), show_values=True))
        self.assertEqual(result, 0)
        self.assertIn("tail.length", output.getvalue())
        self.assertIn("137", output.getvalue())
        self.assertNotIn("test-secret-never-print", output.getvalue())
        self.assertIn("(secret; redacted)", output.getvalue())

    def test_get_rejects_wifi_secret(self):
        output = io.StringIO()
        error = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(error):
            result = nvs.cmd_get(Namespace(image=str(self.image), key="wifi.password"))
        self.assertEqual(result, 2)
        self.assertNotIn("test-secret-never-print", output.getvalue() + error.getvalue())


if __name__ == "__main__":
    unittest.main()