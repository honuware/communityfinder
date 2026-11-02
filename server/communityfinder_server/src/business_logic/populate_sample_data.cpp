#include "business_logic/populate_sample_data.h"

#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "business_logic/events/event_helper.h"
#include "business_logic/events/event_ical.h"
#include "db_schema/categories.h"
#include "db_schema/event_sources.h"
#include "db_schema/events.h"
#include "db_schema/freshness.h"
#include "db_schema/guide_links.h"
#include "db_schema/guide_pages.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/organizations.h"
#include "db_schema/places.h"
#include "db_schema/scene_tags.h"
#include "db_schema/series.h"
#include "db_schema/service_listings.h"
#include "db_schema/venue_profiles.h"
#include "db_schema/venues.h"
#include "sql_util/table_helpers/categories.h"
#include "sql_util/table_helpers/event_sources.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/guide_links.h"
#include "sql_util/table_helpers/guide_page_scene_tag_assignments.h"
#include "sql_util/table_helpers/guide_pages.h"
#include "sql_util/table_helpers/neighborhoods.h"
#include "sql_util/table_helpers/organization_scene_tag_assignments.h"
#include "sql_util/table_helpers/organizations.h"
#include "sql_util/table_helpers/place_scene_tag_assignments.h"
#include "sql_util/table_helpers/places.h"
#include "sql_util/table_helpers/scene_tags.h"
#include "sql_util/table_helpers/series.h"
#include "sql_util/table_helpers/series_category_assignments.h"
#include "sql_util/table_helpers/series_scene_tag_assignments.h"
#include "sql_util/table_helpers/service_listings.h"
#include "sql_util/table_helpers/venue_profile_scene_tag_assignments.h"
#include "sql_util/table_helpers/venue_profiles.h"
#include "sql_util/table_helpers/venues.h"
#include "util/types.h"

