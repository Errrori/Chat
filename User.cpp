#include "pch.h"
#include "User.h"
#include "manager/DatabaseManager.h"
#include "Utils.h"

namespace User
{
    bool AddUser(const std::string& name, const std::string& password, const std::string& uid)
    {
        auto db = DatabaseManager::GetDbClient();
        std::string hash_password = Utils::PasswordHashed(password);
        std::string sql = "INSERT INTO users (username, password, uid) VALUES (?, ?, ?)";

        bool success = false;
        std::promise<bool> pro;
        auto f = pro.get_future();

        db->execSqlAsync(
            sql,
            [&pro](const drogon::orm::Result& result) {
                LOG_INFO << "User added successfully";
                pro.set_value(true);
            },
            [&pro](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "Failed to add user: " << e.base().what();
                pro.set_value(false);
            },
            name, hash_password, uid);

        try {
            success = f.get();
        }
        catch (...) {
            LOG_ERROR << "Exception occurred when adding user";
            success = false;
        }

        return success;
    }

    bool DeleteUser(const std::string& uid)
    {
        auto db = DatabaseManager::GetDbClient();

        std::string verifySql = "SELECT id FROM users WHERE uid = ?";

        bool success = false;
        std::promise<bool> pro;
        auto f = pro.get_future();

        db->execSqlAsync(
            verifySql,
            [&pro, db, uid](const drogon::orm::Result& result) {
                if (result.size() == 0) {
                    LOG_INFO << "Invalid username or password";
                    pro.set_value(false);
                    return;
                }


                std::string deleteSql = "DELETE FROM users WHERE uid = ?";
                db->execSqlAsync(
                    deleteSql,
                    [&pro](const drogon::orm::Result& result) {
                        LOG_INFO << "User deleted successfully";
                        pro.set_value(true);
                    },
                    [&pro](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "Failed to delete user: " << e.base().what();
                        pro.set_value(false);
                    },
                    uid);
            },
            [&pro](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "Failed to verify user: " << e.base().what();
                pro.set_value(false);
            },
            uid);

        try {
            success = f.get();
        }
        catch (...) {
            LOG_ERROR << "Exception occurred when deleting user";
            success = false;
        }

        return success;
    }

    bool FindUserByName(const std::string& username)
    {
        auto db = DatabaseManager::GetDbClient();

        std::string sql = "SELECT username FROM users WHERE username = ?";

        bool success = false;
        std::promise<bool> pro;
        auto f = pro.get_future();

        db->execSqlAsync(
            sql,
            [&pro](const drogon::orm::Result& result) {
                if (result.size() == 0) {
                    LOG_INFO << "User not found";
                    pro.set_value(false);
                    return;
                }

                LOG_INFO << "Found user: " << result[0]["username"].as<std::string>();
                pro.set_value(true);
            },
            [&pro](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "Failed to find user: " << e.base().what();
                pro.set_value(false);
            },
            username);

        try {
            success = f.get();
        }
        catch (...) {
            LOG_ERROR << "Exception occurred when finding user";
            success = false;
        }

        return success;
    }

    bool FindUserByUid(const std::string& uid)
    {
        auto db = DatabaseManager::GetDbClient();

        std::string sql = "SELECT username FROM users WHERE uid = ?";

        bool success = false;
        std::promise<bool> pro;
        auto f = pro.get_future();

        db->execSqlAsync(
            sql,
            [&pro](const drogon::orm::Result& result) {
                if (result.size() == 0) {
                    LOG_INFO << "User not found";
                    pro.set_value(false);
                    return;
                }

                LOG_INFO << "Found user: " << result[0]["username"].as<std::string>();
                pro.set_value(true);
            },
            [&pro](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "Failed to find user: " << e.base().what();
                pro.set_value(false);
            },
            uid);

        try {
            success = f.get();
        }
        catch (...) {
            LOG_ERROR << "Exception occurred when finding user";
            success = false;
        }

        return success;
    }

