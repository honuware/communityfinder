#include <filesystem>
#include <cstdlib>
#include <string>

#include "db_schema/make_database_info.h"
#include "endpoints/web_app.h"
#include "endpoints/endpoint_registrations.h"
#include "endpoints/security_headers.h"
#include "util/secrets/secrets_helper.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_at_rest.h"
#include "util/mail/mail_helper.h"
#include "util/types.h"
#include "util/logging.h"
#include "sql_util/database_access/production_transaction_provider.h"
#include "business_logic/app_database_config.h"
#include "business_logic/auth/server_config.h"
#include "business_logic/auth/cookie_manager.h"
#include "sql_util/table_helpers/admin_column_redactions.h"
#include "business_logic/tenancy/tenant_context.h"
#include "business_logic/tenancy/tenant_header.h"
#include "business_logic/tenancy/tenant_resolver.h"
#include "business_logic/tenancy/tenant_resources.h"
#include "business_logic/tenancy/control_database.h"
#include "sql_util/table_helpers/tenants.h"
#include "db_schema/tenants.h"

int main() {
    InitializeLogging();
    char* port = getenv("PORT");
    uint16_t iPort = static_cast<uint16_t>(port ? std::stoi(port) : 18081);
    LogInfo() << "PORT = " << iPort << "\n";
    LogInfo() << "Current path is " << std::filesystem::current_path() << "\n";
    DatabaseHelper databaseHelper = MakeProductionDatabaseHelper(App::kDatabaseName);
    TransactionProviderPtr transactionProvider =
        MakeProductionTransactionProvider(databaseHelper);
    std::unique_ptr<DbSchema::DatabaseInfo> databaseInfo;
    databaseInfo = std::make_unique<DbSchema::DatabaseInfo>(
        DbSchema::MakeDatabaseInfo(App::kDatabaseName));

    // At-rest secrets bootstrap (sequence matters):
    //   1. Build a plaintext SecretsHelper just to read the prod-mode flag.
    //   2. Construct SecretsAtRest. In prod, fails fast if HONUWARE_SECRET_KEY is
    //      unset/wrong; in dev, falls back to a fixed dev key with a [DEV] log line.
    //   3. Migrate any legacy plaintext config_secrets rows in place (idempotent).
    //   4. Re-create the SecretsHelper wired through SecretsAtRest — every later
    //      read decrypts, every write encrypts.
    Secrets::SecretsHelperPtr secretsHelper =
        Secrets::MakeSecretsHelper(databaseHelper);
    bool isProd = false;
    transactionProvider->RunInTransaction([&](Transaction& transaction) {
        std::string val = secretsHelper->LookupSecret(
            transaction, Secrets::kServerProductionMode);
        isProd = StringToBool(val);
    });

    Secrets::SecretsAtRestPtr secretsAtRest = Secrets::MakeSecretsAtRest(isProd);

    transactionProvider->RunInTransaction([&](Transaction& transaction) {
        Secrets::MigrateSecretsToEncrypted(
            transaction, databaseHelper, secretsAtRest);
    });

    secretsHelper = Secrets::MakeSecretsHelper(databaseHelper, secretsAtRest);

    Mail::MailHelperPtr mailHelper;
    Auth::CookieManagerFactoryPtr cookieManagerFactory = Auth::MakeCookieManagerFactory();
    TableHelpers::ColumnRedactionSet columnRedactions;
    transactionProvider->RunInTransaction([&](Transaction& transaction) {
        mailHelper = Mail::MakeMailHelper(transaction, secretsHelper);

        // Load the column redaction set once at startup — it's read on every CRUD
        // response, so we don't re-query it per request.
        TableHelpers::AdminColumnRedactions redactionsHelper(databaseHelper);
        columnRedactions = redactionsHelper.LoadColumnRedactionSet(transaction);
    });
    WebApp webApp(
        databaseHelper,
        databaseHelper.GetDatabaseName(),
        *databaseInfo.get(),
        secretsHelper,
        mailHelper,
        transactionProvider,
        cookieManagerFactory,
        std::move(columnRedactions));
    // The SecurityHeaders middleware emits no `Server` header by default (brand-free
    // framework); the app opts in with its brand.
    webApp.GetApp().get_middleware<Endpoints::SecurityHeaders>().SetServerBanner(
        "CommunityFinder");
    // Install a tenant resolver + resource registry on the WebApp per the
    // HONUWARE_TENANT_MODE switch. CommunityFinder uses the framework's default
    // per-tenant resources factory (no per-tenant app services yet — no Square);
    // each tenant gets its own SecretsHelper over its own database via the GLOBAL
    // at-rest master key.
    {
        const char* modeEnv = std::getenv("HONUWARE_TENANT_MODE");
        const bool controlMode =
            modeEnv != nullptr && std::string(modeEnv) == "control";

        Tenancy::TenantResourceRegistry::Factory tenantFactory =
            [secretsAtRest](const Tenancy::TenantContext& tenantContext) {
                return Tenancy::MakeDefaultTenantResources(tenantContext, secretsAtRest);
            };

        if (controlMode) {
            // Control mode (multi-community): resolve site keys against the control
            // database's `tenants` table. A control-mode process MUST NOT set
            // HONUWARE_DB_NAME — the per-tenant pooled provider honors that override
            // and would misroute every tenant to one database.
            Tenancy::EnsureControlDatabase();
            DatabaseHelper controlDatabaseHelper = Tenancy::MakeControlDatabaseHelper();
            TransactionProviderPtr controlProvider =
                MakeProductionTransactionProvider(controlDatabaseHelper);
            webApp.SetTenantResourceRegistry(
                std::make_shared<Tenancy::TenantResourceRegistry>(tenantFactory));
            webApp.SetTenantResolver(
                std::make_shared<Tenancy::ControlDbTenantResolver>(
                    controlProvider, controlDatabaseHelper),
                Tenancy::TenancyMode::Control);
            int64_t activeCount = 0;
            controlProvider->RunInTransaction([&](Transaction& transaction) {
                TableHelpers::Tenants tenants(controlDatabaseHelper);
                activeCount =
                    static_cast<int64_t>(tenants.ListActive(transaction).size());
            });
            LogInfo() << "Tenancy: control mode — " << activeCount
                      << " active tenant(s) in control database '"
                      << Tenancy::ControlDatabaseName() << "'\n";
        } else {
            // Fixed mode (default): one configured tenant, no control database — the
            // zero-ceremony path for local dev and single-community consumers. The
            // site key defaults to the app database name; HONUWARE_FIXED_SITE_KEY
            // overrides it when a deployment fronts this single tenant with a
            // specific CloudFront site header.
            const char* fixedSiteKey = std::getenv("HONUWARE_FIXED_SITE_KEY");
            Tenancy::TenantContext appTenantContext;
            appTenantContext.tenantId = 1;
            appTenantContext.siteKey =
                (fixedSiteKey != nullptr && fixedSiteKey[0] != '\0')
                    ? std::string(fixedSiteKey)
                    : std::string(App::kDatabaseName);
            appTenantContext.databaseName = std::string(App::kDatabaseName);
            appTenantContext.status = std::string(DbSchema::kTenantStatusActive);
            appTenantContext.maxConnections = 1;
            webApp.SetTenantResourceRegistry(
                std::make_shared<Tenancy::TenantResourceRegistry>(tenantFactory));
            webApp.SetTenantResolver(
                std::make_shared<Tenancy::FixedTenantResolver>(appTenantContext),
                Tenancy::TenancyMode::Fixed);
            LogInfo() << "Tenancy: fixed mode — site '"
                      << appTenantContext.siteKey << "' -> database '"
                      << appTenantContext.databaseName << "'\n";
        }
    }
    // Anchor the app endpoint registration TU (web_app.cpp) into the link before
    // routes are collected; otherwise the static library would dead-strip it and
    // every route would silently disappear. See endpoint_registrations.h.
    Endpoints::RegisterAllEndpoints();
    RoutingBase::AddRoutes(webApp);
    transactionProvider->RunInTransaction([&](Transaction& transaction) {
        Auth::ServerConfig::Initialize(transaction, secretsHelper, &webApp);
        // Fail loud at startup when prod-mode is on but a required guard env var or
        // secret is missing — the throw propagates so the process exits non-zero.
        Auth::ServerConfig::ValidateProdEnvironment(transaction, secretsHelper);
    });
    // One-line startup summary of the security posture (dev AND prod).
    LogInfo() << Auth::ServerConfig::BuildSecurityPostureSummary() << "\n";
    webApp.GetApp().port(iPort).multithreaded().run();

    return 0;
}
