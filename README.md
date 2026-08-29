# minidbg

**一个用 C++ 编写的 x86-64 Linux 调试器。**

minidbg 是一个面向 Linux x86-64 的轻量级命令行调试器，基于 Linux `ptrace` 控制目标进程，支持寄存器 / 内存读写、软件断点、源码级与指令级单步、以及进程内存映射查看等基本调试能力。项目参考 [Writing a Linux Debugger](https://blog.tartanllama.xyz/)

[项目地址](https://github.com/sadsnowcat/minidbg)

## 功能

- 启动并控制目标进程（在 `main()` 中通过 `fork` + `execl` 直接拉起被调试程序，因此没有独立的 `run` 命令）
- 软件断点（x86 `INT3`）
- 读取 / 修改寄存器（`register dump | read | write`）
- 读取 / 修改目标进程内存（`memory read | write`）
- 源码级调试：命中断点 / 单步时显示对应源代码行（`print_source`，基于 DWARF 行号信息）
- 单步执行：
  - `step`  —— 源码级单步（step in）
  - `stepi` —— 指令级单步
  - `next`  —— 步过当前函数调用（step over）
  - `finish`—— 步出当前函数（step out）
- 进程内存映射查看（`vmmap`，解析 `/proc/<pid>/maps`，并高亮当前 PC 所在区间）
- 局部变量查看（`variables`，自解析 DWARF 位置表达式，支持 `DW_OP_fbreg` / 寄存器 / 全局变量）
- 按名符号查找（`symbol`）、调用栈回溯（`backtrace`）与栈检查（`stack`：pwndbg 风格 8 字节内存视图；`frames`：逐帧详情；`tele`：任意地址望远镜）
- 交互式调试 shell（基于 linenoise，支持命令历史与行编辑）
- 命令前缀缩写（如 `c` = `continue`、`b` = `break`、`n` = `next`）

## 环境要求

- Linux x86-64（在 WSL2 下开发验证）
- C++14 兼容的编译器（gcc / clang）
- CMake ≥ 3.16

> 当前针对 x86-64 Linux 开发，暂不支持其他架构或操作系统。

## 依赖

- [libelfin](https://github.com/TartanLlama/libelfin) —— ELF / DWARF 解析（随仓库提供于 `ext/libelfin`，构建时自动 `make`）
- [linenoise](https://github.com/antirez/linenoise) —— 命令行编辑与交互（随仓库提供于 `ext/linenoise`）

## 快速开始

```bash
cmake -B build
cmake --build build
```

构建完成后：

```
build/
├── mydbg       # 调试器本体
└── hello       # 示例目标程序（已带 -gdwarf-4 -g -O0 调试信息）
```

> 首次构建会同时编译 `ext/libelfin`，生成 `libelf++.so` / `libdwarf++.so`。

## 运行

```bash
./build/mydbg ./build/hello
```

启动后进入交互式 shell：

```
minidbg>
```

### 一个完整示例

被调试程序 `hello` 默认是 PIE（地址随机化），调试前建议用 `setarch -R` 关闭 ASLR，或用 `vmmap` 查看实际加载基址：

```bash
setarch -R ./build/mydbg ./build/hello
```

```
minidbg> vmmap
   address-range                     perms offset    dev     inode   object
------------------------------------------------------------------------
   555555554000-555555555000         r--p  00000000  08:30   198963  .../build/hello
 > 555555555000-555555556000         r-xp  00001000  08:30   198963  .../build/hello
   ...
minidbg> break 0x555555555149      # 在 main 入口处下断点（基址 + 偏移）
Set breakpoint at address 0x555555555149
minidbg> c
Hit breakpoint at address 0x555555555149
  #include <stdio.h>

> int main() {
      printf("Hello world\n");
      return 0;
  }
minidbg> n
  int main() {
>     printf("Hello world\n");
      return 0;
  }
minidbg> n
Hello world
  int main() {
      printf("Hello world\n");
>     return 0;
  }
```

## 命令列表

| Command                       | Description                                          | Example                              |
| ----------------------------- | ---------------------------------------------------- | ------------------------------------ |
| `continue` (`c`)              | 继续执行，直到断点或程序结束                         | `c`                                  |
| `break <addr \| func \| file:line>` (`b`) | 设置软件断点，支持**地址 / 函数名 / 源码行**三种形式 | `break main`                         |
| `register dump`               | 打印全部寄存器                                       | `register dump`                      |
| `register read <reg>`         | 读取指定寄存器                                       | `register read rip`                  |
| `register write <reg> <val>`  | 修改指定寄存器                                       | `register write rip 0x555555555149`  |
| `memory read <addr>`          | 读取指定地址处的 8 字节内存                          | `memory read 0x400000`               |
| `memory write <addr> <val>`   | 向指定地址写入 8 字节数据                            | `memory write 0x400000 0x1234`       |
| `vmmap`                       | 打印进程内存映射，并高亮当前 PC 所在区间             | `vmmap`                              |
| `step` (`s`)                  | 源码级单步（step in）                                | `step`                               |
| `stepi`                       | 指令级单步                                           | `stepi`                              |
| `next` (`n`)                  | 步过函数调用（step over）                            | `next`                               |
| `finish` (`fin`)              | 步出当前函数（step out）                             | `finish`                             |
| `symbol <name>`              | 按名字查找符号（函数 / 变量），显示类型与地址           | `symbol main`                        |
| `backtrace` (`bt`)           | 打印调用栈回溯（函数地址 + 偏移 + 符号）                | `backtrace`                          |
| `variables` (`v`)            | 打印当前函数作用域内局部变量的名字、地址与值（自解析 DWARF 位置） | `variables`                          |
| `stack` (`sta`)              | pwndbg 风格栈视图：从 rsp 起每 8 字节一行，指针望远镜解引用（—▸），代码指针标注 (func+off) 与指令字节 | `stack`                              |
| `tele <addr> [count]`        | 望远镜式查看任意地址起的内存（默认 16 行），指针自动解引用 | `tele 0x7fffffffe000 8`               |
| `frames`                     | 逐帧打印调用栈详情：函数、pc、rbp、CFA、返回地址、帧大小、局部变量（rbp 链） | `frames`                             |


### 命令缩写

命令支持前缀缩写，例如 `c` = `continue`、`n` = `next`、`s` = `step`、`b` = `break`、`bt` = `backtrace`、`v` / `vars` = `variables`。注意 `step` 是 `stepi` 的前缀，输入 `stepi` 会被正确路由到指令级单步；`b` 同时是 `break` 与 `backtrace` 的前缀，但因 `break` 分支在前，`b` 固定路由到 `break`，要缩写为 `backtrace` 请用 `bt`；`stack` 因 `s` 会被 `step` 分支优先捕获，最短无歧义缩写是 `sta`。

### 数值格式

地址与数值支持 `0x` 十六进制前缀，也接受十进制。

## 使用注意 / 已知限制

1. **断点支持地址 / 源码行 / 函数名**。`break` 支持三种形式：`break <address>`（地址）、`break <file>:<line>`（源码行）、`break <function>`（函数名）。函数名断点会匹配 DWARF 中同名的 `subprogram` 并下在入口地址；源码行断点按 DWARF 行号信息定位。调试 PIE 前建议用 `setarch -R` 关闭 ASLR 以固定地址。
2. **PIE 与 ASLR**。`hello` 是位置无关可执行文件，每次加载地址不同。调试时可用 `setarch -R ./build/mydbg ./build/hello` 关闭地址随机化，方便固定断点地址。
3. **DWARF 版本**。被调试程序需以 `-gdwarf-4` 编译（CMakeLists 已为 `hello` 配置），否则高版本 DWARF（gcc ≥ 11 默认 DWARF 5）会因旧版 libelfin 不识别新属性而解析失败。
4. **先 `c` 再单步**。程序启动后第一次停在动态加载器（`ld-linux`）中，此时 PC 不在你的代码里，`step` / `next` / `finish` 无法工作（会提示 “Cannot find function”）。请先 `c` 跑到你的断点，再进行单步。
5. **无独立 `run` 命令**。被调试程序在 `main()` 中通过 `fork` + `execl` 直接拉起，进入 shell 即已开始调试，因此没有 `run` 命令（设计如此，非缺失）。

## 项目结构

```
minidbg/
├── CMakeLists.txt
├── src/
│   ├── main.cpp          # 入口：fork + execl 拉起被调试程序
│   ├── debugger.cpp      # 调试器核心逻辑与命令路由
│   ├── breakpoint.cpp    # 软件断点 (INT3)
│   ├── registers.cpp     # 寄存器描述与访问
│   ├── utils.cpp         # 字符串分割等工具
│   ├── vmmap.cpp         # 解析 /proc/<pid>/maps
│   ├── symbol.cpp        # 按名符号查找
│   ├── dwarf_util.cpp    # DWARF 表达式原语（ULEB128/SLEB128 解码）
│   ├── variables.cpp     # variables 命令：自解析 DWARF 局部变量
│   └── stack.cpp         # stack/tele（pwndbg 风格内存转储）与 frames（rbp 链逐帧详情）
├── include/
│   ├── debugger.hpp      # 调试器类定义
│   ├── breakpoint.hpp    # 断点类
│   ├── registers.hpp     # 寄存器描述
│   ├── utils.hpp         # 工具函数声明
│   ├── vmmap.hpp         # vmmap 声明
│   ├── symbol.hpp        # 符号查找声明
│   ├── dwarf_util.hpp    # LEB128 解码声明
│   ├── variables.hpp     # print_variables 与变量收集接口声明
│   └── stack.hpp         # print_stack / print_telescope / print_frames 声明
├── examples/
│   ├── hello.cpp         # 示例目标程序
│   └── stacktest.cpp     # 变量 / 回溯测试目标程序（带 -gdwarf-4 -g -O0）
└── ext/
    ├── linenoise/        # 命令行编辑与交互
    └── libelfin/         # ELF / DWARF 支持（构建时 make）
```
