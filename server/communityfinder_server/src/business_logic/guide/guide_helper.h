#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/table_helpers/guide_links.h"
#include "sql_util/table_helpers/guide_page_scene_tag_assignments.h"
#include "sql_util/table_helpers/guide_pages.h"
#include "sql_util/table_helpers/neighborhoods.h"
#include "sql_util/table_helpers/organization_scene_tag_assignments.h"
#include "sql_util/table_helpers/organizations.h"
#include "sql_util/table_helpers/place_scene_tag_assignments.h"
#include "sql_util/table_helpers/places.h"
#include "sql_util/table_helpers/scene_tags.h"
#include "sql_util/table_helpers/service_listings.h"
#include "sql_util/table_helpers/venue_profile_scene_tag_assignments.h"
#include "sql_util/table_helpers/venue_profiles.h"
#include "sql_util/table_helpers/venues.h"

namespace Guide {

// GD2 §3 — the guide's domain layer: scene, directory, neighborhood and
// graveyard assembly, and the publish rule for guide pages. Read-only; curators
// edit through the admin CRUD editor.
//
// What every list shows:
//   - Directories, scene pages and neighborhood pages leave out retired
//     listings (closed, moved; GD2 OQ10 (a)). The graveyard shows the closed
//     ones, and a closed listing keeps its own page.
//   - A "guide venue" is a venue with a profile. A venue without one isn't in
//     the guide yet.
//   - Scene-tag and neighborhood slugs name enabled vocabulary only, so a
//     disabled scene or neighborhood drops off every item and its page is gone.
//     That's EV1's rule for neighborhoods (Events::PublicEvent), applied to
//     scenes too.

// The freshness every listing carries to the public (Interface Contract 3,
// GD1 §5): the stamp ("Verified {Month Year}") and the closed marker render
// from these. Vocabulary summaries (scenes, neighborhoods) and links aren't
// listings and carry none.
struct Freshness {
    std::string status;
    std::optional<int64_t> lastVerifiedAtUs;  // nullopt = never verified
};

// "Where this scene talks": an outbound link.
struct GuideLink {
    int64_t id = 0;
    std::string label;
    std::string url;
    std::string platform;
};

// A venue as the guide shows it: Stream A's venues row joined with its profile
// (Interface Contract 2). Name, address, url and status are the venue's; the
// slug and editorial are the profile's.
struct GuideVenue {
    int64_t venueId = 0;
    int64_t profileId = 0;
    std::string slug;
    std::string name;
    std::string description;
    std::string address;  // one line, as on event pages (Events::FormatVenueAddress)
    std::string url;
    std::string neighborhoodSlug;
    std::string crowdVibe;
    std::string priceBand;    // empty = not stated
    bool adult = false;
    std::string closureNote;  // empty = none
    std::vector<std::string> sceneTagSlugs;
    // status is the venue's. lastVerifiedAtUs is the OLDER of the two below,
    // and never-verified if either is: the page shows both kinds of fact, and
    // the stamp must not vouch for more than was checked (GD1 §5: never hide a
    // stale date). The two raw dates ride along for the curious.
    Freshness freshness;
    std::optional<int64_t> venueLastVerifiedAtUs;
    std::optional<int64_t> profileLastVerifiedAtUs;
};

struct GuideOrganization {
    int64_t id = 0;
    std::string slug;
    std::string name;
    std::string orgKind;
    std::string description;
    std::string url;
    std::string meetingCadenceText;
    std::string neighborhoodSlug;  // empty = countywide, or it moves around
    bool adult = false;
    std::string closureNote;       // empty = none
    std::vector<std::string> sceneTagSlugs;
    Freshness freshness;
};

struct GuidePlace {
    int64_t id = 0;
    std::string slug;
    std::string name;
    std::string placeKind;
    std::string description;
    std::string addressText;
    std::string neighborhoodSlug;
    bool adult = false;
    std::vector<std::string> sceneTagSlugs;
    Freshness freshness;
};

// Services have no scene tags, links or detail page at MVP (GD3 §3.6).
struct GuideService {
    int64_t id = 0;
    std::string slug;
    std::string name;
    std::string serviceKind;
    std::string description;
    std::string url;
    std::string phone;
    std::string addressText;
    std::string neighborhoodSlug;
    // The MHMDA rendering flag: no person is ever attributed to a health listing.
    bool isHealth = true;
    Freshness freshness;
};

// A guide page, body included. `draft` and `published` matter to the preview
// path; a public read only ever returns published pages.
struct GuidePage {
    int64_t id = 0;
    std::string slug;
    std::string title;
    std::string section;
    std::string bodyMarkdown;
    bool adult = false;
    bool draft = true;
    std::optional<int64_t> publishAtUs;
    bool published = false;
    std::vector<std::string> sceneTagSlugs;
    Freshness freshness;
};

// A section index entry: no body.
struct GuidePageSummary {
    int64_t id = 0;
    std::string slug;
    std::string title;
    std::string section;
    bool adult = false;
    std::optional<int64_t> publishAtUs;
    Freshness freshness;
};

struct VenueDetail {
    GuideVenue venue;
    std::vector<GuideLink> links;
};

struct OrganizationDetail {
    GuideOrganization organization;
    std::vector<GuideLink> links;
};

struct PlaceDetail {
    GuidePlace place;
    std::vector<GuideLink> links;
};

// A scene for the guide's scene grid. The counts are the listings its landing
// page shows.
struct SceneSummary {
    std::string slug;
    std::string displayName;
    std::string description;
    std::string coverage;  // full | link_out: GD3 renders the two postures differently
    int64_t venueCount = 0;
    int64_t organizationCount = 0;
    int64_t placeCount = 0;
};

// A scene landing (GD1 §7). The intro is the published `scenes` page whose slug
// is the scene's; absent until one is written. A link_out scene gets the same
// payload, and GD3 decides what to render.
struct SceneDetail {
    SceneSummary scene;
    std::vector<GuideVenue> venues;
    std::vector<GuideOrganization> organizations;
    std::vector<GuidePlace> places;
    std::vector<GuideLink> links;
    std::optional<GuidePage> introPage;
};

struct NeighborhoodSummary {
    std::string slug;
    std::string displayName;
    std::string region;
    int64_t venueCount = 0;
    int64_t organizationCount = 0;
    int64_t placeCount = 0;
};

// A neighborhood page (GD1 §7). The rundown is the published `new-to-seattle`
// page whose slug is the neighborhood's; absent until one is written.
struct NeighborhoodDetail {
    NeighborhoodSummary neighborhood;
    std::vector<GuideVenue> venues;
    std::vector<GuideOrganization> organizations;
    std::vector<GuidePlace> places;
    std::optional<GuidePage> rundownPage;
};

// The closure graveyard: closed (not moved) venues and organizations, each
// with its closure note.
struct Graveyard {
    std::vector<GuideVenue> venues;
    std::vector<GuideOrganization> organizations;
};

// Empty / false = not filtered.
struct VenueFilters {
    std::string sceneSlug;
    std::string neighborhoodSlug;
    bool includeClosed = false;  // also includes moved (GD2 OQ10 (a))
};

struct OrganizationFilters {
    std::string orgKind;
    std::string sceneSlug;
    std::string neighborhoodSlug;
};

// The publish rule, as TableHelpers::GuidePages' SQL spells it:
//     not a draft AND a publish time AND that time has arrived.
bool IsPublished(bool draft, std::optional<int64_t> publishAtUs, int64_t nowUs);

// The honest stamp for two sets of facts: the older verification, or none if
// either was never verified.
std::optional<int64_t> OlderVerifiedAtUs(std::optional<int64_t> first,
                                         std::optional<int64_t> second);

class GuideHelper {
public:
    explicit GuideHelper(DatabaseHelper databaseHelper);
    GuideHelper(const GuideHelper&) = default;
    GuideHelper& operator=(const GuideHelper&) = default;
    ~GuideHelper() = default;

