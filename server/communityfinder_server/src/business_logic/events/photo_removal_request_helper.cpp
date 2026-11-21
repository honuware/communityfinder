#include "business_logic/events/photo_removal_request_helper.h"

#include <cctype>
#include <iterator>
#include <set>
#include <stdexcept>

#include "business_logic/events/submission_email.h"
#include "db_schema/events.h"
#include "db_schema/people.h"
#include "db_schema/permissions.h"
#include "db_schema/role_assignments.h"
#include "db_schema/role_permissions.h"
#include "sql_util/table_helpers/people.h"
#include "sql_util/table_helpers/permissions.h"
#include "sql_util/table_helpers/role_assignments.h"
#include "sql_util/table_helpers/role_permissions.h"
#include "util/logging.h"
#include "util/mail/tenant_branding.h"
#include "util/secrets/secrets_helper.h"
#include "util/types.h"

namespace Events {
namespace {

constexpr std::string_view kSubjectTemplate = "Photo removal request: {title}";

// One <p> per line so mail clients wrap it sensibly; NormalizeCrLf'd after.
constexpr std::string_view kBodyTemplate = R"(<p>Someone asked {site_name} to remove a photo from <a href="{page_url}">{title}</a>.</p>
<p><strong>Which photo:</strong> {photo}</p>
{message_block}{reply_block}<p>We remove a photo whenever someone asks. In the admin, an event's gallery is in the event editor (Events, then Edit Event); a series' is under Gallery Photos.</p>
)";

constexpr std::string_view kMessageBlockTemplate =
    "<p><strong>Their message:</strong><br>\n{message}</p>\n";
constexpr std::string_view kReplyBlockTemplate =
    "<p><strong>Reply to:</strong> <a href=\"mailto:{reply_email}\">{reply_email}</a></p>\n";
constexpr std::string_view kNoReplyBlock = "<p>They left no reply address.</p>\n";
constexpr std::string_view kPhotoNotGiven = "Not given; their message says which.";

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it == row.end() ? std::string() : it->second;
}

std::string Lower(std::string_view text) {
    std::string lowered(text);
    for (char& c : lowered) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lowered;
}

// Escaped, with each line break kept as <br>.
std::string EscapeLines(std::string_view text) {
    std::string escaped;
    for (char c : EscapeHtml(text)) {
        if (c == '\r') continue;
        if (c == '\n') {
            escaped += "<br>\n";
        } else {
            escaped += c;
        }
    }
    return escaped;
}

// A subject is one line: any control character becomes a space.
std::string OneLine(std::string_view text) {
    std::string line(text);
    for (char& c : line) {
        if (std::iscntrl(static_cast<unsigned char>(c))) c = ' ';
    }
    return line;
}

// "/events/12" or "/events/series/t4t%20night": site-relative, never "//host".
bool IsSitePath(std::string_view path) {
    if (path.size() < 2 || path[0] != '/' || path[1] == '/') return false;
    for (char c : path) {
        const bool allowed = std::isalnum(static_cast<unsigned char>(c)) || c == '/' ||
                             c == '-' || c == '_' || c == '.' || c == '~' || c == '%';
        if (!allowed) return false;
    }
    return true;
}

