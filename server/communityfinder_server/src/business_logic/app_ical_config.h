#pragma once

#include <string>
#include <string_view>

#include "util/ical_generator.h"

// CommunityFinder's iCal identity, supplied at call sites so util/ical_generator
// (part of the brand-free honuware_foundation component, componentization
// Phase 1.1) carries no app-specific literals. The multi-tenant work later
// sources these per tenant instead of from these process-wide constants.
namespace App {

// RFC 5545 PRODID for generated calendars.
inline constexpr std::string_view kICalProdId = "-//CommunityFinder//Events//EN";

// The domain on every generated UID's "id@domain" and the synthetic-UID
// fallback.
inline constexpr std::string_view kICalUidDomain = "communityfinder.local";

// The calendar config passed to ICalGenerator::GenerateICalendar.
inline ICalGenerator::ICalConfig AppICalConfig() {
    return ICalGenerator::ICalConfig{
        std::string(kICalProdId), std::string(kICalUidDomain)};
}

}  // namespace App
