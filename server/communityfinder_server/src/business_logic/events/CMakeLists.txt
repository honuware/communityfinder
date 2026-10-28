# EV1 Slice 4 — the events domain: ingest, review, and the public upcoming list,
# plus the domain → KeyValueTable conversion the endpoints serialize. EV2 §1.1
# adds the events → calendar-entry mapping for the ICS endpoints.
target_sources(communityfinder_core PRIVATE
    event_helper.h
    event_helper.cpp
    event_ical.h
    event_ical.cpp
    events_key_value_table.h
    events_key_value_table.cpp
)

target_sources(communityfinder_test_cases PUBLIC
    event_helper_test.cpp
    event_ical_test.cpp
    events_key_value_table_test.cpp
)
