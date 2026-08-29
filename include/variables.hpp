// 打印当前 PC 所在函数的所有局部变量

#ifndef MINIDBG_VARIABLES_HPP
#define MINIDBG_VARIABLES_HPP

namespace minidbg {
    class debugger;

    void print_variables(debugger &dbg);
} // namespace minidbg

#endif
