#include <oauth2/error/RequestId.h>
#include <drogon/utils/Utilities.h>

namespace common::error
{

bool RequestId::isValid(const std::string &v)
{
    // Must be non-empty and at most 128 characters (Requirement 6.3 / 6.5).
    if (v.empty() || v.size() > 128)
    {
        return false;
    }

    // Only ASCII alphanumerics and `-`/`_` are permitted.
    for (const unsigned char c : v)
    {
        const bool isAlnum = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                             (c >= 'a' && c <= 'z');
        if (!isAlnum && c != '-' && c != '_')
        {
            return false;
        }
    }
    return true;
}

std::string RequestId::generate()
{
    // Reuse drogon's UUID generator (same convention as
    // observability/AuditLogger). getUuid() yields a 36-char hyphenated UUID,
    // which is non-empty, within 1..128 and unique across requests on the same
    // instance.
    return drogon::utils::getUuid();
}

std::string RequestId::resolve(const drogon::HttpRequestPtr &req)
{
    if (req)
    {
        const std::string header = req->getHeader(kHeader);
        if (isValid(header))
        {
            return header;
        }
    }
    return generate();
}

}  // namespace common::error
