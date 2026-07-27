#include "repl/repl.h"

#include <iostream>
#include <string>
#include <vector>

#include <replxx.hxx>

#include "command_registry.h"
#include "console_colors.h"

namespace TestHelper {

namespace {

// Tab completion callback
replxx::Replxx::completions_t HookCompletion(
    const std::string& context, int& contextLen,
    const CommandRegistry& registry) {

    replxx::Replxx::completions_t completions;

    // Find the last space to determine what we're completing
    auto lastSpace = context.rfind(' ');
    std::string prefix;
    bool completingFlag = false;
    std::string commandName;

    if (lastSpace == std::string::npos) {
        // Completing the command name
        prefix = context;
        contextLen = static_cast<int>(prefix.size());
    } else {
        // Completing a flag
        prefix = context.substr(lastSpace + 1);
        contextLen = static_cast<int>(prefix.size());
        commandName = context.substr(0, context.find(' '));
        completingFlag = true;
    }

    if (!completingFlag) {
        // Complete command names and aliases
        for (const auto& name : registry.GetAllNamesAndAliases()) {
            if (name.find(prefix) == 0) {
                completions.emplace_back(name);
            }
        }
    } else if (prefix.find("--") == 0) {
        // Complete flag names for the current command
        std::string flagPrefix = prefix.substr(2);
        for (const auto& flag : registry.GetFlagNames(commandName)) {
            std::string fullFlag = flag.substr(2); // strip leading --
            if (fullFlag.find(flagPrefix) == 0) {
                completions.emplace_back(flag + "=");
            }
        }
    }

    return completions;
}

}  // namespace

ReplAction RunRepl(TestHelperContext& ctx, const std::string& prePopulated) {
    replxx::Replxx rx;

    // Load history
    std::string historyFile;
#ifdef _WIN32
    const char* userProfile = getenv("USERPROFILE");
    if (userProfile) historyFile = std::string(userProfile) + "\\.communityfinder_test_history";
#else
    const char* home = getenv("HOME");
    if (home) historyFile = std::string(home) + "/.communityfinder_test_history";
#endif
    if (!historyFile.empty()) {
        rx.history_load(historyFile);
    }

    // Set up tab completion
    rx.set_completion_callback(
        [&](const std::string& input, int& contextLen) {
            return HookCompletion(input, contextLen, ctx.registry);
        });

    // Set max history size
    rx.set_max_history_size(500);

    // If pre-populated, set the initial input
    if (!prePopulated.empty()) {
        rx.set_preload_buffer(prePopulated);
    }

    std::cout << Color::kBoldCyan << "Command mode." << Color::kReset
              << Color::kDim << " Type 'help' for commands, 'back' to return to dashboard, 'quit' to exit."
              << Color::kReset << "\n";

    while (true) {
        // Build prompt with current user (using ANSI codes for color)
        std::string prompt;
        if (ctx.currentPersonId > 0) {
            auto spacePos = ctx.currentPersonName.find(' ');
            std::string firstName = (spacePos != std::string::npos)
                ? ctx.currentPersonName.substr(0, spacePos)
                : ctx.currentPersonName;
            prompt = std::string(Color::kBoldGreen) + "cf" + Color::kReset
                   + " [" + Color::kCyan + firstName + Color::kReset + "]> ";
        } else {
            prompt = std::string(Color::kBoldYellow) + "cf" + Color::kReset + "> ";
        }

        const char* input = rx.input(prompt);

        if (input == nullptr) {
            // Ctrl+D
            return ReplAction::Quit;
        }

        std::string line(input);

        // Trim whitespace
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            // Empty line — return to dashboard
            return ReplAction::Dashboard;
        }
        line = line.substr(start, line.find_last_not_of(" \t") - start + 1);

        // Add to history
        rx.history_add(line);

        // Check for exit commands
        if (line == "quit" || line == "exit") {
            if (!historyFile.empty()) rx.history_save(historyFile);
            return ReplAction::Quit;
        }
        if (line == "back" || line == "dashboard") {
            if (!historyFile.empty()) rx.history_save(historyFile);
            return ReplAction::Dashboard;
        }

        // Handle "help <command>" as a special case
        if (line.substr(0, 5) == "help " && line.size() > 5) {
            ctx.registry.PrintCommandHelp(line.substr(5));
            continue;
        }

        // Parse and execute
        auto parsed = ParseCommandLine(line);
        if (parsed.name.empty()) continue;

        ExecuteCommand(ctx, parsed.name, parsed.args);
    }

    if (!historyFile.empty()) rx.history_save(historyFile);
    return ReplAction::Quit;
}

}  // namespace TestHelper
