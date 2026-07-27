# The database_helper executable: --recreate_database / --migrate.
#
# create_database (the framework+app schema DDL + seed composition) compiles
# directly into the exe rather than a separate library — it is used only here.
# (knottyyoga split it into a knotty_yoga_database_helper lib because two spellings
# — knotty_yoga_* vs knottyyoga_* — gave it distinct lib/exe names; CommunityFinder
# has one spelling, so folding create_database into the exe avoids a name clash and
# a needless leaf library.)
add_executable(communityfinder_database_helper
    "main.cpp"
    "create_database.h"
    "create_database.cpp"
)

# communityfinder_core PUBLIC-links every honuware component the seed needs
# (create_framework_tables, PersonHelper, service account, extensions, stored
# procedures, migrations). ABSL for the flags, PQXX for the sql_error catch.
target_link_libraries(communityfinder_database_helper
    ${ABSL_LIB} ${PQXX_LIB} communityfinder_core)

if(WIN32)
    set_target_properties(communityfinder_database_helper PROPERTIES
        LINK_FLAGS "/PDB:communityfinder_database_helper.pdb")
endif()
