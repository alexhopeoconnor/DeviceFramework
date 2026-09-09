ARG PLAYWRIGHT_IMAGE=mcr.microsoft.com/playwright:v1.63.0-noble
FROM ${PLAYWRIGHT_IMAGE} AS device-ui

WORKDIR /work
COPY tests/device-ui/package.json /work/package.json
RUN npm install --no-audit --no-fund
COPY tests/device-ui /work
RUN chmod 755 /work/run-device-ui-contract.sh /work/render-readme-media.sh

ENTRYPOINT ["/work/run-device-ui-contract.sh"]

FROM device-ui AS media
USER root
RUN apt-get update && \
    apt-get install -y --no-install-recommends ffmpeg && \
    rm -rf /var/lib/apt/lists/*
USER pwuser
