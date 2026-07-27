#include <iostream>
#include <memory>
#include <string>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>

#include "command_registry.h"
#include "command_runner.h"
#include "console_colors.h"
#include "dashboard/dashboard.h"
#include "repl/repl.h"

ABSL_FLAG(std::string, command, "", "Run a single command and exit (use 'help' to list)");
ABSL_FLAG(bool, repl, false, "Start in command-line mode (skip dashboard)");
ABSL_FLAG(bool, send_real_email, true, "Send real emails (use --nosend_real_email to capture instead)");

// Generic command flags (used in one-shot mode). Domain-specific flags get added
// alongside each phase's simulation commands.
ABSL_FLAG(std::string, email, "", "Email address");
ABSL_FLAG(std::string, key, "", "Generic key");
ABSL_FLAG(std::string, value, "", "Generic value");
ABSL_FLAG(std::string, table, "", "Table name (for peek)");
ABSL_FLAG(int64_t, limit, 0, "Row limit (for peek)");

int main(int argc, char** argv) {
    absl::ParseCommandLine(argc, argv);
    TestHelper::EnableAnsiColors();

    bool sendRealEmail = absl::GetFlag(FLAGS_send_real_email);
    std::string command = absl::GetFlag(FLAGS_command);

    // Create the shared context
    std::cout << "Connecting to database...\n";
    std::unique_ptr<TestHelper::TestHelperContext> ctx;
    try {
        ctx = TestHelper::CreateContext(sendRealEmail);
    } catch (const std::exception& e) {
        TestHelper::Color::PrintError(std::string("Failed to connect to database: ") + e.what());
        return 1;
    }

    std::cout << "Connected to: " << ctx->databaseHelper.GetDatabaseName() << "\n";

    // Try to auto-login as the default user
    if (TestHelper::LoginAsUser(*ctx, "masonbendixen@gmail.com")) {
        std::cout << "Logged in as " << ctx->currentPersonName
                  << " (" << ctx->currentPersonEmail << ") ID:"
                  << ctx->currentPersonId << "\n";
    } else {
        TestHelper::Color::PrintWarning("Default user not found. Use 'login --email=X' to log in.");
    }

    // Mode 1: One-shot command
    if (!command.empty()) {
        // Build args from a few generic flags
        TestHelper::Args args;
        auto addStr = [&](const std::string& name, const std::string& val) {
            if (!val.empty()) args[name] = val;
        };

        addStr("email", absl::GetFlag(FLAGS_email));
        addStr("key", absl::GetFlag(FLAGS_key));
        addStr("value", absl::GetFlag(FLAGS_value));
        addStr("table", absl::GetFlag(FLAGS_table));
        int64_t limit = absl::GetFlag(FLAGS_limit);
        if (limit != 0) args["limit"] = std::to_string(limit);

        return TestHelper::ExecuteCommand(*ctx, command, args);
    }

    // Mode 3: REPL only
    if (absl::GetFlag(FLAGS_repl)) {
        std::cout << "\n";
        TestHelper::RunRepl(*ctx);
        return 0;
    }

    // Mode 2: Dashboard (default)
    std::cout << "\n";
    return TestHelper::RunDashboard(*ctx);
}
