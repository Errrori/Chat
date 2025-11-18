# 使用多阶段构建
ARG BASE_IMAGE="docker.1ms.run/library/ubuntu:24.04"
ARG RUNTIME_IMAGE="docker.1ms.run/library/ubuntu:24.04"
FROM ${BASE_IMAGE} AS builder

# 设置时区避免交互式提示
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# 安装必要的构建工具和依赖
ARG DEBIAN_FRONTEND=noninteractive
ARG APT_MIRROR=""
RUN set -eux; \
    if [ -n "$APT_MIRROR" ]; then \
      sed -i "s|http://archive.ubuntu.com/ubuntu|$APT_MIRROR|g" /etc/apt/sources.list; \
      sed -i "s|http://security.ubuntu.com/ubuntu|$APT_MIRROR|g" /etc/apt/sources.list; \
    fi; \
    apt-get update && apt-get install -y --no-install-recommends software-properties-common; \
    add-apt-repository -y universe; \
    apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates wget git cmake build-essential pkg-config \
    libjsoncpp-dev uuid-dev libssl-dev zlib1g-dev \
    sqlite3 libsqlite3-dev libcurl4-openssl-dev \
    libdrogon-dev libtrantor-dev libpq-dev libmariadb-dev \
    && rm -rf /var/lib/apt/lists/*

# 安装Drogon框架
# 使用系统包安装 drogon
ARG DROGON_VERSION="v1.9.0"

# 创建工作目录
WORKDIR /app

# 复制源代码
COPY . .

COPY config.json /app/
# 确保所有头文件都在正确的位置
RUN mkdir -p manager controllers middleware models

# 拉取 jwt-cpp 头文件到 /app/third_party 以匹配 CMakeLists
RUN set -eux; \
    mkdir -p /app/third_party; \
    cd /app/third_party; \
    for i in 1 2 3 4 5; do \
      wget -O jwt-cpp.tar.gz https://codeload.github.com/Thalhammer/jwt-cpp/tar.gz/refs/heads/master && break; \
      echo "retry jwt-cpp download $i"; sleep 5; \
    done; \
    tar -xzf jwt-cpp.tar.gz; \
    mv jwt-cpp-* jwt-cpp



# 构建项目
RUN mkdir -p build \
    && cd build \
    && cmake .. -DCMAKE_CXX_STANDARD=20 \
    && make -j $(nproc)

# 使用更小的镜像作为最终镜像
FROM ${RUNTIME_IMAGE}

# 设置时区
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# 安装运行时依赖
ARG DEBIAN_FRONTEND=noninteractive
ARG APT_MIRROR=""
RUN set -eux; \
    if [ -n "$APT_MIRROR" ]; then \
      sed -i "s|http://archive.ubuntu.com/ubuntu|$APT_MIRROR|g" /etc/apt/sources.list; \
      sed -i "s|http://security.ubuntu.com/ubuntu|$APT_MIRROR|g" /etc/apt/sources.list; \
    fi; \
    apt-get update && apt-get install -y --no-install-recommends \
    libdrogon1t64 \
    libpq5 libmariadb3 \
    libjsoncpp25 \
    uuid-runtime \
    libssl3 \
    zlib1g \
    sqlite3 \
    libsqlite3-0 \
    libcurl4 \
    libtrantor1 \
    && rm -rf /var/lib/apt/lists/*

# 创建工作目录
WORKDIR /app

# 从构建阶段复制编译好的可执行文件和必要的配置文件
# 如果使用系统包安装 drogon，以下复制不需要
COPY --from=builder /app/build/Test /app/
COPY --from=builder /app/config.json /app/
COPY --from=builder /app/static /app/static

# 创建必要的目录
RUN mkdir -p /app/uploads

# 暴露应用程序端口（根据您的配置调整）
EXPOSE 10086

# 运行应用程序
CMD ["./Test"]