"""Generate original rectangle glyphs, Unicode cmap and a known AV kern pair."""
from pathlib import Path
import struct


def u16(*values):
    return struct.pack('>' + 'H' * len(values), *values)


def s16(*values):
    return struct.pack('>' + 'h' * len(values), *values)


def box(width=500):
    return (s16(1, 0, 0, width, 700) + u16(3, 0) + bytes([1] * 4)
            + s16(0, width, 0, -width) + s16(0, 0, 700, 0))


head = bytearray(54)
struct.pack_into('>I', head, 0, 0x00010000)
struct.pack_into('>I', head, 12, 0x5F0F3CF5)
struct.pack_into('>H', head, 18, 1000)
glyphs = [bytes(10), box(), box(), box(), box(800), box(400), box(), box(), bytes(10)]
# Distinct private-use glyphs exercise page growth and the cache bound.
glyphs += [box() for _ in range(2048)]
hhea = bytearray(36)
struct.pack_into('>Ihhh', hhea, 0, 0x00010000, 1000, -500, 0)
struct.pack_into('>H', hhea, 34, len(glyphs))
mapping = {cp: 1 for cp in range(33, 256)}
mapping.update({32: 8, ord('A'): 2, ord('V'): 3, 0x3A9: 4, 0x416: 5,
                0x4E2D: 6, 0x1F600: 7, 0xFFFD: 1})
groups = [(cp, cp, glyph) for cp, glyph in sorted(mapping.items())]
groups.append((0xE000, 0xE7FF, 9))
groups.sort()
cmap12 = struct.pack('>HHIII', 12, 0, 16 + 12 * len(groups), 0, len(groups))
cmap12 += b''.join(struct.pack('>III', *group) for group in groups)
offsets = [0]
for glyph in glyphs:
    offsets.append(offsets[-1] + len(glyph))
advances = [600] * len(glyphs)
advances[4], advances[5], advances[8] = 900, 500, 300
tables = {
    'cmap': u16(0, 1, 3, 10) + struct.pack('>I', 12) + cmap12,
    'glyf': b''.join(glyphs),
    'head': bytes(head),
    'hhea': bytes(hhea),
    'hmtx': b''.join(u16(advance, 0) for advance in advances),
    'kern': u16(0, 1, 0, 20, 1, 1, 6, 0, 0, 2, 3) + s16(-100),
    'loca': u16(*(offset // 2 for offset in offsets)),
    'maxp': struct.pack('>IH', 0x00010000, len(glyphs)) + bytes(26),
}
count = len(tables)
power = 2 ** (count.bit_length() - 1)
font = bytearray(struct.pack('>IHHHH', 0x00010000, count, power * 16,
                             power.bit_length() - 1, count * 16 - power * 16))
directory, payload = bytearray(), bytearray()
offset = 12 + 16 * count
for tag, data in sorted(tables.items()):
    padded = data + bytes((-len(data)) % 4)
    checksum = sum(struct.unpack('>' + 'I' * (len(padded) // 4), padded)) & 0xFFFFFFFF
    directory += tag.encode() + struct.pack('>III', checksum, offset, len(data))
    payload += padded
    offset += len(padded)
font += directory + payload
Path(__file__).with_name('box.ttf').write_bytes(font)