namespace {

constexpr int64_t kUsPerMinute = 60LL * 1'000'000LL;
constexpr int64_t kUsPerDay = Events::kUsPerDay;

// The one feed every sample event comes in through, found by name.
constexpr std::string_view kSampleSourceName = "Sample data";
constexpr std::string_view kSampleSourceNotes =
    "Made-up events from database_helper --populate_sample_data. There is no feed behind it.";
constexpr std::string_view kSampleReviewNote = "Sample data, approved by database_helper";
constexpr std::string_view kSampleExternalKeyPrefix = "sample-";

constexpr std::string_view kState = "WA";

// Days of the week as Postgres numbers them (EXTRACT(DOW)).
constexpr int kSunday = 0;
constexpr int kTuesday = 2;
constexpr int kWednesday = 3;
constexpr int kThursday = 4;
constexpr int kFriday = 5;
constexpr int kSaturday = 6;

// ---- the specs: what to write ----

struct LinkSpec {
    std::string_view label;
    std::string_view platform;
    std::string_view url;
};

// A venue and its guide profile. Verification ages are in days; nullopt =
// never verified.
struct VenueSpec {
    std::string_view slug;
    std::string_view name;
    std::string_view description;
    std::string_view street;
    std::string_view city;
    std::string_view zip;
    std::string_view neighborhood;
    std::string_view crowdVibe;
    std::string_view priceBand;
    std::vector<std::string_view> sceneTags;
    std::optional<int> venueVerifiedDaysAgo;
    std::optional<int> profileVerifiedDaysAgo;
    std::string_view status = DbSchema::kFreshnessStatusOpen;
    std::string_view closureNote;
    bool adult = false;
    std::string_view url;
    std::vector<LinkSpec> links;
};

struct OrganizationSpec {
    std::string_view slug;
    std::string_view name;
    std::string_view kind;
    std::string_view description;
    std::string_view cadence;
    std::string_view neighborhood;  // empty = countywide, or it moves around
    std::vector<std::string_view> sceneTags;
    std::optional<int> verifiedDaysAgo;
    std::string_view status = DbSchema::kFreshnessStatusOpen;
    std::string_view closureNote;
    bool adult = false;
    std::vector<LinkSpec> links;
};

struct PlaceSpec {
    std::string_view slug;
    std::string_view name;
    std::string_view kind;
    std::string_view description;
    std::string_view addressText;
    std::string_view neighborhood;
    std::optional<int> verifiedDaysAgo;
    bool adult = false;
    std::string_view volatilityTier;  // empty = the column default
    std::vector<LinkSpec> links;
};

struct ServiceSpec {
    std::string_view slug;
    std::string_view name;
    std::string_view kind;
    std::string_view description;
    std::string_view phone;
    std::string_view neighborhood;
    std::optional<int> verifiedDaysAgo;
};

// Published `publishedDaysAgo` days ago, or scheduled `scheduledInDays` ahead;
// neither = a draft. The verified stamp defaults to the publish time.
struct PageSpec {
    std::string_view slug;
    std::string_view title;
    std::string_view section;
    std::string_view body;
    std::optional<int> publishedDaysAgo;
    std::optional<int> scheduledInDays;
    std::optional<int> verifiedDaysAgo;
    bool adult = false;
    std::vector<std::string_view> sceneTags;
};

struct SeriesSpec {
    std::string_view slug;
    std::string_view title;
    std::string_view description;
    std::string_view cadence;
    std::string_view venue;  // a venue slug, or empty
    std::string_view url;
    std::string_view status = DbSchema::kFreshnessStatusOpen;
    std::optional<int> verifiedDaysAgo;
    std::vector<std::string_view> categories;
    std::vector<std::string_view> sceneTags;
};

// One event. Leave the tags empty on a series event to inherit the series'.
struct EventSpec {
    std::string key;  // the ingest key, after kSampleExternalKeyPrefix
    std::string_view title;
    int64_t startsAtUs = 0;
    int durationMinutes = 0;  // 0 = no set end
    std::string_view venue;   // a venue slug, or empty
    std::string_view locationText;
    std::string_view description;
    std::string_view url;
    std::string_view costText;
    std::vector<std::string_view> categories;
    std::vector<std::string_view> sceneTags;
    std::string_view series;  // a series slug, or empty
    bool approve = true;
};

// ---- the guide ----

const std::vector<VenueSpec>& SampleVenues() {
    static const std::vector<VenueSpec> venues{
        {
            .slug = "the-lantern-room",
            .name = "The Lantern Room",
            .description = "A neighborhood bar with a back room for events.",
            .street = "1400 E Pine St", .city = "Seattle", .zip = "98122",
            .neighborhood = "capitol-hill",
            .crowdVibe = "Bears and their friends after work; louder and younger after 10.",
            .priceBand = "$$",
            .sceneTags = { "bear" },
            .venueVerifiedDaysAgo = 20, .profileVerifiedDaysAgo = 35,
            .url = "https://example.org/lantern-room",
            .links = { { "The Lantern Room on Instagram", DbSchema::kGuideLinksPlatformInstagram,
                         "https://example.org/lantern-ig" } },
        },
        {
            // Its profile has never been verified, so its stamp says so.
            .slug = "driftwood-hall",
            .name = "Driftwood Hall",
            .description = "A dance hall with a stage and a big floor.",
            .street = "5300 Ballard Ave NW", .city = "Seattle", .zip = "98107",
            .neighborhood = "ballard",
            .crowdVibe = "Mixed crowd, here to dance. Drag on weekends.",
            .priceBand = "$$",
            .venueVerifiedDaysAgo = 45,
        },
        {
            .slug = "columbia-city-social-club",
            .name = "Columbia City Social Club",
            .description = "A community space with a café counter.",
            .street = "4800 Rainier Ave S", .city = "Seattle", .zip = "98118",
            .neighborhood = "columbia-city",
            .crowdVibe = "Daytime makers and readers; craft fairs and film nights.",
            .priceBand = "$",
            .sceneTags = { "sapphic", "bipoc" },
            .venueVerifiedDaysAgo = 60, .profileVerifiedDaysAgo = 10,
            .links = { { "Events calendar", DbSchema::kGuideLinksPlatformWebsite,
                         "https://example.org/ccsc-calendar" } },
        },
        {
            .slug = "eastside-commons",
            .name = "Eastside Commons",
            .description = "A community room and café.",
            .street = "200 Bellevue Way NE", .city = "Bellevue", .zip = "98004",
            .neighborhood = "bellevue",
            .crowdVibe = "Board gamers on weeknights, brunch on Sundays.",
            .priceBand = "$",
            .sceneTags = { "geek" },
            .venueVerifiedDaysAgo = 5, .profileVerifiedDaysAgo = 5,
        },
        {
            // In the graveyard: its page still loads, with the closure note.
            .slug = "the-wren",
            .name = "The Wren",
            .description = "A small bar with a patio.",
            .street = "1100 E Union St", .city = "Seattle", .zip = "98122",
            .neighborhood = "capitol-hill",
            .crowdVibe = "The bear bar before the bear bars.",
            .priceBand = "$",
            .sceneTags = { "bear" },
            .venueVerifiedDaysAgo = 200, .profileVerifiedDaysAgo = 200,
            .status = DbSchema::kFreshnessStatusClosed,
            .closureNote = "Closed after twelve years when the building was sold.",
        },
        {
            .slug = "iron-and-oak",
            .name = "Iron & Oak",
            .description = "A leather bar with a dress-code night.",
            .street = "1500 E Olive Way", .city = "Seattle", .zip = "98122",
            .neighborhood = "capitol-hill",
            .crowdVibe = "Leather and gear crowd; dress code on Fridays.",
            .priceBand = "$$",
            .sceneTags = { "leather" },
            .venueVerifiedDaysAgo = 15, .profileVerifiedDaysAgo = 15,
            .adult = true,
            .links = { { "Iron & Oak events", DbSchema::kGuideLinksPlatformWebsite,
                         "https://example.org/iron-and-oak" } },
        },
    };
    return venues;
}

const std::vector<OrganizationSpec>& SampleOrganizations() {
    static const std::vector<OrganizationSpec> organizations{
        {
            // Seasonal: still listed, with a "Seasonal" marker.
            .slug = "cascade-queer-softball",
            .name = "Cascade Queer Softball League",
            .kind = DbSchema::kOrganizationsOrgKindSportsLeague,
            .description = "Recreational and competitive divisions; new players placed on teams.",
            .cadence = "Sundays, April through August",
            .sceneTags = { "jock" },
            .verifiedDaysAgo = 30,
            .status = DbSchema::kFreshnessStatusSeasonal,
            .links = { { "League sign-ups", DbSchema::kGuideLinksPlatformWebsite,
                         "https://example.org/softball-signups" } },
        },
        {
            .slug = "harbor-voices-chorus",
            .name = "Harbor Voices Chorus",
            .kind = DbSchema::kOrganizationsOrgKindChorusPerformance,
            .description = "A community chorus. No audition for the fall session.",
            .cadence = "Rehearsals Monday evenings",
            .neighborhood = "capitol-hill",
            .verifiedDaysAgo = 90,
        },
        {
            .slug = "clear-path-recovery-circle",
            .name = "Clear Path Recovery Circle",
            .kind = DbSchema::kOrganizationsOrgKindRecovery,
            .description = "Peer recovery meetings. Newcomers welcome.",
            .cadence = "Tuesdays and Fridays, 7 PM",
            .neighborhood = "capitol-hill",
            .sceneTags = { "sober" },
            .verifiedDaysAgo = 14,
        },
        {
            // Never verified: its stamp says so.
            .slug = "open-table-fellowship",
            .name = "Open Table Fellowship",
            .kind = DbSchema::kOrganizationsOrgKindFaith,
            .description = "An affirming congregation with a Sunday potluck.",
            .cadence = "Sundays at 11 AM",
            .neighborhood = "first-hill",
        },
        {
            .slug = "lakeside-queer-professionals",
            .name = "Lakeside Queer Professionals",
            .kind = DbSchema::kOrganizationsOrgKindProfessional,
            .description = "Networking and mentoring across Eastside employers.",
            .cadence = "Second Wednesdays",
            .neighborhood = "bellevue",
            .verifiedDaysAgo = 40,
            .links = { { "Mailing list", DbSchema::kGuideLinksPlatformOther,
                         "https://example.org/lqp-list" } },
        },
        {
            .slug = "rolling-dice-gamers",
            .name = "Rolling Dice Gamers",
            .kind = DbSchema::kOrganizationsOrgKindClubHobby,
            .description = "Board and tabletop games; teaching tables every week.",
            .cadence = "Thursdays",
            .neighborhood = "bellevue",
            .sceneTags = { "geek" },
            .verifiedDaysAgo = 7,
            .links = { { "Game night Discord", DbSchema::kGuideLinksPlatformDiscord,
                         "https://example.org/rolling-dice" } },
        },
        {
            .slug = "pacific-leather-collective",
            .name = "Pacific Leather Collective",
            .kind = DbSchema::kOrganizationsOrgKindSocialMeetup,
            .description = "Socials and workshops for the leather community.",
            .cadence = "First Fridays",
            .neighborhood = "capitol-hill",
            .sceneTags = { "leather" },
            .verifiedDaysAgo = 20,
            .adult = true,
        },
        {
            // In the graveyard.
            .slug = "night-owls-book-club",
            .name = "Night Owls Book Club",
            .kind = DbSchema::kOrganizationsOrgKindClubHobby,
            .description = "A monthly book club for older readers.",
            .cadence = "Last Sundays",
            .neighborhood = "wallingford",
            .sceneTags = { "elders" },
            .verifiedDaysAgo = 150,
            .status = DbSchema::kFreshnessStatusClosed,
            .closureNote = "Wound down after its last meeting; members moved to a library group.",
        },
        {
            // Moved: out of the directory and the graveyard, but its page loads.
            .slug = "eastlake-run-club",
            .name = "Eastlake Run Club",
            .kind = DbSchema::kOrganizationsOrgKindSportsLeague,
            .description = "Now meets as part of a citywide running group.",
            .cadence = "Saturdays, 9 AM",
            .sceneTags = { "jock" },
            .verifiedDaysAgo = 60,
            .status = DbSchema::kFreshnessStatusMoved,
        },
    };
    return organizations;
}

const std::vector<PlaceSpec>& SamplePlaces() {
    static const std::vector<PlaceSpec> places{
        {
            // GD1's example of an 18+ place, re-checked monthly while it's in
            // the news. The real entry and its current-rules page are GD10's.
            .slug = "denny-blaine-park",
            .name = "Denny Blaine Park",
            .kind = DbSchema::kPlacesPlaceKindParkBeach,
            .description = "Sample listing. The real entry and its current rules are GD10's.",
            .verifiedDaysAgo = 10,
            .adult = true,
            .volatilityTier = DbSchema::kVolatilityTierMonthly,
        },
        {
            .slug = "rainbow-mural-wall",
            .name = "Rainbow Mural Wall",
            .kind = DbSchema::kPlacesPlaceKindLandmark,
            .description = "A community mural, repainted every few summers.",
            .addressText = "E Pike St and 11th Ave",
            .neighborhood = "capitol-hill",
            .verifiedDaysAgo = 100,
            .links = { { "Mural project", DbSchema::kGuideLinksPlatformWebsite,
                         "https://example.org/rainbow-mural" } },
        },
    };
    return places;
}

// Made-up 555 numbers, all of them.
const std::vector<ServiceSpec>& SampleServices() {
    static const std::vector<ServiceSpec> services{
        { "neighborhood-health-clinic", "Neighborhood Health Clinic",
          DbSchema::kServiceListingsServiceKindStiTesting,
          "Walk-in testing on weekday evenings.", "2065550601", "capitol-hill", 25 },
        { "prep-navigation-program", "PrEP Navigation Program",
          DbSchema::kServiceListingsServiceKindPrep,
          "Help starting PrEP, including with insurance.", "2065550602", "", 50 },
        { "eastside-gender-care", "Eastside Gender Care",
          DbSchema::kServiceListingsServiceKindGenderAffirming,
          "Gender-affirming primary care.", "2065550603", "bellevue", 70 },
        { "affirming-counseling-collective", "Affirming Counseling Collective",
          DbSchema::kServiceListingsServiceKindMentalHealth,
          "Sliding-scale counseling.", "2065550604", "fremont", std::nullopt },
        { "friendly-dental-group", "Friendly Dental Group",
          DbSchema::kServiceListingsServiceKindDental,
          "General dentistry.", "2065550605", "columbia-city", 120 },
        { "northside-barbers", "Northside Barbers",
          DbSchema::kServiceListingsServiceKindEverydayService,
          "Haircuts for every gender, priced by length.", "2065550606", "fremont", 30 },
    };
    return services;
}

// The scene intros and neighborhood rundowns follow GD2's slug conventions: a
// `scenes` page with the scene's slug, a `new-to-seattle` page with the
// neighborhood's. Every body links in-app and says it's sample content.
const std::vector<PageSpec>& SamplePages() {
    static const std::vector<PageSpec> pages{
        {
            .slug = "leather",
            .title = "Leather & kink",
            .section = DbSchema::kGuidePagesSectionScenes,
            .body = R"(Leather and kink nights here are friendly to newcomers, and most of them happen on Capitol Hill.

- **Start at a bar night.** [Iron & Oak](/guide/venues/iron-and-oak) has a dress-code night on Fridays. Nobody minds if your gear is a work in progress.
- **Meet people at a social.** The [Pacific Leather Collective](/guide/orgs/pacific-leather-collective) holds first-Friday socials and the odd workshop.
- **Learn the unwritten rules.** [Etiquette basics](/guide/pages/etiquette-basics), in the 18+ section, covers them.

*Sample content: the venues and groups here are made up.*)",
            .publishedDaysAgo = 30,
            .sceneTags = { "leather" },
        },
        {
            .slug = "bear",
            .title = "Bears",
            .section = DbSchema::kGuidePagesSectionScenes,
            .body = R"(Bears, cubs, otters and their admirers have a weekly home base and a long memory.

- **Thursdays** are [Bear Happy Hour](/events/series/bear-happy-hour) at [The Lantern Room](/guide/venues/the-lantern-room), 6 to 9.
- **The old haunt:** [The Wren](/guide/venues/the-wren) closed after twelve years. Its page stays up in the [graveyard](/guide/graveyard).

*Sample content: the venues here are made up.*)",
            .publishedDaysAgo = 12,
            .sceneTags = { "bear" },
        },
        {
            // A link-out scene's intro: the landing is this plus its links.
            .slug = "trans",
            .title = "Trans Seattle",
            .section = DbSchema::kGuidePagesSectionScenes,
            .body = R"(Trans community spaces are run by trans people, and they do it better than we could. The links below go to the community's own calendars and groups, with credit.

*Sample content: the links are placeholders.*)",
            .publishedDaysAgo = 15,
            .sceneTags = { "trans" },
        },
        {
            .slug = "capitol-hill",
            .title = "Capitol Hill",
            .section = DbSchema::kGuidePagesSectionNewToSeattle,
            .body = R"(In this sample guide, Capitol Hill has the most listings of anywhere.

**Where to start**

- **Bars:** [The Lantern Room](/guide/venues/the-lantern-room) for a low-key night, [Iron & Oak](/guide/venues/iron-and-oak) for leather.
- **Groups:** the [Harbor Voices Chorus](/guide/orgs/harbor-voices-chorus) rehearses on Mondays, and the [Clear Path Recovery Circle](/guide/orgs/clear-path-recovery-circle) meets twice a week.
- **Health:** walk-in testing is listed under [Services](/guide/services). There's more in [Health](/guide/sections/health).

*Sample content: the listings here are made up.*)",
            .publishedDaysAgo = 20,
        },
        {
            .slug = "bellevue",
            .title = "Bellevue",
            .section = DbSchema::kGuidePagesSectionNewToSeattle,
            .body = R"(The Eastside has its own scene, no bridge required.

- **Game nights:** [Board Game Night](/events/series/board-game-night) at [Eastside Commons](/guide/venues/eastside-commons) on Wednesdays, and [Rolling Dice Gamers](/guide/orgs/rolling-dice-gamers) on Thursdays.
- **Work:** [Lakeside Queer Professionals](/guide/orgs/lakeside-queer-professionals) meets on second Wednesdays.

*Sample content: the listings here are made up.*)",
            .publishedDaysAgo = 8,
        },
        {
            .slug = "getting-tested",
            .title = "Getting tested",
            .section = DbSchema::kGuidePagesSectionHealth,
            .body = R"(Testing is quick, often free, and nobody will ask why you came in.

1. **Pick a place.** Clinics with walk-in hours are listed under [Services](/guide/services).
2. **Ask what makes sense for you.** The staff have heard every question.
3. **Thinking about PrEP?** The [PrEP Navigation Program](/guide/services) can help with insurance.

*Sample content: the clinics here are made up.*)",
            .publishedDaysAgo = 5,
        },
        {
            .slug = "making-friends",
            .title = "Making friends in a new city",
            .section = DbSchema::kGuidePagesSectionFreeze,
            .body = R"(The Freeze thaws for regulars. Pick one weekly thing and go three times in a row.

- **Tuesdays:** [Queer Trivia](/events/series/queer-trivia-tuesdays) at Driftwood Hall.
- **Wednesdays:** [Board Game Night](/events/series/board-game-night) in Bellevue.
- **Thursdays:** [Bear Happy Hour](/events/series/bear-happy-hour) on Capitol Hill.

Or join something with a schedule: a [chorus](/guide/orgs/harbor-voices-chorus), a [league](/guide/orgs/cascade-queer-softball), or [any of the groups](/guide/orgs).

*Sample content: the events and groups here are made up.*)",
            .publishedDaysAgo = 6,
        },
        {
            .slug = "new-to-being-sober",
            .title = "New to being sober",
            .section = DbSchema::kGuidePagesSectionNewTo,
            .body = R"(Sober doesn't mean staying home.

- The [Clear Path Recovery Circle](/guide/orgs/clear-path-recovery-circle) meets Tuesdays and Fridays. Newcomers are welcome.
- The [Sober Saturday Hike](/events/series/sober-saturday-hike) goes out every other Saturday, spring through fall.
- The [Sober scene](/guide/scenes/sober) lists more.

*Sample content: the groups here are made up.*)",
            .publishedDaysAgo = 9,
            .sceneTags = { "sober" },
        },
        {
            .slug = "etiquette-basics",
            .title = "Etiquette basics",
            .section = DbSchema::kGuidePagesSectionAdult,
            .body = R"(A few rules hold everywhere:

- **Ask first,** every time, and take the first no.
- **No photos** without everyone's say-so.
- **What's said at a social** stays there.

*Sample content.*)",
            .publishedDaysAgo = 10,
            .adult = true,
        },
        // Never shown: a draft, and a page scheduled for next week.
        {
            .slug = "insurance-and-costs",
            .title = "Insurance and costs",
            .section = DbSchema::kGuidePagesSectionHealth,
            .body = "Not written yet.",
        },
        {
            .slug = "beat-the-freeze",
            .title = "Beat the Freeze",
            .section = DbSchema::kGuidePagesSectionFreeze,
            .body = "Goes up next week.",
            .scheduledInDays = 7,
        },
    };
    return pages;
}

