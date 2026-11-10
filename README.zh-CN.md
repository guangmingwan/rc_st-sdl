# D88 / RetroFW / GMenuNX 中文终端与拼音输入

终端现在按 UTF-8 绘制文字，中文和全角标点占两个字符格。中文的换行、
覆盖、擦除、插入删除、光标和终端尺寸变化都按双列字符处理。
全键盘模式支持终端内的离线拼音输入法，按 Shift 切换中英文；选中的中文以
UTF-8 字节发送到 PTY，因此 sh 和 Vim 等终端程序都可以接收。

## 拼音输入法

全键盘模式启动时显示按键帮助页，按任意键关闭，F1 可再次显示或关闭；日常
输入使用英文直接输入，不显示虚拟键盘。单独按下再松开左/右 Shift，切换中
文和英文。Shift 与字母、数字等组合时，
继续输入大写和符号，不会切换输入法；Ctrl+Space 也可以切换中英文。

候选窗采用适合 480×272 小屏的 Fcitx5 风格：浅色面板、蓝色选中项、拼音
预编辑、带数字的候选词和分页信息。面板跟随终端光标，空间不足时放在上方。
长候选词在面板中省略显示，提交时仍发送完整文字。

| 按键 | 功能 |
| --- | --- |
| 单按 Shift / Ctrl+Space | 切换中英文；切换时取消未提交的拼音 |
| a–z | 输入全拼；支持连续词组，例如 `nihao`、`zhongwenshurufa` |
| `'` / `v` | 拼音分隔，例如 `xi'an`；`v` 代表 ü，例如 `lv` |
| Space | 选择当前高亮候选；默认首选整句 |
| 1–5 | 选择当前页对应候选；选择部分词语后可继续选剩余部分 |
| ↑ / ↓ / Tab / Shift+Tab | 改变高亮候选 |
| PageUp / PageDown / `-` / `=` | 上一页 / 下一页 |
| ← / → / Home / End | 移动拼音编辑光标 |
| Backspace / Delete | 删除待输入的拼音 |
| Enter | 提交原始拼音字母；再次 Enter 才发送回车 |
| Esc | 有预编辑时取消；无预编辑时切回英文并将 Esc 交给程序 |
| Ctrl+. | 切换中英文标点 |
| Ctrl+Alt+F / F | 切换终端显示字体 |
| F1 | 显示或关闭启动帮助页 |

例如，在 shell 中先输入 `echo `，单按 Shift 切到中文，输入 `nihao`，
按 Space 选中“你好”，再按 Enter 执行命令。需要编辑中文文件时，在 Vim 中
先按 `i` 进入插入模式，再单按 Shift 输入拼音；选词后按 Esc 会回到英文和
Vim Normal 模式。如果仍有未提交的拼音，先按一次 Esc 取消，再按一次 Esc
退出插入模式，之后可输入 `:wq` 保存。

