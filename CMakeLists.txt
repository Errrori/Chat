cmake_minimum_required(VERSION 3.5)
project(Test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME} 
    main.cpp
    Authentication.cpp
    DatabaseManager.cpp
    # DbInfoController.cpp  - 已删除
    DebugRoute.cpp
    Users.cpp
    Utils.cpp
    CorsMiddleware.cpp
)

# 查找 Drogon 包
find_package(Drogon REQUIRED)
# 链接 Drogon 库
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)