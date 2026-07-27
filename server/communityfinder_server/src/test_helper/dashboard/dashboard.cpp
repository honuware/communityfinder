#include "dashboard/dashboard.h"

#include <iostream>
#include <string>
#include <vector>
#include <functional>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "command_runner.h"
#include "console_colors.h"
#include "repl/repl.h"

namespace TestHelper {

namespace {

using namespace ftxui;

enum class ScreenAction {
    Back,
    Quit,
    Repl,
    ReplWithCommand,
};

struct ScreenResult {
    ScreenAction action = ScreenAction::Back;
    std::string replCommand;  // For ReplWithCommand
};

// ── Helper: build a status bar element ──
Element StatusBar(TestHelperContext& ctx) {
    std::string userStatus;
    if (ctx.currentPersonId > 0) {
        userStatus = "User: " + ctx.currentPersonName +
            " (" + ctx.currentPersonEmail + ") ID:" +
            std::to_string(ctx.currentPersonId);
    } else {
        userStatus = "No user logged in";
    }
    return hbox({
        text("DB: " + ctx.databaseHelper.GetDatabaseName()) | dim,
        filler(),
        text(userStatus) | dim,
        filler(),
        text("[:] command  [?] help  [Esc] back  [q] quit") | dim,
    }) | borderLight;
}

// ── Helper: data table from SQL ──
struct TableData {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
};

TableData QueryTable(Transaction& transaction, const std::string& sql,
                     const std::vector<std::string>& columns) {
    TableData data;
    data.headers = columns;
    try {
        auto rows = transaction.RunSqlStatementReturningKeyValueTableArray(sql);
        for (const auto& row : rows) {
            std::vector<std::string> vals;
            for (const auto& col : columns) {
                auto it = row.find(col);
                vals.push_back(it != row.end() ? it->second : "");
            }
            data.rows.push_back(vals);
        }
    } catch (const std::exception& e) {
        // Return empty data with an error row so the screen still renders
        data.rows.push_back({"Error: " + std::string(e.what())});
    }
    return data;
}

// Helper: safely load table data within a transaction, catching connection errors
TableData SafeQueryTable(TestHelperContext& ctx, const std::string& sql,
                         const std::vector<std::string>& columns) {
    TableData data;
    data.headers = columns;
    try {
        ctx.transactionProvider->RunInTransaction([&](Transaction& transaction) {
            data = QueryTable(transaction, sql, columns);
        });
    } catch (const std::exception& e) {
        data.rows.push_back({"Error: " + std::string(e.what())});
    }
    return data;
}

// ── Helper: render a table as an FTXUI element ──
Element RenderTable(const TableData& data, int selectedRow = -1) {
    Elements headerCells;
    for (const auto& h : data.headers) {
        headerCells.push_back(text(h) | bold | size(WIDTH, EQUAL, 20));
    }
    Elements tableRows;
    tableRows.push_back(hbox(headerCells) | borderLight);

    for (size_t i = 0; i < data.rows.size(); ++i) {
        Elements cells;
        for (const auto& val : data.rows[i]) {
            cells.push_back(text(val) | size(WIDTH, EQUAL, 20));
        }
        auto row = hbox(cells);
        if (static_cast<int>(i) == selectedRow) {
            row = row | inverted;
        }
        tableRows.push_back(row);
    }
    return vbox(tableRows);
}

// ── People Screen ──
// `people` is a framework table (honuware platform), so this stays domain-free.
// Later phases can add their own screens next to this one.
ScreenResult ShowPeopleScreen(TestHelperContext& ctx) {
    ScreenResult result;
    auto screen = ScreenInteractive::Fullscreen();

    TableData tableData = SafeQueryTable(ctx,
        "SELECT id, first_name || ' ' || last_name AS name, email "
        "FROM people ORDER BY id LIMIT 100",
        {"id", "name", "email"});

    int selected = 0;

    auto handler = CatchEvent(Container::Vertical({}), [&](Event event) {
        if (event == Event::Escape) { result.action = ScreenAction::Back; screen.Exit(); return true; }
        if (event == Event::Character('q')) { result.action = ScreenAction::Quit; screen.Exit(); return true; }
        if (event == Event::Character(':')) { result.action = ScreenAction::Repl; screen.Exit(); return true; }
        if (event == Event::ArrowUp && selected > 0) { selected--; return true; }
        if (event == Event::ArrowDown && selected < static_cast<int>(tableData.rows.size()) - 1) { selected++; return true; }
        if (event == Event::Character('l') && !tableData.rows.empty()) {
            result.action = ScreenAction::ReplWithCommand;
            result.replCommand = "login --person_id=" + tableData.rows[selected][0];
            screen.Exit(); return true;
        }
        return false;
    });

    auto renderer = Renderer(handler, [&] {
        return vbox({
            hbox({ text("People") | bold, filler(), text("[Esc] back  [q] quit") | dim }) | borderHeavy,
            RenderTable(tableData, selected) | flex | border,
            hbox({ text("[l] login as selected  ") | dim,
                   text("[:] command mode") | dim }) | borderLight,
            StatusBar(ctx),
        });
    });

    screen.Loop(renderer);
    return result;
}

}  // namespace

// ── Main Dashboard Loop ──
int RunDashboard(TestHelperContext& ctx) {
    // Map menu entries to screen functions
    using ScreenFn = std::function<ScreenResult(TestHelperContext&)>;
    struct MenuItem {
        std::string label;
        ScreenFn screenFn;  // nullptr for special entries
    };

    std::vector<MenuItem> menuItems = {
        { "People",            ShowPeopleScreen },
        { "Command Mode (:)",  nullptr },
        { "Quit",              nullptr },
    };

    while (true) {
        bool enterRepl = false;
        bool enterHelp = false;
        bool quit = false;
        std::string replPrePopulated;
        int menuSelected = -1;

        auto screen = ScreenInteractive::Fullscreen();

        std::vector<std::string> menuLabels;
        for (const auto& item : menuItems) menuLabels.push_back(item.label);

        int selected = 0;
        auto menu = Menu(&menuLabels, &selected);

        auto menuWithHandler = CatchEvent(menu, [&](Event event) {
            if (event == Event::Return) {
                if (menuLabels[selected] == "Quit") {
                    quit = true; screen.Exit(); return true;
                }
                if (menuLabels[selected] == "Command Mode (:)") {
                    enterRepl = true; screen.Exit(); return true;
                }
                menuSelected = selected;
                screen.Exit();
                return true;
            }
            if (event == Event::Character(':')) {
                enterRepl = true; screen.Exit(); return true;
            }
            if (event == Event::Character('?')) {
                enterHelp = true; screen.Exit(); return true;
            }
            if (event == Event::Character('q')) {
                quit = true; screen.Exit(); return true;
            }
            return false;
        });

        auto renderer = Renderer(menuWithHandler, [&] {
            return vbox({
                hbox({ text("CommunityFinder Test Helper") | bold, filler(), text("[q]uit") | dim }) | borderHeavy,
                menuWithHandler->Render() | flex | border,
                StatusBar(ctx),
            });
        });

        screen.Loop(renderer);

        if (quit) return 0;

        if (enterHelp) {
            // Print help and drop into REPL so the user can see it
            ctx.registry.PrintHelp();
            std::cout << "\n";
            auto replAction = RunRepl(ctx);
            if (replAction == ReplAction::Quit) return 0;
            continue;
        }

        if (enterRepl) {
            auto replAction = RunRepl(ctx, replPrePopulated);
            if (replAction == ReplAction::Quit) return 0;
            continue;
        }

        // Navigate to a sub-screen
        if (menuSelected >= 0 && menuSelected < static_cast<int>(menuItems.size())) {
            auto& item = menuItems[menuSelected];
            if (item.screenFn) {
                auto screenResult = item.screenFn(ctx);
                switch (screenResult.action) {
                    case ScreenAction::Quit: return 0;
                    case ScreenAction::Repl: {
                        auto replAction = RunRepl(ctx);
                        if (replAction == ReplAction::Quit) return 0;
                        break;
                    }
                    case ScreenAction::ReplWithCommand: {
                        auto replAction = RunRepl(ctx, screenResult.replCommand);
                        if (replAction == ReplAction::Quit) return 0;
                        break;
                    }
                    case ScreenAction::Back:
                        break;
                }
            }
        }
    }

    return 0;
}

}  // namespace TestHelper
