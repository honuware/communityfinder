#include "business_logic/events/photo_removal_request_helper.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/person.h"
#include "db_schema/events.h"
#include "db_schema/people.h"
#include "db_schema/permissions.h"
#include "sql_util/database_access/transaction.h"
#include "sql_util/table_helpers/people.h"
#include "sql_util/table_helpers/permissions.h"
#include "sql_util/table_helpers/role_assignments.h"
#include "sql_util/table_helpers/role_permissions.h"
#include "sql_util/table_helpers/roles.h"
#include "test/src/util/database_test_helper.h"
#include "util/mail/mail_helper_test_util.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_helper.h"

// CM3 Phase 2 §12.2 — photo-removal requests: the checks, the email, the rate
// limit, and who it goes to.
namespace Events {
namespace {

PhotoRemovalRequest MakeRemovalRequest() {
    PhotoRemovalRequest request;
    request.pagePath = "/events/12";
    request.pageTitle = "Board Game Night";
    request.photo = "photo 2 of 4 (\"Midnight on the floor.\")";
    return request;
}

std::vector<std::string> AddressesOf(const ::Mail::MailAddressList& list) {
    std::vector<std::string> addresses;
    for (const ::Mail::MailAddress& address : list) addresses.push_back(address.address);
    return addresses;
}

// A confirmed person (or not), in one role.
int64_t AddPersonInRole(Transaction& transaction, DatabaseHelper databaseHelper,
                        std::string_view email, int64_t roleId, bool confirmed = true) {
    int64_t personId = 0;
    if (confirmed) {
        Auth::PersonHelper(databaseHelper).CreateFullyValidatedUser(
            transaction, { std::string(email), "Cass", "Curator" }, "Password123!");
        personId = std::stoll(TableHelpers::People(databaseHelper)
                                  .LookupPersonByEmail(transaction, email)
                                  .at(std::string(DbSchema::kPeopleId)));
    } else {
        personId = TableHelpers::People(databaseHelper)
                       .AddPerson(transaction, email, "Una", "Unconfirmed", "not-a-real-hash");
    }
    TableHelpers::RoleAssignments(databaseHelper).AddRoleAssignment(transaction, personId, roleId);
    return personId;
}

// A role holding one permission (created when it doesn't exist yet).
int64_t AddRoleWith(Transaction& transaction, DatabaseHelper databaseHelper,
                    std::string_view roleName, std::string_view permissionName) {
    TableHelpers::Permissions permissions(databaseHelper);
    int64_t permissionId = 0;
    try {
        permissionId = std::stoll(permissions.GetPermission(transaction, permissionName)
                                      .at(std::string(DbSchema::kPermissionsId)));
    }
    catch (const std::exception&) {
        permissionId = permissions.AddPermission(transaction, permissionName, "test");
    }
    const int64_t roleId = TableHelpers::Roles(databaseHelper).AddRole(transaction, roleName, "test");
    TableHelpers::RolePermissions(databaseHelper).AddRolePermission(transaction, roleId, permissionId);
    return roleId;
}

void SetSiteAddress(Transaction& transaction, Secrets::SecretsHelper& secrets) {
    secrets.AddSecret(transaction, Secrets::kMailSenderName, "Community Finder");
    secrets.AddSecret(transaction, Secrets::kMailSenderAddress, "site@example.org");
}

TEST(PhotoRemovalRequestHelperTest, TheChecksTrimAndSayWhichFieldIsWrong) {
    PhotoRemovalRequest request = MakeRemovalRequest();
    request.pageTitle = "  Board Game Night \n";
    request.replyEmail = " visitor@example.com ";
    PhotoRemovalResult ok = CheckPhotoRemovalRequest(request);
    EXPECT_TRUE(ok.ok) << ok.errorCode;
    EXPECT_EQ(request.pageTitle, "Board Game Night");
    EXPECT_EQ(request.replyEmail, "visitor@example.com");

    struct Case {
        std::string label;
        void (*change)(PhotoRemovalRequest&);
        std::string_view code;
        std::string field;
    };
    const std::vector<Case> cases = {
        { "no page", [](PhotoRemovalRequest& r) { r.pagePath = "  "; }, kPhotoRemovalErrorRequired, "page_path" },
        { "another site", [](PhotoRemovalRequest& r) { r.pagePath = "https://elsewhere.example/x"; }, kPhotoRemovalErrorBadPage, "page_path" },
        { "protocol-relative", [](PhotoRemovalRequest& r) { r.pagePath = "//elsewhere.example/x"; }, kPhotoRemovalErrorBadPage, "page_path" },
        { "markup in the path", [](PhotoRemovalRequest& r) { r.pagePath = "/events/1\"><b>"; }, kPhotoRemovalErrorBadPage, "page_path" },
        { "long path", [](PhotoRemovalRequest& r) { r.pagePath = "/" + std::string(kPhotoRemovalPagePathMaxLength, 'a'); }, kPhotoRemovalErrorTooLong, "page_path" },
        { "no title", [](PhotoRemovalRequest& r) { r.pageTitle = ""; }, kPhotoRemovalErrorRequired, "page_title" },
        { "long title", [](PhotoRemovalRequest& r) { r.pageTitle = std::string(kPhotoRemovalPageTitleMaxLength + 1, 'a'); }, kPhotoRemovalErrorTooLong, "page_title" },
        { "long photo", [](PhotoRemovalRequest& r) { r.photo = std::string(kPhotoRemovalPhotoMaxLength + 1, 'a'); }, kPhotoRemovalErrorTooLong, "photo" },
        { "long message", [](PhotoRemovalRequest& r) { r.message = std::string(kPhotoRemovalMessageMaxLength + 1, 'a'); }, kPhotoRemovalErrorTooLong, "message" },
        { "neither photo nor message", [](PhotoRemovalRequest& r) { r.photo = " "; r.message = ""; }, kPhotoRemovalErrorRequired, "message" },
        { "not an email", [](PhotoRemovalRequest& r) { r.replyEmail = "visitor at example"; }, kPhotoRemovalErrorBadEmail, "reply_email" },
        { "two @s", [](PhotoRemovalRequest& r) { r.replyEmail = "a@b@example.com"; }, kPhotoRemovalErrorBadEmail, "reply_email" },
        { "long email", [](PhotoRemovalRequest& r) { r.replyEmail = std::string(kPhotoRemovalReplyEmailMaxLength, 'a') + "@x.io"; }, kPhotoRemovalErrorTooLong, "reply_email" },
    };
    for (const Case& c : cases) {
        PhotoRemovalRequest changed = MakeRemovalRequest();
        c.change(changed);
        PhotoRemovalResult result = CheckPhotoRemovalRequest(changed);
        EXPECT_FALSE(result.ok) << c.label;
        EXPECT_EQ(result.errorCode, c.code) << c.label;
        EXPECT_EQ(result.errorField, c.field) << c.label;
        EXPECT_FALSE(result.errorMessage.empty()) << c.label;
    }

    // A message alone is enough: the gallery-wide link has no photo to name.
    PhotoRemovalRequest messageOnly = MakeRemovalRequest();
    messageOnly.photo = "";
    messageOnly.message = "The third one, with me in it.";
    EXPECT_TRUE(CheckPhotoRemovalRequest(messageOnly).ok);
}

TEST(PhotoRemovalRequestHelperTest, TheEmailEscapesWhatWasTypedAndLinksOnlyToThisSite) {
    PhotoRemovalRequest request = MakeRemovalRequest();
    request.pageTitle = "<b>Board</b> {title} Night";
    request.message = "Please take it down.\nI'm in the front row & didn't agree.";
    PhotoRemovalEmail email = RenderPhotoRemovalEmail(request, "Community Finder", "https://example.org/");

    EXPECT_EQ(email.subject, "Photo removal request: <b>Board</b> {title} Night");
    EXPECT_NE(email.bodyHtml.find("<a href=\"https://example.org/events/12\">"), std::string::npos);
    EXPECT_NE(email.bodyHtml.find("&lt;b&gt;Board&lt;/b&gt; &#123;title&#125; Night"), std::string::npos);
    EXPECT_EQ(email.bodyHtml.find("<b>Board"), std::string::npos);
    EXPECT_NE(email.bodyHtml.find("photo 2 of 4 (&quot;Midnight on the floor.&quot;)"), std::string::npos);
    EXPECT_NE(email.bodyHtml.find("Please take it down.<br>\r\nI&#39;m in the front row &amp; didn&#39;t agree."),
              std::string::npos);
    // No reply address given: it says so, and there's no mailto.
    EXPECT_NE(email.bodyHtml.find("They left no reply address."), std::string::npos);
    EXPECT_EQ(email.bodyHtml.find("mailto:"), std::string::npos);
    // mailio needs \r\n throughout.
    for (size_t at = email.bodyHtml.find('\n'); at != std::string::npos; at = email.bodyHtml.find('\n', at + 1)) {
        ASSERT_GT(at, 0u);
        EXPECT_EQ(email.bodyHtml[at - 1], '\r');
    }

    request.replyEmail = "visitor@example.com";
    request.photo = "";
    request.pageTitle = "Line one\r\nBcc: someone@example.com";
    email = RenderPhotoRemovalEmail(request, "Community Finder", "https://example.org");
    EXPECT_NE(email.bodyHtml.find("<a href=\"mailto:visitor@example.com\">visitor@example.com</a>"),
              std::string::npos);
    EXPECT_EQ(email.bodyHtml.find("They left no reply address."), std::string::npos);
    EXPECT_NE(email.bodyHtml.find("their message says which"), std::string::npos);
    EXPECT_NE(email.bodyHtml.find("<a href=\"https://example.org/events/12\">"), std::string::npos);
    // The subject stays one line, whatever was typed.
    EXPECT_EQ(email.subject.find('\n'), std::string::npos);
    EXPECT_EQ(email.subject.find('\r'), std::string::npos);
}

TEST(PhotoRemovalRequestHelperTest, TheRateLimitCountsEachVisitorAndTheWholeSite) {
    PhotoRemovalRateLimiter limiter(2, 3, 100);
    EXPECT_TRUE(limiter.TryAcquire("198.51.100.1", 0));
    EXPECT_TRUE(limiter.TryAcquire("198.51.100.1", 10));
    EXPECT_FALSE(limiter.TryAcquire("198.51.100.1", 20));  // their two
    EXPECT_TRUE(limiter.TryAcquire("198.51.100.2", 30));
    EXPECT_FALSE(limiter.TryAcquire("198.51.100.3", 40));  // the site's three

    // The first expires a full window after it; the visitor gets one back.
    EXPECT_FALSE(limiter.TryAcquire("198.51.100.1", 99));
    EXPECT_TRUE(limiter.TryAcquire("198.51.100.1", 100));
    EXPECT_FALSE(limiter.TryAcquire("198.51.100.3", 105));
    EXPECT_TRUE(limiter.TryAcquire("198.51.100.3", 111));

    limiter.Clear();
    EXPECT_TRUE(limiter.TryAcquire("198.51.100.1", 112));
    EXPECT_TRUE(limiter.TryAcquire("198.51.100.1", 113));
}

TEST(PhotoRemovalRequestHelperTest, ItGoesToTheSiteAndEveryoneHoldingManageEvents) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ItGoesToTheSiteAndEveryoneHoldingManageEvents", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        SetSiteAddress(transaction, *secrets);
        PhotoRemovalRequestHelper helper(db);

