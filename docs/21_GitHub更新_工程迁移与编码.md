# 21｜如何将这个更新包覆盖到你以前的 GitHub 仓库（Windows 操作详解）

> **本次更新仅生成本地 GitHub 项目包，不会自动改动你线上的 GitHub 仓库。** 需要你把新包提交到已有仓库，或者连接授权 GitHub 后进行远程操作。

## 1. 已经上传过旧版 GitHub，想原位置更新（推荐）

假设你旧仓库本地位置：`D:\Github\STM32H723_Bootloader_IAP_OTA_Study`。

1. 在 VS Code 打开旧仓库文件夹。`git status` 确认这是有 `.git` 的仓库、当前没有未提交的个人修改；**如果有自己的修改，先提交或另存备份**。
2. 把本次 ZIP 解压出的 `STM32H723_Bootloader_IAP_OTA_Study` 中所有文件/文件夹，复制粘贴到旧仓库根目录，按需要选择“替换”；**不要覆盖/删除 `.git` 文件夹**。
3. 在旧仓库根目录运行：

```powershell
git status
git add -A
git commit -m "docs: expand STM32H723 boot and RTOS context notes"
git push
```

4. 打开 GitHub 线上 README，刷新确认新章节链接可点开。`git add -A` 会包含**新增、修改与删除**；如果你在旧版上加过自己的文件，确认 `git status` 的清单后再提交。

## 2. 本地没有旧仓库，但 GitHub 上已经有

使用现有线上仓库 URL（不要在文字里保留尖括号）：

```powershell
cd D:\Github
git clone https://github.com/你的用户名/你原来的仓库名.git
cd 你原来的仓库名
```

然后复制本 ZIP 解压出来的**内容**进入克隆的仓库根目录，并运行上一节的 `git add -A / commit / push`。

## 3. 新建仓库（之前还未上传）

在 GitHub 网页创建**空仓库**后，在解压出的项目根目录运行：

```powershell
git init
git add .
git commit -m "docs: STM32H723 bootloader IAP OTA deep study"
git branch -M main
git remote add origin https://github.com/你的用户名/新仓库名.git
git push -u origin main
```

如果 `git commit` 提示作者身份未知：

```powershell
git config --global user.name "你的GitHub昵称"
git config --global user.email "你在GitHub上使用的邮箱"
```

## 4. 出现“push rejected / non-fast-forward”

先不要 `git push --force`！检查另一台电脑/网页是否已更新：

```powershell
git fetch origin
git status
git pull --rebase
# 处理冲突后再提交或继续 rebase
git push
```

如果仓库中已有自己的 README 改动，先手动合并冲突；不要盲目覆盖。

## 5. 中文乱码如何解决？

本仓库 `.md/.c/.h/.py` 使用 UTF-8。VS Code：右下角编码 → **Reopen with Encoding → UTF-8**。如果文件历史上是 GBK，则先按原 GBK 打开，再 **Save with Encoding → UTF-8**，不要误把乱码文本再存一次。

Windows PowerShell 可能需要设置终端编码：

```powershell
chcp 65001
```

文件已经损坏时，单纯 `chcp` 不能修复内容，应从 Git 历史或下载包重新恢复。

## 6. 如何确认更新成功？

```powershell
python -m unittest discover -s tests -v
python tools/boot_flow_sim.py --from boot --target app
python tools/flash_layout.py

git log -1 --oneline
git status
```

`git status` 应报告 `working tree clean`（前提没有未跟踪个人文件），GitHub 在线浏览器应能看到 12～21 章的新增 Markdown 文件。
