#pragma once
namespace drogon_model::sqlite3
{
	class Users;
}

class UserInfo
{
private:
    std::string uid;
    std::string username;
    std::string account;
    std::string avatar;
    // Only kept in memory during registration/login; never serialised into cache
    std::string hashed_password;

public:
    static UserInfo FromJson(const Json::Value& json);
    // Only for registration: builds a Users ORM row (requires hashed_password)
    std::optional<drogon_model::sqlite3::Users> ToDbUsers() const;
    bool IsDbValid() const;

    const std::string& getUid()            const { return uid; }
    const std::string& getUsername()       const { return username; }
    const std::string& getAccount()        const { return account; }
    const std::string& getAvatar()         const { return avatar; }
    const std::string& getHashedPassword() const { return hashed_password; }

    void setUid(const std::string& v)            { uid = v; }
    void setUsername(const std::string& v)       { username = v; }
    void setAccount(const std::string& v)        { account = v; }
    void setAvatar(const std::string& v)         { avatar = v; }
    void setHashedPassword(const std::string& v) { hashed_password = v; }

    void setUid(std::string&& v)            { uid = std::move(v); }
    void setUsername(std::string&& v)       { username = std::move(v); }
    void setAccount(std::string&& v)        { account = std::move(v); }
    void setAvatar(std::string&& v)         { avatar = std::move(v); }
    void setHashedPassword(std::string&& v) { hashed_password = std::move(v); }

    std::string ToString() const;
    Json::Value ToJson() const;
    void Reset();
};