    bool ModifyUserName(const std::string& uid, const std::string& name)
    {
        auto db = DatabaseManager::GetDbClient();

        std::string sql = "UPDATE users SET username = ? WHERE uid = ?";

        bool success = false;
        std::promise<bool> pro;
        auto f = pro.get_future();

        db->execSqlAsync(
            sql,
            [&pro](const drogon::orm::Result& result) {
                if (result.affectedRows() == 0) {
                    // δ�ҵ��û�������δ��
                    LOG_INFO << "User not found or name unchanged";
                    pro.set_value(false);
                    return;
                }

                LOG_INFO << "Username modified successfully";
                pro.set_value(true);
            },
            [&pro](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "Failed to modify username: " << e.base().what();
                pro.set_value(false);
            },
            name, uid);

        try {
            success = f.get();
        }
        catch (...) {
            LOG_ERROR << "Exception occurred when modifying username";
            success = false;
        }

        return success;
    }

    bool ModifyUserPassword(const std::string& uid, const std::string& password)
    {
        auto db = DatabaseManager::GetDbClient();


        std::string sql = "UPDATE users SET password = ? WHERE uid = ?";

        bool success = false;
        std::promise<bool> pro;
        auto f = pro.get_future();

        db->execSqlAsync(
            sql,
            [&pro](const drogon::orm::Result& result) {
                if (result.affectedRows() == 0) {
                    LOG_INFO << "User not found or password unchanged";
                    pro.set_value(false);
                    return;
                }

                LOG_INFO << "Password modified successfully";
                pro.set_value(true);
            },
            [&pro](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "Failed to modify password: " << e.base().what();
                pro.set_value(false);
            },
            password, uid);

        try {
            success = f.get();
        }
        catch (...) {
            LOG_ERROR << "Exception occurred when modifying password";
            success = false;
        }

        return success;
    }
    bool GetUserInfoByUid(const std::string& uid, Json::Value& userInfo)
    {
        auto db = DatabaseManager::GetDbClient();

        std::string sql = "SELECT id, username, password, uid, create_time FROM users WHERE uid = ?";

        bool success = false;
        std::promise<bool> pro;
        auto f = pro.get_future();

        db->execSqlAsync(
            sql,
            [&pro, &userInfo](const drogon::orm::Result& result) {
                if (result.size() == 0) {
                    LOG_INFO << "can not find user info";
                    pro.set_value(false);
                    return;
                }

                const auto& row = result[0];
                userInfo["id"] = row["id"].as<int>();
                userInfo["username"] = row["username"].as<std::string>();
                userInfo["uid"] = row["uid"].as<std::string>();
                userInfo["password"] = row["password"].as<std::string>();
                userInfo["create_time"] = row["create_time"].as<std::string>();

                pro.set_value(true);
            },
            [&pro](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "fail to get info��" << e.base().what();
                pro.set_value(false);
            },
            uid);

        try {
            success = f.get();
        }
        catch (...) {
            LOG_ERROR << "exception to find user info";
            success = false;
        }

        return success;
    }

    bool GetUserInfoByName(const std::string& name, Json::Value& userInfo)
    {
        auto db = DatabaseManager::GetDbClient();

        std::string sql = "SELECT id, username, password, uid, create_time FROM users WHERE username = ?";

        bool success = false;
        std::promise<bool> pro;
        auto f = pro.get_future();

        db->execSqlAsync(
            sql,
            [&pro, &userInfo](const drogon::orm::Result& result) {
                if (result.size() == 0) {
                    LOG_INFO << "can not find user info";
                    pro.set_value(false);
                    return;
                }

                const auto& row = result[0];
                userInfo["id"] = row["id"].as<int>();
                userInfo["username"] = row["username"].as<std::string>();
                userInfo["uid"] = row["uid"].as<std::string>();
                userInfo["password"] = row["password"].as<std::string>();
                userInfo["create_time"] = row["create_time"].as<std::string>();

                pro.set_value(true);
            },
            [&pro](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "fail to get info��" << e.base().what();
                pro.set_value(false);
            },
            name);

        try {
            success = f.get();
        }
        catch (...) {
            LOG_ERROR << "exception to find user info";
            success = false;
        }

        return success;
    }

}


