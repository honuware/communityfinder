#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_access/transaction.h"

// Made-up sample data for trying the site out on a dev database: a small guide
// (the same world the UI's offline mock shows: venues, organizations, places,
// services, pages and links) and four weeks of events from the day it runs,
// most of them approved. database_helper --populate_sample_data runs it; it is
// never part of the seed, and nothing in it describes a real place.
//
// Safe to re-run. A listing whose slug is already there is left alone (and so
// are its links and tags). The events come in through the ingest path from
// one "Sample data" feed, each under a fixed key, so a later run moves them
// into the four weeks after THAT run rather than adding more; anything the
// move sends back to pending is approved again.
//
// Needs the seeded vocabularies (PopulateAppTables). A scene tag or
// neighborhood the database lacks is left off whatever wanted it and reported.

struct SampleDataReport {
    // Venues (with their guide profiles), organizations, places, services,
    // guide pages and series.
    int listingsCreated = 0;
    int listingsAlreadyThere = 0;
    int eventsCreated = 0;
    int eventsUpdated = 0;  // moved to this run's dates, or otherwise changed
    int eventsUnchanged = 0;
    int eventsApproved = 0;  // by this run
    // "scene tag bear", "neighborhood ballard": slugs this database lacks.
    std::vector<std::string> missingVocabulary;
};

// `reviewerPersonId` is recorded as the approver of the sample events (a plain
// id, as EventHelper::ReviewEvent stores it).
SampleDataReport PopulateSampleData(
    Transaction& transaction, DatabaseHelper databaseHelper, int64_t reviewerPersonId);
