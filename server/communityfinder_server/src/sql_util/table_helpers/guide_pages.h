#pragma once

#include <cstdint>
#include <string_view>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// GD2 §2.4 — CRUD for guide_pages, the CMS-lite layer.
//
// The publish rule is knottyyoga's blog predicate, and it lives in this
// helper's SQL:
//     draft = false AND publish_at_us IS NOT NULL AND publish_at_us <= now_us()
// Clearing `draft` is not enough: a page with no publish time was never
// scheduled. The GetPublished* reads apply it; the others are the author's
// view (any state), for the preview path and the editors.
class GuidePages {
public:
    explicit GuidePages(DatabaseHelper databaseHelper);
    GuidePages(const GuidePages&) = default;
    GuidePages& operator=(const GuidePages&) = default;
    ~GuidePages() = default;

    // CREATE — throws on a duplicate slug, a section outside GD1's list, or an
    // adult-section page without `adult` set. With no volatility_tier of its
    // own, the cadence follows the section (GD2 OQ9 (a)): adult monthly, health
    // semiannual, everything else the column default, annual.
    int64_t AddPage(Transaction& transaction, const KeyValueTable& values);

    // READ — any state; empty when there is no such page.
    KeyValueTable GetPage(Transaction& transaction, int64_t id) const;

    // READ — any state, by slug (the preview path); empty when there is none.
    KeyValueTable GetPageBySlug(Transaction& transaction, std::string_view slug) const;

    // READ — by slug, only when published; empty otherwise.
    KeyValueTable GetPublishedPageBySlug(Transaction& transaction, std::string_view slug) const;

    // READ — a section's published pages in list shape (no body): id, slug,
    // title, section, adult, publish_at_us, and the freshness fields (status,
    // last_verified_at_us). Ordered by title then id.
    KeyValueTableArray GetPublishedPagesBySection(
        Transaction& transaction, std::string_view section) const;

    // UPDATE — bumps updated_us. A value of "NULL" clears a nullable column.
    // Leaves volatility_tier alone, even when the section changes.
    void UpdatePage(Transaction& transaction, int64_t id, const KeyValueTable& updates);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
