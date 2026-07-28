#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// POST /api/admin/create_user — admin creates an account (first/last/email) and the
// person is emailed a temporary password to sign in and set their own. Gated on the
// `admin_portal` permission. Reuses an existing account by email rather than
// duplicating (reports `already_exists`).
Json::Value PostAdminCreateUser(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
