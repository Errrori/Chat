cmake_minimum_required(VERSION 3.5)
project(Test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME} 
    main.cpp
    Auth/Authentication.cpp
    DatabaseManager.cpp
    Auth/DebugRoute.cpp
    Users.cpp
    Utils.cpp
    Auth/CorsMiddleware.cpp
    Websocket.cpp
)

# 查找 Drogon 包
find_package(Drogon REQUIRED)
# 链接 Drogon 库
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)

# 直接查找系统中的jwt-cpp库
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(JWT_CPP QUIET jwt-cpp)
endif()

# 如果找不到jwt-cpp，则使用头文件包含方式
if(NOT JWT_CPP_FOUND)
    # 只包含jwt-cpp头文件，因为它是header-only库
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

# jwt-cpp是header-only库，不需要链接，只需包含头文件
# 删除以下两行，因为third_party/jwt-cpp目录不存在
# add_subdirectory(third_party/jwt-cpp)
# include_directories(third_party/jwt-cpp/include)

# 删除以下行，因为picojson路径不存在
# include_directories(third_party/jwt-cpp/include/picojson)

# 删除这一行，因为jwt-cpp是header-only库，不需要链接
# 且会导致target_link_libraries重复使用关键字语法
# target_link_libraries(${PROJECT_NAME} jwt-cpp::jwt-cpp)