#pragma once

#include "types.h"

namespace sheaf {

// True when a is listed before b. Dates and sizes default to newest and
// largest first, names and types to A to Z, options.reversed flips that
bool documentLess(const DocumentInfo &a, const DocumentInfo &b, const SortOptions &options);

} // namespace sheaf
