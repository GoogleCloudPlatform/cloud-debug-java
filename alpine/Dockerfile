# WARNING: Stackdriver Debugger is not regularly tested on the Alpine Linux
# platform and support will be on a best effort basis.
# Sample Alpine Linux image including Java 8 and the Stackdriver Debugger agent.
# The final image size should be around 90 MiB.

# Stage 1: Build the agent.
FROM alpine:latest

RUN apk --no-cache add bash git curl gcc g++ make cmake python maven openjdk8
RUN git clone https://github.com/GoogleCloudPlatform/cloud-debug-java

WORKDIR cloud-debug-java
RUN bash build.sh
RUN mkdir -p /opt/cdbg
RUN tar -xvf cdbg_java_agent_service_account.tar.gz -C /opt/cdbg


# Stage 2: Create a minimal image with just Java and the debugger agent.
FROM alpine:latest

RUN apk --no-cache add openjdk8-jre
COPY --from=0 /opt/cdbg /opt/cdbg
