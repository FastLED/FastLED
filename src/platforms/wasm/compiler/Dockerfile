ARG PLATFORM_TAG=""

FROM emscripten/emsdk:3.1.70${PLATFORM_TAG} AS emscripten_base

# Target base image
FROM fedora

# Copy pre-compiled content of Emscripten SDK to target image
COPY --from=emscripten_base /emsdk /emsdk

# Install required tools to run Emscripten SDK
RUN echo "max_parallel_downloads=10" >> /etc/dnf/dnf.conf

RUN dnf install -y git nano which ca-certificates python3 python3-pip dos2unix tar wget unzip make cmake ninja-build ccache && \
    dnf clean all

#RUN dnf install -y git nano which ca-certificates python3 python3-pip dos2unix tar wget unzip make cmake ninja-build clang lld && \
#    dnf clean all



# Install PlatformIO
RUN pip3 install platformio==6.1.16 wormhole-tx

# Create /container/bin directory and add symbolic links
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

# Add print function and PATH modifications to /etc/profile
RUN echo 'print() { echo "$@"; }' >> /etc/profile && \
    echo 'export -f print' >> /etc/profile && \
    echo 'export PATH="/container/bin:/usr/bin:/emsdk:/emsdk/upstream/emscripten:$PATH"' >> /etc/profile && \
    echo 'source /emsdk/emsdk_env.sh' >> /etc/profile

#COPY src/platforms/wasm/compiler/install-arduino-cli.sh /install-arduino-cli.sh
#RUN chmod +x /install-arduino-cli.sh && /install-arduino-cli.sh || echo "Failed to install Arduino CLI"
    

# Set up the entrypoint script
RUN echo '#!/bin/bash' > /entrypoint.sh && \
    echo 'source /emsdk/emsdk_env.sh' >> /entrypoint.sh && \
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

COPY examples /examples

# Create required directories
RUN mkdir -p /mapped /examples/wasm

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

WORKDIR /js

# This is too slow for lots of testing, disabled for now
# Prewarm the cache with a first compile.
#RUN mkdir -p /js/src && mkdir -p /mapped
# COPY /examples/Wasm /mapped/
RUN cp -r /examples/wasm /mapped/ || mkdir -p /mapped/wasm
# RUN source /entrypoint.sh && pio run

# Copy the compile script
COPY src/platforms/wasm/compiler/compile.py /js/
COPY src/platforms/wasm/compiler/process-ino.py /js/
RUN dos2unix /js/compile.py /js/process-ino.py
RUN dos2unix /js/*.sh
RUN chmod +x /js/*.sh

RUN python /js/compile.py || echo "####### ERROR FAILED to run pre warm compiler toolchain. Compile performance will degrade with this docker image."

#Clean up before the volume get's mapped in.
RUN rm -rf /mapped
RUN rm -rf /js/.pio/build/wasm/src/wasm.ino.o || echo "failed to delete wasm.ino.o"

ENTRYPOINT ["/entrypoint.sh"]
CMD ["python", "/js/compile.py"]