中文模式默认将常用标点转换为中文标点，例如 `,` → `，`、`.` → `。`。
shell 命令和 Vim 命令建议在英文模式输入。Ctrl/Alt 快捷键会交给终端程序，
其中 Ctrl+C / Ctrl+[ 同时退出中文状态。

拼音解码使用离线 Google Pinyin 引擎及其系统词库，支持词组、整句候选和
用户词频学习。正常退出终端时，学习数据保存在 `ime-data/user.dat`；升级
时请保留该文件。输入长度上限为 39 个拼音字符、16 个音节，较长文本请分段选词。

这是面向 SDL/framebuffer 的终端内输入法实现。它复现上述 Fcitx5 常见操作和
候选窗风格，候选排序、配置系统、插件、云候选及全部桌面集成功能不等同于
Ubuntu 桌面版 Fcitx5。其他应用若不运行在本终端内，不会自动获得此输入法。

## 构建和复制到设备

需要可由 opkg 安装的软件包时，在开发电脑执行：

```sh
make ipk-d8
```

生成 `dist/st-sdl_0.4.0-1_mipsel.ipk` 及对应 SHA-256 校验文件。包内包含终端、
中文字库、离线拼音词库、许可文件、GMenuNX 启动项和 `st` / `st-256color`
terminfo。打包需要开发电脑上的 `tic`；RetroFW SDK 已提供该工具。

将 IPK 复制到设备后安装，例如：

```sh
opkg install /path/to/st-sdl_0.4.0-1_mipsel.ipk
```

安装路径为 `/home/apps/st-sdl/`，菜单项位于
`/home/.gmenunx/sections/applications/st.lnk`。退出旧终端并从 GMenuNX 重新打开
`st` 即可。打包时不包含运行生成的 `ime-data/user.dat`，升级不会覆盖学习数据；
菜单项作为配置文件管理，保留用户修改。软件包针对提供 SDL 1.2 / uClibc 的
RetroFW D8/D88 固件，无需联网下载中文字库或拼音词库。

也可以仅打包现有的交叉编译产物：

```sh
python3 tools/build_ipk.py
```

用 `make ipk-d8 IPK_VERSION=0.4.0-2` 可指定后续软件包版本。

在开发电脑上运行：

```sh
make package-d8
```

需要 Python 3、首次准备资源时的网络，以及 `/opt/retrofw` 下支持 C++ 的
RetroFW SDK。命令会准备 GNU Unifont 字库、固定版本的拼音引擎源码和
32 位小端系统词库，再调用现有的 `build_d8.sh` 交叉编译。
生成的 `.build/d88/` 目录包含：

```text
d88/
  st
  libst-preload.so
  README.zh-CN.md
  ime-data/
    dict_pinyin.dat
    LICENSE-2.0.txt
    NOTICE.txt
  fonts/
    unifont.hex
    OFL-1.1.txt
    COPYING
    README.upstream
    FONT-NOTICE.txt
    README
```

把整个 `d88` 文件夹复制到 SD 卡上你存放应用的位置，在 GMenuNX 中为其中的
`st` 添加启动项。程序相对于自身位置查找 `fonts/unifont.hex`，启动目录可以
不同。`libst-preload.so` 应放在 `st` 旁边，`ime-data/` 应一起复制。
拼音引擎和 C++ 运行库静态链接到终端，无需在设备安装桌面环境或新增 C++ 库。

如果 SD 卡使用保留 Unix 权限的文件系统，请确认 `st` 有执行权限。

之后只重新编译程序、沿用已准备的字库时，也可以执行：

```sh
sh build_d8.sh
```

这条命令只更新项目根目录的构建产物，不会更新 `.build/d88/`；复制新生成的
`st` 和 `libst-preload.so`，或再次执行 `make package-d8` 更新整套部署目录。

## 字体与显示大小

找到字库后，默认启用 8×16 英文 / 16×16 中文点阵模式，适合清晰显示中文。
全键盘的 Ctrl+Alt+F 会在 Unicode、console、embedded 三种模式间循环。
后两种模式会把中文字形缩小到两格宽度，笔画较密，阅读中文建议使用 Unicode
模式。虚拟键盘的按键字体保持原来的大小。

字库读入后占用约 2.1 MiB 内存，不依赖 SDL_ttf、FreeType 或设备的中文 locale。
字库覆盖的 Unicode BMP 字符（包括常用简繁体汉字）可以显示；字库外的字符用
方框表示，保留其列宽。组合附加符号、复杂文字排版不在这次中文显示支持范围内。

默认依次查找程序旁的 `fonts/unifont.hex` 和
`/usr/share/unifont/unifont.hex`。也可以指定字库：

```sh
./st -f /path/to/unifont.hex
ST_FONT=/path/to/unifont.hex ./st
```

显式指定的字库无效时会报告错误。默认位置没有字库时，程序会提示并使用原有
英文字体，中文显示为方框。格式必须是解压后的 GNU Unifont BMP `.hex`，
每个字形为 8×16 或 16×16；不能直接使用 TTF、TTC、HZK 或 `.hex.gz`。

## 在设备上验证

在终端输入以下命令；也可以在电脑上将它保存为 UTF-8 编码的脚本，复制到
设备后执行：

```sh
printf '\344\270\255\346\226\207\346\265\213\350\257\225\357\274\232\344\275\240\345\245\275\357\274\214\344\270\226\347\225\214\357\274\201\n'
```

预期显示：`中文测试：你好，世界！`。这个测试只发送 UTF-8 字节，不要求
固件安装中文 locale 或中文输入法。

终端处理的是 **UTF-8 输出**。GBK / GB2312 / GB18030 文件需要先转成 UTF-8。
如果上面的测试正常，但某个命令把中文文件名替换成问号或转义串，要检查
那个命令本身的编码、locale 和固件构建选项；终端无法恢复已被程序替换的文字。
本终端的 Shift 拼音输入会直接提交 UTF-8 中文；也可继续使用外部工具提供的
UTF-8 输出。

通过 `ST_IME_DICT=/path/to/dict_pinyin.dat` 和
`ST_IME_USER=/writable/path/user.dat` 可以指定拼音系统词库和用户词库。
系统词库的字长和字节序必须与解码器匹配；不能把电脑端 64 位开发词库复制到
D88，部署包已经包含适用的 32 位小端词库。

## 开发验证

电脑上安装 C/C++ 编译器、SDL 1.2 开发环境和 Vim 后，可以运行回归测试。
首次运行需要下载拼音引擎源码；资源准备后测试可离线运行：

```sh
make TARGET=pc test
```

测试包括实际启动 `/bin/sh` 与 Vim，通过终端按键处理器和 PTY 输入
“你好，世界！”，检查 shell 收到的字节以及 Vim 保存的 UTF-8 文件。
拼音和候选窗预览保存在 `.build/pinyin-preview.bmp`。

需要在 D88 上运行同样的测试时，可先交叉编译测试程序：

```sh
PATH=/opt/retrofw/bin:$PATH make TARGET=d8 CC=mipsel-linux-gcc test-target
```

将 `.build/d8/test-ime`、`.build/d8/test-ime-pty` 复制到设备的临时测试目录，
在该目录创建 `.build/`，然后执行：

```sh
./test-ime /home/apps/st-sdl/ime-data/dict_pinyin.dat /home/apps/st-sdl/fonts/unifont.hex
./test-ime-pty /home/apps/st-sdl/ime-data/dict_pinyin.dat /usr/bin/vim
```

准备字库后，可额外用真实字库生成 480×272 的位图预览：

```sh
.build/test-chinese fonts/unifont.hex
```

预览保存在 `.build/chinese-preview.bmp`。测试涵盖中英混排、双列占位、行末
换行、覆盖半个汉字、擦除、插入删除、尺寸变化、光标绘制、随机编辑和字形裁剪。
这些检查在电脑上运行，设备实机显示仍需按上面的命令验证。
