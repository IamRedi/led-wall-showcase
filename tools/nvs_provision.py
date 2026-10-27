#!/usr/bin/env python3
"""NVS provisioning tool for LED Wall non-secret hardware settings.

Repair cycle 1 (B-L010-R1) local provisioning method: the firmware loads the
documented dotted config vocabulary from the NVS partition at every boot, but
nothing in the firmware WRITES config — provisioning is a deliberate,
Owner-controlled step performed on an NVS partition IMAGE FILE with this tool,
then flashed explicitly with esptool. WiFi credentials are NEVER accepted by
this tool (provision them with the LED-L008-campaign NVS image procedure).

Format (ESP-IDF NVS v2, structure verified against the device's real partition
during the LED-L008 campaign and re-verified against the retained pre-L010
full-flash backup during LED-L010 review cycle 1):

  page (4096 B) = header (32 B) + entry-state bitmap (32 B) + 126 entries x 32 B
  header  = state:u32 (0xFFFFFFFE active) + seq:u32 + version:u8 (0xFE) +
            reserved + crc32 over header[4:28]
  entry   = ns:u8 + type:u8 + span:u8 + chunk:u8 + crc32:u32 + key:16 + data:8
  crc32   = raw reflected CRC-32 (poly 0xEDB88320), init 0, final ^0xFFFFFFFF
            over entry[0:4]+entry[8:32]
  SZ blob = size:u16 + 0xFFFF + crc32(blob) in the entry's data field; the
            string bytes (incl. NUL) follow in the next span-1 entry slots
  bitmap  = 2 bits per entry: 0b10 = active entry, 0b00 = free slot

Operations (ALWAYS on an image file, never the device):
  list : python3 tools/nvs_provision.py list --image nvs.bin
  get  : python3 tools/nvs_provision.py get --image nvs.bin --key tail.length
  add  : python3 tools/nvs_provision.py add --image nvs.bin \
             --key tail.enabled --value 1 --backup nvs.before.bin

`add` refuses to touch existing keys (NVS entries are append-only; the tool
never invalidates entries), refuses secret keys, and refuses to modify an
image without writing the exact pre-modification bytes to a new, exclusive
mode-0600 backup. Existing backup paths are never overwritten. WiFi values are
always redacted by `list`, even when `--show-values` is supplied. Typical flow:

  1. esptool.py --port /dev/ttyUSB0 --baud 115200 read_flash 0x9000 0x5000 nvs.bin
  2. python3 tools/nvs_provision.py add --image nvs.bin --key tail.enabled \
         --value 1 --backup nvs.before.bin
     python3 tools/nvs_provision.py add --image nvs.bin --key tail.length \
         --value <count> --backup nvs.before2.bin
  3. python3 tools/nvs_provision.py list --image nvs.bin   # verify
  4. esptool.py --port /dev/ttyUSB0 --baud 115200 write_flash 0x9000 nvs.bin
"""

import argparse
import os
import struct
import sys

PAGE_SIZE = 4096
PAGE_HEADER_SIZE = 32
ENTRIES_PER_PAGE = 126
ENTRY_SIZE = 32
ENTRIES_OFFSET = PAGE_HEADER_SIZE + 32  # header + entry-state bitmap

PAGE_STATE_ACTIVE = 0xFFFFFFFE
PAGE_STATE_EMPTY = 0xFFFFFFFF

TYPE_U8 = 0x01
TYPE_SZ = 0x21

BIT_ACTIVE = 0b10
BIT_FREE = 0b00

# Non-secret keys this tool may add — exactly the documented dotted vocabulary
# (CONTRACT.md §18) MINUS both WiFi keys. The firmware reads every value as a
# string; this tool writes SZ entries accordingly.
ALLOWED_KEYS = {
    "orientation.flip_odd_rows",
    "orientation.flip_y",
    "brightness.default_level",
    "brightness.ceiling",
    "boot.base_mode",
    "api.http_port",
    "api.ws_port",
    "input.enabled",
    "tail.enabled",
    "tail.length",
    "tail.reverse",
    "ds4.enabled",
}

SECRET_KEYS = {"wifi.ssid", "wifi.password"}


def crc32_nvs(data: bytes) -> int:
    """Raw reflected CRC-32 (poly 0xEDB88320), init 0, final XOR 0xFFFFFFFF —
    bit-verified against the device's real NVS pages (header and entries)."""
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc ^ 0xFFFFFFFF


def entry_crc_ok(entry: bytes) -> bool:
    return crc32_nvs(entry[0:4] + entry[8:32]) == struct.unpack_from("<I", entry, 4)[0]


