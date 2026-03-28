#pragma once
#include <optional>
namespace drogon_model::postgres
{
	class Users;
}

class UserInfo;

class UserInfoBuilder
{
public:
    static UserInfo BuildSignInInfo(const std::string& account,
        const std::string& hashed_password);

	static UserInfo BuildSignInInfo(const Json::Value& json_data);

    static UserInfo BuildSignInInfo(std::string&& account,
        std::string&& hashed_password);

	static UserInfo BuildSignUpInfo(const std::string& username,
        const std::string& account, const std::string& hashed_password) ;

    static UserInfo BuildSignUpInfo(const Json::Value& json_data);

    static UserInfo BuildSignUpInfo(std::string&& username,
        std::string&& account, std::string&& hashed_password);

	static UserInfo BuildProfile(const std::string& username,const std::string& avatar,
        const std::string& account, const std::string& uid, const std::string& signature);

    static UserInfo BuildProfile(const Json::Value& json_data);

    static UserInfo BuildProfile(std::string&& username, std::string&& avatar,
        std::string&& account, std::string&& uid, std::string&& signature);

	static UserInfo BuildCached(const std::string& username,
        const std::string& avatar);

    static UserInfo BuildCached(const Json::Value& json_data);

    static UserInfo BuildCached(std::string&& username,
		std::string&& avatar);

    static UserInfo BuildUpdatedInfo(std::optional<std::string> username,
        std::optional<std::string> avatar, std::optional<std::string> signature);

	static UserInfo BuildUpdatedInfo(const Json::Value& json_data);

    //���Ǹ�����ຯ�������Ե��ã����أ���
	//����ʹ���ƶ�����Ľӿ�
};

class UserInfo
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
    UserInfo() = default;

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