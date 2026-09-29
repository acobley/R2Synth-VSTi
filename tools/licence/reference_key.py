#!/usr/bin/env python3
"""Independent reference for the licence key format, written from the
design doc rather than from the C++ - the stand-in for the Java server.

    python3 tools/licence/reference_key.py SEED_HEX "Name" ID PRODUCT YYYY-MM-DD

Prints the key. tests/LicenceTests.cpp holds one key made by this script
("golden key"); the C++ must produce the identical string. Needs the
`cryptography` package (standard Ed25519, RFC 8032).
"""
import datetime, hashlib, struct, sys
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"

def normalise(name):
    name = name.strip(" \t\n\r\f\v")
    return "".join(c.lower() if "A" <= c <= "Z" else c for c in name)

def b32(data):
    bits = "".join(f"{b:08b}" for b in data)
    bits += "0" * (-len(bits) % 5)
    return "".join(ALPHABET[int(bits[i:i + 5], 2)] for i in range(0, len(bits), 5))

def make_key(seed_hex, name, licence_id, product, date):
    day = (datetime.date.fromisoformat(date) - datetime.date(2026, 1, 1)).days
    name_hash = hashlib.sha256(normalise(name).encode("utf-8")).digest()[:8]
    payload = struct.pack(">BHIH", 1, product, licence_id, day) + name_hash
    sig = Ed25519PrivateKey.from_private_bytes(bytes.fromhex(seed_hex)).sign(payload)
    body = b32(payload + sig)
    return "BNDL1-" + "-".join(body[i:i + 5] for i in range(0, len(body), 5))

if __name__ == "__main__":
    seed, name, lid, product, date = sys.argv[1:6]
    print(make_key(seed, name, int(lid, 0), int(product, 0), date))
