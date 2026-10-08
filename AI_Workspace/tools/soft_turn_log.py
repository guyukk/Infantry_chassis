"""Decode the version-1 USART6 soft-turn stream to CSV (115200, 8N1, 50 Hz)."""
import argparse
import binascii
import csv
import json
import math
import struct
from pathlib import Path

PACKET = struct.Struct('<2sBBIIBBH28f8hH')
FLOAT_FIELDS = ['raw_vx', 'raw_vy', 'raw_wz', 'shaped_vx', 'shaped_vy', 'shaped_wz']
for prefix in ['measured_angle', 'limited_angle', 'angle_error', 'drive_set', 'measured_speed']:
    FLOAT_FIELDS += [f'{prefix}_{i}' for i in range(4)]
FLOAT_FIELDS += ['roll_rad', 'pitch_rad']
CURRENT_FIELDS = [f'drive_current_{i}' for i in range(4)] + [f'steer_current_{i}' for i in range(4)]
FIELDNAMES = ['sequence', 'timestamp_ms', 'mode', 'fault', 'braking', 'holding'] + FLOAT_FIELDS + CURRENT_FIELDS + [
    'roll_deg', 'pitch_deg', 'shaped_translation_m_s']


def decode_packets(data):
    rows, bad_crc, offset = [], 0, 0
    while True:
        start = data.find(b'ST', offset)
        if start < 0 or len(data) - start < PACKET.size: break
        values = PACKET.unpack_from(data, start)
        if values[1] != 1 or values[6] != 0 or values[7] != PACKET.size:
            offset = start + 1
            continue
        packet = data[start:start + PACKET.size]
        if binascii.crc_hqx(packet[:-2], 0xFFFF) != values[-1]:
            bad_crc += 1
            offset = start + 1
            continue
        flags = values[2]
        row = dict(sequence=values[3], timestamp_ms=values[4], mode=values[5],
                   fault=int(bool(flags & 1)), braking=int(bool(flags & 2)), holding=int(bool(flags & 4)))
        row.update(zip(FLOAT_FIELDS, values[8:36]))
        row.update(zip(CURRENT_FIELDS, values[36:44]))
        row['roll_deg'] = math.degrees(row['roll_rad'])
        row['pitch_deg'] = math.degrees(row['pitch_rad'])
        row['shaped_translation_m_s'] = math.hypot(row['shaped_vx'], row['shaped_vy'])
        rows.append(row)
        offset = start + PACKET.size
    return rows, bad_crc


def convert(source, destination):
    rows, bad_crc = decode_packets(source.read_bytes())
    if not rows: raise RuntimeError('No valid version-1 soft-turn records found')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open('x', newline='', encoding='utf-8-sig') as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDNAMES)
        writer.writeheader()
        writer.writerows(rows)
    print(json.dumps({'records': len(rows), 'bad_crc': bad_crc, 'csv': str(destination.resolve())}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    convert(args.input, args.output or args.input.with_suffix('.csv'))