        // Before manage_events exists, the site alone.
        EXPECT_EQ(AddressesOf(helper.Recipients(transaction, *secrets)),
                  std::vector<std::string>{ "site@example.org" });

        const int64_t curatorRole = AddRoleWith(transaction, db, DbSchema::kRoleNameCurator, DbSchema::kPermissionManageEvents);
        const int64_t adminRole = AddRoleWith(transaction, db, "admin", DbSchema::kPermissionManageEvents);
        const int64_t volunteerRole = AddRoleWith(transaction, db, "volunteer", "manage_guide");
        AddPersonInRole(transaction, db, "curator@example.com", curatorRole);
        const int64_t adminId = AddPersonInRole(transaction, db, "admin@example.com", adminRole);
        // In both roles, still one email.
        TableHelpers::RoleAssignments(db).AddRoleAssignment(transaction, adminId, curatorRole);
        // A curator without the permission is left out, as is an unconfirmed address.
        AddPersonInRole(transaction, db, "volunteer@example.com", volunteerRole);
        AddPersonInRole(transaction, db, "unconfirmed@example.com", curatorRole, false);
        // The site's own address, as a person: one email, not two.
        AddPersonInRole(transaction, db, "SITE@example.org", curatorRole);

        ::Mail::MailAddressList recipients = helper.Recipients(transaction, *secrets);
        EXPECT_EQ(AddressesOf(recipients),
                  (std::vector<std::string>{ "site@example.org", "curator@example.com", "admin@example.com" }));
        EXPECT_EQ(recipients[0].name, "Community Finder");
        EXPECT_EQ(recipients[1].name, "Cass Curator");
    });
}

