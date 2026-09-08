ARG PLAYWRIGHT_IMAGE=mcr.microsoft.com/playwright:v1.63.0-noble
FROM ${PLAYWRIGHT_IMAGE}

WORKDIR /work
COPY tests/device-ui/package.json /work/package.json
RUN npm install --no-audit --no-fund
COPY tests/device-ui /work

ENTRYPOINT ["/work/run-device-ui-contract.sh"]
