#pragma once
namespace drogon_model::sqlite3
{
	class Users;
}

    constexpr int DefaultVal = -1;
class UserInfo
{
private:
    std::string username;
    std::string uid;
    std::string account;
    std::string hashed_password;
    std::string avatar;
    std::string email;
    int posts = DefaultVal;
    int followers = DefaultVal;
    int following = DefaultVal;
    int level = DefaultVal;
    int status = 0; // 0: normal, 1: banned
    int64_t create_time = -1;
    int64_t last_login_time = -1;
    std::string signature;

public:

	static UserInfo FromJson(const Json::Value& json);

	std::optional<drogon_model::sqlite3::Users> ToDbUsers() const;
    bool IsDbValid() const;

	const std::string& getUsername() const { return username; }
    const std::string& getUid() const { return uid; }
    const std::string& getAccount() const { return account; }
    const std::string& getHashedPassword() const { return hashed_password; }
    const std::string& getAvatar() const { return avatar; }
    const std::string& getEmail() const { return email; }
    int getPosts() const { return posts; }
    int getFollowers() const { return followers; }
    int getFollowing() const { return following; }
    int getLevel() const { return level; }
    int getStatus() const { return status; }
    int64_t getCreateTime() const { return create_time; }
    int64_t getLastLoginTime() const { return last_login_time; }
    const std::string& getSignature() const { return signature; }

	void setUsername(const std::string& value) { username = value; }
    void setUid(const std::string& value) { uid = value; }
    void setAccount(const std::string& value) { account = value; }
    void setHashedPassword(const std::string& value) { hashed_password = value; }
    void setAvatar(const std::string& value) { avatar = value; }
    void setEmail(const std::string& value) { email = value; }
    void setPosts(int value) { posts = value; }
    void setFollowers(int value) { followers = value; }
    void setFollowing(int value) { following = value; }
    void setLevel(int value) { level = value; }
    void setStatus(int value) { status = value; }
    void setCreateTime(int64_t value) { create_time = value; }
    void setLastLoginTime(int64_t value) { last_login_time = value; }
    void setSignature(const std::string& value) { signature = value; }

	void setUsername(std::string&& value) { username = std::move(value); }
    void setUid(std::string&& value) { uid = std::move(value); }
    void setAccount(std::string&& value) { account = std::move(value); }
    void setHashedPassword(std::string&& value) { hashed_password = std::move(value); }
    void setAvatar(std::string&& value) { avatar = std::move(value); }
    void setEmail(std::string&& value) { email = std::move(value); }
    void setSignature(std::string&& value) { signature = std::move(value); }

	std::string ToString() const;
    Json::Value ToJson() const;
    void Reset();
};

