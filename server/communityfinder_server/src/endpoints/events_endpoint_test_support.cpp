#include "events_endpoint_test_support.h"

#include <gtest/gtest.h>

#include "business_logic/auth/cookie_manager_test_util.h"
#include "business_logic/auth/person.h"
#include "business_logic/events/event_helper.h"
#include "db_schema/categories.h"
#include "db_schema/event_sources.h"
#include "db_schema/events.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/people.h"
#include "db_schema/scene_tags.h"
#include "db_schema/series.h"
#include "db_schema/venues.h"
#include "sql_util/table_helpers/people.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_helper_test_util.h"
#include "util/thread_pool.h"
#include "util/types.h"

namespace Endpoints::EventsEndpointTest {

World MakeWorld(Transaction& transaction, DatabaseHelper databaseHelper) {
    World world;
    TableHelpers::Neighborhoods neighborhoods(databaseHelper);
    auto addNeighborhood = [&](std::string_view slug) {
        return neighborhoods.AddNeighborhood(transaction, {
            { std::string(DbSchema::kNeighborhoodsSlug), std::string(slug) },
            { std::string(DbSchema::kNeighborhoodsDisplayName), std::string(slug) },
            { std::string(DbSchema::kNeighborhoodsRegion),
              std::string(DbSchema::kNeighborhoodsRegionSeattleCore) },
        });
    };
    int64_t capitolHill = addNeighborhood("capitol-hill");
    int64_t ballard = addNeighborhood("ballard");

    TableHelpers::Venues venues(databaseHelper);
    world.theCuff = venues.AddVenue(transaction, {
        { std::string(DbSchema::kVenuesName), "The Cuff" },
        { std::string(DbSchema::kVenuesNeighborhoodId), StringFromInt(capitolHill) },
    });
    world.sunsetTavern = venues.AddVenue(transaction, {
        { std::string(DbSchema::kVenuesName), "Sunset Tavern" },
        { std::string(DbSchema::kVenuesNeighborhoodId), StringFromInt(ballard) },
    });

    TableHelpers::Categories categories(databaseHelper);
    auto addCategory = [&](std::string_view slug, int64_t sortOrder) {
        return categories.AddCategory(transaction, {
            { std::string(DbSchema::kCategoriesSlug), std::string(slug) },
            { std::string(DbSchema::kCategoriesDisplayName), std::string(slug) },
            { std::string(DbSchema::kCategoriesSortOrder), StringFromInt(sortOrder) },
        });
    };
    int64_t dance = addCategory("dance", 10);
    addCategory("drag", 20);
    addCategory("bar", 30);

    TableHelpers::SceneTags sceneTags(databaseHelper);
    auto addSceneTag = [&](std::string_view slug, int64_t sortOrder) {
        return sceneTags.AddSceneTag(transaction, {
            { std::string(DbSchema::kSceneTagsSlug), std::string(slug) },
            { std::string(DbSchema::kSceneTagsDisplayName), std::string(slug) },
            { std::string(DbSchema::kSceneTagsSortOrder), StringFromInt(sortOrder) },
        });
    };
    int64_t trans = addSceneTag("trans", 10);
    addSceneTag("bears", 20);

    world.feed = TableHelpers::EventSources(databaseHelper).AddSource(transaction, {
        { std::string(DbSchema::kEventSourcesName), "Community calendar" },
        { std::string(DbSchema::kEventSourcesKind), std::string(DbSchema::kEventSourcesKindIcsFeed) },
    });

    world.t4t = TableHelpers::Series(databaseHelper).AddSeries(transaction, {
        { std::string(DbSchema::kSeriesTitle), "T4T" },
        { std::string(DbSchema::kSeriesSlug), "t4t" },
    });
    TableHelpers::SeriesCategoryAssignments(databaseHelper).Assign(transaction, world.t4t, dance);
    TableHelpers::SeriesSceneTagAssignments(databaseHelper).Assign(transaction, world.t4t, trans);
    return world;
}

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

int64_t AddEvent(Transaction& transaction, DatabaseHelper databaseHelper, const World& world,
                 const EventSpec& spec) {
    Events::IngestItem item;
    item.sourceId = world.feed;
    item.externalKey = spec.externalKey;
    item.title = "Event " + spec.externalKey;
    item.startsAtUs = spec.startsAtUs;
    item.venueId = spec.venueId;
    item.seriesSlug = spec.seriesSlug;
    item.categorySlugs = spec.categorySlugs;
    item.sceneTagSlugs = spec.sceneTagSlugs;

    Events::EventHelper helper(databaseHelper);
    Events::IngestResult result = helper.IngestEvents(transaction, { item });
    EXPECT_TRUE(result.ok) << result.errorCode;
    if (!result.ok) {
        return 0;
    }
    int64_t eventId = result.items[0].eventId;
    if (spec.status != DbSchema::kEventsStatusPending) {
        Events::ReviewAction action = spec.status == DbSchema::kEventsStatusApproved
            ? Events::ReviewAction::Approve : Events::ReviewAction::Reject;
        EXPECT_TRUE(helper.ReviewEvent(transaction, eventId, action, 1, "").ok) << spec.status;
    }
    return eventId;
}

int64_t SignIn(Transaction& transaction, TestDatabaseUtil& testDb, EndpointTestHelper& helper,
               std::string_view email, bool manageEvents) {
    auto secrets = helper.GetSecretsHelper();
    // Without a session length, a fresh token reads as expired.
    if (secrets->LookupSecret(transaction, Secrets::kAuthSessionMaxDuractioninMicros).empty()) {
        secrets->AddSecret(transaction, Secrets::kAuthSessionMaxDuractioninMicros,
                           std::to_string(30LL * 60LL * 1000000LL));
    }

    Auth::PersonHelper personHelper(testDb.GetDatabaseHelper());
    Auth::PersonInfo info{ std::string(email), "Test", "Curator" };
    personHelper.CreateFullyValidatedUser(transaction, info, "Password123!");
    std::string token;
    EXPECT_TRUE(personHelper.CreateSessionToken(transaction, secrets, info.email, token));
    int64_t personId = std::stoll(TableHelpers::People(testDb.GetDatabaseHelper())
                                      .LookupPersonByEmail(transaction, info.email)
                                      .at(std::string(DbSchema::kPeopleId)));
    if (manageEvents) {
        helper.GrantPermissionToPerson(transaction, personId, DbSchema::kPermissionManageEvents);
    }

    Auth::CookieProperties cookieProperties;
    cookieProperties.path = "/";
    cookieProperties.sameSite = Auth::CookieSameSitePolicy::None;
    helper.GetCookieManagerTest()->SetCookie("session_token", token, cookieProperties);
    return personId;
}

crow::response Request(EndpointTestHelper& helper, crow::HTTPMethod method,
                       const std::string& url, const std::string& body,
                       const std::string& query) {
    crow::request req;
    req.method = method;
    req.url = url;
    req.body = body;
    if (!query.empty()) {
        req.url_params = crow::query_string(("?" + query).c_str());
    }
    crow::response resp;
    helper.GetWebApp().GetApp().handle_full(req, resp);
    ThreadPool::GetInstance().Shutdown();
    return resp;
}

const Json::Value* FindById(const Json::JsonArray& items, int64_t id) {
    for (const Json::Value& item : items) {
        const Json::Value* itemId = nullptr;
        if (item.HasChild("id", &itemId) && itemId->Is<int64_t>()
            && itemId->Get<int64_t>() == id) {
            return &item;
        }
    }
    return nullptr;
}

std::vector<int64_t> IdsOf(const Json::JsonArray& items) {
    std::vector<int64_t> ids;
    for (const Json::Value& item : items) {
        ids.push_back(item["id"].Get<int64_t>());
    }
    return ids;
}

}  // namespace Endpoints::EventsEndpointTest
