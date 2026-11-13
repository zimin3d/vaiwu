# Builds whisper.cpp from source and runs its HTTP server. No prebuilt image
# dependency, so the tag can't disappear on us. Model is downloaded at build.
FROM debian:bookworm-slim AS build
ARG WHISPER_MODEL=small
RUN apt-get update && apt-get install -y --no-install-recommends \
        git build-essential cmake ca-certificates curl && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /opt
RUN git clone --depth 1 https://github.com/ggerganov/whisper.cpp.git
WORKDIR /opt/whisper.cpp
RUN cmake -B build -DWHISPER_BUILD_SERVER=ON -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release -j"$(nproc)"
RUN bash ./models/download-ggml-model.sh ${WHISPER_MODEL}

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends libgomp1 ca-certificates && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /app
# The server binary name changed across whisper.cpp versions; copy whichever exists.
COPY --from=build /opt/whisper.cpp/build/bin/ /app/bin/
COPY --from=build /opt/whisper.cpp/models/ /app/models/
ARG WHISPER_MODEL=small
ENV WHISPER_MODEL=${WHISPER_MODEL}
EXPOSE 8080
# Bind to all interfaces INSIDE the container only; it is never published to the
# host - the Caddy gateway is the only exposed port (see docker-compose.yml).
CMD ["/bin/sh","-c","BIN=$(ls /app/bin/whisper-server /app/bin/server 2>/dev/null | head -1); exec \"$BIN\" -m /app/models/ggml-${WHISPER_MODEL}.bin --host 0.0.0.0 --port 8080 -t $(nproc)"]
