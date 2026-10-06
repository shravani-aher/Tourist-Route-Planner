FROM debian:12-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel 2 && cmake --install build --prefix /opt/planner

FROM debian:12-slim
COPY --from=build /opt/planner /opt/planner
WORKDIR /opt/planner
EXPOSE 8080
CMD ["./pune_route_planner", "--host", "0.0.0.0"]
