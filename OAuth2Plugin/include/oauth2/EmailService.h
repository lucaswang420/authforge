#pragma once

#include <string>
#include <functional>

namespace oauth2
{

/**
 * @brief Abstract email service interface
 * Implementations: SMTP, SendGrid, AWS SES, Console (dev), etc.
 */
class IEmailService
{
  public:
    virtual ~IEmailService() = default;

    /**
     * @brief Send an email asynchronously
     * @param to Recipient email address
     * @param subject Email subject
     * @param body Email body (HTML or plain text)
     * @param callback Called with true on success, false on failure
     */
    virtual void sendEmail(
      const std::string &to,
      const std::string &subject,
      const std::string &body,
      std::function<void(bool)> &&callback
    ) = 0;
};

/**
 * @brief Console email service for development/testing
 * Logs emails to console instead of sending them
 */
class ConsoleEmailService : public IEmailService
{
  public:
    void sendEmail(
      const std::string &to,
      const std::string &subject,
      const std::string &body,
      std::function<void(bool)> &&callback
    ) override;
};

}  // namespace oauth2