TEST(PhotoRemovalRequestHelperTest, EachRecipientGetsTheirOwnEmail) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EachRecipientGetsTheirOwnEmail", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        SetSiteAddress(transaction, *secrets);
        const int64_t curatorRole = AddRoleWith(transaction, db, DbSchema::kRoleNameCurator, DbSchema::kPermissionManageEvents);
        AddPersonInRole(transaction, db, "curator@example.com", curatorRole);
        PhotoRemovalRequestHelper helper(db);
        PhotoRemovalRateLimiter limiter(10, 10, kPhotoRemovalRateWindowUs);
        ::Mail::Test::TestMailHelperPtr mail = ::Mail::Test::MakeTestMailHelper();

        PhotoRemovalResult sent = helper.Send(transaction, *secrets, mail, MakeRemovalRequest(), limiter, "198.51.100.1", 0);
        ASSERT_TRUE(sent.ok) << sent.errorCode;
        EXPECT_FALSE(sent.dropped);
        EXPECT_EQ(sent.emailsSent, 2);
        ASSERT_EQ(mail->GetMessages().size(), 2u);
        // One address per message, so curators never see each other's.
        for (const ::Mail::MailMessage& message : mail->GetMessages()) {
            ASSERT_EQ(message.GetTo().size(), 1u);
            EXPECT_EQ(message.GetFrom().address, "site@example.org");
            EXPECT_EQ(message.GetSubject(), "Photo removal request: Board Game Night");
        }
        EXPECT_EQ(mail->GetMessages()[0].GetTo()[0].address, "site@example.org");
        EXPECT_EQ(mail->GetMessages()[1].GetTo()[0].address, "curator@example.com");

        // The honeypot: success to the bot, nothing sent, nothing counted.
        PhotoRemovalRequest bot = MakeRemovalRequest();
        bot.website = "http://spam.example";
        PhotoRemovalResult dropped = helper.Send(transaction, *secrets, mail, bot, limiter, "198.51.100.1", 1);
        EXPECT_TRUE(dropped.ok);
        EXPECT_TRUE(dropped.dropped);
        EXPECT_EQ(mail->GetMessages().size(), 2u);

        // A refused form sends nothing.
        PhotoRemovalRequest blank = MakeRemovalRequest();
        blank.pageTitle = "";
        EXPECT_EQ(helper.Send(transaction, *secrets, mail, blank, limiter, "198.51.100.1", 2).errorCode,
                  kPhotoRemovalErrorRequired);
        EXPECT_EQ(mail->GetMessages().size(), 2u);

        // No way to send: NOT_SENT, so the form can show the address instead.
        EXPECT_EQ(helper.Send(transaction, *secrets, nullptr, MakeRemovalRequest(), limiter, "198.51.100.1", 3).errorCode,
                  kPhotoRemovalErrorNotSent);

        // Over the limit: refused before anything is sent.
        PhotoRemovalRateLimiter tight(1, 10, kPhotoRemovalRateWindowUs);
        ASSERT_TRUE(helper.Send(transaction, *secrets, mail, MakeRemovalRequest(), tight, "198.51.100.9", 0).ok);
        const size_t before = mail->GetMessages().size();
        EXPECT_EQ(helper.Send(transaction, *secrets, mail, MakeRemovalRequest(), tight, "198.51.100.9", 1).errorCode,
                  kPhotoRemovalErrorRateLimited);
        EXPECT_EQ(mail->GetMessages().size(), before);
    });
}

}  // namespace
}  // namespace Events
