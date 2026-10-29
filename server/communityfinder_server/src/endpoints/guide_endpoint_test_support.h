#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <crow.h>

#include "endpoints/endpoint_test_helper.h"
#include "test/src/util/database_test_helper.h"
#include "util/json_value.h"

// Shared setup for the GD2 §4 guide endpoint tests (get_guide_*_test). Plain
// helpers each test calls at its top — not a fixture.
namespace Endpoints::GuideEndpointTest {

// The guide the tests share. Neighborhoods capitol-hill and ballard; scene
// tags leather (full coverage) then bears (link_out). Slugs in quotes.
struct World {
    int64_t capitolHill = 0;
    int64_t ballard = 0;
    int64_t leather = 0;
    int64_t bears = 0;
    // Venues (venue ids): "example-bar" is open in capitol-hill, tagged
    // leather, with one link; "closed-bar" is closed in ballard, tagged
    // leather, with a closure note.
    int64_t exampleBar = 0;
    int64_t closedBar = 0;
    // Organizations: "example-league" (sports-league, capitol-hill, leather,
    // one link); "example-chorus" (chorus-performance, ballard, bears);
    // "closed-club" (club-hobby, closed, with a closure note).
    int64_t league = 0;
    int64_t chorus = 0;
    int64_t closedClub = 0;
    // Place "example-beach": 18+, capitol-hill, leather, one link.
    int64_t beach = 0;
    // Services: "example-clinic" (sti-testing, health care, an all-digit
    // phone number); "example-dry-cleaner" (everyday-service, not health care).
    int64_t clinic = 0;
    int64_t dryCleaner = 0;
    // Pages: "leather" (scenes, published: the leather intro); "capitol-hill"
    // (new-to-seattle, published: its rundown); "health-basics" (health,
    // published, tagged leather); "draft-page" (health, a draft);
    // "scheduled-page" (health, publishes tomorrow).
    int64_t leatherIntro = 0;
    int64_t capitolHillRundown = 0;
    int64_t healthBasics = 0;
    int64_t draftPage = 0;
    int64_t scheduledPage = 0;
};
World MakeWorld(Transaction& transaction, DatabaseHelper databaseHelper);

// An anonymous-or-signed-in GET through the routes; `query` is "a=1&b=2"
// without the "?". Shuts the thread pool down before returning: an endpoint's
// async writes re-enter the test's connection otherwise (CLAUDE.md).
crow::response Get(EndpointTestHelper& helper, const std::string& url,
                   const std::string& query = "");

// Creates a person and signs them in on the helper's cookie manager (later
// requests run as them). With `manageGuide`, grants manage_guide first — at
// most once per test (it creates a role).
void SignIn(Transaction& transaction, TestDatabaseUtil& testDb, EndpointTestHelper& helper,
            std::string_view email, bool manageGuide);

// The "slug"s of a JSON array of objects, in order.
std::vector<std::string> SlugsOf(const Json::Value& items);

// The object with this "slug" in a JSON array, or nullptr — find items by
// slug, never by position.
const Json::Value* FindBySlug(const Json::Value& items, std::string_view slug);

// The strings of a JSON array of strings, in order.
std::vector<std::string> Strings(const Json::Value& array);

}  // namespace Endpoints::GuideEndpointTest