// The link-out scenes' landings are mostly these.
const std::map<std::string_view, std::vector<LinkSpec>>& SampleSceneLinks() {
    static const std::map<std::string_view, std::vector<LinkSpec>> links{
        { "leather", { { "Leather scene chat", DbSchema::kGuideLinksPlatformDiscord,
                         "https://example.org/leather-chat" } } },
        { "sapphic", { { "Sapphic Seattle calendar", DbSchema::kGuideLinksPlatformWebsite,
                         "https://example.org/sapphic-calendar" },
                       { "Sapphic meetups", DbSchema::kGuideLinksPlatformMeetup,
                         "https://example.org/sapphic-meetups" } } },
        { "trans", { { "Trans community Discord", DbSchema::kGuideLinksPlatformDiscord,
                       "https://example.org/trans-discord" },
                     { "Trans resource list", DbSchema::kGuideLinksPlatformWebsite,
                       "https://example.org/trans-resources" } } },
        { "qtpoc", { { "QTPOC collective", DbSchema::kGuideLinksPlatformInstagram,
                       "https://example.org/qtpoc" } } },
    };
    return links;
}

// ---- the series ----

const std::vector<SeriesSpec>& SampleSeries() {
    static const std::vector<SeriesSpec> series{
        {
            .slug = "bear-happy-hour",
            .title = "Bear Happy Hour",
            .description = "The weekly after-work hang for bears, cubs, otters and admirers.",
            .cadence = "Thursdays, 6–9 PM",
            .venue = "the-lantern-room",
            .url = "https://example.org/bear-happy-hour",
            .verifiedDaysAgo = 20,
            .categories = { "bar" },
            .sceneTags = { "bear" },
        },
        {
            .slug = "queer-trivia-tuesdays",
            .title = "Queer Trivia Tuesdays",
            .description = "Teams of up to six. Prizes for the top three, and for the worst team name.",
            .cadence = "Tuesdays, 7:30 PM",
            .venue = "driftwood-hall",
            .verifiedDaysAgo = 9,
            .categories = { "bar", "community" },
        },
        {
            .slug = "board-game-night",
            .title = "Board Game Night",
            .description = "Bring a game or learn one. Snacks provided.",
            .cadence = "Wednesdays, 7 PM",
            .venue = "eastside-commons",
            .verifiedDaysAgo = 6,
            .categories = { "community" },
            .sceneTags = { "geek" },
        },
        {
            .slug = "sober-saturday-hike",
            .title = "Sober Saturday Hike",
            .description = "An easy group hike, no drinking before or after. Carpools from Capitol Hill.",
            .cadence = "Every other Saturday, spring through fall",
            .url = "https://example.org/sober-saturday-hike",
            .status = DbSchema::kFreshnessStatusSeasonal,
            .categories = { "outdoors" },
            .sceneTags = { "sober" },
        },
        {
            // Ended: off the series index, but its page renders, with no dates.
            .slug = "drag-brunch-club",
            .title = "Drag Brunch Club",
            .description = "Sunday drag brunch. The last one was in the spring.",
            .cadence = "Sundays",
            .venue = "driftwood-hall",
            .status = DbSchema::kFreshnessStatusClosed,
            .verifiedDaysAgo = 120,
            .categories = { "drag" },
        },
    };
    return series;
}

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it != row.end() ? it->second : std::string();
}

