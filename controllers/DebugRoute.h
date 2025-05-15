#pragma once
#include <drogon/drogon.h>
#include <drogon/HttpController.h>


class DbInfoController : public drogon::HttpController<DbInfoController>
{
    public:
        METHOD_LIST_BEGIN
			//database
            ADD_METHOD_TO(DbInfoController::GetDbInfo, "/debug/db_info", drogon::Get,"CorsMiddleware");
			ADD_METHOD_TO(DbInfoController::GetOnlineUsers, "/debug/online_users", drogon::Get, "CorsMiddleware");
			//ÔÝÊ±ÒÆ³ý
			//ADD_METHOD_TO(DbInfoController::ImportUsers, "/debug/import", drogon::Post, "CorsMiddleware");
			//user
            ADD_METHOD_TO(DbInfoController::GetUserById, "/user/get_user", drogon::Get, "CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::ModifyName, "/user/modify/username", drogon::Post, "CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::ModifyPassword, "/user/modify/password", drogon::Post, "CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::ModifyUserAvatar, "/user/modify/avatar", drogon::Post, "CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::DeleteUser, "/user/cancel", drogon::Post, "CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::GetAllRecords, "/user/get_all_records", drogon::Get,"CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::GetChatRecords, "/user/get_records", drogon::Get, "CorsMiddleware");

        METHOD_LIST_END

        void GetDbInfo(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
        void HandleDbInfoOptions(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
        void ModifyName(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
        void ModifyPassword(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
        void DeleteUser(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
        void GetUserById(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
        //void ImportUsers(const drogon::HttpRequestPtr& req,
        //        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
		void GetOnlineUsers(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
		void GetChatRecords(const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback);
        void GetAllRecords(const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback);
        void ModifyUserAvatar(const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};


