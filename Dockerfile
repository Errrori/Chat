# 使用多阶段构建，新增base镜像统一处理源和时区（核心优化）
# 默认使用国内镜像，避免 Docker Hub 鉴权超时；需要时可通过 --build-arg BASE_IMAGE=... 覆盖
ARG BASE_IMAGE="swr.cn-north-4.myhuaweicloud.com/ddn-k8s/docker.io/library/ubuntu:24.04"
# 运行时镜像复用base，避免重复配置
FROM ${BASE_IMAGE} AS base

# 统一设置时区（仅在base执行一次）
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# 彻底替换为阿里云源（直接覆盖sources.list，比sed更可靠）
RUN echo "deb http://mirrors.aliyun.com/ubuntu/ noble main restricted universe multiverse" > /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ noble-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ noble-backports main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb http://mirrors.aliyun.com/ubuntu/ noble-security main restricted universe multiverse" >> /etc/apt/sources.list


# 构建阶段基于base，复用源和时区配置
FROM base AS builder

# 安装构建工具和依赖（仅保留必要包，清理缓存）
ARG DEBIAN_FRONTEND=noninteractive
RUN set -eux; \
    apt-get update && apt-get install -y --no-install-recommends \
    clang \
    software-properties-common \
    ca-certificates wget git cmake build-essential pkg-config \
    libjsoncpp-dev uuid-dev libssl-dev zlib1g-dev \
    sqlite3 libsqlite3-dev libcurl4-openssl-dev \
    libdrogon-dev libtrantor-dev libpq-dev libmariadb-dev libbrotli-dev libyaml-cpp-dev \
    libhiredis-dev \
    && add-apt-repository -y universe \
    && rm -rf /var/lib/apt/lists/*  # 清理缓存减小体积

# 安装Drogon框架（使用系统包，保持不变）
ARG DROGON_VERSION="v1.9.0"

# 创建工作目录
WORKDIR /app

# 复制源代码（保持不变）
COPY . .
COPY config.json /app/

# 确保头文件目录存在（保持不变）
RUN mkdir -p manager controllers middleware models

# 拉取jwt-cpp头文件（保留重试优化）
RUN set -eux; \
    mkdir -p /app/third_party; \
    cd /app/third_party; \
    for i in 1 2 3 4 5 6 7; do \
      wget -t 3 -T 10 -O jwt-cpp.tar.gz https://codeload.github.com/Thalhammer/jwt-cpp/tar.gz/refs/heads/master && break; \
      echo "retry jwt-cpp download (attempt $i)"; sleep 3; \
    done; \
    tar -xzf jwt-cpp.tar.gz; \
    mv jwt-cpp-* jwt-cpp

# Set compiler to Clang to work around GCC bug
ENV CC=clang
ENV CXX=clang++

# 构建项目（多线程编译，保持不变）
RUN mkdir -p build \
    && cd build \
    && cmake .. -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Debug \
    && make -j $(nproc)  # 利用所有CPU核心加速编译


# 最终运行时镜像基于base，复用源和时区配置
FROM base AS runtime

# 安装运行时依赖（仅保留必要库，清理缓存）
ARG DEBIAN_FRONTEND=noninteractive
RUN set -eux; \
    apt-get update && apt-get install -y --no-install-recommends \
    libdrogon1t64 \
    libpq5 libmariadb3 libbrotli1 libyaml-cpp0.8 \
    libjsoncpp25 \
    uuid-runtime \
    libssl3 \
    zlib1g \
    sqlite3 \
    libsqlite3-0 \
    libcurl4 \
    libtrantor1 \
    ca-certificates \
    && update-ca-certificates \
    && rm -rf /var/lib/apt/lists/*  # 清理缓存

# 显式指定证书路径，避免运行时库找不到系统 CA 文件
ENV SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt
ENV CURL_CA_BUNDLE=/etc/ssl/certs/ca-certificates.crt

# 创建工作目录
WORKDIR /app

# 从构建阶段复制产物（保持不变）
COPY --from=builder /app/build/Test /app/
COPY --from=builder /app/config.json /app/
COPY --from=builder /app/static /app/static
COPY --from=builder /usr/lib/x86_64-linux-gnu/libhiredis.so* /usr/lib/x86_64-linux-gnu/

# 创建必要目录
RUN mkdir -p /app/uploads

# 暴露端口
EXPOSE 10086

# 运行应用
CMD ["./Test"]