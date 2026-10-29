#include "guide_pages.h"

#include "db_schema/freshness.h"
#include "db_schema/guide_pages.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

// The publish rule, repeated verbatim in both statements below:
//     draft = false AND publish_at_us IS NOT NULL AND publish_at_us <= now_us()
// Spelled out each time, as in knottyyoga's blog_posts, so every statement
// stays one readable literal.
constexpr std::string_view kSqlGetPublishedBySlug =
    "SELECT * FROM guide_pages "
    "WHERE slug = $1 "
    "AND draft = false AND publish_at_us IS NOT NULL AND publish_at_us <= now_us()";

constexpr std::string_view kSqlGetPublishedBySection =
    "SELECT id, slug, title, section, adult, publish_at_us, status, last_verified_at_us "
    "FROM guide_pages "
    "WHERE section = $1 "
    "AND draft = false AND publish_at_us IS NOT NULL AND publish_at_us <= now_us() "
    "ORDER BY title ASC, id ASC";

}  // namespace

GuidePages::GuidePages(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t GuidePages::AddPage(Transaction& transaction, const KeyValueTable& values) {
    KeyValueTable row = values;
    const std::string sectionColumn(DbSchema::kGuidePagesSection);
    const std::string tierColumn(DbSchema::kGuidePagesVolatilityTier);
    if (!row.contains(tierColumn) && row.contains(sectionColumn)) {
        const std::string& section = row.at(sectionColumn);
        if (section == DbSchema::kGuidePagesSectionAdult) {
            row[tierColumn] = std::string(DbSchema::kVolatilityTierMonthly);
        } else if (section == DbSchema::kGuidePagesSectionHealth) {
            row[tierColumn] = std::string(DbSchema::kVolatilityTierSemiannual);
        }
    }
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kGuidePagesTable,
        row,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTable GuidePages::GetPage(Transaction& transaction, int64_t id) const {
    return DbCrud::GetRow(
        transaction,
        databaseHelper_,
        DbSchema::kGuidePagesTable,
        DbSchema::kGuidePagesId,
        StringFromInt(id));
}

KeyValueTable GuidePages::GetPageBySlug(Transaction& transaction, std::string_view slug) const {
    return DbCrud::LookupRowByValue(
        transaction,
        databaseHelper_,
        DbSchema::kGuidePagesTable,
        DbSchema::kGuidePagesSlug,
        slug);
}

KeyValueTable GuidePages::GetPublishedPageBySlug(
    Transaction& transaction, std::string_view slug) const {
    KeyValueTableArray rows = transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetPublishedBySlug), std::string(slug));
    return rows.empty() ? KeyValueTable{} : rows.front();
}

KeyValueTableArray GuidePages::GetPublishedPagesBySection(
    Transaction& transaction, std::string_view section) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetPublishedBySection), std::string(section));
}

void GuidePages::UpdatePage(Transaction& transaction, int64_t id, const KeyValueTable& updates) {
    KeyValueTable values = updates;
    values[std::string(DbSchema::kGuidePagesUpdatedUs)] =
        std::string(DbSchema::kDatabaseInfoDefaultNow);
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kGuidePagesTable,
        DbSchema::kGuidePagesId,
        StringFromInt(id),
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

}  // namespace TableHelpers
