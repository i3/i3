# Dev Container for i3wm development
# Includes Docker-in-Docker for building and running the i3 container

FROM mcr.microsoft.com/devcontainers/base:ubuntu-22.04

# Install development tools
RUN apt-get update && apt-get install -y \
    curl \
    wget \
    git \
    jq \
    vim \
    htop \
    tree \
    make \
    # Python for API development
    python3 \
    python3-pip \
    python3-venv \
    # For viewing screenshots
    imagemagick \
    && rm -rf /var/lib/apt/lists/*

# Install Python packages for development
RUN pip3 install --no-cache-dir \
    flask \
    flask-cors \
    requests \
    pillow \
    pytest \
    black \
    flake8 \
    httpie

# Set up workspace
WORKDIR /workspace

# Default shell
ENV SHELL=/bin/bash
