FROM python:3.12-alpine

WORKDIR /app
COPY server.py /app/server.py

ENV ACS_HOST=0.0.0.0 \
    ACS_PORT=3000 \
    ACS_DATA_FILE=/data/devices.json \
    ACS_ONLINE_WINDOW=180 \
    PYTHONUNBUFFERED=1

VOLUME ["/data"]
EXPOSE 3000
HEALTHCHECK --interval=10s --timeout=3s --retries=3 \
  CMD wget -qO- http://127.0.0.1:3000/healthz || exit 1

CMD ["python3", "/app/server.py"]
