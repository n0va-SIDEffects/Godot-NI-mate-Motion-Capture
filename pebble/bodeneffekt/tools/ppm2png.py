#!/usr/bin/env python3
"""PPM aus worldpreview in ein PNG umwandeln, mit ganzzahliger Vergroesserung."""
import struct, sys, zlib

def main():
    src, dst = sys.argv[1], sys.argv[2]
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    d = open(src, 'rb').read()
    i, fields = 0, []
    while len(fields) < 4:
        while d[i:i + 1].isspace():
            i += 1
        j = i
        while not d[j:j + 1].isspace():
            j += 1
        fields.append(d[i:j])
        i = j
    i += 1
    w, h, px = int(fields[1]), int(fields[2]), d[i:]
    raw = bytearray()
    for y in range(h):
        row = bytearray()
        for x in range(w):
            row += px[(y * w + x) * 3:(y * w + x) * 3 + 3] * scale
        for _ in range(scale):
            raw += b'\x00' + row
    def chunk(tag, data):
        c = tag + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c))
    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', w * scale, h * scale, 8, 2, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(bytes(raw), 9))
           + chunk(b'IEND', b''))
    open(dst, 'wb').write(png)
    print(f"{dst}: {w*scale}x{h*scale}")

main()
