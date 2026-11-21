#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "util/mail/mail_helper.h"

class Transaction;

namespace Secrets {
class SecretsHelper;
}

namespace Events {

// CM3 Phase 2 §12.2–12.3 (OQ11, OQ19) — "Ask us to remove a photo". The
// gallery's form sends what the old mailto said; this checks it and emails the
// site address plus everyone holding manage_events. Nothing is stored: the
// email is the request.

// What a visitor sends. Everything is trimmed before it's checked.
struct PhotoRemovalRequest {
    // The page the photo is on, site-relative ("/events/12"). The email's link
    // is built from the site's own address, so it can never point off-site.
    std::string pagePath;
    std::string pageTitle;
    // Which photo: "photo 2 of 4 ("Midnight on the floor.")". Empty from the
    // gallery-wide link, where the message says which.
    std::string photo;
    std::string message;
    std::string replyEmail;  // optional
    std::string website;     // the honeypot: the form hides it, so only a bot fills it in
};

inline constexpr size_t kPhotoRemovalPagePathMaxLength = 300;
inline constexpr size_t kPhotoRemovalPageTitleMaxLength = 200;
inline constexpr size_t kPhotoRemovalPhotoMaxLength = 500;
inline constexpr size_t kPhotoRemovalMessageMaxLength = 2000;
inline constexpr size_t kPhotoRemovalReplyEmailMaxLength = 254;

// Per visitor (the client IP), and for the whole site so a flood from many
// addresses still can't fill the curators' inboxes. Both per rolling hour.
inline constexpr int64_t kPhotoRemovalMaxPerVisitorPerHour = 3;
inline constexpr int64_t kPhotoRemovalMaxSiteWidePerHour = 20;
inline constexpr int64_t kPhotoRemovalRateWindowUs = 60LL * 60LL * 1'000'000LL;

inline constexpr std::string_view kPhotoRemovalErrorRequired = "REQUIRED";
inline constexpr std::string_view kPhotoRemovalErrorTooLong = "TOO_LONG";
inline constexpr std::string_view kPhotoRemovalErrorBadPage = "BAD_PAGE";
inline constexpr std::string_view kPhotoRemovalErrorBadEmail = "BAD_EMAIL";
inline constexpr std::string_view kPhotoRemovalErrorRateLimited = "RATE_LIMITED";
// No email could be sent; the form shows the site's address instead.
inline constexpr std::string_view kPhotoRemovalErrorNotSent = "NOT_SENT";

struct PhotoRemovalResult {
    bool ok = false;
    // A filled-in honeypot: ok, so the bot sees success, but nothing was sent.
    bool dropped = false;
    std::string errorCode;
    std::string errorField;  // the request field, as the endpoint names it
    std::string errorMessage;
    int64_t emailsSent = 0;
};

struct PhotoRemovalEmail {
    std::string subject;   // one line
    std::string bodyHtml;  // already NormalizeCrLf'd
};

// The request as words. Everything the visitor typed is HTML-escaped; the
// page link is `siteUrl` + the page path; the reply address appears only when
// given. `request` is expected to have passed CheckPhotoRemovalRequest.
PhotoRemovalEmail RenderPhotoRemovalEmail(
    const PhotoRemovalRequest& request, std::string_view siteName, std::string_view siteUrl);

// Trims `request` in place and checks it: page path and title required, the
// path site-relative, a photo or a message (so we know which one), the
// lengths, and a reply address that looks like one. ok when it passes.
PhotoRemovalResult CheckPhotoRemovalRequest(PhotoRemovalRequest& request);

// A rolling-window count per visitor plus a site-wide one, in memory. A
// restart forgets it, which is fine for a form this rare; only accepted
// requests are counted, so memory stays bounded by the site-wide limit.
class PhotoRemovalRateLimiter {
public:
    PhotoRemovalRateLimiter(int64_t maxPerVisitor, int64_t maxSiteWide, int64_t windowUs);

    // True, counting it, when `visitor` may send one more at `nowUs`.
    bool TryAcquire(std::string_view visitor, int64_t nowUs);
    void Clear();

private:
    void Expire(int64_t nowUs);

    const int64_t maxPerVisitor_;
    const int64_t maxSiteWide_;
    const int64_t windowUs_;
    std::mutex mutex_;
    std::map<std::string, std::deque<int64_t>, std::less<>> byVisitor_;
    std::deque<int64_t> siteWide_;
};

// The one the endpoint uses, with the limits above. Tests Clear() it first.
PhotoRemovalRateLimiter& SharedPhotoRemovalRateLimiter();

class PhotoRemovalRequestHelper {
public:
    explicit PhotoRemovalRequestHelper(DatabaseHelper databaseHelper);

    // The site's address first, then everyone holding manage_events through
    // any role (curators and admins), each address once. People without a
    // confirmed email are left out.
    ::Mail::MailAddressList Recipients(Transaction& transaction, Secrets::SecretsHelper& secrets);

    // The honeypot, the checks, the rate limit, then one email per recipient,
    // so curators' addresses aren't shown to each other. ok when at least one
    // went; NOT_SENT when none did.
    PhotoRemovalResult Send(
        Transaction& transaction,
        Secrets::SecretsHelper& secrets,
        const ::Mail::MailHelperPtr& mailHelper,
        PhotoRemovalRequest request,
        PhotoRemovalRateLimiter& limiter,
        std::string_view visitor,
        int64_t nowUs);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace Events