    // --- Scenes ---

    // Every enabled scene in display order, with its counts.
    std::vector<SceneSummary> GetSceneOverview(Transaction& transaction);

    // An enabled scene's landing; nullopt for an unknown or disabled slug.
    std::optional<SceneDetail> GetSceneDetail(Transaction& transaction, std::string_view slug);

    // --- Venues ---

    // Guide venues, ordered by name. An unknown scene or neighborhood slug
    // matches nothing.
    std::vector<GuideVenue> GetVenuesDirectory(
        Transaction& transaction, const VenueFilters& filters);

    // A venue page by its profile's slug, any status (a closed venue's page is
    // its graveyard entry); nullopt when there is no such profile.
    std::optional<VenueDetail> GetVenueProfileBySlug(
        Transaction& transaction, std::string_view slug);

    // --- Organizations ---

    // Active organizations, ordered by name.
    std::vector<GuideOrganization> GetOrganizationsDirectory(
        Transaction& transaction, const OrganizationFilters& filters);

    // Any status; nullopt when there is no such organization.
    std::optional<OrganizationDetail> GetOrganizationBySlug(
        Transaction& transaction, std::string_view slug);

    // --- Places and services ---

    // Active places, ordered by name.
    std::vector<GuidePlace> GetPlaces(Transaction& transaction);