// Plain enough to catch a typo, not a full RFC 5322 parse: one @, something
// on each side, a dot in the domain, and no spaces.
bool LooksLikeEmail(std::string_view email) {
    const size_t at = email.find('@');
    if (at == std::string_view::npos || at == 0 || email.find('@', at + 1) != std::string_view::npos) {
        return false;
    }
    const std::string_view domain = email.substr(at + 1);
    if (domain.size() < 3 || domain.front() == '.' || domain.back() == '.' ||
        domain.find('.') == std::string_view::npos || domain.find("..") != std::string_view::npos) {
        return false;
    }
    for (char c : email) {
        if (std::isspace(static_cast<unsigned char>(c)) || std::iscntrl(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

PhotoRemovalResult Fail(std::string_view code, std::string_view field, std::string_view message) {
    PhotoRemovalResult result;
    result.errorCode = std::string(code);
    result.errorField = std::string(field);
    result.errorMessage = std::string(message);
    return result;
}

}  // namespace

PhotoRemovalEmail RenderPhotoRemovalEmail(
    const PhotoRemovalRequest& request, std::string_view siteName, std::string_view siteUrl) {
    std::string base(siteUrl);
    while (!base.empty() && base.back() == '/') base.pop_back();

    const std::string messageBlock = request.message.empty()
        ? std::string()
        : FormatString(kMessageBlockTemplate, { { "message", EscapeLines(request.message) } });
    const std::string replyBlock = request.replyEmail.empty()
        ? std::string(kNoReplyBlock)
        : FormatString(kReplyBlockTemplate, { { "reply_email", EscapeHtml(request.replyEmail) } });

    PhotoRemovalEmail email;
    email.subject = OneLine(FormatString(kSubjectTemplate, { { "title", request.pageTitle } }));
    email.bodyHtml = NormalizeCrLf(FormatString(kBodyTemplate, {
        { "site_name", EscapeHtml(siteName) },
        { "page_url", EscapeHtml(base + request.pagePath) },
        { "title", EscapeHtml(request.pageTitle) },
        { "photo", request.photo.empty() ? std::string(kPhotoNotGiven) : EscapeHtml(request.photo) },
        { "message_block", messageBlock },
        { "reply_block", replyBlock },
    }));
    return email;
}

PhotoRemovalResult CheckPhotoRemovalRequest(PhotoRemovalRequest& request) {
    request.pagePath = StringTrim(request.pagePath);
    request.pageTitle = StringTrim(request.pageTitle);
    request.photo = StringTrim(request.photo);
    request.message = StringTrim(request.message);
    request.replyEmail = StringTrim(request.replyEmail);

    if (request.pagePath.empty()) {
        return Fail(kPhotoRemovalErrorRequired, "page_path", "Which page is the photo on?");
    }
    if (request.pagePath.size() > kPhotoRemovalPagePathMaxLength) {
        return Fail(kPhotoRemovalErrorTooLong, "page_path", "The page address is too long.");
    }
    if (!IsSitePath(request.pagePath)) {
        return Fail(kPhotoRemovalErrorBadPage, "page_path", "The page must be one of this site's.");
    }
    if (request.pageTitle.empty()) {
        return Fail(kPhotoRemovalErrorRequired, "page_title", "Which page is the photo on?");
    }
    if (request.pageTitle.size() > kPhotoRemovalPageTitleMaxLength) {
        return Fail(kPhotoRemovalErrorTooLong, "page_title", "The page title is too long.");
    }
    if (request.photo.size() > kPhotoRemovalPhotoMaxLength) {
        return Fail(kPhotoRemovalErrorTooLong, "photo", "The photo's description is too long.");
    }
    if (request.message.size() > kPhotoRemovalMessageMaxLength) {
        return Fail(kPhotoRemovalErrorTooLong, "message",
                    "Please keep your message under " +
                        std::to_string(kPhotoRemovalMessageMaxLength) + " characters.");
    }
    if (request.photo.empty() && request.message.empty()) {
        return Fail(kPhotoRemovalErrorRequired, "message",
                    "Tell us which photo: its number or what it shows.");
    }
    if (request.replyEmail.size() > kPhotoRemovalReplyEmailMaxLength) {
        return Fail(kPhotoRemovalErrorTooLong, "reply_email", "That email address is too long.");
    }
    if (!request.replyEmail.empty() && !LooksLikeEmail(request.replyEmail)) {
        return Fail(kPhotoRemovalErrorBadEmail, "reply_email",
                    "That doesn't look like an email address.");
    }
    PhotoRemovalResult result;
    result.ok = true;
    return result;
}

// --- The rate limit -----------------------------------------------------------

PhotoRemovalRateLimiter::PhotoRemovalRateLimiter(
    int64_t maxPerVisitor, int64_t maxSiteWide, int64_t windowUs)
    : maxPerVisitor_(maxPerVisitor), maxSiteWide_(maxSiteWide), windowUs_(windowUs) {}

void PhotoRemovalRateLimiter::Expire(int64_t nowUs) {
    // A request counts until a full window has passed since it.
    const int64_t cutoff = nowUs - windowUs_;
    while (!siteWide_.empty() && siteWide_.front() <= cutoff) {
        siteWide_.pop_front();
    }
    for (auto it = byVisitor_.begin(); it != byVisitor_.end();) {
        std::deque<int64_t>& times = it->second;
        while (!times.empty() && times.front() <= cutoff) {
            times.pop_front();
        }
        it = times.empty() ? byVisitor_.erase(it) : std::next(it);
    }
}

bool PhotoRemovalRateLimiter::TryAcquire(std::string_view visitor, int64_t nowUs) {
    std::lock_guard<std::mutex> lock(mutex_);
    Expire(nowUs);
    auto it = byVisitor_.find(visitor);
    const int64_t visitorCount = it == byVisitor_.end() ? 0 : static_cast<int64_t>(it->second.size());
    if (visitorCount >= maxPerVisitor_ || static_cast<int64_t>(siteWide_.size()) >= maxSiteWide_) {
        return false;
    }
    byVisitor_[std::string(visitor)].push_back(nowUs);
    siteWide_.push_back(nowUs);
    return true;
}

void PhotoRemovalRateLimiter::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    byVisitor_.clear();
    siteWide_.clear();
}

PhotoRemovalRateLimiter& SharedPhotoRemovalRateLimiter() {
    static PhotoRemovalRateLimiter limiter(
        kPhotoRemovalMaxPerVisitorPerHour, kPhotoRemovalMaxSiteWidePerHour, kPhotoRemovalRateWindowUs);
    return limiter;
}

// --- The helper ---------------------------------------------------------------

PhotoRemovalRequestHelper::PhotoRemovalRequestHelper(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {}

::Mail::MailAddressList PhotoRemovalRequestHelper::Recipients(
    Transaction& transaction, Secrets::SecretsHelper& secrets) {
    ::Mail::MailAddressList recipients;
    std::set<std::string> seen;
    auto add = [&](::Mail::MailAddress address) {
        const std::string key = Lower(StringTrim(address.address));
        if (key.empty() || !seen.insert(key).second) return;
        recipients.push_back(std::move(address));
    };
    add(::Mail::LoadSenderAddress(secrets, transaction));

    int64_t permissionId = 0;
    try {
        permissionId = std::stoll(TableHelpers::Permissions(databaseHelper_)
                                      .GetPermission(transaction, DbSchema::kPermissionManageEvents)
                                      .at(std::string(DbSchema::kPermissionsId)));
    }
    catch (const std::exception&) {
        // Not seeded (only a bare test database): the site address alone.
        return recipients;
    }

    // Through every role that holds it, each person once, in id order.
    std::set<int64_t> personIds;
    TableHelpers::RoleAssignments roleAssignments(databaseHelper_);
    for (const KeyValueTable& grant :
         TableHelpers::RolePermissions(databaseHelper_)
             .GetRolePermissionsForPermission(transaction, permissionId)) {
        const int64_t roleId = std::stoll(Column(grant, DbSchema::kRolePermissionsRoleId));
        for (const KeyValueTable& assignment :
             roleAssignments.GetRoleAssignmentsForRole(transaction, roleId)) {
            personIds.insert(std::stoll(Column(assignment, DbSchema::kRoleAssignmentsPersonId)));
        }
    }

    TableHelpers::People people(databaseHelper_);
    for (int64_t personId : personIds) {
        const KeyValueTable person = people.GetPersonById(transaction, personId);
        // An unconfirmed address may not be theirs.
        if (person.empty() || Column(person, DbSchema::kPeopleEmailVerifiedAt).empty()) {
            continue;
        }
        const std::string name = StringTrim(Column(person, DbSchema::kPeopleFirstName) + " " +
                                            Column(person, DbSchema::kPeopleLastName));
        add({ name, Column(person, DbSchema::kPeopleEmail) });
    }
    return recipients;
}

PhotoRemovalResult PhotoRemovalRequestHelper::Send(
    Transaction& transaction,
    Secrets::SecretsHelper& secrets,
    const ::Mail::MailHelperPtr& mailHelper,
    PhotoRemovalRequest request,
    PhotoRemovalRateLimiter& limiter,
    std::string_view visitor,
    int64_t nowUs) {
    if (!StringTrim(request.website).empty()) {
        PhotoRemovalResult dropped;
        dropped.ok = true;
        dropped.dropped = true;
        return dropped;
    }
    PhotoRemovalResult result = CheckPhotoRemovalRequest(request);
    if (!result.ok) {
        return result;
    }
    // Counted only once it passes, so a refused form costs the visitor nothing.
    if (!limiter.TryAcquire(visitor, nowUs)) {
        return Fail(kPhotoRemovalErrorRateLimited, "",
                    "You've sent several requests in the last hour. Please try again later, "
                    "or email us.");
    }

    const ::Mail::TenantBranding branding = ::Mail::LoadTenantBranding(secrets, transaction);
    const PhotoRemovalEmail email =
        RenderPhotoRemovalEmail(request, branding.studioName, branding.websiteUrl);
    const ::Mail::MailAddress from = ::Mail::LoadSenderAddress(secrets, transaction);
    const ::Mail::MailAddressList recipients = Recipients(transaction, secrets);
    for (const ::Mail::MailAddress& to : recipients) {
        try {
            if (!mailHelper) {
                throw std::runtime_error("no mail helper");
            }
            ::Mail::MailMessage message(from, to);
            message.SetSubject(email.subject);
            message.SetBodyHtml(email.bodyHtml);
            mailHelper->SendMail(message);
            ++result.emailsSent;
        }
        catch (const std::exception& e) {
            LogError() << "[photo_removal_requests] event=mail_failed error=\"" << e.what() << "\"\n";
        }
    }
    if (result.emailsSent == 0) {
        LogError() << "[photo_removal_requests] event=not_sent recipients=" << recipients.size() << "\n";
        return Fail(kPhotoRemovalErrorNotSent, "",
                    "We couldn't send your request just now. Please email us instead.");
    }
    return result;
}

}  // namespace Events
