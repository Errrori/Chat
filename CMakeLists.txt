cmake_minimum_required(VERSION 3.5)
project(Test CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加当前目录及子目录到包含路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/auth
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/filter
    ${CMAKE_CURRENT_SOURCE_DIR}/middleware
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/Common
    ${CMAKE_CURRENT_SOURCE_DIR}/Service
    ${CMAKE_CURRENT_SOURCE_DIR}/Data
    ${CMAKE_CURRENT_SOURCE_DIR}/static
)

# 添加预编译头文件处理
add_library(pch INTERFACE)
target_include_directories(pch INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# 收集所有源文件
set(SOURCES
    main.cpp
    HttpClient.cpp
    utils.cpp
    pch.cpp
    Container.cpp

    # Auth module
    auth/SecretProvider.cpp
    auth/TokenService.cpp
    auth/TokenFactory.cpp
    auth/PasswordService.cpp
    auth/AccountValidator.cpp
    
    # Controllers
    controllers/Authentication.cpp
    controllers/RelationshipController.cpp
    controllers/ThreadController.cpp
    controllers/ChatController.cpp
    controllers/FileController.cpp
    controllers/UserController.cpp
    
    # Filters
    filter/TokenVerifyFilter.cpp
    middleware/CORSMiddleware.cpp
    
    # Common layer
    Common/User.cpp
    Common/ChatMessage.cpp
    Common/ChatThread.cpp
    Common/AIMessage.cpp
    Common/AIRequestMsg.cpp
    Common/Notice.cpp
    
    # Service layer
    Service/UserService.cpp
    Service/MessageService.cpp
    Service/ThreadService.cpp
    Service/ConnectionService.cpp
    Service/FileService.cpp
    Service/RelationshipService.cpp
    Service/RedisService.cpp
    
    # Data layer
    Data/SQLiteUserRepository.cpp
    Data/SQLiteMessageRepository.cpp
    Data/SQLiteThreadRepository.cpp
    Data/SQLiteRelationshipRepository.cpp
    
    # Models
    models/Users.cc
    models/GroupChats.cc
    models/GroupMembers.cc
    models/Messages.cc    
    models/Threads.cc  
    models/PrivateChats.cc
    models/AiChats.cc
    models/AiContext.cc
    models/FriendRequests.cc
    models/Notifications.cc
    models/Friendships.cc
    models/RelationshipEvent.cc
    models/Block.cc
)

add_executable(${PROJECT_NAME} ${SOURCES})

# 查找 Drogon 包
find_package(Drogon REQUIRED)

# 查找 CURL 包
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# 查找 UUID 包

# 添加jwt-cpp的包含路径
include_directories(third_party/jwt-cpp/include)

# 使用 C++20，协程无需额外编译选项

# 链接 Drogon 库和 CURL 库
# 查找 hiredis（Redis 客户端库）
find_package(hiredis QUIET)
if(hiredis_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon CURL::libcurl OpenSSL::SSL OpenSSL::Crypto uuid pch hiredis::hiredis)
else()
    # 回退到 pkg-config 或手动链接
    find_library(HIREDIS_LIB hiredis)
    target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon CURL::libcurl OpenSSL::SSL OpenSSL::Crypto uuid pch ${HIREDIS_LIB})
endif()