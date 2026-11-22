# -*- coding: utf-8 -*-
"""离线工具单元测试。无需开发板、HAL 或交叉编译器。"""
import pathlib
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from check_vectors import inspect_vector_bytes  # noqa: E402


class CheckVectorsTests(unittest.TestCase):
    @staticmethod
    def blob(msp=0x20020000, reset=0x08020101, size=32):
        return struct.pack("<II", msp, reset) + bytes(max(0, size - 8))

    def test_valid_app(self):
        result = inspect_vector_bytes(self.blob(), 0x08020000, 0x08080000)
        self.assertEqual(result["issues"], [])
        self.assertEqual(result["reset_instruction_address"], 0x08020100)

    def test_thumb_bit_invalid(self):
        result = inspect_vector_bytes(self.blob(reset=0x08020100),
                                      0x08020000, 0x08080000)
        self.assertTrue(any("Thumb" in x for x in result["issues"]))

    def test_wrong_link_address(self):
        result = inspect_vector_bytes(self.blob(reset=0x08000101),
                                      0x08020000, 0x08080000)
        self.assertTrue(any("不在指定固件" in x for x in result["issues"]))

    def test_msp_unaligned(self):
        result = inspect_vector_bytes(self.blob(msp=0x2001FFFC),
                                      0x08020000, 0x08080000)
        self.assertTrue(any("8 字节" in x for x in result["issues"]))

    def test_msp_outside_ram(self):
        result = inspect_vector_bytes(self.blob(msp=0xFFFFFFFF),
                                      0x08020000, 0x08080000)
        self.assertTrue(any("SRAM" in x for x in result["issues"]))

    def test_too_large(self):
        result = inspect_vector_bytes(self.blob(size=0x60001),
                                      0x08020000, 0x08080000)
        self.assertTrue(any("超过指定分区" in x for x in result["issues"]))

    def test_short(self):
        with self.assertRaises(ValueError):
            inspect_vector_bytes(b"\x00" * 7, 0x08020000, 0x08080000)


if __name__ == "__main__":
    unittest.main()
