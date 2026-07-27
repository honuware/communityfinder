#pragma once

#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace TestHelper {

// ANSI color codes for terminal output.
// Used in REPL and one-shot modes (not in FTXUI dashboard, which has its own styling).
namespace Color {

constexpr const char* kReset     = "\033[0m";
constexpr const char* kBold      = "\033[1m";
constexpr const char* kDim       = "\033[2m";

constexpr const char* kRed       = "\033[31m";
constexpr const char* kGreen     = "\033[32m";
constexpr const char* kYellow    = "\033[33m";
constexpr const char* kBlue      = "\033[34m";
constexpr const char* kMagenta   = "\033[35m";
constexpr const char* kCyan      = "\033[36m";
constexpr const char* kWhite     = "\033[37m";
constexpr const char* kGray      = "\033[90m";

constexpr const char* kBoldRed     = "\033[1;31m";
constexpr const char* kBoldGreen   = "\033[1;32m";
constexpr const char* kBoldYellow  = "\033[1;33m";
constexpr const char* kBoldCyan    = "\033[1;36m";

// Colorize a status value based on its meaning. Generic status vocabulary —
// later phases can extend this as the domain grows.
inline std::string ColorizeStatus(const std::string& status) {
    if (status == "active" || status == "confirmed") {
        return std::string(kGreen) + status + kReset;
    }
    if (status == "pending" || status == "waitlisted") {
        return std::string(kYellow) + status + kReset;
    }
    if (status == "cancelled" || status == "expired" || status == "revoked") {
        return std::string(kGray) + status + kReset;
    }
    return status;
}

// Standard colored output helpers.
inline void PrintSuccess(const std::string& msg) {
    std::cout << kBoldGreen << msg << kReset << "\n";
}

inline void PrintError(const std::string& msg) {
    std::cout << kBoldRed << "Error: " << kReset << kRed << msg << kReset << "\n";
}

inline void PrintWarning(const std::string& msg) {
    std::cout << kBoldYellow << "Warning: " << kReset << kYellow << msg << kReset << "\n";
}

inline void PrintHeader(const std::string& msg) {
    std::cout << kBoldCyan << msg << kReset << "\n";
}

inline void PrintDim(const std::string& msg) {
    std::cout << kDim << msg << kReset << "\n";
}

}  // namespace Color

// Enable ANSI escape sequences on Windows 10+.
// Call once at startup. No-op on other platforms.
inline void EnableAnsiColors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
    }
#endif
}

}  // namespace TestHelper
