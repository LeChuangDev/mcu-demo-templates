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
  用两个后台线程把 gcc 的 stdout/stderr 按【行】转发，编码判定规则：
    1) 先按 UTF-8 严格解码 —— 本工程源码是 UTF-8，gcc 诊断中引用的
       源码行（含中文注释）会原样输出 UTF-8 字节，必须原样透传；
    2) UTF-8 解不开才按 GB18030（GBK 超集，任意字节都能解）解码 ——
       源码路径里的中文由 MinGW CRT 按系统 ANSI 代码页(GBK)输出。
  若不分情况一律按 GBK 解码，UTF-8 源码行会被二次转码成“鐑у綍”式乱码。
  按行判定是因为一行诊断通常只含一种编码（路径行 GBK / 源码行 UTF-8），
  同时避免了 read() 切块切在汉字中间的问题。

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


def _decode_line(raw):
    """一行原始字节 -> str：优先 UTF-8，失败再按 GB18030。

    源码行是 UTF-8（原样透传）；含中文的路径行是 GBK（转成 UTF-8）。
    """
    try:
        return raw.decode('utf-8')
    except UnicodeDecodeError:
        return raw.decode('gb18030', errors='replace')


def _pump(raw_stream, out_binary):
    """按行把一条子进程字节流转成 UTF-8 转发。

    先攒够一整行再解码，既避免切块切在汉字中间，也保证同一行只按
    一种编码判定（见文件头说明）。
    """
    buf = bytearray()
    try:
        while True:
            chunk = raw_stream.read(4096)
            if not chunk:
                break
            buf.extend(chunk)
            while True:
                pos = buf.find(b'\n')
                if pos < 0:
                    break
                line = bytes(buf[:pos + 1])
                del buf[:pos + 1]
                out_binary.write(_decode_line(line).encode('utf-8'))
                out_binary.flush()
        if buf:
            out_binary.write(_decode_line(bytes(buf)).encode('utf-8'))
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
        target=_pump, args=(proc.stdout, sys.stdout.buffer),
        daemon=True)
    t_err = threading.Thread(
        target=_pump, args=(proc.stderr, sys.stderr.buffer),
        daemon=True)
    t_out.start()
    t_err.start()

    try:
        rc = proc.wait()
    except KeyboardInterrupt:
        proc.terminate()
        proc.wait()
        return 130

    # ★ 必须等两路转发线程把管道读空再退出：
    #   线程是 daemon，主线程一返回 Python 就会杀掉它们，
    #   短命进程（如只有一个警告）的输出可能来不及刷到终端。
    t_out.join(5)
    t_err.join(5)
    return rc


if __name__ == '__main__':
    sys.exit(main())
