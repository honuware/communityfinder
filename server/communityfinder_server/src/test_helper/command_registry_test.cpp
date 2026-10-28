#include "command_registry.h"

#include <gtest/gtest.h>

// The REPL's line parser (EV1 §6 added the quotes, so names can have spaces).
namespace TestHelper {
namespace {

TEST(ParseCommandLineTest, QuotedValuesKeepTheirSpaces) {
    ParsedCommand parsed =
        ParseCommandLine(R"(create-venue --name="The Cuff" --neighborhood=capitol-hill)");
    EXPECT_EQ(parsed.name, "create-venue");
    ASSERT_EQ(parsed.args.size(), 2u);
    EXPECT_EQ(parsed.args.at("name"), "The Cuff");
    EXPECT_EQ(parsed.args.at("neighborhood"), "capitol-hill");
}

TEST(ParseCommandLineTest, BareFlagsAreTrueAndStrayWordsAreIgnored) {
    ParsedCommand parsed = ParseCommandLine("  ingest-demo --changed stray\t--approve ");
    EXPECT_EQ(parsed.name, "ingest-demo");
    ASSERT_EQ(parsed.args.size(), 2u);
    EXPECT_EQ(parsed.args.at("changed"), "true");
    EXPECT_EQ(parsed.args.at("approve"), "true");
}

TEST(ParseCommandLineTest, AnEmptyQuotedValueStaysEmpty) {
    ParsedCommand parsed = ParseCommandLine(R"(create-event --title="" --starts_in_hours=0)");
    EXPECT_EQ(parsed.args.at("title"), "");
    EXPECT_EQ(parsed.args.at("starts_in_hours"), "0");
}

TEST(ParseCommandLineTest, ABlankLineHasNoCommand) {
    EXPECT_TRUE(ParseCommandLine("   ").name.empty());
    EXPECT_TRUE(ParseCommandLine("").name.empty());
}

}  // namespace
}  // namespace TestHelper
