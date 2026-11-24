# -*- coding: utf-8 -*-
"""模拟程序/Flash map 的回归测试；不是板端测试。"""
import pathlib
import sys
import unittest

TOOLS = pathlib.Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
from boot_flow_sim import simulate  # noqa: E402
from flash_layout import sectors, validate_ranges, REQUEST_START, SRAM4_END  # noqa: E402


class BootFlowTests(unittest.TestCase):
    def test_boot_to_app_has_jump(self):
        log = "\n".join(simulate("boot", "app"))
        self.assertIn("JumpNow", log)
        self.assertIn("APP main", log)

    def test_app_to_boot_skips_jump(self):
        log = "\n".join(simulate("app", "boot"))
        self.assertIn("boot main", log)
        self.assertNotIn("JumpNow():", log)

    def test_invalid_target_no_reset(self):
        log = "\n".join(simulate("boot", "app", target_valid=False))
        self.assertNotIn("NVIC_SystemReset", log)

    def test_sector_count(self):
        self.assertEqual(len(sectors()), 4)
        self.assertEqual(sectors()[1][1], 0x08020000)
        self.assertEqual(sectors()[-1][2], 0x08080000)

    def test_reserved_sram4_last_12(self):
        self.assertEqual(SRAM4_END - REQUEST_START, 12)

    def test_valid_layout(self):
        self.assertEqual(validate_ranges(), [])

    def test_overlap_rejected(self):
        self.assertTrue(validate_ranges(app_start=0x08000000))

    def test_unaligned_rejected(self):
        self.assertTrue(validate_ranges(app_start=0x08020004))


if __name__ == "__main__":
    unittest.main()
