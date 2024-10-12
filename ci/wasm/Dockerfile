FROM trzeci/emscripten:1.39.17-upstream AS emscripten_base
# A good practice is to don't use `latest` - even if some effort is made to make sure that `latest` version is stable

# Target base image
FROM fedora

# Copy pre-compiled content of Emscripten SDK to target image
COPY --from=emscripten_base /emsdk_portable /emsdk_portable

# Install required tools to run Emscripten SDK

RUN dnf install -y git
RUN dnf install -y nano which
RUN dnf install -y ca-certificates
RUN dnf install -y python3 python3-pip
RUN dnf install -y dos2unix

# Create symlinks for python and pip
#RUN ln -s /usr/bin/python3 /usr/bin/python && \
#    ln -s /usr/bin/pip3 /usr/bin/pip

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

# THIS IS A WORK IN PROGRESS!!!!
COPY platformio.ini /platformio.ini

# Set up the entrypoint script
RUN echo '#!/bin/bash' > /entrypoint.sh && \
    echo 'source /emsdk_portable/emsdk_env.sh' >> /entrypoint.sh && \
    echo 'exec "$@"' >> /entrypoint.sh && \
    chmod +x /entrypoint.sh


COPY ./*.* /js/fastled/
COPY ./src/*.* /js/fastled/src/
COPY ./src/fx/ /js/fastled/src/fx/
COPY ./src/lib8tion/ /js/fastled/src/lib8tion

# Copy platforms directory separately, since it changes a lot during development
COPY ./src/platforms/ /js/fastled/src/platforms/


# Hack for now to get the wasm platform to compile
COPY src/platforms/stub/wasm/platformio.ini /wasm/platformio.ini
COPY src/platforms/stub/wasm/wasm.py /wasm/wasm.py
COPY src/platforms/stub/wasm/index.html /wasm/index.html
COPY src/platforms/stub/wasm/_exports.hpp /wasm/_exports.hpp
COPY src/platforms/stub/wasm/_timer.hpp /wasm/_timer.hpp
COPY src/platforms/stub/wasm/message_queue.h /wasm/message_queue.h

# There is some download that needs to happen on the first compile.
# so we will prewarm the cache by building a dummy project.
RUN cp -r /wasm /wasm-prewarm && cp -r /js/fastled /wasm-prewarm/fastled && cd /wasm-prewarm && pio run && cd .. && rm -rf /wasm-prewarm


WORKDIR /js

COPY ci/wasm/entrypoint.sh /js/compile.sh

RUN dos2unix /js/compile.sh

# Create compile.sh in /js directory
RUN chmod +x /js/compile.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/bin/bash", "/js/compile.sh"]
