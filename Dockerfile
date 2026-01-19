FROM alpine:latest


RUN apk add --no-cache \
    sudo \
    gcc \
    musl-dev \
    python3 \
    py3-pip \
    python3-dev


RUN echo "root ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

WORKDIR /app
COPY soul.py soul.c /app/

RUN gcc soul.c -o soul -lpthread -O3 -static
RUN chmod +x /app/soul

RUN pip install --no-cache-dir requests --break-system-packages

CMD ["python3", "soul.py"]
