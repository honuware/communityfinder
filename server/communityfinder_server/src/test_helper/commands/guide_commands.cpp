#include "commands/guide_commands.h"

#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "business_logic/guide/guide_helper.h"
#include "db_schema/freshness.h"
#include "db_schema/guide_links.h"
#include "db_schema/guide_pages.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/organizations.h"
#include "db_schema/places.h"
#include "db_schema/scene_tags.h"
#include "db_schema/service_listings.h"
#include "db_schema/venue_profiles.h"
#include "db_schema/venues.h"
#include "sql_util/table_helpers/guide_links.h"
#include "sql_util/table_helpers/guide_pages.h"
#include "sql_util/table_helpers/neighborhoods.h"
#include "sql_util/table_helpers/organization_scene_tag_assignments.h"
#include "sql_util/table_helpers/organizations.h"
#include "sql_util/table_helpers/places.h"
#include "sql_util/table_helpers/scene_tags.h"
#include "sql_util/table_helpers/service_listings.h"
#include "sql_util/table_helpers/venue_profile_scene_tag_assignments.h"
#include "sql_util/table_helpers/venue_profiles.h"
#include "sql_util/table_helpers/venues.h"
#include "util/types.h"

#include "command_runner.h"
#include "console_colors.h"

namespace TestHelper {

namespace {

// Every demo row says where it came from, and claims nothing about a real place.
constexpr std::string_view kDemoDescription = "A demo listing from test_helper create-demo-guide.";
constexpr std::string_view kDemoClosureNote = "A demo closure note: closed in 2021.";

// The demo's slugs. A re-run finds these and leaves the rows alone.
constexpr std::string_view kDemoVenueSlug = "demo-bar";
constexpr std::string_view kDemoClosedVenueSlug = "demo-closed-bar";
constexpr std::string_view kDemoLeagueSlug = "demo-softball-league";
constexpr std::string_view kDemoChorusSlug = "demo-chorus";
constexpr std::string_view kDemoPlaceSlug = "demo-denny-blaine";
constexpr std::string_view kDemoPublishedPageSlug = "demo-published-page";
constexpr std::string_view kDemoDraftPageSlug = "demo-draft-page";

std::string Arg(const Args& args, const char* name) {
    auto it = args.find(name);
    return it != args.end() ? it->second : std::string();
}

// A whole number, or nullopt when the flag is missing or isn't one.
std::optional<int64_t> IntArg(const Args& args, const char* name) {
    std::string text = Arg(args, name);
    if (text.empty()) return std::nullopt;
    try {
        size_t used = 0;
        int64_t value = std::stoll(text, &used);
        if (used == text.size()) return value;
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

// microseconds since epoch → "YYYY-MM-DD HH:MM UTC".
std::string FormatUtc(int64_t us) {
    std::time_t seconds = static_cast<std::time_t>(us / 1'000'000);
    std::tm parts{};
#if defined(_WIN32)
    gmtime_s(&parts, &seconds);
#else
    gmtime_r(&seconds, &parts);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M UTC", &parts);
    return buffer;
}

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it != row.end() ? it->second : std::string();
}

int64_t IdColumn(const KeyValueTable& row, std::string_view column) {
    std::string value = Column(row, column);
    return value.empty() ? 0 : std::stoll(value);
}

// --- create-demo-guide ---

// One line of create-demo-guide's report.
struct DemoRow {
    std::string kind;
    int64_t id = 0;
    std::string slug;
    bool created = false;
    std::string notes;
};

// The seeded vocabulary the demo tags itself with. The test database has none
// of it and a trimmed dev database may lack some, so a missing slug is skipped
// and reported rather than fatal. 0 = not found.
struct DemoVocabulary {
    int64_t capitolHill = 0;
    int64_t leather = 0;
    int64_t bear = 0;
    int64_t jock = 0;
    std::vector<std::string> missing;
};

DemoVocabulary LoadDemoVocabulary(TestHelperContext& ctx, Transaction& transaction) {
    DemoVocabulary vocabulary;
    TableHelpers::SceneTags sceneTags(ctx.databaseHelper);
    auto sceneTag = [&](std::string_view slug) -> int64_t {
        int64_t id = IdColumn(sceneTags.GetBySlug(transaction, slug), DbSchema::kSceneTagsId);
        if (id == 0) vocabulary.missing.push_back("scene tag " + std::string(slug));
        return id;
    };
    vocabulary.leather = sceneTag("leather");
    vocabulary.bear = sceneTag("bear");
    vocabulary.jock = sceneTag("jock");
    vocabulary.capitolHill = IdColumn(
        TableHelpers::Neighborhoods(ctx.databaseHelper).GetBySlug(transaction, "capitol-hill"),
        DbSchema::kNeighborhoodsId);
    if (vocabulary.capitolHill == 0) vocabulary.missing.push_back("neighborhood capitol-hill");
    return vocabulary;
}

struct DemoVenueSpec {
    std::string_view name;
    std::string_view slug;
    std::string_view status;
    bool closed = false;     // with a closure note
    bool withLink = false;
    int64_t neighborhoodId = 0;
    int64_t sceneTagId = 0;
};

// A venue and its guide profile, or the pair already there under the profile's
// slug. The row's id is the venue's; the notes carry the profile's, which
// verify-listing --table=venue_profiles needs.
DemoRow AddDemoVenue(TestHelperContext& ctx, Transaction& transaction, const DemoVenueSpec& spec) {
    DemoRow row{ "venue", 0, std::string(spec.slug) };
    TableHelpers::VenueProfiles profiles(ctx.databaseHelper);
    KeyValueTable existing = profiles.GetProfileBySlug(transaction, spec.slug);
    int64_t profileId = 0;
    if (!existing.empty()) {
        row.id = IdColumn(existing, DbSchema::kVenueProfilesVenueId);
        profileId = IdColumn(existing, DbSchema::kVenueProfilesId);
    } else {
        KeyValueTable venue{
            { std::string(DbSchema::kVenuesName), std::string(spec.name) },
            { std::string(DbSchema::kVenuesDescription), std::string(kDemoDescription) },
            { std::string(DbSchema::kVenuesStatus), std::string(spec.status) },
        };
        if (spec.neighborhoodId > 0) {
            venue[std::string(DbSchema::kVenuesNeighborhoodId)] = StringFromInt(spec.neighborhoodId);
        }
        row.id = TableHelpers::Venues(ctx.databaseHelper).AddVenue(transaction, venue);

        KeyValueTable profile{
            { std::string(DbSchema::kVenueProfilesVenueId), StringFromInt(row.id) },
            { std::string(DbSchema::kVenueProfilesSlug), std::string(spec.slug) },
            { std::string(DbSchema::kVenueProfilesCrowdVibe), std::string(kDemoDescription) },
            { std::string(DbSchema::kVenueProfilesPriceBand), "$$" },
        };
        if (spec.closed) {
            profile[std::string(DbSchema::kVenueProfilesClosureNote)] = std::string(kDemoClosureNote);
        }
        profileId = profiles.AddProfile(transaction, profile);
        if (spec.sceneTagId > 0) {
            TableHelpers::VenueProfileSceneTagAssignments(ctx.databaseHelper)
                .Assign(transaction, profileId, spec.sceneTagId);
        }
        if (spec.withLink) {
            TableHelpers::GuideLinks(ctx.databaseHelper).AddLink(transaction, {
                { std::string(DbSchema::kGuideLinksLabel), "Demo link" },
                { std::string(DbSchema::kGuideLinksUrl), "https://example.org/" + std::string(spec.slug) },
                { std::string(DbSchema::kGuideLinksPlatform),
                  std::string(DbSchema::kGuideLinksPlatformWebsite) },
                { std::string(DbSchema::kGuideLinksVenueProfileId), StringFromInt(profileId) },
            });
        }
        row.created = true;
    }
    row.notes = std::string(spec.status) + (spec.closed ? " + closure note" : "")
                + (spec.withLink ? ", 1 link" : "") + ", profile " + StringFromInt(profileId);
    return row;
}

DemoRow AddDemoOrganization(TestHelperContext& ctx, Transaction& transaction,
                            std::string_view name, std::string_view slug,
                            std::string_view orgKind, int64_t neighborhoodId, int64_t sceneTagId) {
    DemoRow row{ "organization", 0, std::string(slug), false, std::string(orgKind) };
    TableHelpers::Organizations organizations(ctx.databaseHelper);
    KeyValueTable existing = organizations.GetOrganizationBySlug(transaction, slug);
    if (!existing.empty()) {
        row.id = IdColumn(existing, DbSchema::kOrganizationsId);
        return row;
    }
    KeyValueTable values{
        { std::string(DbSchema::kOrganizationsName), std::string(name) },
        { std::string(DbSchema::kOrganizationsSlug), std::string(slug) },
        { std::string(DbSchema::kOrganizationsOrgKind), std::string(orgKind) },
        { std::string(DbSchema::kOrganizationsDescription), std::string(kDemoDescription) },
    };
    if (neighborhoodId > 0) {
        values[std::string(DbSchema::kOrganizationsNeighborhoodId)] = StringFromInt(neighborhoodId);
    }
    row.id = organizations.AddOrganization(transaction, values);
    if (sceneTagId > 0) {
        TableHelpers::OrganizationSceneTagAssignments(ctx.databaseHelper)
            .Assign(transaction, row.id, sceneTagId);
    }
    row.created = true;
    return row;
}

// GD1's example of an 18+ place. Monthly while it's in the news: data, not
// schema (GD2 §1.3).
DemoRow AddDemoPlace(TestHelperContext& ctx, Transaction& transaction) {
    DemoRow row{ "place", 0, std::string(kDemoPlaceSlug), false, "18+, re-checked monthly" };
    TableHelpers::Places places(ctx.databaseHelper);
    KeyValueTable existing = places.GetPlaceBySlug(transaction, kDemoPlaceSlug);
    if (!existing.empty()) {
        row.id = IdColumn(existing, DbSchema::kPlacesId);
        return row;
    }
    row.id = places.AddPlace(transaction, {
        { std::string(DbSchema::kPlacesName), "Demo: Denny Blaine Park" },
        { std::string(DbSchema::kPlacesSlug), std::string(kDemoPlaceSlug) },
        { std::string(DbSchema::kPlacesPlaceKind), std::string(DbSchema::kPlacesPlaceKindParkBeach) },
        { std::string(DbSchema::kPlacesDescription), std::string(kDemoDescription) },
        { std::string(DbSchema::kPlacesAdult), "true" },
        { std::string(DbSchema::kPlacesVolatilityTier), std::string(DbSchema::kVolatilityTierMonthly) },
    });
    row.created = true;
    return row;
}

// A health-section page: published as of now, or a draft with no publish time.
DemoRow AddDemoPage(TestHelperContext& ctx, Transaction& transaction, std::string_view title,
                    std::string_view slug, bool published) {
    DemoRow row{ "guide page", 0, std::string(slug), false, published ? "published" : "draft" };
    TableHelpers::GuidePages pages(ctx.databaseHelper);
    KeyValueTable existing = pages.GetPageBySlug(transaction, slug);
    if (!existing.empty()) {
        row.id = IdColumn(existing, DbSchema::kGuidePagesId);
        return row;
    }
    KeyValueTable values{
        { std::string(DbSchema::kGuidePagesTitle), std::string(title) },
        { std::string(DbSchema::kGuidePagesSlug), std::string(slug) },
        { std::string(DbSchema::kGuidePagesSection), std::string(DbSchema::kGuidePagesSectionHealth) },
        { std::string(DbSchema::kGuidePagesBodyMarkdown),
          "A demo guide page from test_helper create-demo-guide." },
        { std::string(DbSchema::kGuidePagesDraft), published ? "false" : "true" },
    };
    if (published) {
        values[std::string(DbSchema::kGuidePagesPublishAtUs)] = StringFromInt(NowUs(transaction));
    }
    row.id = pages.AddPage(transaction, values);
    row.created = true;
    return row;
}

int RunCreateDemoGuide(TestHelperContext& ctx, Transaction& transaction, const Args& /*args*/) {
    DemoVocabulary vocabulary = LoadDemoVocabulary(ctx, transaction);

    std::vector<DemoRow> rows;
    rows.push_back(AddDemoVenue(ctx, transaction, {
        "Demo: Neighborhood Bar", kDemoVenueSlug, DbSchema::kFreshnessStatusOpen,
        false, true, vocabulary.capitolHill, vocabulary.leather }));
    rows.push_back(AddDemoVenue(ctx, transaction, {
        "Demo: Closed Bar", kDemoClosedVenueSlug, DbSchema::kFreshnessStatusClosed,
        true, false, vocabulary.capitolHill, vocabulary.bear }));
    rows.push_back(AddDemoOrganization(ctx, transaction, "Demo: Softball League", kDemoLeagueSlug,
                                       DbSchema::kOrganizationsOrgKindSportsLeague,
                                       vocabulary.capitolHill, vocabulary.jock));
    rows.push_back(AddDemoOrganization(ctx, transaction, "Demo: Community Chorus", kDemoChorusSlug,
                                       DbSchema::kOrganizationsOrgKindChorusPerformance, 0, 0));
    rows.push_back(AddDemoPlace(ctx, transaction));
    rows.push_back(AddDemoPage(ctx, transaction, "Demo: Published Page", kDemoPublishedPageSlug, true));
    rows.push_back(AddDemoPage(ctx, transaction, "Demo: Draft Page", kDemoDraftPageSlug, false));

    std::cout << Color::kBoldCyan << std::left
              << std::setw(14) << "Kind"
              << std::setw(7) << "ID"
              << std::setw(24) << "Slug"
              << std::setw(15) << "Result"
              << "Notes"
              << Color::kReset << "\n";
    int created = 0;
    for (const DemoRow& row : rows) {
        if (row.created) ++created;
        std::cout << std::setw(14) << row.kind
                  << std::setw(7) << row.id
                  << std::setw(24) << row.slug
                  << std::setw(15) << (row.created ? "created" : "already there")
                  << row.notes << "\n";
    }
    if (!vocabulary.missing.empty()) {
        std::string missing;
        for (const std::string& item : vocabulary.missing) {
            if (!missing.empty()) missing += ", ";
            missing += item;
        }
        Color::PrintWarning("Not in this database, so left off the demo: " + missing);
    }
    Color::PrintSuccess("Created " + std::to_string(created) + ", already there "
                        + std::to_string(rows.size() - created) + ".");
    Color::PrintDim("Next: publish-page --slug=" + std::string(kDemoDraftPageSlug)
                    + " · verify-listing --table=places --id=<id> · list-graveyard"
                    + " · GET /api/guide/venues/" + std::string(kDemoVenueSlug));
    return 0;
}

// --- publish-page ---

int RunPublishPage(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    std::string slug = Arg(args, "slug");
    if (slug.empty()) {
        Color::PrintError("--slug is required");
        return 1;
    }
    std::optional<Guide::GuidePage> page =
        Guide::GuideHelper(ctx.databaseHelper).GetGuidePageAnyState(transaction, slug);
    if (!page) {
        Color::PrintError("No guide page with slug " + slug);
        return 1;
    }
    if (page->published) {
        Color::PrintSuccess("Page " + StringFromInt(page->id) + " (" + slug + ") is already published");
        return 0;
    }

    // The publish act: no longer a draft, and public from now (§2.4's rule).
    int64_t nowUs = NowUs(transaction);
    TableHelpers::GuidePages(ctx.databaseHelper).UpdatePage(transaction, page->id, {
        { std::string(DbSchema::kGuidePagesDraft), "false" },
        { std::string(DbSchema::kGuidePagesPublishAtUs), StringFromInt(nowUs) },
    });
    Color::PrintSuccess("Published page " + StringFromInt(page->id) + ": " + page->title
                        + " (" + FormatUtc(nowUs) + ")");
    Color::PrintDim("Public now at /api/guide/pages/" + slug);
    return 0;
}

// --- verify-listing ---

// The tables a person verifies (GD1 §5): the guide's listings, plus venues,
// whose facts guide curators check too (GD2 OQ3).
struct ListingTable {
    std::function<KeyValueTable(Transaction&, int64_t)> get;
    std::function<void(Transaction&, int64_t, const KeyValueTable&)> update;
    std::string_view verifiedColumn;
    std::string_view nameColumn;  // what the report calls the row
};

constexpr std::string_view kListingTableNames =
    "venues, venue_profiles, organizations, places, service_listings, guide_pages";

std::optional<ListingTable> ListingTableFor(DatabaseHelper databaseHelper, std::string_view table) {
    using Get = std::function<KeyValueTable(Transaction&, int64_t)>;
    using Update = std::function<void(Transaction&, int64_t, const KeyValueTable&)>;
    if (table == DbSchema::kVenuesTable) {
        return ListingTable{
            Get([=](Transaction& t, int64_t id) { return TableHelpers::Venues(databaseHelper).GetVenue(t, id); }),
            Update([=](Transaction& t, int64_t id, const KeyValueTable& u) { TableHelpers::Venues(databaseHelper).UpdateVenue(t, id, u); }),
            DbSchema::kVenuesLastVerifiedAtUs, DbSchema::kVenuesName };
    }
    if (table == DbSchema::kVenueProfilesTable) {
        return ListingTable{
            Get([=](Transaction& t, int64_t id) { return TableHelpers::VenueProfiles(databaseHelper).GetProfile(t, id); }),
            Update([=](Transaction& t, int64_t id, const KeyValueTable& u) { TableHelpers::VenueProfiles(databaseHelper).UpdateProfile(t, id, u); }),
            DbSchema::kVenueProfilesLastVerifiedAtUs, DbSchema::kVenueProfilesSlug };
    }
    if (table == DbSchema::kOrganizationsTable) {
        return ListingTable{
            Get([=](Transaction& t, int64_t id) { return TableHelpers::Organizations(databaseHelper).GetOrganization(t, id); }),
            Update([=](Transaction& t, int64_t id, const KeyValueTable& u) { TableHelpers::Organizations(databaseHelper).UpdateOrganization(t, id, u); }),
            DbSchema::kOrganizationsLastVerifiedAtUs, DbSchema::kOrganizationsName };
    }
    if (table == DbSchema::kPlacesTable) {
        return ListingTable{
            Get([=](Transaction& t, int64_t id) { return TableHelpers::Places(databaseHelper).GetPlace(t, id); }),
            Update([=](Transaction& t, int64_t id, const KeyValueTable& u) { TableHelpers::Places(databaseHelper).UpdatePlace(t, id, u); }),
            DbSchema::kPlacesLastVerifiedAtUs, DbSchema::kPlacesName };
    }
    if (table == DbSchema::kServiceListingsTable) {
        return ListingTable{
            Get([=](Transaction& t, int64_t id) { return TableHelpers::ServiceListings(databaseHelper).GetServiceListing(t, id); }),
            Update([=](Transaction& t, int64_t id, const KeyValueTable& u) { TableHelpers::ServiceListings(databaseHelper).UpdateServiceListing(t, id, u); }),
            DbSchema::kServiceListingsLastVerifiedAtUs, DbSchema::kServiceListingsName };
    }
    if (table == DbSchema::kGuidePagesTable) {
        return ListingTable{
            Get([=](Transaction& t, int64_t id) { return TableHelpers::GuidePages(databaseHelper).GetPage(t, id); }),
            Update([=](Transaction& t, int64_t id, const KeyValueTable& u) { TableHelpers::GuidePages(databaseHelper).UpdatePage(t, id, u); }),
            DbSchema::kGuidePagesLastVerifiedAtUs, DbSchema::kGuidePagesTitle };
    }
    return std::nullopt;
}

int RunVerifyListing(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    std::string table = Arg(args, "table");
    std::optional<ListingTable> listing = ListingTableFor(ctx.databaseHelper, table);
    if (!listing) {
        Color::PrintError("--table must be one of: " + std::string(kListingTableNames));
        return 1;
    }
    std::optional<int64_t> id = IntArg(args, "id");
    if (!id) {
        Color::PrintError("--id must be a row id");
        return 1;
    }
    KeyValueTable row = listing->get(transaction, *id);
    if (row.empty()) {
        Color::PrintError("No " + table + " row with id " + StringFromInt(*id));
        return 1;
    }

    // The verification act (GD1 §5): a person has just confirmed the facts.
    int64_t nowUs = NowUs(transaction);
    listing->update(transaction, *id, {
        { std::string(listing->verifiedColumn), StringFromInt(nowUs) },
    });
    Color::PrintSuccess("Verified " + table + " " + StringFromInt(*id) + " ("
                        + Column(row, listing->nameColumn) + ") at " + FormatUtc(nowUs));
    if (table == DbSchema::kVenuesTable || table == DbSchema::kVenueProfilesTable) {
        Color::PrintDim("A guide venue's stamp is the older of its venue's and its profile's "
                        "verified dates, so verify both (GD2 OQ11 (a)).");
    }
    return 0;
}

// --- list-graveyard ---

int RunListGraveyard(TestHelperContext& ctx, Transaction& transaction, const Args& /*args*/) {
    Guide::Graveyard graveyard = Guide::GuideHelper(ctx.databaseHelper).GetGraveyard(transaction);
    if (graveyard.venues.empty() && graveyard.organizations.empty()) {
        Color::PrintDim("(empty — a venue or organization joins the graveyard when its status is "
                        "closed; create-demo-guide adds one)");
        return 0;
    }
    std::cout << Color::kBoldCyan << std::left
              << std::setw(14) << "Kind"
              << std::setw(7) << "ID"
              << std::setw(24) << "Slug"
              << std::setw(30) << "Name"
              << "Closure note"
              << Color::kReset << "\n";
    auto print = [](std::string_view kind, int64_t id, const std::string& slug,
                    const std::string& name, const std::string& closureNote) {
        std::cout << std::setw(14) << kind
                  << std::setw(7) << id
                  << std::setw(24) << slug
                  << std::setw(30) << name
                  << (closureNote.empty() ? "-" : closureNote) << "\n";
    };
    for (const Guide::GuideVenue& venue : graveyard.venues) {
        print("venue", venue.venueId, venue.slug, venue.name, venue.closureNote);
    }
    for (const Guide::GuideOrganization& organization : graveyard.organizations) {
        print("organization", organization.id, organization.slug, organization.name,
              organization.closureNote);
    }
    Color::PrintDim(std::to_string(graveyard.venues.size() + graveyard.organizations.size())
                    + " closed listing(s), as /api/guide/graveyard serves them.");
    return 0;
}

}  // namespace

void RegisterGuideCommands(CommandRegistry& registry) {
    registry.Register({
        "create-demo-guide", "dg", "Guide",
        "Create a small demo guide (safe to re-run): two venues (one closed), two orgs, "
        "an 18+ place, a published page and a draft",
        {},
        RunCreateDemoGuide
    });

    registry.Register({
        "publish-page", "pp", "Guide",
        "Publish a guide page now: no longer a draft, publish time = now",
        {{ "--slug", "Guide page slug, e.g. demo-draft-page", true }},
        RunPublishPage
    });

    registry.Register({
        "verify-listing", "vl", "Guide",
        "Mark a listing verified now (sets last_verified_at_us: the verification act)",
        {{ "--table", "venues | venue_profiles | organizations | places | service_listings | guide_pages", true },
         { "--id", "Row id", true }},
        RunVerifyListing
    });

    registry.Register({
        "list-graveyard", "lg", "Guide",
        "List the closed venues and organizations, as /api/guide/graveyard serves them",
        {},
        RunListGraveyard
    });
}

}  // namespace TestHelper
