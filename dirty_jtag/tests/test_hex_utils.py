#!/usr/bin/env python3
from pathlib import Path
import tempfile
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'host'))
from hex_utils import parse_ihex_lines, parse_ihex_file, dspic_words_from_ihex, make_dspic_rows, pack_dspic_row_words, HexError


def rec(addr, typ, data):
    data=bytes(data)
    body=bytes([len(data), (addr>>8)&0xff, addr&0xff, typ]) + data
    chk=(-sum(body)) & 0xff
    return ':' + body.hex().upper() + f'{chk:02X}'


def main():
    lines=[
        rec(0, 0, [0x56,0x34,0x12,0x00, 0xef,0xcd,0xab,0x00]),
        rec(8, 0, [0x03,0x02,0x01,0x00]),
        rec(0, 1, []),
    ]
    assert len(parse_ihex_lines(lines)) == 3
    with tempfile.TemporaryDirectory() as td:
        p=Path(td)/'a.hex'; p.write_text('\n'.join(lines)+'\n')
        mem=parse_ihex_file(str(p))
        assert mem[0] == 0x56 and mem[2] == 0x12
        words=dspic_words_from_ihex(str(p))
        assert words[0] == 0x123456
        assert words[2] == 0xabcdef
        assert words[4] == 0x010203
        rows=make_dspic_rows(words, 4, 0x100)
        assert len(rows) == 1
        assert rows[0].pc_address == 0
        assert rows[0].words == (0x123456,0xabcdef,0x010203,0xffffff)
        packed=pack_dspic_row_words(rows[0].words)
        assert packed[:9] == bytes([0x56,0x34,0x12,0xef,0xcd,0xab,0x03,0x02,0x01])
        bad=Path(td)/'bad.hex'; bad.write_text(':0000000100\n')
        try:
            parse_ihex_file(str(bad))
            raise AssertionError('bad checksum accepted')
        except HexError:
            pass
    print('test_hex_utils: PASS')

if __name__ == '__main__':
    main()
