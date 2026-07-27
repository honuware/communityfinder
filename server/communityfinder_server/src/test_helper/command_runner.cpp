#include "command_runner.h"

#include <iostream>

#include "commands/utility_commands.h"
#include "console_colors.h"
#include "business_logic/app_database_config.h"
#include "db_schema/people.h"
#include "sql_util/database_access/production_transaction_provider.h"
#include "sql_util/table_helpers/people.h"
#include "util/mail/mail_helper.h"
#include "util/mail/mail_helper_test_util.h"
#include "util/secrets/secrets_helper.h"
#include "util/secrets/secrets_at_rest.h"
#include "util/secrets/secret_keys.h"
#include "util/types.h"

namespace TestHelper {

std::unique_ptr<TestHelperContext> CreateContext(bool sendRealEmail) {
    DatabaseHelper databaseHelper = MakeProductionDatabaseHelper(App::kDatabaseName);
    TransactionProviderPtr transactionProvider =
        MakeProductionTransactionProvider(databaseHelper);
    // Phase 8.1 of the security review encrypted config_secrets.value at
    // rest, so the helper must mirror the server's bootstrap (main.cpp):
    // a plain SecretsHelper first to resolve production mode, then the
    // real one wrapping SecretsAtRest so every read decrypts. Without
    // this, LookupSecret returns raw "v1:..." ciphertext and things like
    // the mail port blow up with "invalid stoul argument".
    Secrets::SecretsHelperPtr secretsHelper =
        Secrets::MakeSecretsHelper(databaseHelper);
    bool isProd = false;
    transactionProvider->RunInTransaction([&](Transaction& transaction) {
        std::string val = secretsHelper->LookupSecret(
            transaction, Secrets::kServerProductionMode);
        isProd = StringToBool(val);
    });
    Secrets::SecretsAtRestPtr secretsAtRest = Secrets::MakeSecretsAtRest(isProd);
    secretsHelper = Secrets::MakeSecretsHelper(databaseHelper, secretsAtRest);

    Mail::MailHelperPtr mailHelper;

    transactionProvider->RunInTransaction([&](Transaction& transaction) {
        if (sendRealEmail) {
            mailHelper = Mail::MakeMailHelper(transaction, secretsHelper);
        } else {
            mailHelper = Mail::Test::MakeTestMailHelper();
        }
    });

    auto ctx = std::make_unique<TestHelperContext>(databaseHelper);
    ctx->transactionProvider = std::move(transactionProvider);
    ctx->secretsHelper = std::move(secretsHelper);
    ctx->mailHelper = std::move(mailHelper);
    ctx->sendRealEmail = sendRealEmail;

    RegisterAllCommands(ctx->registry);

    return ctx;
}

void RegisterAllCommands(CommandRegistry& registry) {
    // Login is framework-level: it needs LoginAsUser (below). Everything else
    // (help / status / peek + later-phase simulation commands) lives in the
    // utility command file.
    registry.Register({
        "login", "", "Utility",
        "Switch the current user context",
        {
            { "--email", "Email address to login as", false },
            { "--person_id", "Person ID to login as", false },
        },
        [](TestHelperContext& ctx, Transaction&, const Args& args) -> int {
            auto emailIt = args.find("email");
            if (emailIt != args.end() && !emailIt->second.empty()) {
                if (LoginAsUser(ctx, emailIt->second)) {
                    std::cout << "Logged in as " << ctx.currentPersonName
                              << " (" << ctx.currentPersonEmail << ") ID:"
                              << ctx.currentPersonId << "\n";
                    return 0;
                }
                std::cout << "User not found: " << emailIt->second << "\n";
                return 1;
            }
            auto pidIt = args.find("person_id");
            if (pidIt != args.end() && !pidIt->second.empty()) {
                int64_t pid = std::stoll(pidIt->second);
                if (LoginAsUser(ctx, pid)) {
                    std::cout << "Logged in as " << ctx.currentPersonName
                              << " (" << ctx.currentPersonEmail << ") ID:"
                              << ctx.currentPersonId << "\n";
                    return 0;
                }
                std::cout << "Person not found: " << pidIt->second << "\n";
                return 1;
            }
            std::cout << "Specify --email=<email> or --person_id=<id>\n";
            return 1;
        }
    });

    RegisterUtilityCommands(registry);
}

int ExecuteCommand(TestHelperContext& ctx, const std::string& commandName, const Args& args) {
    const CommandInfo* cmd = ctx.registry.Find(commandName);
    if (!cmd) {
        Color::PrintError("Unknown command: " + commandName);
        std::cout << Color::kDim << "Use 'help' to list available commands." << Color::kReset << "\n";
        return 1;
    }

    int result = 0;
    try {
        ctx.transactionProvider->RunInTransaction([&](Transaction& transaction) {
            result = cmd->execute(ctx, transaction, args);
        });
    } catch (const std::exception& e) {
        Color::PrintError(std::string("Command '") + commandName + "' failed: " + e.what());
        return 1;
    }
    return result;
}

bool LoginAsUser(TestHelperContext& ctx, const std::string& email) {
    bool found = false;
    ctx.transactionProvider->RunInTransaction([&](Transaction& transaction) {
        TableHelpers::People people(ctx.databaseHelper);
        KeyValueTable personKv = people.LookupPersonByEmail(transaction, email);
        if (!personKv.empty()) {
            ctx.currentPersonId = std::stoll(
                personKv.at(std::string(DbSchema::kPeopleId)));
            ctx.currentPersonName =
                personKv.at(std::string(DbSchema::kPeopleFirstName)) + " " +
                personKv.at(std::string(DbSchema::kPeopleLastName));
            ctx.currentPersonEmail = email;
            found = true;
        }
    });
    return found;
}

bool LoginAsUser(TestHelperContext& ctx, int64_t personId) {
    bool found = false;
    ctx.transactionProvider->RunInTransaction([&](Transaction& transaction) {
        TableHelpers::People people(ctx.databaseHelper);
        KeyValueTable personKv = people.GetPersonById(transaction, personId);
        if (!personKv.empty()) {
            ctx.currentPersonId = personId;
            ctx.currentPersonName =
                personKv.at(std::string(DbSchema::kPeopleFirstName)) + " " +
                personKv.at(std::string(DbSchema::kPeopleLastName));
            ctx.currentPersonEmail =
                personKv.at(std::string(DbSchema::kPeopleEmail));
            found = true;
        }
    });
    return found;
}

}  // namespace TestHelper
