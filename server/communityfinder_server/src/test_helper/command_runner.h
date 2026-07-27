#pragma once

#include <memory>
#include <string>

#include "command_registry.h"
#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_access/transaction_provider.h"
#include "util/secrets/secrets_helper.h"
#include "util/mail/mail_helper.h"

namespace TestHelper {

struct TestHelperContext {
    DatabaseHelper databaseHelper;
    TransactionProviderPtr transactionProvider;
    Secrets::SecretsHelperPtr secretsHelper;
    Mail::MailHelperPtr mailHelper;
    CommandRegistry registry;

    // Current logged-in user
    int64_t currentPersonId = 0;
    std::string currentPersonName;
    std::string currentPersonEmail;

    // Config
    bool sendRealEmail = false;

    explicit TestHelperContext(DatabaseHelper dbHelper)
        : databaseHelper(dbHelper) {}
};

// Create the shared context with real production services.
// Connects to the database (App::kDatabaseName), reads secrets (decrypting at
// rest to mirror the server bootstrap). Mail uses the captured test helper by
// default unless sendRealEmail is true.
// Returns unique_ptr because DatabaseHelper has no default constructor.
std::unique_ptr<TestHelperContext> CreateContext(bool sendRealEmail);

// Register all commands into the context's registry.
void RegisterAllCommands(CommandRegistry& registry);

// Execute a single command by name with the given args.
// Returns 0 on success, non-zero on error.
int ExecuteCommand(TestHelperContext& ctx, const std::string& commandName, const Args& args);

// Look up a person by email and set as the current user.
// Returns true if found.
bool LoginAsUser(TestHelperContext& ctx, const std::string& email);

// Look up a person by ID and set as the current user.
bool LoginAsUser(TestHelperContext& ctx, int64_t personId);

}  // namespace TestHelper
