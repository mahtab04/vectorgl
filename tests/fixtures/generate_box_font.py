"""Generate our tiny, original rectangle-glyph TTF fixture; no external fonts."""
from pathlib import Path
import struct


def u16(*values):
    return struct.pack('>' + 'H' * len(values), *values)


def s16(*values):
    return struct.pack('>' + 'h' * len(values), *values)


head = bytearray(54)
struct.pack_into('>I', head, 0, 0x00010000)
struct.pack_into('>I', head, 12, 0x5F0F3CF5)
struct.pack_into('>H', head, 18, 1000)
hhea = bytearray(36)
struct.pack_into('>Ihhh', hhea, 0, 0x00010000, 800, -200, 0)
struct.pack_into('>H', hhea, 34, 2)
empty_glyph = bytes(10)
box_glyph = (s16(1, 0, 0, 500, 700) + u16(3, 0) + bytes([1] * 4)
             + s16(0, 500, 0, -500) + s16(0, 0, 700, 0))
cmap4 = (u16(4, 480, 0, 4, 4, 1, 0) + u16(255, 65535, 0)
         + u16(32, 65535) + s16(0, 1) + u16(4, 0) + u16(0, *([1] * 223)))
tables = {
    'cmap': u16(0, 1, 3, 1) + struct.pack('>I', 12) + cmap4,
    'glyf': empty_glyph + box_glyph,
    'head': bytes(head),
    'hhea': bytes(hhea),
    'hmtx': u16(600, 0, 600, 0),
    'loca': u16(0, len(empty_glyph) // 2, (len(empty_glyph) + len(box_glyph)) // 2),
    'maxp': struct.pack('>IH', 0x00010000, 2) + bytes(26),
}
font = bytearray(struct.pack('>IHHHH', 0x00010000, len(tables), 64, 2, 48))
directory = bytearray()
payload = bytearray()
offset = 12 + 16 * len(tables)
for tag, data in sorted(tables.items()):
    padded = data + bytes((-len(data)) % 4)
    checksum = sum(struct.unpack('>' + 'I' * (len(padded) // 4), padded)) & 0xFFFFFFFF
    directory += tag.encode() + struct.pack('>III', checksum, offset, len(data))
    payload += padded
    offset += len(padded)
font += directory + payload
Path(__file__).with_name('box.ttf').write_bytes(font)
