# Overview of this docker file
# This is a emscripten docker file that is used to compile FastLED for the web assembly platform.
# The goals of this docker are:
#   * Compile the inputs as quickly as possible.
#   * Be very fast to rebuild
# Container size is important, but not the primary goal.
#
# The emscripten build is based on emscripten. We create a custom `wasm_compiler_flags.py``
# that performs the necessary transformations to make this work.
#
# Maximizing compiler speed
# First let's talk about why platformio is typically slow
#   * Network checking to see if the repo's are up to date
#   * Unnecessary recompilation of cpp files that haven't changed.
#
# We address these slow compiler speeds via a few tricks
#   * The emscripten compiler is wrapped in ccache so that identical compilations are cached.
#   * We prewarm the ccache with a pre-compile of an example
#     * This will generate the initial cache and also install the necessary tools.
#
# Pre-warming the cache
#   * We have two pre-warm cycles. We do this for speed reasons.
#     * The first pre-warm is done with a copy of fastled downloaded from the github repo
#       * Docker is very relaxed on invalidating a cache layer for statements cloning a github repo. We want this.
#       * So we clone the repo early in the docker image build cycle. Then we run a pre-warm compile.
#         * This is the slowest pre-warm, and once built, tends to always be cached. However, it tends to go out of date
#           quickly, which will make the cache be less effective.
#   * The second pre-warm is done with a fresh copy of the fastled repo from the host machine that is copied over the
#     git hub repo. We copy directories piece by piece to maximize the cache hit rate. Core files then platform files.
#     * During developement when many files are changed, this second pre-warm will almost always be invalidated, however
#       it is very fast to rebuild because the initial tool download of platformio has already been done with the first pre-warm.
#     * After this second pre-warm, the ccache will have the exact state of the repo in it's cache.
#
# Final state
#  * The final state of the docker image is a pre-warmed cached with the entire instance ready to compile.
#  * The calling code is expected to provide volume mapped directory that will be inserted into the docker image when it runs.
#  * However, this is not true when using this in server mode (see server.py) where the code to be compiled is injected into the
#    container instance via a FastAPI route, the compilation is performed. Typically the web compiler is much better at holding
#    on the cached files. Server.py will also periodically git pull the fastled repo to keep in sync with the latest changes.


# This will be set to arm64 to support MacOS M1+ devices (and Linux-based arm64 devices)
ARG PLATFORM_TAG=""

# Use only Emscripten base image
FROM emscripten/emsdk:3.1.70${PLATFORM_TAG}

ENV DEBIAN_FRONTEND=noninteractive

# Update the apt-get package list. This takes a long time, so we do it first to maximize cache hits.
# Also install apt-fast first
RUN apt-get update && \
    apt-get install -y software-properties-common && \
    add-apt-repository ppa:apt-fast/stable -y && \
    apt-get update && \
    apt-get -y install apt-fast

