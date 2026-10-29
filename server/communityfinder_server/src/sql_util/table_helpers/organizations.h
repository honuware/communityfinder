#pragma once

#include <cstdint>
#include <string_view>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// GD2 §2.2 — CRUD for organizations: leagues, choruses, clubs, recovery
// groups, congregations.
class Organizations {
public:
    explicit Organizations(DatabaseHelper databaseHelper);
    Organizations(const Organizations&) = default;
    Organizations& operator=(const Organizations&) = default;
    ~Organizations() = default;

    // CREATE — throws on a duplicate slug or an org_kind outside GD1's list.
    int64_t AddOrganization(Transaction& transaction, const KeyValueTable& values);

    // READ — empty when there is no such organization.
    KeyValueTable GetOrganization(Transaction& transaction, int64_t id) const;

    // READ — by slug (the organization page's URL key); empty when there is none.
    KeyValueTable GetOrganizationBySlug(Transaction& transaction, std::string_view slug) const;

    // READ — one kind's organizations, any status, ordered by name then id.
    KeyValueTableArray GetOrganizationsByKind(
        Transaction& transaction, std::string_view orgKind) const;

    // READ — the organizations in a neighborhood, any status, ordered by name
    // then id. Countywide ones (no neighborhood) are in none.
    KeyValueTableArray GetOrganizationsByNeighborhood(
        Transaction& transaction, int64_t neighborhoodId) const;

    // READ — every organization, ordered by name then id. Without includeClosed,
    // the retired statuses (closed, moved) are left out, as in GetActiveSeries;
    // seasonal and unverified stay.
    KeyValueTableArray GetOrganizations(Transaction& transaction, bool includeClosed) const;

    // UPDATE — bumps updated_us. A value of "NULL" clears a nullable column.
    void UpdateOrganization(Transaction& transaction, int64_t id, const KeyValueTable& updates);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
