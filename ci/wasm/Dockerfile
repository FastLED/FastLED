FROM trzeci/emscripten:1.39.17-upstream AS emscripten_base
# A good practice is to don't use `latest` - even if some effort is made to make sure that `latest` version is stable

# Target base image
FROM fedora

# Copy pre-compiled content of Emscripten SDK to target image
COPY --from=emscripten_base /emsdk_portable /emsdk_portable

# Install required tools to run Emscripten SDK
RUN dnf install -y python python-pip ca-certificates nano git

# Install PlatformIO
RUN pip install platformio==6.1.16

# Create a custom print script
RUN echo '#!/bin/sh' > /usr/bin/print && \
    echo 'echo "$@"' >> /usr/bin/print && \
    chmod +x /usr/bin/print

# Add print function to /etc/profile to make it available in all shells
RUN echo 'print() { echo "$@"; }' >> /etc/profile && \
    echo 'export -f print' >> /etc/profile

# THIS IS A WORK IN PROGRESS!!!!
COPY platformio.ini /platformio.ini

# Set up the entrypoint script
RUN echo '#!/bin/bash' > /entrypoint.sh && \
    echo 'source /emsdk_portable/emsdk_env.sh' >> /entrypoint.sh && \
    echo 'exec "$@"' >> /entrypoint.sh && \
    chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/bin/bash"]
