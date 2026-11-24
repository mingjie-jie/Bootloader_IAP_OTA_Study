# -*- coding: utf-8 -*-
"""检查关键文档、源码、两个头文件，以及 Markdown 相对路径。"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class RepositoryTests(unittest.TestCase):
    def test_expected_files(self):
        must_exist = [
            "README.md", "original/MY_OTA_JUMP.c", "original/MY_OTA_JUMP.h",
            "configs/bootloader/MY_OTA_JUMP.h",
            "configs/application/MY_OTA_JUMP.h",
            "docs/02_上电启动全流程.md", "docs/03_源码逐函数精读.md",
            "docs/06_IAP与OTA完整升级设计.md", "docs/07_寄存器与汇编原理.md",
            "docs/12_函数指针_BX_BLX与PC.md",
            "docs/13_跳转前的严格条件与安全顺序.md",
            "docs/14_FreeRTOS_PendSV_现场保存条件.md",
            "docs/15_异常进入_异常返回_EXC_RETURN.md",
            "docs/16_原始跳转代码_逐行状态表.md",
            "docs/17_OTA升级与掉电恢复_深度设计.md",
            "docs/18_寄存器观测_实机调试.md",
            "docs/19_故障注入_选择题与参考答案.md",
            "docs/20_官方启动文件_逐阶段深读.md",
            "docs/21_GitHub更新_工程迁移与编码.md",
            "tools/boot_flow_sim.py", "tools/flash_layout.py",
            "linker-snippets/README.md",
        ]
        for name in must_exist:
            with self.subTest(path=name):
                self.assertTrue((ROOT / name).is_file())

    def test_header_targets(self):
        boot = (ROOT / "configs/bootloader/MY_OTA_JUMP.h").read_text(encoding="utf-8")
        app = (ROOT / "configs/application/MY_OTA_JUMP.h").read_text(encoding="utf-8")
        self.assertIn("MY_OTA_APP_ADDRESS    0x08020000UL", boot)
        self.assertIn("MY_OTA_APP_FLASH_END  0x08080000UL", boot)
        self.assertIn("MY_OTA_APP_ADDRESS    0x08000000UL", app)
        self.assertIn("MY_OTA_APP_FLASH_END  0x08020000UL", app)

    def test_markdown_links(self):
        pattern = re.compile(r"(?<!!)\[[^\]]+\]\(([^)]+)\)")
        for path in [ROOT / "README.md", *(ROOT / "docs").glob("*.md")]:
            content = path.read_text(encoding="utf-8")
            for link in pattern.findall(content):
                if "://" in link or link.startswith("#") or link.startswith("mailto:"):
                    continue
                dest = (path.parent / link.split("#")[0]).resolve()
                with self.subTest(path=path.name, link=link):
                    self.assertTrue(dest.exists(), str(dest))


if __name__ == "__main__":
    unittest.main()