# Update apt and install required packages
RUN apt-fast install -y \
    git \
    gawk \
    nano \
    ca-certificates \
    python3 \
    python3-pip \
    dos2unix \
    tar \
    wget \
    unzip \
    make \
    cmake \
    ninja-build \
    ccache \
    zlib1g \
    zlib1g-dev \
    gcc \
    g++ \
    rsync \
    && rm -rf /var/lib/apt/lists/*


# /container/bin contains symbolic links to python3 and pip3 as python and pip that we use for the compiler.
RUN mkdir -p /container/bin && \
    ln -s /usr/bin/python3 /container/bin/python && \
    ln -s /usr/bin/pip3 /container/bin/pip


# Add Python and Emscripten to PATH
ENV PATH="/container/bin:/usr/local/bin:/usr/bin:/emsdk:/emsdk/upstream/emscripten:${PATH}"
ENV CCACHE_DIR=/ccache
ENV CCACHE_MAXSIZE=1G

# Create a custom print script
RUN echo '#!/bin/sh' > /usr/bin/print && \
    echo 'echo "$@"' >> /usr/bin/print && \
    chmod +x /usr/bin/print

# Add print function (which seems to be missing, strangly) and PATH modifications to /etc/profile
RUN echo 'print() { echo "$@"; }' >> /etc/profile && \
    echo 'export -f print' >> /etc/profile && \
    echo 'export PATH="/container/bin:/usr/bin:/emsdk:/emsdk/upstream/emscripten:$PATH"' >> /etc/profile && \
    echo 'source /emsdk/emsdk_env.sh' >> /etc/profile

# This was added to try and fix formatting issues. It didn't fix it but it's better to force everything to use
# utf-8, as god intended it.
ENV LANG=en_US.UTF-8
ENV LC_CTYPE=UTF-8
RUN echo 'export LANG=en_US.UTF-8' >> /etc/profile && \
    echo 'export LC_CTYPE=UTF-8' >> /etc/profile

# This is disabled code to try and use the arduino-cli to transform an *.ino file into a cpp file
# to enable the auto-predeclaration of missing functions. However, Arduino-cli is absolutely gigantic
# and requires that we compile it against one of the platforms. This doesn't work for us because
# we don't want a compile failure because of something weird in the platform we are compiling against, which
# isn't wasm.
#COPY src/platforms/wasm/compiler/install-arduino-cli.sh /install-arduino-cli.sh
#RUN chmod +x /install-arduino-cli.sh && /install-arduino-cli.sh || echo "Failed to install Arduino CLI"





# Get the compiler requirements and install them.
COPY src/platforms/wasm/compiler/requirements.txt /install/requirements.txt
RUN pip install -r /install/requirements.txt

RUN pio settings set check_platformio_interval 9999
RUN pio settings set enable_telemetry 0

# FIRST PRE-WARM CYCLE and initial setup: Download the fastled repo from the github and pre-warm the cache with a compilation.
# This is by far the most expensive part of the build, because platformio needs to download initial tools. This
# pre-warm cycle is "sticky" and tends to stay in the cache for a long time since docker is very relaxed about
# invalidating cache layers that clone a github repo.
RUN git clone https://github.com/fastled/FastLED.git --depth 1 /git/fastled && \
    mkdir -p /js/fastled && rm -rf /git/fastled/.git && \
    rsync -a /git/fastled/ /js/fastled/ && rm -rf /git/fastled

# Set up the entrypoint script
RUN echo '#!/bin/bash' > /entrypoint.sh && \
    echo 'source /emsdk/emsdk_env.sh' >> /entrypoint.sh && \
    echo 'python /js/init_runtime.py' >> /entrypoint.sh && \
    echo 'exec "$@"' >> /entrypoint.sh && \
    chmod +x /entrypoint.sh
    
# Create symlinks for wasm platform files.
COPY src/platforms/wasm/compiler/init_runtime.py /js/init_runtime.py



WORKDIR /js

ARG NO_PREWARM=0
ENV NO_PREWARM=${NO_PREWARM}

RUN python /js/init_runtime.py

# First pre-warm cycle - always do it as part of the build.
RUN mkdir -p /logs
RUN /js/prewarm.sh --force > /logs/prewarm.log.1

COPY *.json /js/fastled/


COPY src/*.* /js/fastled/src/
COPY src/fx /js/fastled/src/fx
COPY src/fl /js/fastled/src/fl
COPY src/lib8tion /js/fastled/src/lib8tion
COPY src/third_party /js/fastled/src/third_party
COPY src/platforms /js/fastled/src/platforms


# SECOND PRE-WARM CYCLE: Copy the fastled repo from the host machine and pre-warm the cache with that compilation. This will
# be much quicker than the first pre-warm cycle.
RUN /js/prewarm.sh  > /logs/prewarm.log.2


ENTRYPOINT ["/entrypoint.sh"]
CMD ["python", "/js/run.py", "server"]
