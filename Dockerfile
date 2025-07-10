# Use a specific Ubuntu version for better long-term stability
FROM ubuntu:22.04

# Set environment variables to avoid interactive prompts during installation
ENV DEBIAN_FRONTEND=noninteractive
ENV ZEPHYR_SDK_VERSION="0.16.5"
ENV ZEPHYR_SDK_INSTALL_DIR="/opt/zephyr-sdk-${ZEPHYR_SDK_VERSION}"
ENV PATH="${ZEPHYR_SDK_INSTALL_DIR}/usr/bin:${PATH}"

# 1. Install required host packages from the workflow
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    python3-pip \
    clang-tidy \
    gcc-multilib \
    wget \
    xz-utils && \
    rm -rf /var/lib/apt/lists/*

# 2. Install west, the Zephyr meta-tool
RUN pip3 install west

# 3. Download and install the specified Zephyr SDK version
RUN wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz && \
    tar -xf zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz && \
    rm zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz && \
    mv zephyr-sdk-${ZEPHYR_SDK_VERSION} ${ZEPHYR_SDK_INSTALL_DIR} && \
    ${ZEPHYR_SDK_INSTALL_DIR}/setup.sh -t arm-zephyr-eabi

# Set the default command to a shell, ready for user commands
CMD ["/bin/bash"]