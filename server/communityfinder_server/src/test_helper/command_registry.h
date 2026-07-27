#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "sql_util/database_access/transaction.h"

namespace TestHelper {

// Forward declare
struct TestHelperContext;

// Parsed arguments from a command line
using Args = std::map<std::string, std::string>;

// A command function takes the shared context, a transaction, and parsed args
using CommandFn = std::function<int(TestHelperContext& ctx, Transaction& transaction, const Args& args)>;

struct FlagInfo {
    std::string name;         // e.g. "--table"
    std::string description;  // e.g. "Table name"
    bool required = false;
};

struct CommandInfo {
    std::string name;
    std::string alias;        // short alias, e.g. "ls"
    std::string category;     // "Utility", etc.
    std::string description;
    std::vector<FlagInfo> flags;
    CommandFn execute;
};

class CommandRegistry {
public:
    void Register(CommandInfo info);

    const CommandInfo* Find(const std::string& nameOrAlias) const;

    std::vector<std::string> GetCommandNames() const;
    std::vector<std::string> GetAllNamesAndAliases() const;
    std::vector<std::string> GetFlagNames(const std::string& command) const;
    std::vector<std::string> GetCategories() const;
    std::vector<const CommandInfo*> GetByCategory(const std::string& category) const;

    void PrintHelp() const;
    void PrintCommandHelp(const std::string& nameOrAlias) const;

private:
    std::vector<CommandInfo> commands_;
    std::map<std::string, size_t> nameIndex_;   // name → commands_ index
    std::map<std::string, size_t> aliasIndex_;  // alias → commands_ index
};

// Parse a command line string into command name + args
// e.g. "peek --table=people --limit=5" → ("peek", {{"table","people"},{"limit","5"}})
struct ParsedCommand {
    std::string name;
    Args args;
};

ParsedCommand ParseCommandLine(const std::string& line);

}  // namespace TestHelper
