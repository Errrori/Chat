#pragma once

#include "Common/User.h"
#include <drogon/HttpController.h>

/// Result returned by IUserRepository::AddUserCoro.
enum class AddUserStatus : int8_t
{
    Inserted      =  0,  ///< Row was newly inserted.
    AlreadyExists =  1,  ///< account UNIQUE constraint — no row written.
    Error         = -1,  ///< Unexpected DB error.
};

class IUserRepository
{
public:
	virtual ~IUserRepository() = default;

	// Returns hashed password for the given account (login use only)
	virtual drogon::Task<std::string> GetHashedPassword(const std::string& account) const = 0;

	// Returns auth/login related identity by account.
	virtual drogon::Task<UserInfo> GetAuthUserByAccount(const std::string& account) const = 0;

	// Returns the hot-path display projection used by chat/notice flows.
	virtual drogon::Task<UserInfo> GetDisplayProfileByUid(const std::string& uid) const = 0;

	// Returns the full externally visible profile by uid.
	virtual drogon::Task<UserInfo> GetUserProfileByUid(const std::string& uid) const = 0;

	// Returns the search card projection used by account lookup.
	virtual drogon::Task<UserInfo> FindUserByAccount(const std::string& account) const = 0;

	// Persists a new user (requires hashed_password set on info).
	// Uses INSERT ... ON CONFLICT (account) DO NOTHING — safe for concurrent calls.
	virtual drogon::Task<AddUserStatus> AddUserCoro(const UserInfo& info) = 0;

	// Updates mutable user profile fields by uid.
	virtual drogon::Task<bool> UpdateUserProfile(const std::string& uid, const UserInfo& update_info) = 0;
};

