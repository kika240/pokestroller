#pragma once
#include <sstream>
#include <stdexcept>
// Portable logging: no recent libc++ dependency, no process-wide abort.
class Log {
public:
    template <typename... Args> static void Info(const char*, Args&&...) {}
    template <typename... Args> static void Warn(const char*, Args&&...) {}
    template <typename... Args> static void Error(const char*, Args&&...) {}
    template <typename... Args>
    [[noreturn]] static void Fatal(const char* message, Args&&... args) {
        std::ostringstream out;
        out << message << std::hex;
        ((out << ' ' << args), ...);
        throw std::runtime_error(out.str());
    }
};
