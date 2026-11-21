# EV1 Slice 4 — the events domain: ingest, review, and the public upcoming list,
# plus the domain → KeyValueTable conversion the endpoints serialize. EV2 §1.1
# adds the events → calendar-entry mapping for the ICS endpoints. EV3 §3 adds
# the public submission pipeline: its helper, decision email and rate-limit
# settings. EV4 §10 adds the past-events settings. CM3 adds the video-link
# parser, the gallery and its cap, and the per-row photo visibility rule.
target_sources(communityfinder_core PRIVATE
    event_gallery_helper.h
    event_gallery_helper.cpp
    event_helper.h
    event_helper.cpp
    event_ical.h
    event_ical.cpp
    events_key_value_table.h
    events_key_value_table.cpp
    gallery_settings.h
    gallery_settings.cpp
    past_event_settings.h
    past_event_settings.cpp
    photo_removal_request_helper.h
    photo_removal_request_helper.cpp
    photo_visibility.h
    photo_visibility.cpp
    submission_email.h
    submission_email.cpp
    submission_helper.h
    submission_helper.cpp
    submission_settings.h
    submission_settings.cpp
    video_link.h
    video_link.cpp
)

target_sources(communityfinder_test_cases PUBLIC
    event_gallery_helper_test.cpp
    event_helper_test.cpp
    event_ical_test.cpp
    events_key_value_table_test.cpp
    gallery_settings_test.cpp
    past_event_settings_test.cpp
    photo_removal_request_helper_test.cpp
    photo_visibility_test.cpp
    submission_email_test.cpp
    submission_helper_test.cpp
    submission_settings_test.cpp
    video_link_test.cpp
)
