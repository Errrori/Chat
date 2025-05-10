#pragma once
#include <drogon/drogon.h>
#include <drogon/HttpController.h>

class DbInfoController : public drogon::HttpController<DbInfoController>
{
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(DbInfoController::GetDbInfo, "/db_info", drogon::Get,"CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::ModifyName, "/modify_name", drogon::Post, "CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::ModifyPassword, "/modify_password", drogon::Post, "CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::DeleteUser, "/delete_user", drogon::Post, "CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::GetUserById, "/get_user", drogon::Get, "CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::ImportUsers, "/import", drogon::Post, "CorsMiddleware");
            ADD_METHOD_TO(DbInfoController::GetOnlineUsers, "/users_online", drogon::Get, "CorsMiddleware");
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
        void ImportUsers(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
		void GetOnlineUsers(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};


