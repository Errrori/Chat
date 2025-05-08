#pragma once
#ifdef _WIN32
#pragma comment(lib, "Rpcrt4.lib") 
#endif
#include <string>

class Utils
{
public:
	static std::string GenerateUid();
	static std::string PasswordHashed(const std::string& password);
};

