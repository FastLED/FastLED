FROM trzeci/emscripten:1.39.17-upstream AS emscripten_base
# A good practice is to don't use `latest` - even if some effort is made to make sure that `latest` version is stable

# Target base image
FROM fedora

# Copy pre-compiled content of Emscripten SDK to target image
COPY --from=emscripten_base /emsdk_portable /emsdk_portable

# Install required tools to run Emscripten SDK

RUN echo "max_parallel_downloads=10" >> /etc/dnf/dnf.conf

RUN dnf install -y git nano which ca-certificates python3 python3-pip dos2unix tar wget unzip make cmake ninja-build && \
    dnf clean all

#RUN dnf install -y git nano which ca-certificates python3 python3-pip dos2unix tar wget unzip make cmake ninja-build clang lld && \
#    dnf clean all



# Install PlatformIO
RUN pip3 install platformio==6.1.16

# Create /container/bin directory and add symbolic links
RUN mkdir -p /container/bin && \
    ln -s /usr/bin/python3 /container/bin/python && \
    ln -s /usr/bin/pip3 /container/bin/pip

# Add Python and Emscripten to PATH
ENV PATH="/container/bin:/usr/local/bin:/usr/bin:/emsdk_portable:/emsdk_portable/upstream/emscripten:${PATH}"

# Create a custom print script
RUN echo '#!/bin/sh' > /usr/bin/print && \
    echo 'echo "$@"' >> /usr/bin/print && \
    chmod +x /usr/bin/print

# Add print function and PATH modifications to /etc/profile
RUN echo 'print() { echo "$@"; }' >> /etc/profile && \
    echo 'export -f print' >> /etc/profile && \
    echo 'export PATH="/container/bin:/usr/bin:/emsdk_portable:/emsdk_portable/upstream/emscripten:$PATH"' >> /etc/profile && \
    echo 'source /emsdk_portable/emsdk_env.sh' >> /etc/profile

# wormhole-tx allows to send/recieve files to the host machine. The command `tx` becomes available.
RUN pip3 install wormhole-tx || true

# src\platforms\stub\wasm\compiler\install-arduino-cli.sh

COPY src/platforms/wasm/compiler/install-arduino-cli.sh /install-arduino-cli.sh
RUN chmod +x /install-arduino-cli.sh && /install-arduino-cli.sh || echo "Failed to install Arduino CLI"


# Set up the entrypoint script
RUN echo '#!/bin/bash' > /entrypoint.sh && \
    echo 'source /emsdk_portable/emsdk_env.sh' >> /entrypoint.sh && \
    echo 'exec "$@"' >> /entrypoint.sh && \
    chmod +x /entrypoint.sh

# In order to speed up the build, copy files that are not likely
# to be changed first.
COPY ./*.json /js/fastled/
COPY ./*.properties /js/fastled/
COPY ./src/*.* /js/fastled/src/
COPY ./src/fx/ /js/fastled/src/fx/
COPY ./src/lib8tion/ /js/fastled/src/lib8tion
COPY ./src/third_party/ /js/fastled/src/third_party

# Copy platforms directory separately, since it changes a lot during development
COPY ./src/platforms/ /js/fastled/src/platforms/


# Hack for now to get the wasm platform to compile
RUN cp /js/fastled/src/platforms/wasm/compiler/platformio.ini /js/platformio.ini && \
    cp /js/fastled/src/platforms/wasm/compiler/wasm_compiler_flags.py /js/wasm_compiler_flags.py && \
    cp /js/fastled/src/platforms/wasm/compiler/index.html /js/index.html && \
    cp /js/fastled/src/platforms/wasm/compiler/index.css /js/index.css && \
    cp /js/fastled/src/platforms/wasm/compiler/*.js /js/ && \
    cp /js/fastled/src/platforms/wasm/compiler/Arduino.h /js/Arduino.h && \
    cp /js/fastled/src/platforms/wasm/compiler/arduino-pre-process.sh /js/arduino-pre-process.sh && \
    cp /js/fastled/src/platforms/wasm/compiler/*.sh /js/

# Build with --build-arg DEBUG=1 to enable debug mode
ARG DEBUG=0
ENV DEBUG=${DEBUG}
# now set it in /etc/profile, if it exists
RUN if \
    [ -f /etc/profile ] && \
    [ "${DEBUG}" = "1" ]; then \
    echo 'export DEBUG=1' >> /etc/profile; \
    echo 'echo "Debug mode enabled"' >> /etc/profile; \
    fi


WORKDIR /js

# This is too slow for lots of testing, disabled for now
# Prewarm the cache with a first compile.
#RUN mkdir -p /js/src && echo -e "#include \"platforms/wasm/exports.h\" #include \"FastLed.h\"\nvoid setup() {}\nvoid loop(){}\n" > /js/src/prewarm_main.cpp
#RUN source /entrypoint.sh && pio run && rm -rf /js/src/prewarm_main.cpp

# Copy the compile script
COPY src/platforms/wasm/compiler/compile.py /js/
COPY src/platforms/wasm/compiler/process-ino.py /js/
RUN dos2unix /js/compile.py /js/process-ino.py
RUN dos2unix /js/*.sh
RUN chmod +x /js/*.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["python", "/js/compile.py"]
