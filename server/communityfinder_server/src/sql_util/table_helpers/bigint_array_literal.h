#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace TableHelpers {

// A Postgres array literal of ids, "{1,2,3}", for the batch reads' single
// `= ANY($1::bigint[])` parameter (knottyyoga's co-instructors pattern). One
// bound parameter however many ids, so the SQL stays a constant. Empty input
// gives "{}", which matches nothing.
std::string BigintArrayLiteral(const std::vector<int64_t>& ids);

}  // namespace TableHelpers
