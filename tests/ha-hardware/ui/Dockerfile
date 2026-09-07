# syntax=docker/dockerfile:1.7
ARG PLAYWRIGHT_IMAGE
FROM ${PLAYWRIGHT_IMAGE}

RUN npm install --prefix /opt/playwright --omit=dev @playwright/test@1.63.0 && npm cache clean --force

COPY run-visual-test-harness.sh /usr/local/bin/run-visual-test-harness
RUN chmod 755 /usr/local/bin/run-visual-test-harness

ENTRYPOINT ["/usr/local/bin/run-visual-test-harness"]
