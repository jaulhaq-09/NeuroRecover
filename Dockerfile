FROM ubuntu:22.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y build-essential cmake g++ && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libstdc++6 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/build/neurorecover /usr/local/bin/neurorecover
RUN mkdir -p /output
VOLUME ["/dev", "/output"]
ENTRYPOINT ["/usr/local/bin/neurorecover"]
CMD ["--help"]

# Usage:
#   docker build -t neurorecover .
#   docker run --privileged -v /dev:/dev -v $(pwd)/out:/output neurorecover /dev/sdb /output -t all -d -f -r
