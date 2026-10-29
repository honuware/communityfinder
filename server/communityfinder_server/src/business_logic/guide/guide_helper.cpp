#include "business_logic/guide/guide_helper.h"

#include <algorithm>
#include <iterator>
#include <tuple>

#include "business_logic/events/event_helper.h"
#include "db_schema/freshness.h"
#include "db_schema/guide_links.h"
#include "db_schema/guide_page_scene_tag_assignments.h"
#include "db_schema/guide_pages.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/organization_scene_tag_assignments.h"
#include "db_schema/organizations.h"
#include "db_schema/place_scene_tag_assignments.h"
#include "db_schema/places.h"
#include "db_schema/scene_tags.h"
#include "db_schema/service_listings.h"
#include "db_schema/venue_profile_scene_tag_assignments.h"
#include "db_schema/venue_profiles.h"
#include "db_schema/venues.h"
#include "util/types.h"

namespace Guide {

namespace {

using IdToSlug = std::map<int64_t, std::string>;
using IdToSlugs = std::map<int64_t, std::vector<std::string>>;

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it == row.end() ? std::string() : it->second;
}

int64_t IdColumn(const KeyValueTable& row, std::string_view column) {
    return std::stoll(Column(row, column));
}

// NULL reads back as "", which is "no value" here.
std::optional<int64_t> OptionalIntColumn(const KeyValueTable& row, std::string_view column) {
    std::string value = Column(row, column);
    if (value.empty()) {
        return std::nullopt;
    }
    return std::stoll(value);
}

// Postgres booleans read back as "t" / "f".
bool BoolColumn(const KeyValueTable& row, std::string_view column) {
    return StringToBool(Column(row, column));
}

IdToSlug SlugsById(const KeyValueTableArray& rows, std::string_view idColumn,
                   std::string_view slugColumn) {
    IdToSlug slugs;
    for (const KeyValueTable& row : rows) {
        slugs[IdColumn(row, idColumn)] = Column(row, slugColumn);
    }
    return slugs;
}

// The row's neighborhood slug; empty when unset or the neighborhood is disabled.
std::string NeighborhoodSlug(const KeyValueTable& row, std::string_view column,
                             const IdToSlug& neighborhoodSlugs) {
    if (auto neighborhoodId = OptionalIntColumn(row, column)) {
        auto it = neighborhoodSlugs.find(*neighborhoodId);
        if (it != neighborhoodSlugs.end()) return it->second;
    }
    return {};
}

// owner id → its enabled scene tags' slugs, in the order the GetFor* query
// returned them (the tags' display order). A disabled tag is left out.
IdToSlugs SceneTagSlugsByOwner(const KeyValueTableArray& rows, std::string_view ownerColumn,
                               std::string_view sceneTagIdColumn,
                               const IdToSlug& enabledSceneTagSlugs) {
    IdToSlugs slugs;
    for (const KeyValueTable& row : rows) {
        auto it = enabledSceneTagSlugs.find(IdColumn(row, sceneTagIdColumn));
        if (it != enabledSceneTagSlugs.end()) {
            slugs[IdColumn(row, ownerColumn)].push_back(it->second);
        }
    }
    return slugs;
}

std::vector<std::string> SlugsFor(const IdToSlugs& slugs, int64_t ownerId) {
    auto it = slugs.find(ownerId);
    return it == slugs.end() ? std::vector<std::string>() : it->second;
}

bool Contains(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

// GD2 OQ10 (a): the statuses that leave the active lists.
bool IsRetired(const std::string& status) {
    return status == DbSchema::kFreshnessStatusClosed || status == DbSchema::kFreshnessStatusMoved;
}

template <class Item, class Predicate>
std::vector<Item> Filtered(const std::vector<Item>& items, Predicate keep) {
    std::vector<Item> kept;
    std::copy_if(items.begin(), items.end(), std::back_inserter(kept), keep);
    return kept;
}

template <class Item>
std::vector<Item> Tagged(const std::vector<Item>& items, const std::string& sceneSlug) {
    return Filtered(items, [&](const Item& item) { return Contains(item.sceneTagSlugs, sceneSlug); });
}

template <class Item>
std::vector<Item> InNeighborhood(const std::vector<Item>& items, const std::string& neighborhoodSlug) {
    return Filtered(items, [&](const Item& item) { return item.neighborhoodSlug == neighborhoodSlug; });
}

template <class Item>
int64_t CountOf(const std::vector<Item>& items) {
    return static_cast<int64_t>(items.size());
}

// The vocabulary row with `slug`; nullptr when there is none (or, the rows
// being enabled-only, when it is disabled).
const KeyValueTable* FindBySlug(const KeyValueTableArray& rows, std::string_view slugColumn,
                                std::string_view slug) {
    for (const KeyValueTable& row : rows) {
        if (Column(row, slugColumn) == slug) return &row;
    }
    return nullptr;
}

std::vector<GuideLink> ToGuideLinks(const KeyValueTableArray& rows) {
    std::vector<GuideLink> links;
    links.reserve(rows.size());
    for (const KeyValueTable& row : rows) {
        GuideLink link;
        link.id = IdColumn(row, DbSchema::kGuideLinksId);
        link.label = Column(row, DbSchema::kGuideLinksLabel);
        link.url = Column(row, DbSchema::kGuideLinksUrl);
        link.platform = Column(row, DbSchema::kGuideLinksPlatform);
        links.push_back(std::move(link));
    }
    return links;
}

SceneSummary ToSceneSummary(const KeyValueTable& row) {
    SceneSummary scene;
    scene.slug = Column(row, DbSchema::kSceneTagsSlug);
    scene.displayName = Column(row, DbSchema::kSceneTagsDisplayName);
    scene.description = Column(row, DbSchema::kSceneTagsDescription);
    scene.coverage = Column(row, DbSchema::kSceneTagsCoverage);
    return scene;
}

NeighborhoodSummary ToNeighborhoodSummary(const KeyValueTable& row) {
    NeighborhoodSummary neighborhood;
    neighborhood.slug = Column(row, DbSchema::kNeighborhoodsSlug);
    neighborhood.displayName = Column(row, DbSchema::kNeighborhoodsDisplayName);
    neighborhood.region = Column(row, DbSchema::kNeighborhoodsRegion);
    return neighborhood;
}

}  // namespace

bool IsPublished(bool draft, std::optional<int64_t> publishAtUs, int64_t nowUs) {
    return !draft && publishAtUs.has_value() && *publishAtUs <= nowUs;
}

std::optional<int64_t> OlderVerifiedAtUs(std::optional<int64_t> first,
                                         std::optional<int64_t> second) {
    if (!first || !second) {
        return std::nullopt;
    }
    return std::min(*first, *second);
}

GuideHelper::GuideHelper(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper),
      sceneTags_(databaseHelper),
      neighborhoods_(databaseHelper),
      venues_(databaseHelper),
      venueProfiles_(databaseHelper),
      organizations_(databaseHelper),
      places_(databaseHelper),
      serviceListings_(databaseHelper),
      guidePages_(databaseHelper),
      venueProfileSceneTags_(databaseHelper),
      organizationSceneTags_(databaseHelper),
      placeSceneTags_(databaseHelper),
      guidePageSceneTags_(databaseHelper),
      guideLinks_(databaseHelper) {
}

int64_t GuideHelper::NowUs(Transaction& transaction) const {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

GuideHelper::Vocabulary GuideHelper::LoadVocabulary(Transaction& transaction) const {
    Vocabulary vocabulary;
    vocabulary.sceneTagRows = sceneTags_.GetAll(transaction);
    vocabulary.neighborhoodRows = neighborhoods_.GetAll(transaction);
    vocabulary.sceneTagSlugs = SlugsById(
        vocabulary.sceneTagRows, DbSchema::kSceneTagsId, DbSchema::kSceneTagsSlug);
    vocabulary.neighborhoodSlugs = SlugsById(
        vocabulary.neighborhoodRows, DbSchema::kNeighborhoodsId, DbSchema::kNeighborhoodsSlug);
    return vocabulary;
}

GuideHelper::ActiveListings GuideHelper::LoadActiveListings(
    Transaction& transaction, const Vocabulary& vocabulary) {
    ActiveListings listings;
    listings.venues = AllVenues(transaction, vocabulary, false);
    listings.organizations = AssembleOrganizations(
        transaction, organizations_.GetOrganizations(transaction, false), vocabulary);
    listings.places = AssemblePlaces(transaction, places_.GetPlaces(transaction, false), vocabulary);
    return listings;
}

// --- Scenes ---

std::vector<SceneSummary> GuideHelper::GetSceneOverview(Transaction& transaction) {
    const Vocabulary vocabulary = LoadVocabulary(transaction);
    const ActiveListings listings = LoadActiveListings(transaction, vocabulary);
    std::vector<SceneSummary> scenes;
    scenes.reserve(vocabulary.sceneTagRows.size());
    for (const KeyValueTable& row : vocabulary.sceneTagRows) {
        SceneSummary scene = ToSceneSummary(row);
        scene.venueCount = CountOf(Tagged(listings.venues, scene.slug));
        scene.organizationCount = CountOf(Tagged(listings.organizations, scene.slug));
        scene.placeCount = CountOf(Tagged(listings.places, scene.slug));
        scenes.push_back(std::move(scene));
    }
    return scenes;
}

std::optional<SceneDetail> GuideHelper::GetSceneDetail(
    Transaction& transaction, std::string_view slug) {
    const Vocabulary vocabulary = LoadVocabulary(transaction);
    const KeyValueTable* row = FindBySlug(vocabulary.sceneTagRows, DbSchema::kSceneTagsSlug, slug);
    if (row == nullptr) {
        return std::nullopt;
    }
    const ActiveListings listings = LoadActiveListings(transaction, vocabulary);
    SceneDetail detail;
    detail.scene = ToSceneSummary(*row);
    detail.venues = Tagged(listings.venues, detail.scene.slug);
    detail.organizations = Tagged(listings.organizations, detail.scene.slug);
    detail.places = Tagged(listings.places, detail.scene.slug);
    detail.scene.venueCount = CountOf(detail.venues);
    detail.scene.organizationCount = CountOf(detail.organizations);
    detail.scene.placeCount = CountOf(detail.places);
    detail.links = ToGuideLinks(
        guideLinks_.GetLinksForSceneTag(transaction, IdColumn(*row, DbSchema::kSceneTagsId)));
    detail.introPage = PublishedPageInSection(
        transaction, slug, DbSchema::kGuidePagesSectionScenes, vocabulary);
    return detail;
}

// --- Venues ---

std::vector<GuideVenue> GuideHelper::GetVenuesDirectory(
    Transaction& transaction, const VenueFilters& filters) {
    const Vocabulary vocabulary = LoadVocabulary(transaction);
    std::vector<GuideVenue> venues;
    for (GuideVenue& venue : AllVenues(transaction, vocabulary, filters.includeClosed)) {
        if (!filters.sceneSlug.empty() && !Contains(venue.sceneTagSlugs, filters.sceneSlug)) {
            continue;
        }
        if (!filters.neighborhoodSlug.empty() && venue.neighborhoodSlug != filters.neighborhoodSlug) {
            continue;
        }
        venues.push_back(std::move(venue));
    }
    return venues;
}

std::optional<VenueDetail> GuideHelper::GetVenueProfileBySlug(
    Transaction& transaction, std::string_view slug) {
    KeyValueTable profile = venueProfiles_.GetProfileBySlug(transaction, slug);
    if (profile.empty()) {
        return std::nullopt;
    }
    std::vector<GuideVenue> venues =
        AssembleVenues(transaction, { profile }, LoadVocabulary(transaction));
    if (venues.empty()) {
        return std::nullopt;
    }
    VenueDetail detail;
    detail.venue = std::move(venues.front());
    detail.links = ToGuideLinks(
        guideLinks_.GetLinksForVenueProfile(transaction, detail.venue.profileId));
    return detail;
}

// --- Organizations ---

std::vector<GuideOrganization> GuideHelper::GetOrganizationsDirectory(
    Transaction& transaction, const OrganizationFilters& filters) {
    const Vocabulary vocabulary = LoadVocabulary(transaction);
    const KeyValueTableArray rows = filters.orgKind.empty()
        ? organizations_.GetOrganizations(transaction, false)
        : organizations_.GetOrganizationsByKind(transaction, filters.orgKind);
    std::vector<GuideOrganization> organizations;
    for (GuideOrganization& organization : AssembleOrganizations(transaction, rows, vocabulary)) {
        // GetOrganizationsByKind reads any status.
        if (IsRetired(organization.freshness.status)) {
            continue;
        }
        if (!filters.sceneSlug.empty()
            && !Contains(organization.sceneTagSlugs, filters.sceneSlug)) {
            continue;
        }
        if (!filters.neighborhoodSlug.empty()
            && organization.neighborhoodSlug != filters.neighborhoodSlug) {
            continue;
        }
        organizations.push_back(std::move(organization));
    }
    return organizations;
}

std::optional<OrganizationDetail> GuideHelper::GetOrganizationBySlug(
    Transaction& transaction, std::string_view slug) {
    KeyValueTable row = organizations_.GetOrganizationBySlug(transaction, slug);
    if (row.empty()) {
        return std::nullopt;
    }
    OrganizationDetail detail;
    detail.organization =
        std::move(AssembleOrganizations(transaction, { row }, LoadVocabulary(transaction)).front());
    detail.links = ToGuideLinks(
        guideLinks_.GetLinksForOrganization(transaction, detail.organization.id));
    return detail;
}

// --- Places and services ---

std::vector<GuidePlace> GuideHelper::GetPlaces(Transaction& transaction) {
    return AssemblePlaces(
        transaction, places_.GetPlaces(transaction, false), LoadVocabulary(transaction));
}

std::optional<PlaceDetail> GuideHelper::GetPlaceBySlug(
    Transaction& transaction, std::string_view slug) {
    KeyValueTable row = places_.GetPlaceBySlug(transaction, slug);
    if (row.empty()) {
        return std::nullopt;
    }
    PlaceDetail detail;
    detail.place = std::move(AssemblePlaces(transaction, { row }, LoadVocabulary(transaction)).front());
    detail.links = ToGuideLinks(guideLinks_.GetLinksForPlace(transaction, detail.place.id));
    return detail;
}

std::vector<GuideService> GuideHelper::GetServices(
    Transaction& transaction, std::string_view serviceKind) {
    const KeyValueTableArray rows = serviceKind.empty()
        ? serviceListings_.GetServiceListings(transaction, false)
        : serviceListings_.GetServiceListingsByKind(transaction, serviceKind);
    // GetServiceListingsByKind reads any status.
    return Filtered(AssembleServices(rows, LoadVocabulary(transaction)),
                    [](const GuideService& service) { return !IsRetired(service.freshness.status); });
}

// --- Neighborhoods ---

std::vector<NeighborhoodSummary> GuideHelper::GetNeighborhoodsOverview(Transaction& transaction) {
    const Vocabulary vocabulary = LoadVocabulary(transaction);
    const ActiveListings listings = LoadActiveListings(transaction, vocabulary);
    std::vector<NeighborhoodSummary> neighborhoods;
    neighborhoods.reserve(vocabulary.neighborhoodRows.size());
    for (const KeyValueTable& row : vocabulary.neighborhoodRows) {
        NeighborhoodSummary neighborhood = ToNeighborhoodSummary(row);
        neighborhood.venueCount = CountOf(InNeighborhood(listings.venues, neighborhood.slug));
        neighborhood.organizationCount =
            CountOf(InNeighborhood(listings.organizations, neighborhood.slug));
        neighborhood.placeCount = CountOf(InNeighborhood(listings.places, neighborhood.slug));
        neighborhoods.push_back(std::move(neighborhood));
    }
    return neighborhoods;
}

std::optional<NeighborhoodDetail> GuideHelper::GetNeighborhoodDetail(
    Transaction& transaction, std::string_view slug) {
    const Vocabulary vocabulary = LoadVocabulary(transaction);
    const KeyValueTable* row =
        FindBySlug(vocabulary.neighborhoodRows, DbSchema::kNeighborhoodsSlug, slug);
    if (row == nullptr) {
        return std::nullopt;
    }
    const ActiveListings listings = LoadActiveListings(transaction, vocabulary);
    NeighborhoodDetail detail;
    detail.neighborhood = ToNeighborhoodSummary(*row);
    detail.venues = InNeighborhood(listings.venues, detail.neighborhood.slug);
    detail.organizations = InNeighborhood(listings.organizations, detail.neighborhood.slug);
    detail.places = InNeighborhood(listings.places, detail.neighborhood.slug);
    detail.neighborhood.venueCount = CountOf(detail.venues);
    detail.neighborhood.organizationCount = CountOf(detail.organizations);
    detail.neighborhood.placeCount = CountOf(detail.places);
    detail.rundownPage = PublishedPageInSection(
        transaction, slug, DbSchema::kGuidePagesSectionNewToSeattle, vocabulary);
    return detail;
}

// --- The graveyard ---

Graveyard GuideHelper::GetGraveyard(Transaction& transaction) {
    const Vocabulary vocabulary = LoadVocabulary(transaction);
    auto closed = [](const auto& item) {
        return item.freshness.status == DbSchema::kFreshnessStatusClosed;
    };
    Graveyard graveyard;
    graveyard.venues = Filtered(AllVenues(transaction, vocabulary, true), closed);
    graveyard.organizations = Filtered(
        AssembleOrganizations(transaction, organizations_.GetOrganizations(transaction, true),
                              vocabulary),
        closed);
    return graveyard;
}

// --- Guide pages ---

std::optional<GuidePage> GuideHelper::GetPublishedGuidePage(
    Transaction& transaction, std::string_view slug) {
    KeyValueTable row = guidePages_.GetPublishedPageBySlug(transaction, slug);
    if (row.empty()) {
        return std::nullopt;
    }
    return AssemblePage(transaction, row, LoadVocabulary(transaction));
}

std::optional<GuidePage> GuideHelper::GetGuidePageAnyState(
    Transaction& transaction, std::string_view slug) {
    KeyValueTable row = guidePages_.GetPageBySlug(transaction, slug);
    if (row.empty()) {
        return std::nullopt;
    }
    return AssemblePage(transaction, row, LoadVocabulary(transaction));
}

std::vector<GuidePageSummary> GuideHelper::GetPublishedPagesBySection(
    Transaction& transaction, std::string_view section) {
    std::vector<GuidePageSummary> pages;
    for (const KeyValueTable& row : guidePages_.GetPublishedPagesBySection(transaction, section)) {
        GuidePageSummary page;
        page.id = IdColumn(row, DbSchema::kGuidePagesId);
        page.slug = Column(row, DbSchema::kGuidePagesSlug);
        page.title = Column(row, DbSchema::kGuidePagesTitle);
        page.section = Column(row, DbSchema::kGuidePagesSection);
        page.adult = BoolColumn(row, DbSchema::kGuidePagesAdult);
        page.publishAtUs = OptionalIntColumn(row, DbSchema::kGuidePagesPublishAtUs);
        page.freshness.status = Column(row, DbSchema::kGuidePagesStatus);
        page.freshness.lastVerifiedAtUs =
            OptionalIntColumn(row, DbSchema::kGuidePagesLastVerifiedAtUs);
        pages.push_back(std::move(page));
    }
    return pages;
}

std::optional<GuidePage> GuideHelper::PublishedPageInSection(
    Transaction& transaction, std::string_view slug, std::string_view section,
    const Vocabulary& vocabulary) {
    KeyValueTable row = guidePages_.GetPublishedPageBySlug(transaction, slug);
    if (row.empty() || Column(row, DbSchema::kGuidePagesSection) != section) {
        return std::nullopt;
    }
    return AssemblePage(transaction, row, vocabulary);
}

// --- Assembly ---

std::vector<GuideVenue> GuideHelper::AllVenues(
    Transaction& transaction, const Vocabulary& vocabulary, bool includeRetired) {
    std::vector<GuideVenue> venues =
        AssembleVenues(transaction, venueProfiles_.GetProfiles(transaction), vocabulary);
    if (!includeRetired) {
        venues = Filtered(venues, [](const GuideVenue& venue) {
            return !IsRetired(venue.freshness.status);
        });
    }
    // The profiles come back by slug; the name is the venue row's, so the sort
    // happens here.
    std::stable_sort(venues.begin(), venues.end(), [](const GuideVenue& a, const GuideVenue& b) {
        return std::tie(a.name, a.venueId) < std::tie(b.name, b.venueId);
    });
    return venues;
}

std::vector<GuideVenue> GuideHelper::AssembleVenues(
    Transaction& transaction, const KeyValueTableArray& profileRows,
    const Vocabulary& vocabulary) {
    if (profileRows.empty()) {
        return {};
    }
    std::vector<int64_t> profileIds;
    std::vector<int64_t> venueIds;
    for (const KeyValueTable& profile : profileRows) {
        profileIds.push_back(IdColumn(profile, DbSchema::kVenueProfilesId));
        venueIds.push_back(IdColumn(profile, DbSchema::kVenueProfilesVenueId));
    }
    std::map<int64_t, KeyValueTable> venuesById;
    for (KeyValueTable& venue : venues_.GetVenuesByIds(transaction, venueIds)) {
        int64_t id = IdColumn(venue, DbSchema::kVenuesId);
        venuesById[id] = std::move(venue);
    }
    const IdToSlugs sceneTags = SceneTagSlugsByOwner(
        venueProfileSceneTags_.GetForVenueProfiles(transaction, profileIds),
        DbSchema::kVenueProfileSceneTagAssignmentsVenueProfileId,
        DbSchema::kVenueProfileSceneTagAssignmentsSceneTagId,
        vocabulary.sceneTagSlugs);

    std::vector<GuideVenue> result;
    result.reserve(profileRows.size());
    for (const KeyValueTable& profile : profileRows) {
        auto venueIt = venuesById.find(IdColumn(profile, DbSchema::kVenueProfilesVenueId));
        if (venueIt == venuesById.end()) {
            continue;  // venue_id is a cascading FK, so a profile always has its venue
        }
        const KeyValueTable& venue = venueIt->second;
        GuideVenue item;
        item.venueId = venueIt->first;
        item.profileId = IdColumn(profile, DbSchema::kVenueProfilesId);
        item.slug = Column(profile, DbSchema::kVenueProfilesSlug);
        item.name = Column(venue, DbSchema::kVenuesName);
        item.description = Column(venue, DbSchema::kVenuesDescription);
        item.address = Events::FormatVenueAddress(
            Column(venue, DbSchema::kVenuesAddress), Column(venue, DbSchema::kVenuesCity),
            Column(venue, DbSchema::kVenuesState), Column(venue, DbSchema::kVenuesZip));
        item.url = Column(venue, DbSchema::kVenuesUrl);
        item.neighborhoodSlug =
            NeighborhoodSlug(venue, DbSchema::kVenuesNeighborhoodId, vocabulary.neighborhoodSlugs);
        item.crowdVibe = Column(profile, DbSchema::kVenueProfilesCrowdVibe);
        item.priceBand = Column(profile, DbSchema::kVenueProfilesPriceBand);
        item.adult = BoolColumn(profile, DbSchema::kVenueProfilesAdult);
        item.closureNote = Column(profile, DbSchema::kVenueProfilesClosureNote);
        item.sceneTagSlugs = SlugsFor(sceneTags, item.profileId);
        item.venueLastVerifiedAtUs = OptionalIntColumn(venue, DbSchema::kVenuesLastVerifiedAtUs);
        item.profileLastVerifiedAtUs =
            OptionalIntColumn(profile, DbSchema::kVenueProfilesLastVerifiedAtUs);
        item.freshness.status = Column(venue, DbSchema::kVenuesStatus);
        item.freshness.lastVerifiedAtUs =
            OlderVerifiedAtUs(item.venueLastVerifiedAtUs, item.profileLastVerifiedAtUs);
        result.push_back(std::move(item));
    }
    return result;
}

std::vector<GuideOrganization> GuideHelper::AssembleOrganizations(
    Transaction& transaction, const KeyValueTableArray& rows, const Vocabulary& vocabulary) {
    if (rows.empty()) {
        return {};
    }
    std::vector<int64_t> ids;
    for (const KeyValueTable& row : rows) {
        ids.push_back(IdColumn(row, DbSchema::kOrganizationsId));
    }
    const IdToSlugs sceneTags = SceneTagSlugsByOwner(
        organizationSceneTags_.GetForOrganizations(transaction, ids),
        DbSchema::kOrganizationSceneTagAssignmentsOrganizationId,
        DbSchema::kOrganizationSceneTagAssignmentsSceneTagId,
        vocabulary.sceneTagSlugs);

    std::vector<GuideOrganization> result;
    result.reserve(rows.size());
    for (const KeyValueTable& row : rows) {
        GuideOrganization item;
        item.id = IdColumn(row, DbSchema::kOrganizationsId);
        item.slug = Column(row, DbSchema::kOrganizationsSlug);
        item.name = Column(row, DbSchema::kOrganizationsName);
        item.orgKind = Column(row, DbSchema::kOrganizationsOrgKind);
        item.description = Column(row, DbSchema::kOrganizationsDescription);
        item.url = Column(row, DbSchema::kOrganizationsUrl);
        item.meetingCadenceText = Column(row, DbSchema::kOrganizationsMeetingCadenceText);
        item.neighborhoodSlug = NeighborhoodSlug(
            row, DbSchema::kOrganizationsNeighborhoodId, vocabulary.neighborhoodSlugs);
        item.closureNote = Column(row, DbSchema::kOrganizationsClosureNote);
        item.sceneTagSlugs = SlugsFor(sceneTags, item.id);
        item.freshness.status = Column(row, DbSchema::kOrganizationsStatus);
        item.freshness.lastVerifiedAtUs =
            OptionalIntColumn(row, DbSchema::kOrganizationsLastVerifiedAtUs);
        result.push_back(std::move(item));
    }
    return result;
}

std::vector<GuidePlace> GuideHelper::AssemblePlaces(
    Transaction& transaction, const KeyValueTableArray& rows, const Vocabulary& vocabulary) {
    if (rows.empty()) {
        return {};
    }
    std::vector<int64_t> ids;
    for (const KeyValueTable& row : rows) {
        ids.push_back(IdColumn(row, DbSchema::kPlacesId));
    }
    const IdToSlugs sceneTags = SceneTagSlugsByOwner(
        placeSceneTags_.GetForPlaces(transaction, ids),
        DbSchema::kPlaceSceneTagAssignmentsPlaceId,
        DbSchema::kPlaceSceneTagAssignmentsSceneTagId,
        vocabulary.sceneTagSlugs);

    std::vector<GuidePlace> result;
    result.reserve(rows.size());
    for (const KeyValueTable& row : rows) {
        GuidePlace item;
        item.id = IdColumn(row, DbSchema::kPlacesId);
        item.slug = Column(row, DbSchema::kPlacesSlug);
        item.name = Column(row, DbSchema::kPlacesName);
        item.placeKind = Column(row, DbSchema::kPlacesPlaceKind);
        item.description = Column(row, DbSchema::kPlacesDescription);
        item.addressText = Column(row, DbSchema::kPlacesAddressText);
        item.neighborhoodSlug =
            NeighborhoodSlug(row, DbSchema::kPlacesNeighborhoodId, vocabulary.neighborhoodSlugs);
        item.adult = BoolColumn(row, DbSchema::kPlacesAdult);
        item.sceneTagSlugs = SlugsFor(sceneTags, item.id);
        item.freshness.status = Column(row, DbSchema::kPlacesStatus);
        item.freshness.lastVerifiedAtUs = OptionalIntColumn(row, DbSchema::kPlacesLastVerifiedAtUs);
        result.push_back(std::move(item));
    }
    return result;
}

std::vector<GuideService> GuideHelper::AssembleServices(
    const KeyValueTableArray& rows, const Vocabulary& vocabulary) const {
    std::vector<GuideService> result;
    result.reserve(rows.size());
    for (const KeyValueTable& row : rows) {
        GuideService item;
        item.id = IdColumn(row, DbSchema::kServiceListingsId);
        item.slug = Column(row, DbSchema::kServiceListingsSlug);
        item.name = Column(row, DbSchema::kServiceListingsName);
        item.serviceKind = Column(row, DbSchema::kServiceListingsServiceKind);
        item.description = Column(row, DbSchema::kServiceListingsDescription);
        item.url = Column(row, DbSchema::kServiceListingsUrl);
        item.phone = Column(row, DbSchema::kServiceListingsPhone);
        item.addressText = Column(row, DbSchema::kServiceListingsAddressText);
        item.neighborhoodSlug = NeighborhoodSlug(
            row, DbSchema::kServiceListingsNeighborhoodId, vocabulary.neighborhoodSlugs);
        item.isHealth = BoolColumn(row, DbSchema::kServiceListingsIsHealth);
        item.freshness.status = Column(row, DbSchema::kServiceListingsStatus);
        item.freshness.lastVerifiedAtUs =
            OptionalIntColumn(row, DbSchema::kServiceListingsLastVerifiedAtUs);
        result.push_back(std::move(item));
    }
    return result;
}

GuidePage GuideHelper::AssemblePage(
    Transaction& transaction, const KeyValueTable& row, const Vocabulary& vocabulary) {
    GuidePage page;
    page.id = IdColumn(row, DbSchema::kGuidePagesId);
    page.slug = Column(row, DbSchema::kGuidePagesSlug);
    page.title = Column(row, DbSchema::kGuidePagesTitle);
    page.section = Column(row, DbSchema::kGuidePagesSection);
    page.bodyMarkdown = Column(row, DbSchema::kGuidePagesBodyMarkdown);
    page.adult = BoolColumn(row, DbSchema::kGuidePagesAdult);
    page.draft = BoolColumn(row, DbSchema::kGuidePagesDraft);
    page.publishAtUs = OptionalIntColumn(row, DbSchema::kGuidePagesPublishAtUs);
    page.published = IsPublished(page.draft, page.publishAtUs, NowUs(transaction));
    page.sceneTagSlugs = SlugsFor(
        SceneTagSlugsByOwner(guidePageSceneTags_.GetForGuidePage(transaction, page.id),
                             DbSchema::kGuidePageSceneTagAssignmentsGuidePageId,
                             DbSchema::kGuidePageSceneTagAssignmentsSceneTagId,
                             vocabulary.sceneTagSlugs),
        page.id);
    page.freshness.status = Column(row, DbSchema::kGuidePagesStatus);
    page.freshness.lastVerifiedAtUs = OptionalIntColumn(row, DbSchema::kGuidePagesLastVerifiedAtUs);
    return page;
}

}  // namespace Guide
