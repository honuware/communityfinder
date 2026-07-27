#include "commands/utility_commands.h"

#include <cctype>
#include <iostream>
#include <string>

#include "command_runner.h"
#include "console_colors.h"
#include "util/types.h"

// Standing convention: each later phase adds simulation commands here for its
// hard-to-engineer scenarios (Phase 4: verified users / expired sessions;
// Phase 10: past/expired events; …).

namespace TestHelper {

namespace {

// A table/column identifier is safe to splice into SQL only when it is a plain
// [A-Za-z0-9_] token. The generic DB helpers parameterize VALUES but not
// identifiers, so we gate the identifier ourselves before interpolating it.
bool IsSafeIdentifier(const std::string& id) {
    if (id.empty()) return false;
    for (char c : id) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
            return false;
        }
    }
    return true;
}

}  // namespace

void RegisterUtilityCommands(CommandRegistry& registry) {

    // help — list all commands, or details for one.
    registry.Register({
        "help", "?", "Utility",
        "Show available commands or help for a specific command",
        {{ "--command", "Command to get help for", false }},
        [](TestHelperContext& ctx, Transaction&, const Args& args) -> int {
            auto it = args.find("command");
            if (it != args.end() && !it->second.empty()) {
                ctx.registry.PrintCommandHelp(it->second);
            } else {
                ctx.registry.PrintHelp();
            }
            return 0;
        }
    });

    // status / whoami — DB name + current logged-in person + mail mode.
    registry.Register({
        "status", "whoami", "Utility",
        "Show the database name and the current logged-in person",
        {},
        [](TestHelperContext& ctx, Transaction&, const Args&) -> int {
            std::cout << Color::kBold << "Database: " << Color::kReset
                      << ctx.databaseHelper.GetDatabaseName() << "\n";
            if (ctx.currentPersonId > 0) {
                std::cout << Color::kBold << "User:     " << Color::kReset
                          << ctx.currentPersonName
                          << " (" << ctx.currentPersonEmail << ") ID:"
                          << ctx.currentPersonId << "\n";
            } else {
                std::cout << Color::kBold << "User:     " << Color::kReset
                          << Color::kDim << "no user logged in "
                          << "(use 'login --email=<email>')" << Color::kReset
                          << "\n";
            }
            std::cout << Color::kBold << "Mail:     " << Color::kReset
                      << (ctx.sendRealEmail ? "REAL (sends)" : "test (captured)")
                      << "\n";
            return 0;
        }
    });

    // peek / list-table — dump the first N rows of any framework table.
    registry.Register({
        "peek", "list-table", "Utility",
        "Print the first N rows of a table (defaults to the people table)",
        {{ "--table", "Table name (default: people)", false },
         { "--limit", "Max rows to print (default: 10)", false }},
        [](TestHelperContext& ctx, Transaction& transaction, const Args& args) -> int {
            std::string table = "people";
            auto tIt = args.find("table");
            if (tIt != args.end() && !tIt->second.empty()) {
                table = tIt->second;
            }
            if (!IsSafeIdentifier(table)) {
                Color::PrintError("Invalid table name: " + table);
                return 1;
            }

            int limit = 10;
            auto lIt = args.find("limit");
            if (lIt != args.end() && !lIt->second.empty()) {
                try {
                    limit = std::stoi(lIt->second);
                } catch (const std::exception&) {
                    Color::PrintError("Invalid --limit value: " + lIt->second);
                    return 1;
                }
            }
            if (limit <= 0) limit = 10;

            std::string sql =
                "SELECT * FROM " + table + " LIMIT " + std::to_string(limit);
            KeyValueTableArray rows =
                transaction.RunSqlStatementReturningKeyValueTableArray(sql);

            if (rows.empty()) {
                Color::PrintDim("(no rows in " + table + ")");
                return 0;
            }

            for (size_t i = 0; i < rows.size(); ++i) {
                std::cout << Color::kBoldCyan << "── " << table << " row "
                          << i << " ──" << Color::kReset << "\n";
                for (const auto& [key, val] : rows[i]) {
                    std::cout << "  " << Color::kCyan << key << Color::kReset
                              << ": " << val << "\n";
                }
            }
            std::cout << Color::kDim << rows.size() << " row(s) shown."
                      << Color::kReset << "\n";
            return 0;
        }
    });
}

}  // namespace TestHelper
