FROM trzeci/emscripten:1.39.17-upstream AS emscripten_base
# A good practice is to don't use `latest` - even if some effort is made to make sure that `latest` version is stable

# Target base image
FROM fedora

# Copy pre-compiled content of Emscripten SDK to target image
COPY --from=emscripten_base /emsdk_portable /emsdk_portable

# Install required tools to run Emscripten SDK
RUN dnf install -y python python-pip ca-certificates nano

# Install PlatformIO
RUN pip install platformio==6.1.16

# Add alias for `print` command
RUN echo "alias print='echo'" >> /etc/bashrc
RUN echo "alias print='echo'" >> /etc/profile

# Optionally, create a print command in case a non-interactive shell is used
RUN echo '#!/bin/sh' > /usr/local/bin/print && \
    echo 'echo "$@"' >> /usr/local/bin/print && \
    chmod +x /usr/local/bin/print

# THIS IS A WORK IN PROGRESS!!!!
COPY platformio.ini /platformio.ini

# Set up the entrypoint script
RUN echo '#!/bin/bash' > /entrypoint.sh && \
    echo 'source /emsdk_portable/emsdk_env.sh' >> /entrypoint.sh && \
    echo 'exec "$@"' >> /entrypoint.sh && \
    chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/bin/bash"]
