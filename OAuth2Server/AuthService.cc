#include "AuthService.h"
#include <oauth2/models/Users.h>
#include <oauth2/models/Roles.h>
#include <oauth2/models/UserRoles.h>
#include <oauth2/PasswordHasher.h>
#include <drogon/utils/Utilities.h>
#include <algorithm>

using namespace drogon;
using namespace drogon::orm;

namespace services
{

void AuthService::validateUser(
  const std::string &username,
  const std::string &password,
  std::function<void(std::optional<AuthResult>)> &&callback
)
{
    auto sharedCb =
      std::make_shared<std::function<void(std::optional<AuthResult>)>>(std::move(callback));
    try
    {
        auto mapper = Mapper<drogon_model::oauth_test::Users>(app().getDbClient());

        // Find user by username
        mapper.findOne(
          {drogon_model::oauth_test::Users::Cols::_username, CompareOperator::EQ, username},
          [sharedCb, password](const drogon_model::oauth_test::Users &user) {
              // Compute Hash using PasswordHasher (supports PBKDF2 + legacy SHA-256)
              std::string salt = user.getValueOfSalt();
              std::string dbHash = user.getValueOfPasswordHash();

              bool valid = oauth2::utils::PasswordHasher::verify(password, dbHash, salt);

              if (valid)
              {
                  // Check if password hash needs upgrade to PBKDF2
                  if (oauth2::utils::PasswordHasher::needsRehash(dbHash))
                  {
                      // Async upgrade: rehash with PBKDF2
                      try
                      {
                          std::string newHash = oauth2::utils::PasswordHasher::hash(password);
                          auto db = app().getDbClient();
                          int userId = user.getValueOfId();
                          db->execSqlAsync(
                            "UPDATE users SET password_hash = $1, salt = '' WHERE id = $2",
                            [userId](const drogon::orm::Result &) {
                                LOG_INFO << "Upgraded password hash to PBKDF2 for user "
                                         << userId;
                            },
                            [userId](const drogon::orm::DrogonDbException &e) {
                                LOG_WARN << "Failed to upgrade password hash for user " << userId
                                         << ": " << e.base().what();
                            },
                            newHash,
                            userId
                          );
                      }
                      catch (const std::exception &e)
                      {
                          LOG_WARN << "Password rehash failed: " << e.what();
                      }
                  }

                  AuthResult result;
                  result.internalId = user.getValueOfId();
                  result.publicSub = user.getValueOfPublicSub();
                  (*sharedCb)(result);
              }
              else
              {
                  (*sharedCb)(std::nullopt);
              }
          },
          [sharedCb](const DrogonDbException &e) {
              LOG_WARN << "Validate User Failed: " << e.base().what();
              (*sharedCb)(std::nullopt);
          }
        );
    }
    catch (const DrogonDbException &e)
    {
        LOG_WARN << "Validate User Init Failed: " << e.base().what();
        (*sharedCb)(std::nullopt);
    }
}

void AuthService::registerUser(
  const std::string &username,
  const std::string &password,
  const std::string &email,
  std::function<void(const std::string &error)> &&callback
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const std::string &error)>>(std::move(callback));
    // Hash Password with Argon2id
    std::string salt = "";  // Argon2id embeds its own salt
    std::string passwordHash;
    try
    {
        passwordHash = oauth2::utils::PasswordHasher::hash(password);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Password hashing failed: " << e.what();
        (*sharedCb)("Internal Server Error");
        return;
    }

    drogon_model::oauth_test::Users newUser;
    newUser.setUsername(username);
    newUser.setPasswordHash(passwordHash);
    newUser.setSalt(salt);
    if (!email.empty())
        newUser.setEmail(email);

