# Dockerfile for building OAuth2Backend
# Matches Linux CI environment: Ubuntu 22.04 + Drogon v1.9.12

FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies (matches Linux CI)
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    gcc \
    g++ \
    libjsoncpp-dev \
    uuid-dev \
    libpq-dev \
    libssl-dev \
    zlib1g-dev \
    libhiredis-dev \
    postgresql-client \
    redis-tools \
    python3 \
    python3-pip \
    wget \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install Conan (matches Linux CI)
RUN wget -qO- https://dl.bintray.com/conan/installer.sh | sh
ENV PATH="/root/.conan:${PATH}"

# Install Drogon v1.9.12 (matches CI version)
ENV DROGON_VERSION=v1.9.12
RUN git clone --depth 1 --branch ${DROGON_VERSION} https://github.com/drogonframework/drogon \
    && cd drogon \
    && git submodule update --init --recursive \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_MYSQL=OFF \
    && make -j$(nproc) \
    && make install \
    && cd / \
    && rm -rf drogon

# Set working directory
WORKDIR /app

# Copy project files
COPY . /app/

# Note: Database and Redis are provided by docker-compose services

CMD ["/bin/bash"]
