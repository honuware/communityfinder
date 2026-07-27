#include "command_registry.h"

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "console_colors.h"

namespace TestHelper {

void CommandRegistry::Register(CommandInfo info) {
    size_t index = commands_.size();
    nameIndex_[info.name] = index;
    if (!info.alias.empty()) {
        aliasIndex_[info.alias] = index;
    }
    commands_.push_back(std::move(info));
}

const CommandInfo* CommandRegistry::Find(const std::string& nameOrAlias) const {
    auto nameIt = nameIndex_.find(nameOrAlias);
    if (nameIt != nameIndex_.end()) {
        return &commands_[nameIt->second];
    }
    auto aliasIt = aliasIndex_.find(nameOrAlias);
    if (aliasIt != aliasIndex_.end()) {
        return &commands_[aliasIt->second];
    }
    return nullptr;
}

std::vector<std::string> CommandRegistry::GetCommandNames() const {
    std::vector<std::string> names;
    names.reserve(commands_.size());
    for (const auto& cmd : commands_) {
        names.push_back(cmd.name);
    }
    return names;
}

std::vector<std::string> CommandRegistry::GetAllNamesAndAliases() const {
    std::vector<std::string> result;
    result.reserve(commands_.size() * 2);
    for (const auto& cmd : commands_) {
        result.push_back(cmd.name);
        if (!cmd.alias.empty()) {
            result.push_back(cmd.alias);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> CommandRegistry::GetFlagNames(const std::string& command) const {
    auto* cmd = Find(command);
    if (!cmd) return {};
    std::vector<std::string> flags;
    for (const auto& f : cmd->flags) {
        flags.push_back(f.name);
    }
    return flags;
}

std::vector<std::string> CommandRegistry::GetCategories() const {
    std::vector<std::string> categories;
    for (const auto& cmd : commands_) {
        if (std::find(categories.begin(), categories.end(), cmd.category) == categories.end()) {
            categories.push_back(cmd.category);
        }
    }
    return categories;
}

std::vector<const CommandInfo*> CommandRegistry::GetByCategory(const std::string& category) const {
    std::vector<const CommandInfo*> result;
    for (const auto& cmd : commands_) {
        if (cmd.category == category) {
            result.push_back(&cmd);
        }
    }
    return result;
}

void CommandRegistry::PrintHelp() const {
    auto categories = GetCategories();
    for (const auto& cat : categories) {
        std::cout << "\n" << Color::kBoldCyan << "--- " << cat << " ---"
                  << Color::kReset << "\n";
        auto cmds = GetByCategory(cat);
        for (const auto* cmd : cmds) {
            std::cout << "  " << Color::kBold << std::left << std::setw(32)
                      << cmd->name << Color::kReset;
            if (!cmd->alias.empty()) {
                std::cout << " " << Color::kDim << "(" << cmd->alias << ")"
                          << Color::kReset;
            }
            std::cout << "  " << cmd->description << "\n";
        }
    }
    std::cout << "\n" << Color::kDim
              << "Use 'help <command>' for details on a specific command."
              << Color::kReset << "\n";
}

void CommandRegistry::PrintCommandHelp(const std::string& nameOrAlias) const {
    auto* cmd = Find(nameOrAlias);
    if (!cmd) {
        Color::PrintError("Unknown command: " + nameOrAlias);
        return;
    }

    std::cout << Color::kBoldCyan << cmd->name << Color::kReset;
    if (!cmd->alias.empty()) {
        std::cout << " " << Color::kDim << "(alias: " << cmd->alias << ")"
                  << Color::kReset;
    }
    std::cout << "\n  " << cmd->description << "\n";

    if (!cmd->flags.empty()) {
        std::cout << "\n  " << Color::kBold << "Flags:" << Color::kReset << "\n";
        for (const auto& flag : cmd->flags) {
            std::cout << "    " << Color::kCyan << std::left << std::setw(24)
                      << flag.name << Color::kReset << flag.description;
            if (flag.required) {
                std::cout << " " << Color::kYellow << "(required)" << Color::kReset;
            }
            std::cout << "\n";
        }
    }
}

ParsedCommand ParseCommandLine(const std::string& line) {
    ParsedCommand result;
    std::istringstream stream(line);
    std::string token;

    // First token is the command name
    if (!(stream >> token)) return result;
    result.name = token;

    // Remaining tokens are flags: --key=value or --flag
    while (stream >> token) {
        if (token.substr(0, 2) != "--") continue;
        std::string flagBody = token.substr(2);
        auto eqPos = flagBody.find('=');
        if (eqPos != std::string::npos) {
            result.args[flagBody.substr(0, eqPos)] = flagBody.substr(eqPos + 1);
        } else {
            result.args[flagBody] = "true";
        }
    }

    return result;
}

}  // namespace TestHelper