    try
    {
        auto db = app().getDbClient();
        // Start Transaction? For now, just chain.

        auto mapper = Mapper<drogon_model::oauth_test::Users>(db);

        // Async Insert
        mapper.insert(
          newUser,
          [sharedCb, db](const drogon_model::oauth_test::Users &u) {
              // Assign Default Role "user"
              try
              {
                  auto roleMapper = Mapper<drogon_model::oauth_test::Roles>(db);
                  roleMapper.findOne(
                    Criteria(
                      drogon_model::oauth_test::Roles::Cols::_name, CompareOperator::EQ, "user"
                    ),
                    [sharedCb,
                     db,
                     userId = u.getValueOfId()](const drogon_model::oauth_test::Roles &role) {
                        try
                        {
                            auto urMapper = Mapper<drogon_model::oauth_test::UserRoles>(db);
                            drogon_model::oauth_test::UserRoles ur;
                            ur.setUserId(userId);
                            ur.setRoleId(role.getValueOfId());

                            urMapper.insert(
                              ur,
                              [sharedCb](const drogon_model::oauth_test::UserRoles &) {
                                  (*sharedCb)("");  // Success
                              },
                              [sharedCb](const DrogonDbException &e) {
                                  LOG_ERROR << "Assign Role Failed: " << e.base().what();
                                  (*sharedCb)("");  // Treat as success
                                                    // for now (User
                                                    // created), but log
                                                    // error
                              }
                            );
                        }
                        catch (...)
                        {
                            (*sharedCb)("");
                        }
                    },
                    [sharedCb](const DrogonDbException &e) {
                        LOG_ERROR << "Default Role 'user' not found: " << e.base().what();
                        (*sharedCb)("");  // User created w/o role
                    }
                  );
              }
              catch (...)
              {
                  (*sharedCb)("");
              }
          },
          [sharedCb](const DrogonDbException &e) {
              LOG_ERROR << "Register Failed: " << e.base().what();
              // Usually duplicate username
              (*sharedCb)("Registration Failed (Username likely exists)");
          }
        );
    }
    catch (const DrogonDbException &e)
    {
        LOG_ERROR << "Register Init Failed: " << e.base().what();
        (*sharedCb)("Internal Server Error");
    }
}

void AuthService::getUserInfo(
  int userId,
  std::function<void(std::optional<Json::Value> userInfo)> &&callback
)
{
    auto sharedCb = std::make_shared<std::function<void(std::optional<Json::Value> userInfo)>>(
      std::move(callback)
    );

    try
    {
        auto db = app().getDbClient();
        auto userMapper = Mapper<drogon_model::oauth_test::Users>(db);

        userMapper.findByPrimaryKey(
          userId,
          [sharedCb, db, userId](const drogon_model::oauth_test::Users &user) {
              // Fetch roles
              db->execSqlAsync(
                "SELECT r.name FROM roles r JOIN user_roles ur ON r.id = "
                "ur.role_id WHERE ur.user_id = $1",
                [sharedCb, user, userId](const Result &r) {
                    Json::Value json;
                    json["sub"] = user.getValueOfPublicSub();
                    json["name"] = user.getValueOfUsername();
                    json["email"] = user.getValueOfEmail();

                    Json::Value roles(Json::arrayValue);
                    for (auto row : r)
                    {
                        roles.append(row["name"].as<std::string>());
                    }
                    json["roles"] = roles;

                    (*sharedCb)(json);
                },
                [sharedCb, user, userId](const DrogonDbException &e) {
                    // Error fetching roles, just return user info with
                    // empty roles
                    LOG_WARN << "Failed to fetch roles for user " << userId << ": "
                             << e.base().what();
                    Json::Value json;
                    json["sub"] = user.getValueOfPublicSub();
                    json["name"] = user.getValueOfUsername();
                    json["email"] = user.getValueOfEmail();
                    json["roles"] = Json::Value(Json::arrayValue);
                    (*sharedCb)(json);
                },
                userId
              );
          },
          [sharedCb](const DrogonDbException &e) {
              LOG_WARN << "Get User Info Failed: " << e.base().what();
              (*sharedCb)(std::nullopt);
          }
        );
    }
    catch (const DrogonDbException &e)
    {
        LOG_WARN << "Get User Info Init Failed: " << e.base().what();
        (*sharedCb)(std::nullopt);
    }
}

}  // namespace services
