# 使用多阶段构建
FROM docker.1ms.run/library/ubuntu:20.04 AS builder

# 设置时区避免交互式提示
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# 安装必要的构建工具和依赖
RUN apt-get update && apt-get install -y \
    git \
    software-properties-common \
    cmake \
    libjsoncpp-dev \
    uuid-dev \
    openssl \
    libssl-dev \
    zlib1g-dev \
    sqlite3 \
    libsqlite3-dev \
    libcurl4-openssl-dev \
    && add-apt-repository -y ppa:ubuntu-toolchain-r/test \
    && apt-get update && apt-get install -y gcc-11 g++-11 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100 \
    && rm -rf /var/lib/apt/lists/*

# 安装Drogon框架
RUN git clone https://github.com/drogonframework/drogon \
    && cd drogon \
    && git submodule update --init \
    && mkdir build \
    && cd build \
    && cmake .. -DCMAKE_CXX_STANDARD=20 \
    && make -j $(nproc) \
    && make install

RUN git clone --depth=1 https://github.com/Thalhammer/jwt-cpp.git third_party/jwt-cpp \
    && mkdir -p third_party/jwt-cpp/build \
    && cd third_party/jwt-cpp/build \
    && cmake .. -DCMAKE_CXX_STANDARD=20 \
    && make -j $(nproc) \
    && make install

# 创建工作目录
WORKDIR /app

# 复制源代码
COPY . .

COPY config.json /app/
# 确保所有头文件都在正确的位置
RUN mkdir -p manager controllers middleware models



# 构建项目
RUN mkdir -p build \
    && cd build \
    && cmake .. -DCMAKE_CXX_STANDARD=20 -DCMAKE_CXX_FLAGS="-fcoroutines" \
    && make -j $(nproc)

# 使用更小的镜像作为最终镜像
FROM docker.1ms.run/library/ubuntu:20.04

# 设置时区
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# 安装运行时依赖
RUN apt-get update && apt-get install -y \
    libjsoncpp-dev \
    uuid-runtime \
    libssl-dev \
    zlib1g \
    sqlite3 \
    libsqlite3-dev \
    libcurl4 \
    && rm -rf /var/lib/apt/lists/*

# 创建工作目录
WORKDIR /app

# 从构建阶段复制编译好的可执行文件和必要的配置文件
COPY --from=builder /app/build/Test /app/
COPY --from=builder /app/config.json /app/
COPY --from=builder /app/static /app/static

# 创建必要的目录
RUN mkdir -p /app/uploads

# 暴露应用程序端口（根据您的配置调整）
EXPOSE 10086

# 运行应用程序
CMD ["./Test"]