    // Any status; nullopt when there is no such place.
    std::optional<PlaceDetail> GetPlaceBySlug(Transaction& transaction, std::string_view slug);

    // Active services of one kind (empty = every kind), ordered by name.
    std::vector<GuideService> GetServices(Transaction& transaction, std::string_view serviceKind);

    // --- Neighborhoods ---

    // Every enabled neighborhood in display order, with its counts.
    std::vector<NeighborhoodSummary> GetNeighborhoodsOverview(Transaction& transaction);

    // An enabled neighborhood's page; nullopt for an unknown or disabled slug.
    std::optional<NeighborhoodDetail> GetNeighborhoodDetail(
        Transaction& transaction, std::string_view slug);

    // --- The graveyard ---

    Graveyard GetGraveyard(Transaction& transaction);

    // --- Guide pages ---

    // nullopt unless the page exists and is published.
    std::optional<GuidePage> GetPublishedGuidePage(Transaction& transaction, std::string_view slug);

    // The preview path: any state. Who may see an unpublished page is the
    // endpoint's call (manage_guide).
    std::optional<GuidePage> GetGuidePageAnyState(Transaction& transaction, std::string_view slug);

    // A section's published pages, ordered by title.
    std::vector<GuidePageSummary> GetPublishedPagesBySection(
        Transaction& transaction, std::string_view section);

private:
    DatabaseHelper databaseHelper_;
    TableHelpers::SceneTags sceneTags_;
    TableHelpers::Neighborhoods neighborhoods_;
    TableHelpers::Venues venues_;
    TableHelpers::VenueProfiles venueProfiles_;
    TableHelpers::Organizations organizations_;
    TableHelpers::Places places_;
    TableHelpers::ServiceListings serviceListings_;
    TableHelpers::GuidePages guidePages_;
    TableHelpers::VenueProfileSceneTagAssignments venueProfileSceneTags_;
    TableHelpers::OrganizationSceneTagAssignments organizationSceneTags_;
    TableHelpers::PlaceSceneTagAssignments placeSceneTags_;
    TableHelpers::GuidePageSceneTagAssignments guidePageSceneTags_;
    TableHelpers::GuideLinks guideLinks_;

    // The enabled vocabulary: what an item's slugs may name. Rows are in
    // display order.
    struct Vocabulary {
        KeyValueTableArray sceneTagRows;
        KeyValueTableArray neighborhoodRows;
        std::map<int64_t, std::string> sceneTagSlugs;      // id → slug
        std::map<int64_t, std::string> neighborhoodSlugs;  // id → slug
    };
    Vocabulary LoadVocabulary(Transaction& transaction) const;

    // Every active guide venue, organization and place: what scene and
    // neighborhood pages filter down, so their counts are their lists' sizes.
    struct ActiveListings {
        std::vector<GuideVenue> venues;
        std::vector<GuideOrganization> organizations;
        std::vector<GuidePlace> places;
    };
    ActiveListings LoadActiveListings(Transaction& transaction, const Vocabulary& vocabulary);

    int64_t NowUs(Transaction& transaction) const;

    // Rows → public shapes, resolving venues, neighborhoods and scene tags in
    // batch queries (a fixed number, however many rows). Input order is kept.
    std::vector<GuideVenue> AssembleVenues(
        Transaction& transaction, const KeyValueTableArray& profileRows,
        const Vocabulary& vocabulary);
    std::vector<GuideOrganization> AssembleOrganizations(
        Transaction& transaction, const KeyValueTableArray& rows, const Vocabulary& vocabulary);
    std::vector<GuidePlace> AssemblePlaces(
        Transaction& transaction, const KeyValueTableArray& rows, const Vocabulary& vocabulary);
    std::vector<GuideService> AssembleServices(
        const KeyValueTableArray& rows, const Vocabulary& vocabulary) const;
    GuidePage AssemblePage(Transaction& transaction, const KeyValueTable& row,
                           const Vocabulary& vocabulary);

    // Every guide venue, ordered by name then venue id; without
    // includeRetired, closed and moved ones are left out.
    std::vector<GuideVenue> AllVenues(
        Transaction& transaction, const Vocabulary& vocabulary, bool includeRetired);

    // The published `section` page whose slug is `slug`: the slug conventions'
    // lookup (a scene's intro, a neighborhood's rundown).
    std::optional<GuidePage> PublishedPageInSection(
        Transaction& transaction, std::string_view slug, std::string_view section,
        const Vocabulary& vocabulary);
};

}  // namespace Guide
