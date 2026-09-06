# syntax=docker/dockerfile:1.7
FROM python:3.12-slim

WORKDIR /tests
COPY --from=ha_testkit . /opt/ha-mqtt-contract
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt && \
    pip install --no-cache-dir /opt/ha-mqtt-contract
COPY bootstrap.py verify_e2e.py fixture_schema.json ./
COPY fixtures ./fixtures

CMD ["python", "verify_e2e.py"]
