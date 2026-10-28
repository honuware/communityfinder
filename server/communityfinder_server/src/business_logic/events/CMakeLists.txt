# EV1 Slice 4 — the events domain: ingest, review, and the public upcoming list,
# plus the domain → KeyValueTable conversion the endpoints serialize.
target_sources(communityfinder_core PRIVATE
    event_helper.h
    event_helper.cpp
    events_key_value_table.h
    events_key_value_table.cpp
)

target_sources(communityfinder_test_cases PUBLIC
    event_helper_test.cpp
    events_key_value_table_test.cpp
)