int64_t IdColumn(const KeyValueTable& row, std::string_view column) {
    std::string value = Column(row, column);
    return value.empty() ? 0 : std::stoll(value);
}

// ---- the writer ----

class SampleDataWriter {
public:
    SampleDataWriter(Transaction& transaction, DatabaseHelper databaseHelper,
                     SampleDataReport& report)
        : transaction_(transaction), databaseHelper_(databaseHelper), report_(report) {
        nowUs_ = std::stoll(transaction_.RunSqlStatementReturningOneValue("SELECT now_us()"));
        LoadVocabulary();
    }

    void AddVenue(const VenueSpec& spec);
    void AddOrganization(const OrganizationSpec& spec);
    void AddPlace(const PlaceSpec& spec);
    void AddService(const ServiceSpec& spec);
    void AddPage(const PageSpec& spec);
    void AddSceneLinks(std::string_view sceneSlug, const std::vector<LinkSpec>& links);
    void AddSeries(const SeriesSpec& spec);
    void AddEvents(const std::vector<EventSpec>& specs, int64_t reviewerPersonId);

    std::vector<EventSpec> SampleEvents();

private:
    void LoadVocabulary();
    int64_t SceneTagId(std::string_view slug);
    int64_t NeighborhoodId(std::string_view slug);
    int64_t CategoryId(std::string_view slug);
    void Missing(const std::string& item);
    void Count(bool created);
    int64_t DaysAgoUs(int days) const { return nowUs_ - days * kUsPerDay; }
    void SetVerified(KeyValueTable& row, std::string_view column, std::optional<int> daysAgo) const;
    void AddLinks(std::string_view parentColumn, int64_t parentId, const std::vector<LinkSpec>& links);
    int64_t SampleSourceId();

    // Seattle wall-clock time `dayOffset` days from today, DST-aware.
    int64_t SeattleTimeUs(int dayOffset, int hour, int minute = 0);
    int SeattleDayOfWeek();

    Transaction& transaction_;
    DatabaseHelper databaseHelper_;
    SampleDataReport& report_;
    int64_t nowUs_ = 0;
    std::map<std::string, int64_t, std::less<>> sceneTagIds_;
    std::map<std::string, int64_t, std::less<>> neighborhoodIds_;
    std::map<std::string, int64_t, std::less<>> categoryIds_;
    std::map<std::string, int64_t, std::less<>> venueIds_;  // by profile slug
};

