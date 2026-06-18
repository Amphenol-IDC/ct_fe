#!/usr/bin/env python3
import sys
import os
import binascii

if len(sys.argv) < 2:
    print("Usage: append_crc.py <input_bin> [output_bin]")
    sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2] if len(sys.argv) > 2 else input_file.replace(".bin", "_crc.bin")

# Read firmware binary
with open(input_file, "rb") as f:
    data = f.read()

# Calculate CRC32
crc = binascii.crc32(data) & 0xFFFFFFFF
print(f"Firmware length: {len(data)} bytes")
print(f"Appending CRC32: {crc:08X}")

# Append CRC (little-endian)
with open(output_file, "wb") as f:
    f.write(data)
    f.write(crc.to_bytes(4, 'little'))

print(f"Created: {output_file}")
