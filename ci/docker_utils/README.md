# Legacy AVR8JS Docker Helpers

The local `bash test --run uno` command and AVR8JS CI now use native fbuild.
These older helpers are retained for manual use and are not required by the
maintained test path. The Docker Hub image is not assumed to be available.

This directory hosts the **AVR8JS emulator** Docker helper only.

## What's here now

- **`Dockerfile.avr8js`** — legacy AVR8JS-based emulator image definition.
- **`avr8js/`** — Node packages + entrypoint scripts baked into the avr8js
  image.
- **`avr8js_docker.py`** — legacy Python runner (`DockerAVR8jsRunner`) for
  manually invoking a locally built image.
