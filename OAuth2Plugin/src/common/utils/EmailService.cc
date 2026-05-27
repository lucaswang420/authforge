#include <oauth2/EmailService.h>
#include <oauth2/ConfigManager.h>
#include <drogon/drogon.h>
#include <drogon/HttpClient.h>
#include <sstream>
#include <thread>
#include <memory>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace oauth2
{

// ============================================================
// ConsoleEmailService (development)
// ============================================================

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

// ============================================================
// SmtpEmailService (production)
// Uses system command as a portable fallback
// ============================================================

void SmtpEmailService::sendEmail(
  const std::string &to,
  const std::string &subject,
  const std::string &body,
  std::function<void(bool)> &&callback
)
{
    auto config = config_;
    auto cb = std::make_shared<std::function<void(bool)>>(std::move(callback));

    // Use a detached thread to avoid blocking the event loop
    std::thread([config, to, subject, body, cb]() {
        // Build the Python smtp command (works on all platforms with Python)
        std::ostringstream cmd;
#ifdef _WIN32
        std::string pythonCmd = "python";
#else
        std::string pythonCmd = "python3";
#endif
        cmd << pythonCmd << " -c \""
            << "import smtplib; "
            << "from email.mime.text import MIMEText; "
            << "msg = MIMEText('''" << body << "'''); "
            << "msg['Subject'] = '" << subject << "'; "
            << "msg['From'] = '" << config.fromName << " <" << config.username << ">'; "
            << "msg['To'] = '" << to << "'; "
            << "s = smtplib.SMTP_SSL('" << config.host << "', " << config.port << "); "
            << "s.login('" << config.username << "', '" << config.password << "'); "
            << "s.sendmail('" << config.username << "', '" << to << "', msg.as_string()); "
            << "s.quit(); "
            << "print('OK')"
            << "\" 2>&1";

        FILE *pipe = popen(cmd.str().c_str(), "r");
        if (!pipe)
        {
            LOG_ERROR << "SMTP: Failed to execute send command";
            if (*cb) (*cb)(false);
            return;
        }

        char buffer[256];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe))
        {
            result += buffer;
        }
        int status = pclose(pipe);

        if (status == 0 && result.find("OK") != std::string::npos)
        {
            LOG_INFO << "SMTP: Email sent to " << to;
            if (*cb) (*cb)(true);
        }
        else
        {
            LOG_ERROR << "SMTP: Failed to send to " << to << ": " << result;
            if (*cb) (*cb)(false);
        }
    }).detach();
}

// ============================================================
// Global email service factory
// ============================================================

IEmailService &getEmailService()
{
    static std::unique_ptr<IEmailService> instance;
    static std::once_flag initFlag;

    std::call_once(initFlag, []() {
        using common::config::ConfigManager;
        const char *smtpHost = ConfigManager::getEnv("OAUTH2_SMTP_HOST");
        const char *smtpUser = ConfigManager::getEnv("OAUTH2_SMTP_USER");
        const char *smtpPass = ConfigManager::getEnv("OAUTH2_SMTP_PASSWORD");

        if (smtpHost && smtpUser && smtpPass &&
            std::strlen(smtpHost) > 0 && std::strlen(smtpUser) > 0 && std::strlen(smtpPass) > 0)
        {
            SmtpEmailService::Config config;
            config.host = smtpHost;
            config.username = smtpUser;
            config.password = smtpPass;

            const char *smtpPort = ConfigManager::getEnv("OAUTH2_SMTP_PORT");
            if (smtpPort) config.port = std::atoi(smtpPort);

            const char *smtpFrom = ConfigManager::getEnv("OAUTH2_SMTP_FROM_NAME");
            if (smtpFrom) config.fromName = smtpFrom;

            const char *smtpSsl = ConfigManager::getEnv("OAUTH2_SMTP_SSL");
            if (smtpSsl && std::string(smtpSsl) == "false") config.useSsl = false;

            LOG_INFO << "Email service: SMTP (" << config.host << ":" << config.port << ")";
            instance = std::make_unique<SmtpEmailService>(config);
        }
        else
        {
            LOG_INFO << "Email service: Console (set OAUTH2_SMTP_* env vars to enable SMTP)";
            instance = std::make_unique<ConsoleEmailService>();
        }
    });

    return *instance;
}

}  // namespace oauth2