void Set(KeyValueTable& row, std::string_view column, std::string_view value) {
    if (!value.empty()) row[std::string(column)] = std::string(value);
}

void SampleDataWriter::LoadVocabulary() {
    for (const KeyValueTable& row : TableHelpers::SceneTags(databaseHelper_).GetAll(transaction_)) {
        sceneTagIds_[Column(row, DbSchema::kSceneTagsSlug)] = IdColumn(row, DbSchema::kSceneTagsId);
    }
    for (const KeyValueTable& row :
         TableHelpers::Neighborhoods(databaseHelper_).GetAll(transaction_)) {
        neighborhoodIds_[Column(row, DbSchema::kNeighborhoodsSlug)] =
            IdColumn(row, DbSchema::kNeighborhoodsId);
    }
    for (const KeyValueTable& row : TableHelpers::Categories(databaseHelper_).GetAll(transaction_)) {
        categoryIds_[Column(row, DbSchema::kCategoriesSlug)] = IdColumn(row, DbSchema::kCategoriesId);
    }
}

void SampleDataWriter::Missing(const std::string& item) {
    std::vector<std::string>& missing = report_.missingVocabulary;
    if (std::find(missing.begin(), missing.end(), item) == missing.end()) missing.push_back(item);
}

int64_t SampleDataWriter::SceneTagId(std::string_view slug) {
    auto it = sceneTagIds_.find(slug);
    if (it != sceneTagIds_.end()) return it->second;
    Missing("scene tag " + std::string(slug));
    return 0;
}

int64_t SampleDataWriter::NeighborhoodId(std::string_view slug) {
    if (slug.empty()) return 0;
    auto it = neighborhoodIds_.find(slug);
    if (it != neighborhoodIds_.end()) return it->second;
    Missing("neighborhood " + std::string(slug));
    return 0;
}

int64_t SampleDataWriter::CategoryId(std::string_view slug) {
    auto it = categoryIds_.find(slug);
    if (it != categoryIds_.end()) return it->second;
    Missing("category " + std::string(slug));
    return 0;
}

void SampleDataWriter::Count(bool created) {
    if (created) {
        ++report_.listingsCreated;
    } else {
        ++report_.listingsAlreadyThere;
    }
}

void SampleDataWriter::SetVerified(KeyValueTable& row, std::string_view column,
                                   std::optional<int> daysAgo) const {
    if (daysAgo) row[std::string(column)] = StringFromInt(DaysAgoUs(*daysAgo));
}

void SampleDataWriter::AddLinks(std::string_view parentColumn, int64_t parentId,
                                const std::vector<LinkSpec>& links) {
    TableHelpers::GuideLinks guideLinks(databaseHelper_);
    int sortOrder = 0;
    for (const LinkSpec& link : links) {
        sortOrder += 10;
        guideLinks.AddLink(transaction_, {
            { std::string(DbSchema::kGuideLinksLabel), std::string(link.label) },
            { std::string(DbSchema::kGuideLinksUrl), std::string(link.url) },
            { std::string(DbSchema::kGuideLinksPlatform), std::string(link.platform) },
            { std::string(DbSchema::kGuideLinksSortOrder), StringFromInt(sortOrder) },
            { std::string(parentColumn), StringFromInt(parentId) },
        });
    }
}

void SampleDataWriter::AddVenue(const VenueSpec& spec) {
    TableHelpers::VenueProfiles profiles(databaseHelper_);
    KeyValueTable existing = profiles.GetProfileBySlug(transaction_, spec.slug);
    if (!existing.empty()) {
        venueIds_[std::string(spec.slug)] = IdColumn(existing, DbSchema::kVenueProfilesVenueId);
        Count(false);
        return;
    }

    KeyValueTable venue{
        { std::string(DbSchema::kVenuesName), std::string(spec.name) },
        { std::string(DbSchema::kVenuesStatus), std::string(spec.status) },
        { std::string(DbSchema::kVenuesState), std::string(kState) },
    };
    Set(venue, DbSchema::kVenuesDescription, spec.description);
    Set(venue, DbSchema::kVenuesAddress, spec.street);
    Set(venue, DbSchema::kVenuesCity, spec.city);
    Set(venue, DbSchema::kVenuesZip, spec.zip);
    Set(venue, DbSchema::kVenuesUrl, spec.url);
    if (int64_t neighborhoodId = NeighborhoodId(spec.neighborhood)) {
        venue[std::string(DbSchema::kVenuesNeighborhoodId)] = StringFromInt(neighborhoodId);
    }
    SetVerified(venue, DbSchema::kVenuesLastVerifiedAtUs, spec.venueVerifiedDaysAgo);
    int64_t venueId = TableHelpers::Venues(databaseHelper_).AddVenue(transaction_, venue);
    venueIds_[std::string(spec.slug)] = venueId;

    KeyValueTable profile{
        { std::string(DbSchema::kVenueProfilesVenueId), StringFromInt(venueId) },
        { std::string(DbSchema::kVenueProfilesSlug), std::string(spec.slug) },
        { std::string(DbSchema::kVenueProfilesAdult), spec.adult ? "true" : "false" },
    };
    Set(profile, DbSchema::kVenueProfilesCrowdVibe, spec.crowdVibe);
    Set(profile, DbSchema::kVenueProfilesPriceBand, spec.priceBand);
    Set(profile, DbSchema::kVenueProfilesClosureNote, spec.closureNote);
    SetVerified(profile, DbSchema::kVenueProfilesLastVerifiedAtUs, spec.profileVerifiedDaysAgo);
    int64_t profileId = profiles.AddProfile(transaction_, profile);

    TableHelpers::VenueProfileSceneTagAssignments sceneTags(databaseHelper_);
    for (std::string_view slug : spec.sceneTags) {
        if (int64_t sceneTagId = SceneTagId(slug)) sceneTags.Assign(transaction_, profileId, sceneTagId);
    }
    AddLinks(DbSchema::kGuideLinksVenueProfileId, profileId, spec.links);
    Count(true);
}

void SampleDataWriter::AddOrganization(const OrganizationSpec& spec) {
    TableHelpers::Organizations organizations(databaseHelper_);
    if (!organizations.GetOrganizationBySlug(transaction_, spec.slug).empty()) {
        Count(false);
        return;
    }

    KeyValueTable values{
        { std::string(DbSchema::kOrganizationsName), std::string(spec.name) },
        { std::string(DbSchema::kOrganizationsSlug), std::string(spec.slug) },
        { std::string(DbSchema::kOrganizationsOrgKind), std::string(spec.kind) },
        { std::string(DbSchema::kOrganizationsUrl), "https://example.org/" + std::string(spec.slug) },
        { std::string(DbSchema::kOrganizationsStatus), std::string(spec.status) },
        { std::string(DbSchema::kOrganizationsAdult), spec.adult ? "true" : "false" },
    };
    Set(values, DbSchema::kOrganizationsDescription, spec.description);
    Set(values, DbSchema::kOrganizationsMeetingCadenceText, spec.cadence);
    Set(values, DbSchema::kOrganizationsClosureNote, spec.closureNote);
    if (int64_t neighborhoodId = NeighborhoodId(spec.neighborhood)) {
        values[std::string(DbSchema::kOrganizationsNeighborhoodId)] = StringFromInt(neighborhoodId);
    }
    SetVerified(values, DbSchema::kOrganizationsLastVerifiedAtUs, spec.verifiedDaysAgo);
    int64_t organizationId = organizations.AddOrganization(transaction_, values);

    TableHelpers::OrganizationSceneTagAssignments sceneTags(databaseHelper_);
    for (std::string_view slug : spec.sceneTags) {
        if (int64_t sceneTagId = SceneTagId(slug)) {
            sceneTags.Assign(transaction_, organizationId, sceneTagId);
        }
    }
    AddLinks(DbSchema::kGuideLinksOrganizationId, organizationId, spec.links);
    Count(true);
}

