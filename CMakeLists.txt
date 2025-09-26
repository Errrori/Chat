cmake_minimum_required(VERSION 3.5)
project(Test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加当前目录及子目录到包含路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/manager
    ${CMAKE_CURRENT_SOURCE_DIR}/middleware
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/static
)

# 添加预编译头文件处理
add_library(pch INTERFACE)
target_include_directories(pch INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${PROJECT_NAME} 
    main.cpp
    controllers/Authentication.cpp
    manager/DatabaseManager.cpp
    manager/MessageManager.cpp
    controllers/DebugRoute.cpp
    controllers/ThreadController.cpp
    utils.cpp
    middleware/CorsMiddleware.cpp
    models/Users.cc
    models/GroupChats.cc
    models/GroupMembers.cc
    models/Messages.cc    
    models/Threads.cc  
    models/PrivateChats.cc
    models/AiChats.cc
    manager/ConnectionManager.cpp
    manager/ThreadManager.cpp
    controllers/ChatController.cpp
    controllers/FileController.cpp
    MsgDispatcher.cpp
    manager/FileManager.cpp
    filter/TokenVerifyFilter.cpp
    controllers/AIController.cpp
    AIService.cpp
    pch.cpp
    Common/User.cpp
    Container.cpp
    Service/UserService.cpp
    Data/SQLiteUserRepository.cpp
)

# 查找 Drogon 包
find_package(Drogon REQUIRED)

# 查找 CURL 包
find_package(CURL REQUIRED)

# 添加jwt-cpp的包含路径
include_directories(third_party/jwt-cpp/include)

# 链接 Drogon 库和 CURL 库
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon CURL::libcurl pch)