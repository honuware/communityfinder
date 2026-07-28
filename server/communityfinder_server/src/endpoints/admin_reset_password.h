#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// POST /api/admin/reset_password — admin resets an account's password (by email). A
// fresh temporary password is generated, must_change_password is set, and the person
// is emailed the temporary password. Gated on the `admin_portal` permission.
Json::Value PostAdminResetPassword(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
