#include "bigint_array_literal.h"

#include "util/types.h"

namespace TableHelpers {

std::string BigintArrayLiteral(const std::vector<int64_t>& ids) {
    std::string literal = "{";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) literal += ",";
        literal += StringFromInt(ids[i]);
    }
    literal += "}";
    return literal;
}

}  // namespace TableHelpers
