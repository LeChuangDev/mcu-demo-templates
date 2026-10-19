# -*- coding: utf-8 -*-
"""
gcc_utf8_wrapper.py —— arm-none-eabi-gcc 输出编码转码包装器
=======================================================================
【为什么需要它】
  Windows 中文版下，arm-none-eabi-gcc（MinGW 运行库）拿到的源码路径里
  若含中文（如 C:/.../项目/...），诊断信息（警告/错误/行号）会按系统
  ANSI 代码页（GBK/936）输出原始字节；而 Ninja / VSCode 终端默认按
  UTF-8 解码 -> 路径显示成 “椤圭洰 / ��Ŀ” 之类乱码。
  已实测：chcp 65001、LANG/LC_ALL=C.UTF-8 对该 gcc 均无效。

【做法】
  本脚本夹在 CMake/Ninja 与 gcc 之间（CMAKE_C_COMPILER_LAUNCHER）：
    ninja -> sccache.bat -> python gcc_utf8_wrapper.py <gcc> <编译参数...>
  用两个后台线程把 gcc 的 stdout/stderr 原始字节按 GB18030（GBK 超集，
  对任意字节都能解码，不会抛异常）增量解码后以 UTF-8 原样转发，
  不改动任何编译参数、不改变退出码，只做编码转换。

【为什么外层 .bat 叫 sccache.bat】
  clangd 从 compile_commands.json 看到编译命令最前面是本包装器时，
  必须把它识别为“编译启动器”并跳过，否则会把 python/.bat 当成编译器。
  clangd 内置的启动器白名单包括 sccache/ccache/distcc，因此借用名字。
  （与真正的 sccache 编译缓存无任何关系）

【关闭方式】
  配置时加 -DENABLE_GCC_UTF8_WRAPPER=OFF 即可完全不使用本包装器。
=======================================================================
"""

import subprocess
import sys
import threading


def _pump(raw_stream, out_binary, encoding):
    """把一条子进程字节流增量转码转发出去。

    用 IncrementalDecoder 是因为一个汉字占 2 字节，read() 的切块边界
    可能恰好切在汉字中间，整块 decode 会出替换符。
    """
    decoder = __import__('codecs').getincrementaldecoder(encoding)(errors='replace')
    try:
        while True:
            chunk = raw_stream.read(4096)
            if not chunk:
                tail = decoder.decode(b'', final=True)
                if tail:
                    out_binary.write(tail.encode('utf-8'))
                    out_binary.flush()
                break
            text = decoder.decode(chunk)
            if text:
                out_binary.write(text.encode('utf-8'))
                out_binary.flush()
    except Exception:
        # 转发失败也绝不能影响编译本身：静默结束该路转发
        pass


def main():
    if len(sys.argv) < 2:
        sys.stderr.write('gcc_utf8_wrapper: missing compiler path\n')
        return 2

    gcc = sys.argv[1]
    args = sys.argv[2:]

    # CREATE_NO_WINDOW：防止 Python 拉起 gcc 时闪黑窗
    creationflags = 0
    if sys.platform == 'win32':
        creationflags = getattr(subprocess, 'CREATE_NO_WINDOW', 0x08000000)

    proc = subprocess.Popen(
        [gcc] + args,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        creationflags=creationflags,
    )

    t_out = threading.Thread(
        target=_pump, args=(proc.stdout, sys.stdout.buffer, 'gb18030'),
        daemon=True)
    t_err = threading.Thread(
        target=_pump, args=(proc.stderr, sys.stderr.buffer, 'gb18030'),
        daemon=True)
    t_out.start()
    t_err.start()

    try:
        return proc.wait()
    except KeyboardInterrupt:
        proc.terminate()
        proc.wait()
        return 130


if __name__ == '__main__':
    sys.exit(main())
