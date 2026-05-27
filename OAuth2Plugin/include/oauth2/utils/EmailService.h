#pragma once

#include <string>
#include <functional>

namespace oauth2
{

/**
 * @brief Abstract email service interface
 */
class IEmailService
{
  public:
    virtual ~IEmailService() = default;

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

/**
 * @brief SMTP email service for production
 * Sends real emails via SMTP (163, Gmail, SendGrid, etc.)
 */
class SmtpEmailService : public IEmailService
{
  public:
    struct Config
    {
        std::string host = "smtp.163.com";
        int port = 465;
        std::string username;  // full email: xxx@163.com
        std::string password;  // authorization code
        std::string fromName = "OAuth2 Platform";
        bool useSsl = true;
    };

    explicit SmtpEmailService(const Config &config) : config_(config) {}

    void sendEmail(
      const std::string &to,
      const std::string &subject,
      const std::string &body,
      std::function<void(bool)> &&callback
    ) override;

  private:
    Config config_;
};

/**
 * @brief Get the global email service instance
 * Returns SmtpEmailService if configured, otherwise ConsoleEmailService
 */
IEmailService &getEmailService();

}  // namespace oauth2
