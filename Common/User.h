#pragma once
#include <optional>
namespace drogon_model::postgres
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
    std::optional<drogon_model::postgres::Users> ToDbUsers() const;
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

class UserCachedInfo
{
public:
    static UserCachedInfo FromJson(const Json::Value& json);

    const std::string& getUsername() const { return username; }
    const std::string& getAvatar() const { return avatar; }

    void setUsername(const std::string& value) { username = value; }
    void setUsername(std::string&& value) { username = std::move(value); }
    void setAvatar(const std::string& value) { avatar = value; }
    void setAvatar(std::string&& value) { avatar = std::move(value); }

    bool IsEmpty() const;
    Json::Value ToJson() const;

private:
    std::string username;
    std::string avatar;
};

class UserSearchCard
{
public:
    const std::string& getUid() const { return uid; }
    const std::string& getUsername() const { return username; }
    const std::string& getAvatar() const { return avatar; }
    const std::string& getSignature() const { return signature; }

    void setUid(const std::string& value) { uid = value; }
    void setUid(std::string&& value) { uid = std::move(value); }
    void setUsername(const std::string& value) { username = value; }
    void setUsername(std::string&& value) { username = std::move(value); }
    void setAvatar(const std::string& value) { avatar = value; }
    void setAvatar(std::string&& value) { avatar = std::move(value); }
    void setSignature(const std::string& value) { signature = value; }
    void setSignature(std::string&& value) { signature = std::move(value); }

    bool IsEmpty() const { return uid.empty(); }
    Json::Value ToJson() const;

private:
    std::string uid;
    std::string username;
    std::string avatar;
    std::string signature;
};

class VisitorIdentity
{
	
};

class UserProfile
{
public:
    const std::string& getUid() const { return uid; }
    const std::string& getAccount() const { return account; }
    const std::string& getUsername() const { return username; }
    const std::string& getAvatar() const { return avatar; }
    const std::string& getSignature() const { return signature; }

    void setUid(const std::string& value) { uid = value; }
    void setUid(std::string&& value) { uid = std::move(value); }
    void setAccount(const std::string& value) { account = value; }
    void setAccount(std::string&& value) { account = std::move(value); }
    void setUsername(const std::string& value) { username = value; }
    void setUsername(std::string&& value) { username = std::move(value); }
    void setAvatar(const std::string& value) { avatar = value; }
    void setAvatar(std::string&& value) { avatar = std::move(value); }
    void setSignature(const std::string& value) { signature = value; }
    void setSignature(std::string&& value) { signature = std::move(value); }

    bool IsEmpty() const { return uid.empty(); }
    Json::Value ToJson() const;

private:
    std::string uid;
    std::string account;
    std::string username;
    std::string avatar;
    std::string signature;
};

class UserProfilePatch
{
public:
    static UserProfilePatch FromJson(const Json::Value& json);

    const std::optional<std::string>& getUsername() const { return username; }
    const std::optional<std::string>& getAvatar() const { return avatar; }
    const std::optional<std::string>& getSignature() const { return signature; }

    void setUsername(std::optional<std::string> value) { username = std::move(value); }
    void setAvatar(std::optional<std::string> value) { avatar = std::move(value); }
    void setSignature(std::optional<std::string> value) { signature = std::move(value); }

    bool HasUpdates() const;
    bool AffectsDisplayProfile() const;
    Json::Value ToJson() const;

private:
    std::optional<std::string> username;
    std::optional<std::string> avatar;
    std::optional<std::string> signature;
};

class UsersInfo;

class UserInfoBuilder
{
public:
    static UsersInfo BuildSignInInfo(const std::string& account,
        const std::string& hashed_password);

	static UsersInfo BuildSignInInfo(const Json::Value& json_data);

    static UsersInfo BuildSignInInfo(std::string&& account,
        std::string&& hashed_password);

	static UsersInfo BuildSignUpInfo(const std::string& username,
        const std::string& account, const std::string& hashed_password) ;

    static UsersInfo BuildSignUpInfo(const Json::Value& json_data);

    static UsersInfo BuildSignUpInfo(std::string&& username,
        std::string&& account, std::string&& hashed_password);

	static UsersInfo BuildProfile(const std::string& username,const std::string& avatar,
        const std::string& account, const std::string& uid, const std::string& signature);

    static UsersInfo BuildProfile(const Json::Value& json_data);

    static UsersInfo BuildProfile(std::string&& username, std::string&& avatar,
        std::string&& account, std::string&& uid, std::string&& signature);

	static UsersInfo BuildCached(const std::string& username,
        const std::string& avatar);

    static UsersInfo BuildCached(const Json::Value& json_data);

    static UsersInfo BuildCached(std::string&& username,
		std::string&& avatar);

    static UsersInfo BuildUpdatedInfo(std::optional<std::string> username,
        std::optional<std::string> avatar, std::optional<std::string> signature);

	static UsersInfo BuildUpdatedInfo(const Json::Value& json_data);

    //���Ǹ�����ຯ�������Ե��ã����أ���
	//����ʹ���ƶ�����Ľӿ�
};

class UsersInfo
{
    friend UserInfoBuilder;
public:
    enum Type :std::int8_t
    {
        Invalid,
        SignIn,
        SignUp,
        Profile,
        Cached,
        UpdateInfo
    };

public:
    UsersInfo() = default;

    bool IsValid() const;
    bool HasUpdates() const;
    bool AffectsDisplayProfile() const;
    Json::Value ToJson() const;

    //add get methods
	Type GetType() const { return type_; }
    const std::optional<std::string>& GetUid() const { return uid_; }
    const std::optional<std::string>& GetAccount() const { return account_; }
    const std::optional<std::string>& GetUsername() const { return username_; }
    const std::optional<std::string>& GetHashedPassword() const { return hashed_password_; }
    const std::optional<std::string>& GetAvatar() const { return avatar_; }
	const std::optional<std::string>& GetSignature() const { return signature_; }

private:
    void SetType(Type type) { type_ = type; }
    void SetUid(const std::string& uid) { uid_ = uid; }
    void SetAccount(const std::string& account) { account_ = account; }
	void SetUsername(const std::string& username) { username_ = username; }
	void SetHashedPassword(const std::string& hashed_password) { hashed_password_ = hashed_password; }
	void SetAvatar(const std::string& avatar) { avatar_ = avatar; }
	void SetSignature(const std::string& signature) { signature_ = signature; }
	void SetUid(std::string&& uid) { uid_ = std::move(uid); }
	void SetAccount(std::string&& account) { account_ = std::move(account); }
	void SetUsername(std::string&& username) { username_ = std::move(username); }
	void SetHashedPassword(std::string&& hashed_password) { hashed_password_ = std::move(hashed_password); }
	void SetAvatar(std::string&& avatar) { avatar_ = std::move(avatar); }
	void SetSignature(std::string&& signature) { signature_ = std::move(signature); }

private:
    Type type_ = Invalid;
    std::optional<std::string> uid_;
    std::optional<std::string> account_;
    std::optional<std::string> username_;
    std::optional<std::string> hashed_password_;
    std::optional<std::string> avatar_;
    std::optional<std::string> signature_;
};