#include <oauth2/EmailService.h>
#include <drogon/drogon.h>

namespace oauth2
{

void ConsoleEmailService::sendEmail(
  const std::string &to,
  const std::string &subject,
  const std::string &body,
  std::function<void(bool)> &&callback
)
{
    LOG_INFO << "========== EMAIL (Console) ==========";
    LOG_INFO << "To: " << to;
    LOG_INFO << "Subject: " << subject;
    LOG_INFO << "Body: " << body.substr(0, 200) << (body.length() > 200 ? "..." : "");
    LOG_INFO << "=====================================";

    if (callback)
        callback(true);
}

}  // namespace oauth2