void SampleDataWriter::AddPlace(const PlaceSpec& spec) {
    TableHelpers::Places places(databaseHelper_);
    if (!places.GetPlaceBySlug(transaction_, spec.slug).empty()) {
        Count(false);
        return;
    }

    KeyValueTable values{
        { std::string(DbSchema::kPlacesName), std::string(spec.name) },
        { std::string(DbSchema::kPlacesSlug), std::string(spec.slug) },
        { std::string(DbSchema::kPlacesPlaceKind), std::string(spec.kind) },
        { std::string(DbSchema::kPlacesAdult), spec.adult ? "true" : "false" },
    };
    Set(values, DbSchema::kPlacesDescription, spec.description);
    Set(values, DbSchema::kPlacesAddressText, spec.addressText);
    Set(values, DbSchema::kPlacesVolatilityTier, spec.volatilityTier);
    if (int64_t neighborhoodId = NeighborhoodId(spec.neighborhood)) {
        values[std::string(DbSchema::kPlacesNeighborhoodId)] = StringFromInt(neighborhoodId);
    }
    SetVerified(values, DbSchema::kPlacesLastVerifiedAtUs, spec.verifiedDaysAgo);
    int64_t placeId = places.AddPlace(transaction_, values);
    AddLinks(DbSchema::kGuideLinksPlaceId, placeId, spec.links);
    Count(true);
}

void SampleDataWriter::AddService(const ServiceSpec& spec) {
    TableHelpers::ServiceListings services(databaseHelper_);
    if (!services.GetServiceListingBySlug(transaction_, spec.slug).empty()) {
        Count(false);
        return;
    }

    const bool isHealth = spec.kind != DbSchema::kServiceListingsServiceKindEverydayService;
    KeyValueTable values{
        { std::string(DbSchema::kServiceListingsName), std::string(spec.name) },
        { std::string(DbSchema::kServiceListingsSlug), std::string(spec.slug) },
        { std::string(DbSchema::kServiceListingsServiceKind), std::string(spec.kind) },
        { std::string(DbSchema::kServiceListingsUrl), "https://example.org/" + std::string(spec.slug) },
        { std::string(DbSchema::kServiceListingsIsHealth), isHealth ? "true" : "false" },
    };
    Set(values, DbSchema::kServiceListingsDescription, spec.description);
    Set(values, DbSchema::kServiceListingsPhone, spec.phone);
    if (int64_t neighborhoodId = NeighborhoodId(spec.neighborhood)) {
        values[std::string(DbSchema::kServiceListingsNeighborhoodId)] = StringFromInt(neighborhoodId);
    }
    SetVerified(values, DbSchema::kServiceListingsLastVerifiedAtUs, spec.verifiedDaysAgo);
    services.AddServiceListing(transaction_, values);
    Count(true);
}

void SampleDataWriter::AddPage(const PageSpec& spec) {
    TableHelpers::GuidePages pages(databaseHelper_);
    if (!pages.GetPageBySlug(transaction_, spec.slug).empty()) {
        Count(false);
        return;
    }

    const bool draft = !spec.publishedDaysAgo && !spec.scheduledInDays;
    KeyValueTable values{
        { std::string(DbSchema::kGuidePagesTitle), std::string(spec.title) },
        { std::string(DbSchema::kGuidePagesSlug), std::string(spec.slug) },
        { std::string(DbSchema::kGuidePagesSection), std::string(spec.section) },
        { std::string(DbSchema::kGuidePagesBodyMarkdown), std::string(spec.body) },
        { std::string(DbSchema::kGuidePagesAdult), spec.adult ? "true" : "false" },
        { std::string(DbSchema::kGuidePagesDraft), draft ? "true" : "false" },
    };
    if (spec.publishedDaysAgo) {
        values[std::string(DbSchema::kGuidePagesPublishAtUs)] =
            StringFromInt(DaysAgoUs(*spec.publishedDaysAgo));
    } else if (spec.scheduledInDays) {
        values[std::string(DbSchema::kGuidePagesPublishAtUs)] =
            StringFromInt(DaysAgoUs(-*spec.scheduledInDays));
    }
    SetVerified(values, DbSchema::kGuidePagesLastVerifiedAtUs,
                spec.verifiedDaysAgo ? spec.verifiedDaysAgo : spec.publishedDaysAgo);
    int64_t pageId = pages.AddPage(transaction_, values);

    TableHelpers::GuidePageSceneTagAssignments sceneTags(databaseHelper_);
    for (std::string_view slug : spec.sceneTags) {
        if (int64_t sceneTagId = SceneTagId(slug)) sceneTags.Assign(transaction_, pageId, sceneTagId);
    }
    Count(true);
}

// A scene's links aren't a listing of their own, so they're matched by
// address: a re-run adds only the ones that aren't there yet.
void SampleDataWriter::AddSceneLinks(std::string_view sceneSlug, const std::vector<LinkSpec>& links) {
    int64_t sceneTagId = SceneTagId(sceneSlug);
    if (sceneTagId == 0) return;
    KeyValueTableArray existing =
        TableHelpers::GuideLinks(databaseHelper_).GetLinksForSceneTag(transaction_, sceneTagId);
    std::vector<LinkSpec> missing;
    for (const LinkSpec& link : links) {
        bool there = std::any_of(existing.begin(), existing.end(), [&](const KeyValueTable& row) {
            return Column(row, DbSchema::kGuideLinksUrl) == link.url;
        });
        if (!there) missing.push_back(link);
    }
    AddLinks(DbSchema::kGuideLinksSceneTagId, sceneTagId, missing);
}

void SampleDataWriter::AddSeries(const SeriesSpec& spec) {
    TableHelpers::Series series(databaseHelper_);
    if (!series.GetSeriesBySlug(transaction_, spec.slug).empty()) {
        Count(false);
        return;
    }

    KeyValueTable values{
        { std::string(DbSchema::kSeriesTitle), std::string(spec.title) },
        { std::string(DbSchema::kSeriesSlug), std::string(spec.slug) },
        { std::string(DbSchema::kSeriesStatus), std::string(spec.status) },
    };
    Set(values, DbSchema::kSeriesDescription, spec.description);
    Set(values, DbSchema::kSeriesCadenceText, spec.cadence);
    Set(values, DbSchema::kSeriesUrl, spec.url);
    if (!spec.venue.empty()) {
        auto venue = venueIds_.find(spec.venue);
        if (venue != venueIds_.end()) {
            values[std::string(DbSchema::kSeriesVenueId)] = StringFromInt(venue->second);
        }
    }
    SetVerified(values, DbSchema::kSeriesLastVerifiedAtUs, spec.verifiedDaysAgo);
    int64_t seriesId = series.AddSeries(transaction_, values);

    TableHelpers::SeriesCategoryAssignments categories(databaseHelper_);
    for (std::string_view slug : spec.categories) {
        if (int64_t categoryId = CategoryId(slug)) categories.Assign(transaction_, seriesId, categoryId);
    }
    TableHelpers::SeriesSceneTagAssignments sceneTags(databaseHelper_);
    for (std::string_view slug : spec.sceneTags) {
        if (int64_t sceneTagId = SceneTagId(slug)) sceneTags.Assign(transaction_, seriesId, sceneTagId);
    }
    Count(true);
}

