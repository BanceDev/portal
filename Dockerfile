FROM ubuntu:latest

WORKDIR /app

# Install C development deps
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    g++ \
    make \
    cmake \
    libsdl2-dev \
    libglew-dev \
    libglm-dev \
    libgl1-mesa-dev \
    libx11-dev \
    wget \
    git

COPY . .

RUN chmod +x ./docker_install.sh

RUN sh ./docker_install.sh

RUN rm -rf /var/lib/apt/lists/*

WORKDIR /app/bin/Debug/server

CMD ["./PortalServer"]
