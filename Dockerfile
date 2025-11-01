# File: Dockerfile (Native Build Environment - FINAL)
FROM ubuntu:22.04

# Maintainer label
LABEL maintainer="RIcardo Loya"

# Set up non-interactive mode and install base dependencies
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    gcc \
    build-essential \
    wget \
    tar \
    bison \
    flex \
    git \
    # Python tools for the CLI setup
    python3 \
    python3-pip \
    python3-venv \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory inside the container
WORKDIR /usr/src/simtemp_driver

# Final command to run: we rely on an entrypoint script to run 'build.sh'
CMD ["/bin/bash"]