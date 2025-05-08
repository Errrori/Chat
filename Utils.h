#pragma once
#ifdef _WIN32
#pragma comment(lib, "Rpcrt4.lib") 
#endif
#include <string>

static constexpr unsigned EffectiveTime = 30 * 24 * 60 * 60;
static constexpr unsigned SecretLength = 32;
static const std::string SecretFilePath = "jwt_secret.json";

class Utils
{
public:
	struct UserInfo
	{
		std::string username;
		std::string uid;
	};

public:
	static std::string GenerateUid();
	static std::string GenerateSecret(size_t len = SecretLength);
	static std::string GetJwtSecret();
	static std::string PasswordHashed(const std::string& password);
	static std::string LoadJwtSecret(const std::string& file_path = SecretFilePath);
	static std::string GenJWT(UserInfo info);
	static bool VerifyJWT(const std::string& token, UserInfo& info);
};