// ---- the events ----

int64_t SampleDataWriter::SeattleTimeUs(int dayOffset, int hour, int minute) {
    // Today's date in Seattle, plus the offset and the time of day, read back
    // as a Seattle wall-clock time: Postgres applies the right UTC offset on
    // either side of a clock change.
    const std::string zone(Events::kEventsTimeZone);
    return std::stoll(transaction_.RunSqlStatementReturningOneValue(
        "SELECT (EXTRACT(EPOCH FROM ((date_trunc('day', to_timestamp($1::bigint / 1000000.0) "
        "AT TIME ZONE $2) + make_interval(days => $3::int, hours => $4::int, mins => $5::int)) "
        "AT TIME ZONE $2)) * 1000000)::bigint",
        StringFromInt(nowUs_), zone, StringFromInt(dayOffset), StringFromInt(hour),
        StringFromInt(minute)));
}

int SampleDataWriter::SeattleDayOfWeek() {
    return std::stoi(transaction_.RunSqlStatementReturningOneValue(
        "SELECT EXTRACT(DOW FROM to_timestamp($1::bigint / 1000000.0) AT TIME ZONE $2)::int",
        StringFromInt(nowUs_), std::string(Events::kEventsTimeZone)));
}

// Every event lands in the four weeks from now, most of them dated relative
// to today so the calendar always has something tonight, this weekend and in
// each of the next weeks. A weekly series gets its next four dates.
std::vector<EventSpec> SampleDataWriter::SampleEvents() {
    const int today = SeattleDayOfWeek();
    // Tonight if it hasn't started yet, otherwise tomorrow night.
    auto tonightOrTomorrow = [&](int hour, int minute) {
        int64_t tonight = SeattleTimeUs(0, hour, minute);
        return tonight > nowUs_ ? tonight : SeattleTimeUs(1, hour, minute);
    };
    // Days from today to the next time `dayOfWeek` comes round with
    // `hour:minute` still ahead: today, if it hasn't passed yet.
    auto firstDay = [&](int dayOfWeek, int hour, int minute) {
        int days = (dayOfWeek - today + 7) % 7;
        return SeattleTimeUs(days, hour, minute) > nowUs_ ? days : days + 7;
    };
    auto nextOn = [&](int dayOfWeek, int hour, int minute) {
        return SeattleTimeUs(firstDay(dayOfWeek, hour, minute), hour, minute);
    };

    std::vector<EventSpec> events{
        { .key = "karaoke-night", .title = "Karaoke Night", .startsAtUs = tonightOrTomorrow(20, 0),
          .durationMinutes = 180, .venue = "the-lantern-room",
          .description = "Sign up at the bar. Duets encouraged.", .costText = "Free",
          .categories = { "bar", "music" } },
        { .key = "drag-revue", .title = "Late Show: Drag Revue",
          .startsAtUs = tonightOrTomorrow(22, 30), .durationMinutes = 120,
          .venue = "driftwood-hall", .description = "Four queens, two kings, one very tired DJ.",
          .url = "https://example.org/drag-revue", .costText = "$15",
          .categories = { "drag", "nightlife" } },
        { .key = "open-mic", .title = "Open Mic (awaiting review)",
          .startsAtUs = SeattleTimeUs(1, 20), .durationMinutes = 120, .venue = "driftwood-hall",
          .description = "Sample event left pending, for the review queue.",
          .categories = { "music" }, .approve = false },

        // The weekend, Friday to Sunday: each on its next day still ahead.
        { .key = "leather-night", .title = "Leather Night",
          .startsAtUs = nextOn(kFriday, 21, 0), .durationMinutes = 300, .venue = "iron-and-oak",
          .description = "Dress code encouraged, not enforced. Coat check at the door.",
          .costText = "$10 cover", .categories = { "bar" }, .sceneTags = { "leather" } },
        { .key = "queer-craft-fair", .title = "Queer Craft Fair",
          .startsAtUs = nextOn(kSaturday, 14, 0), .durationMinutes = 240,
          .venue = "columbia-city-social-club",
          .description = "Thirty local makers, zines, prints and a button station.",
          .costText = "Free", .categories = { "arts", "community" }, .sceneTags = { "sapphic" } },
        { .key = "disco-inferno", .title = "Disco Inferno Dance Party",
          .startsAtUs = nextOn(kSaturday, 22, 0), .durationMinutes = 240,
          .venue = "driftwood-hall", .costText = "$12", .categories = { "nightlife", "music" } },
        { .key = "drag-brunch", .title = "Drag Brunch", .startsAtUs = nextOn(kSunday, 11, 0),
          .durationMinutes = 120, .venue = "eastside-commons",
          .description = "Bottomless mimosas optional; reservations recommended.",
          .url = "https://example.org/eastside-drag-brunch", .costText = "$35",
          .categories = { "drag" } },

        // Spread over the coming weeks.
        { .key = "elders-tea", .title = "Elders Afternoon Tea", .startsAtUs = SeattleTimeUs(10, 15),
          .durationMinutes = 120, .venue = "columbia-city-social-club",
          .description = "Tea and conversation for LGBTQ+ elders and their friends.",
          .costText = "Free", .categories = { "community" }, .sceneTags = { "elders" } },
        { .key = "pride-planning-picnic", .title = "Pride Planning Picnic",
          .startsAtUs = SeattleTimeUs(12, 12), .durationMinutes = 180,
          .locationText = "Volunteer Park, near the water tower",
          .description = "Bring a dish and an idea for next year's Pride.", .costText = "Free",
          .categories = { "pride", "community" } },
        { .key = "food-bank-shift", .title = "Food Bank Volunteer Shift",
          .startsAtUs = SeattleTimeUs(16, 9), .durationMinutes = 180,
          .locationText = "Community food bank, Rainier Valley",
          .description = "Sorting and packing. Closed-toe shoes, please.", .costText = "Free",
          .categories = { "volunteer" } },
        { .key = "furry-meet", .title = "Furry Meet & Greet", .startsAtUs = SeattleTimeUs(18, 18),
          .durationMinutes = 180, .venue = "eastside-commons",
          .description = "A relaxed meet-and-greet. Partial suits welcome.", .costText = "Free",
          .categories = { "community" }, .sceneTags = { "furry" } },
        { .key = "chorus-fall-concert", .title = "Harbor Voices Fall Concert",
          .startsAtUs = SeattleTimeUs(20, 19), .durationMinutes = 120, .venue = "driftwood-hall",
          .description = "The Harbor Voices Chorus sings its fall set.", .costText = "$20",
          .categories = { "music" } },
        { .key = "softball-clinic", .title = "Softball Skills Clinic",
          .startsAtUs = SeattleTimeUs(22, 10), .durationMinutes = 180,
          .locationText = "A North Seattle ballfield (details on sign-up)",
          .description = "Throwing, catching and batting for every level, ahead of spring sign-ups.",
          .costText = "Free", .categories = { "sports" }, .sceneTags = { "jock" } },
        { .key = "film-night", .title = "Queer Shorts Film Night",
          .startsAtUs = SeattleTimeUs(25, 19, 30), .durationMinutes = 120,
          .venue = "columbia-city-social-club",
          .description = "Eight short films by Pacific Northwest trans and nonbinary filmmakers.",
          .costText = "$8 suggested", .categories = { "theatre", "arts" },
          .sceneTags = { "trans" } },
    };

    // The weekly series: each one's next four dates, inheriting its tags. Keyed
    // by occurrence, so a later run moves them along rather than adding more.
    auto weekly = [&](std::string_view series, std::string_view title, int dayOfWeek, int hour,
                      int minute, int durationMinutes, std::string_view venue,
                      std::string_view costText) {
        const int first = firstDay(dayOfWeek, hour, minute);
        for (int occurrence = 1; occurrence <= 4; ++occurrence) {
            events.push_back({
                .key = std::string(series) + "-" + std::to_string(occurrence),
                .title = title,
                .startsAtUs = SeattleTimeUs(first + (occurrence - 1) * 7, hour, minute),
                .durationMinutes = durationMinutes,
                .venue = venue,
                .costText = costText,
                .series = series,
            });
        }
    };
    weekly("bear-happy-hour", "Bear Happy Hour", kThursday, 18, 0, 180, "the-lantern-room",
           "No cover");
    weekly("queer-trivia-tuesdays", "Queer Trivia", kTuesday, 19, 30, 120, "driftwood-hall", "Free");
    weekly("board-game-night", "Board Game Night", kWednesday, 19, 0, 180, "eastside-commons",
           "Free");

    // Every other Saturday, from a trailhead, with no set end.
    const int firstHike = firstDay(kSaturday, 10, 0);
    for (int occurrence = 1; occurrence <= 2; ++occurrence) {
        events.push_back({
            .key = "sober-saturday-hike-" + std::to_string(occurrence),
            .title = "Sober Saturday Hike",
            .startsAtUs = SeattleTimeUs(firstHike + (occurrence - 1) * 14, 10, 0),
            .locationText = "Cougar Mountain, Red Town trailhead",
            .url = "https://example.org/sober-saturday-hike",
            .costText = "Free",
            .series = "sober-saturday-hike",
        });
    }
    return events;
}

