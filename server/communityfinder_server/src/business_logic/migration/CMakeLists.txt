# App migration composition: app_migrations (empty stream) + all_migrations
# (honuware framework migrations ++ app migrations, separate id namespaces).
target_sources(communityfinder_core PRIVATE
    app_migrations.h
    app_migrations.cpp
    all_migrations.h
    all_migrations.cpp
)
