# minidbg

**An x86 Linux debugger written in C++.**

minidbg 是一个面向 Linux x86-64 的轻量级命令行调试器，使用 Linux `ptrace` 控制目标进程，并提供寄存、内存和软件断点等基本调试能力。

## 功能

当前支持：

- 启动并控制目标进程
- 软件断点（x86 `INT3`）
- 读取和修改寄存器
- 读取和修改目标进程内存
- 交互式调试 shell
- 命令前缀缩写

## 环境要求

- Linux x86-64
- C++14 compatible compiler
- CMake

> MiniDbg 当前针对 x86-64 Linux 开发，暂不支持其他架构或操作系统。

## 依赖

MiniDbg 使用以下第三方库：

- [libelfin](https://github.com/TartanLlama/libelfin/tree/cc25b47c11f6798f79f09ec2345b18d663f9d548)
- [Linenoise](https://github.com/antirez/linenoise/tree/c894b9e59f02203dbe4e2be657572cf88c4230c3)

第三方依赖已经随仓库提供于 `ext/` 目录，无需额外安装。

## 快速开始

```bash
cmake -B build
cmake --build build
```

构建完成后：

```
build/
├── mydbg       # MiniDbg 调试器
└── hello       # 示例目标程序
```

## 运行

使用 MiniDbg 调试示例程序：

```
./build/mydbg ./build/hello
```

启动后进入交互式 shell：

```
minidbg>
```

## 命令列表

| Command                      | Description                              | Example                        |
| ---------------------------- | ---------------------------------------- | ------------------------------ |
| `continue`                   | 继续执行目标程序，直到遇到断点或程序结束 | `continue`                     |
| `break <addr>`               | 在指定地址设置软件断点                   | `break 0x401136`               |
| `register dump`              | 打印全部寄存器                           | `register dump`                |
| `register read <reg>`        | 读取指定寄存器                           | `register read rip`            |
| `register write <reg> <val>` | 修改指定寄存器                           | `register write rip 0x401136`  |
| `memory read <addr>`         | 读取指定地址处的 8 字节内存              | `memory read 0x400000`         |
| `memory write <addr> <val>`  | 向指定地址写入 8 字节数据                | `memory write 0x400000 0x1234` |

### 命令缩写

命令支持前缀缩写，例如：

```
minidbg> c
```

等价于：

```
minidbg> continue
```

### 数值

地址和数值支持 `0x` 十六进制前缀，例如：

```
0x401136
0x1234
```

## 项目结构

```
minidbg/
├── CMakeLists.txt
├── src/
│   └── main.cpp
├── include/
│   ├── debugger.hpp      # 调试器核心逻辑与命令路由
│   ├── breakpoint.hpp    # 软件断点
│   └── registers.hpp     # 寄存器描述与访问
├── examples/
│   └── hello.c           # 示例目标程序
└── ext/
    ├── linenoise/        # 命令行编辑与交互
    └── libelfin/         # ELF / DWARF 支持
```