int64_t SampleDataWriter::SampleSourceId() {
    TableHelpers::EventSources sources(databaseHelper_);
    for (const KeyValueTable& source : sources.GetEnabledSources(transaction_)) {
        if (Column(source, DbSchema::kEventSourcesName) == kSampleSourceName) {
            return IdColumn(source, DbSchema::kEventSourcesId);
        }
    }
    return sources.AddSource(transaction_, {
        { std::string(DbSchema::kEventSourcesName), std::string(kSampleSourceName) },
        { std::string(DbSchema::kEventSourcesKind), std::string(DbSchema::kEventSourcesKindIcsFeed) },
        { std::string(DbSchema::kEventSourcesNotes), std::string(kSampleSourceNotes) },
    });
}

void SampleDataWriter::AddEvents(const std::vector<EventSpec>& specs, int64_t reviewerPersonId) {
    const int64_t sourceId = SampleSourceId();
    std::vector<Events::IngestItem> items;
    items.reserve(specs.size());
    for (const EventSpec& spec : specs) {
        Events::IngestItem item;
        item.sourceId = sourceId;
        item.externalKey = std::string(kSampleExternalKeyPrefix) + spec.key;
        item.title = std::string(spec.title);
        item.startsAtUs = spec.startsAtUs;
        if (spec.durationMinutes > 0) {
            item.endsAtUs = spec.startsAtUs + spec.durationMinutes * kUsPerMinute;
        }
        if (!spec.description.empty()) item.description = std::string(spec.description);
        if (!spec.url.empty()) item.url = std::string(spec.url);
        if (!spec.costText.empty()) item.costText = std::string(spec.costText);
        if (!spec.locationText.empty()) item.locationText = std::string(spec.locationText);
        if (!spec.venue.empty()) {
            auto venue = venueIds_.find(spec.venue);
            if (venue != venueIds_.end()) item.venueId = venue->second;
        }
        // Always stated, so a re-run sets them back exactly: an empty list
        // means "the series' tags".
        item.seriesSlug = std::string(spec.series);
        item.categorySlugs = std::vector<std::string>(spec.categories.begin(), spec.categories.end());
        item.sceneTagSlugs = std::vector<std::string>(spec.sceneTags.begin(), spec.sceneTags.end());
        items.push_back(std::move(item));
    }

    Events::EventHelper helper(databaseHelper_);
    Events::IngestResult result = helper.IngestEvents(transaction_, items);
    if (!result.ok) {
        throw std::runtime_error("The sample events didn't ingest: " + result.errorCode
                                 + " on item " + std::to_string(result.errorItemIndex));
    }

    TableHelpers::Events events(databaseHelper_);
    for (size_t index = 0; index < result.items.size(); ++index) {
        const Events::IngestItemResult& item = result.items[index];
        if (item.outcome == Events::kIngestOutcomeCreated) {
            ++report_.eventsCreated;
        } else if (item.outcome == Events::kIngestOutcomeUpdated
                   || item.outcome == Events::kIngestOutcomeRevertedToPending) {
            ++report_.eventsUpdated;
        } else {
            ++report_.eventsUnchanged;
        }
        for (const std::string& slug : item.ignoredCategorySlugs) Missing("category " + slug);
        for (const std::string& slug : item.ignoredSceneTagSlugs) Missing("scene tag " + slug);

        const std::string status =
            Column(events.GetEvent(transaction_, item.eventId), DbSchema::kEventsStatus);
        if (specs[index].approve && status == DbSchema::kEventsStatusPending) {
            Events::ReviewResult review = helper.ReviewEvent(
                transaction_, item.eventId, Events::ReviewAction::Approve, reviewerPersonId,
                kSampleReviewNote);
            if (review.ok) ++report_.eventsApproved;
        }
    }
}

}  // namespace

SampleDataReport PopulateSampleData(
    Transaction& transaction, DatabaseHelper databaseHelper, int64_t reviewerPersonId) {
    SampleDataReport report;
    SampleDataWriter writer(transaction, databaseHelper, report);

    // Venues before the series and events that take place at them.
    for (const VenueSpec& venue : SampleVenues()) writer.AddVenue(venue);
    for (const OrganizationSpec& organization : SampleOrganizations()) {
        writer.AddOrganization(organization);
    }
    for (const PlaceSpec& place : SamplePlaces()) writer.AddPlace(place);
    for (const ServiceSpec& service : SampleServices()) writer.AddService(service);
    for (const PageSpec& page : SamplePages()) writer.AddPage(page);
    for (const auto& [sceneSlug, links] : SampleSceneLinks()) writer.AddSceneLinks(sceneSlug, links);
    for (const SeriesSpec& series : SampleSeries()) writer.AddSeries(series);
    writer.AddEvents(writer.SampleEvents(), reviewerPersonId);
    return report;
}
