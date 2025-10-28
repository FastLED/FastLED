# SimAVR Docker Image for FastLED Testing
# This image provides SimAVR for running AVR firmware simulations

FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install SimAVR and dependencies
RUN apt-get update && apt-get install -y \
    simavr \
    avr-libc \
    gcc-avr \
    gdb-avr \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Default command is to run simavr
ENTRYPOINT ["simavr"]
CMD ["--help"]
