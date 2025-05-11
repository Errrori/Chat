cmake_minimum_required(VERSION 3.5)
project(Test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME} 
    main.cpp
    Authentication.cpp
    DatabaseManager.cpp
    DebugRoute.cpp
    Users.cpp
    Utils.cpp
    CorsMiddleware.cpp
    chatRecord.cc
    ConnectionManager.cpp
    EchoController.cpp
    PublicChatController.cpp
)

# 查找 Drogon 包
find_package(Drogon REQUIRED)
# 链接 Drogon 库
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon jwt-cpp::jwt-cpp)


# 添加jwt-cpp库
add_subdirectory(third_party/jwt-cpp)
include_directories(third_party/jwt-cpp/include)
# 添加picojson的包含路径
include_directories(third_party/jwt-cpp/include/picojson)

# 在您的目标链接中添加jwt-cpp
target_link_libraries(${PROJECT_NAME} jwt-cpp::jwt-cpp)