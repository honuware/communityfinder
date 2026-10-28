#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <crow.h>

#include "endpoints/endpoint_test_helper.h"
#include "test/src/util/database_test_helper.h"
#include "util/json_value.h"

// Shared setup for the EV1 events endpoint tests (get_upcoming_events_test,
// get_event_detail_test, get_event_series_test, admin_ingest_events_test,
// admin_review_event_test). Plain helpers each test calls at its top — not a
// fixture.
namespace Endpoints::EventsEndpointTest {

// The places, vocabulary, feed and series the tests share: categories dance /
// drag / bar, scene tags trans / bears, and the series T4T, which carries
// category `dance` and scene `trans` for its instances to inherit.
struct World {
    int64_t theCuff = 0;       // venue in capitol-hill
    int64_t sunsetTavern = 0;  // venue in ballard
    int64_t feed = 0;          // an ICS feed source with no venue of its own
    int64_t t4t = 0;           // series, slug "t4t"
};
World MakeWorld(Transaction& transaction, DatabaseHelper databaseHelper);

int64_t NowUs(Transaction& transaction);

// An event from `world.feed`, ingested through EventHelper and then reviewed
// into `status` (approved / rejected; pending is left as ingested).
struct EventSpec {
    std::string externalKey;
    int64_t startsAtUs = 0;
    std::string status = "approved";
    std::optional<int64_t> venueId;
    std::optional<std::string> seriesSlug;
    std::optional<std::vector<std::string>> categorySlugs;
    std::optional<std::vector<std::string>> sceneTagSlugs;
};
int64_t AddEvent(Transaction& transaction, DatabaseHelper databaseHelper, const World& world,
                 const EventSpec& spec);

// Creates a person, signs them in on the helper's cookie manager (later
// requests run as them), and returns their id. With `manageEvents`, grants
// manage_events first — at most once per test (it creates a role).
int64_t SignIn(Transaction& transaction, TestDatabaseUtil& testDb, EndpointTestHelper& helper,
               std::string_view email, bool manageEvents);

// One request through the routes; `query` is "a=1&b=2" without the "?". Shuts
// the thread pool down before returning: an endpoint's async writes re-enter
// the test's connection otherwise (CLAUDE.md).
crow::response Request(EndpointTestHelper& helper, crow::HTTPMethod method,
                       const std::string& url, const std::string& body = "",
                       const std::string& query = "");

// The object with this "id" in a JSON array, or nullptr — find items by id,
// never by position.
const Json::Value* FindById(const Json::JsonArray& items, int64_t id);

// The "id"s of a JSON array of objects, in order.
std::vector<int64_t> IdsOf(const Json::JsonArray& items);

}  // namespace Endpoints::EventsEndpointTest