def bitmap_state(page: bytes, slot: int) -> int:
    byte = page[PAGE_HEADER_SIZE + slot // 4]
    return (byte >> ((slot % 4) * 2)) & 0b11


def set_bitmap_state(image: bytearray, page_index: int, slot: int, state: int) -> None:
    offset = page_index * PAGE_SIZE + PAGE_HEADER_SIZE + slot // 4
    shift = (slot % 4) * 2
    image[offset] = (image[offset] & ~(0b11 << shift)) | (state << shift)


def page_state(page: bytes) -> int:
    return struct.unpack_from("<I", page, 0)[0]


def ledwall_namespace_index(page: bytes) -> int:
    """The namespace index assigned to 'ledwall' on this page, or 0."""
    for i in range(ENTRIES_PER_PAGE):
        entry = page[ENTRIES_OFFSET + i * ENTRY_SIZE:ENTRIES_OFFSET + (i + 1) * ENTRY_SIZE]
        if entry[1] == 0xFF or not entry_crc_ok(entry):
            continue
        if entry[0] == 0 and entry[1] == TYPE_U8:
            key = entry[8:24].split(b"\x00")[0].decode("ascii", "replace")
            if key == "ledwall":
                return entry[24]
    return 0


def iter_namespace(page: bytes, ns_index: int):
    """Yields (slot, key, type, value) for active entries of one namespace."""
    i = 0
    while i < ENTRIES_PER_PAGE:
        entry = page[ENTRIES_OFFSET + i * ENTRY_SIZE:ENTRIES_OFFSET + (i + 1) * ENTRY_SIZE]
        span = entry[2]
        if entry[1] == 0xFF or not entry_crc_ok(entry) or bitmap_state(page, i) != BIT_ACTIVE:
            i += 1
            continue
        if span < 1:
            span = 1
        if entry[0] == ns_index:
            key = entry[8:24].split(b"\x00")[0].decode("ascii", "replace")
            if entry[1] == TYPE_SZ:
                size = entry[24] | (entry[25] << 8)
                blob_start = ENTRIES_OFFSET + (i + 1) * ENTRY_SIZE
                blob = page[blob_start:blob_start + size]
                if len(blob) == size and crc32_nvs(blob) == struct.unpack_from("<I", entry, 28)[0]:
                    yield i, key, "SZ", blob.split(b"\x00")[0].decode("utf-8", "replace")
                else:
                    yield i, key, "SZ", "(blob CRC mismatch)"
            elif entry[1] == TYPE_U8:
                yield i, key, "U8", str(entry[24])
            else:
                yield i, key, hex(entry[1]), ""
        i += span


def load_pages(image: bytes):
    """Yields (index, page) for every non-empty page."""
    for p in range(len(image) // PAGE_SIZE):
        page = image[p * PAGE_SIZE:(p + 1) * PAGE_SIZE]
        if page_state(page) != PAGE_STATE_EMPTY:
            yield p, page


def find_ledwall_ns(image: bytes) -> int:
    for _p, page in load_pages(image):
        ns = ledwall_namespace_index(page)
        if ns != 0:
            return ns
    return 0


def key_exists(image: bytes, ns_index: int, key: str):
    for p, page in load_pages(image):
        for _slot, existing, _typ, _value in iter_namespace(page, ns_index):
            if existing == key:
                return p
    return None


def find_append_slot(image: bytes, ns_index: int, span: int):
    """(page_index, slot) of the active page holding `span` consecutive free
    slots, preferring the page that already carries ledwall entries."""
    candidates = []
    for p, page in load_pages(image):
        if page_state(page) != PAGE_STATE_ACTIVE:
            continue
        holds_ns = ledwall_namespace_index(page) == ns_index
        free_run = 0
        first_slot = None
        for slot in range(ENTRIES_PER_PAGE):
            if bitmap_state(page, slot) == BIT_FREE:
                if first_slot is None:
                    first_slot = slot
                free_run += 1
                if free_run == span:
                    candidates.append((0 if holds_ns else 1, p, first_slot))
                    break
            else:
                free_run = 0
                first_slot = None
    if not candidates:
        return None
    candidates.sort()
    return candidates[0][1], candidates[0][2]


def make_entry(ns: int, key: str, value: str):
    """(entry bytes, blob bytes) for one string value, ready to append."""
    if not key or len(key) > 16 or not key.isascii():
        raise ValueError("key must be 1..16 ASCII characters")
    blob = value.encode("utf-8") + b"\x00"
    if len(blob) > PAGE_SIZE - ENTRY_SIZE:
        raise ValueError("value too long for a single-page string entry")
    entry = bytearray(32)
    entry[0] = ns
    entry[1] = TYPE_SZ
    entry[2] = 1 + (len(blob) + ENTRY_SIZE - 1) // ENTRY_SIZE  # span
    entry[3] = 0xFF
    entry[8:8 + len(key)] = key.encode("ascii")
    struct.pack_into("<H", entry, 24, len(blob))
    struct.pack_into("<H", entry, 26, 0xFFFF)
    struct.pack_into("<I", entry, 28, crc32_nvs(blob))
    struct.pack_into("<I", entry, 4, crc32_nvs(bytes(entry[0:4]) + bytes(entry[8:32])))
    return bytes(entry), blob


def require_image(args) -> bytes:
    with open(args.image, "rb") as fh:
        image = fh.read()
    if len(image) == 0 or len(image) % PAGE_SIZE != 0:
        raise ValueError(f"{args.image} is not a non-empty multiple of {PAGE_SIZE} bytes")
    return image


def cmd_list(args) -> int:
    image = require_image(args)
    ns_index = find_ledwall_ns(image)
    if ns_index == 0:
        print("no 'ledwall' namespace found")
        return 0
    printed = False
    for p, page in load_pages(image):
        for _slot, key, typ, value in iter_namespace(page, ns_index):
            if key in SECRET_KEYS:
                shown = "(secret; redacted)"
            else:
                shown = value if args.show_values else "(hidden; --show-values to display)"
            print(f"page {p}: {key:28s} {typ:3s} {shown}")
            printed = True
    if not printed:
        print("no 'ledwall' namespace entries found")
    return 0


def cmd_get(args) -> int:
    if args.key in SECRET_KEYS:
        print("ERROR: secret keys are never printed by this tool", file=sys.stderr)
        return 2
    image = require_image(args)
    ns_index = find_ledwall_ns(image)
    if ns_index == 0:
        print("no 'ledwall' namespace found", file=sys.stderr)
        return 1
    for _p, page in load_pages(image):
        for _slot, key, _typ, value in iter_namespace(page, ns_index):
            if key == args.key:
                print(value)
                return 0
    print(f"key not found: {args.key}", file=sys.stderr)
    return 1


def cmd_add(args) -> int:
    if args.key in SECRET_KEYS or args.key not in ALLOWED_KEYS:
        print(f"ERROR: key '{args.key}' is not in the tool's allowed non-secret set "
              f"({sorted(ALLOWED_KEYS)}); WiFi keys are never accepted — provision "
              f"them with the LED-L008-campaign NVS image procedure", file=sys.stderr)
        return 2
    image = bytearray(require_image(args))
    original = bytes(image)

    ns_index = find_ledwall_ns(bytes(image))
    if ns_index == 0:
        print("ERROR: no 'ledwall' namespace definition found in the image; provision "
              "WiFi credentials first (LED-L008 procedure)", file=sys.stderr)
        return 3
    clash = key_exists(bytes(image), ns_index, args.key)
    if clash is not None:
        print(f"ERROR: key '{args.key}' already exists on page {clash}. This tool "
              f"never invalidates existing entries; use the documented LED-L008 "
              f"re-provision procedure instead", file=sys.stderr)
        return 3

    entry, blob = make_entry(ns_index, args.key, args.value)
    span = entry[2]
    target = find_append_slot(bytes(image), ns_index, span)
    if target is None:
        print("ERROR: no active NVS page has enough consecutive free slots for this "
              "entry; the partition needs the documented re-provision procedure",
              file=sys.stderr)
        return 4
    page_index, slot = target
    base = page_index * PAGE_SIZE + ENTRIES_OFFSET + slot * ENTRY_SIZE
    image[base:base + ENTRY_SIZE] = entry
    image[base + ENTRY_SIZE:base + ENTRY_SIZE + len(blob)] = blob
    for s in range(slot, slot + span):
        set_bitmap_state(image, page_index, s, BIT_ACTIVE)

    # Preserve the exact pre-change image. NVS images contain WiFi credentials,
    # so create the backup privately and exclusively; never clobber an existing
    # path or expose the copied secrets through permissive file mode bits.
    try:
        fd = os.open(args.backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "wb") as fh:
            fh.write(original)
            fh.flush()
            os.fsync(fh.fileno())
    except FileExistsError:
        print(f"ERROR: backup path already exists: {args.backup}", file=sys.stderr)
        return 5
    except OSError as exc:
        print(f"ERROR: could not create pre-modification backup: {exc}", file=sys.stderr)
        return 5

    with open(args.image, "wb") as fh:
        fh.write(bytes(image))
    print(f"added {args.key}={args.value} on page {page_index} slot {slot} "
          f"(span {span}); pre-modification copy: {args.backup}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="LED Wall NVS provisioning (image-file only; never touches the device)")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_list = sub.add_parser("list", help="list ledwall namespace keys")
    p_list.add_argument("--image", required=True)
    p_list.add_argument("--show-values", action="store_true")
    p_list.set_defaults(func=cmd_list)

    p_get = sub.add_parser("get", help="print one non-secret key's value")
    p_get.add_argument("--image", required=True)
    p_get.add_argument("--key", required=True)
    p_get.set_defaults(func=cmd_get)

    p_add = sub.add_parser("add", help="add one non-secret key (refuses existing keys)")
    p_add.add_argument("--image", required=True)
    p_add.add_argument("--key", required=True)
    p_add.add_argument("--value", required=True)
    p_add.add_argument("--backup", required=True,
                       help="path for the mandatory pre-modification copy")
    p_add.set_defaults(func=cmd_add)

    args = parser.parse_args()
    try:
        return args.func(args)
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
