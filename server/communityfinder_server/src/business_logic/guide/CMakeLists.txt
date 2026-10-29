# GD2 §3 — the guide's domain: scene, directory, neighborhood and graveyard
# assembly and the guide-page publish rule, plus the domain → KeyValueTable
# conversion the endpoints serialize.
target_sources(communityfinder_core PRIVATE
    guide_helper.h
    guide_helper.cpp
    guide_key_value_table.h
    guide_key_value_table.cpp
)

target_sources(communityfinder_test_cases PUBLIC
    guide_helper_test.cpp
    guide_key_value_table_test.cpp
)
