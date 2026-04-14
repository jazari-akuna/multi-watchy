# syntax=docker/dockerfile:1.6
#
# gb-builder: reproducible Android build environment for the patched
# Gadgetbridge fork (with the Watchy driver applied). All network pulls
# (JDK, cmdline-tools, SDK platform/build-tools) happen at image build
# time so `docker run` can work offline once the Gradle cache is warm.
FROM eclipse-temurin:17-jdk

ARG HOST_UID=1000
ARG HOST_GID=1000
ARG CMDLINE_TOOLS_URL=https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
ARG ANDROID_PLATFORM=android-34
ARG ANDROID_BUILD_TOOLS=34.0.0

ENV DEBIAN_FRONTEND=noninteractive \
    ANDROID_HOME=/opt/android-sdk \
    ANDROID_SDK_ROOT=/opt/android-sdk \
    GRADLE_USER_HOME=/home/builder/.gradle \
    LANG=C.UTF-8

# Base packages required by setup.sh (git) and SDK bootstrap (unzip/wget/curl).
RUN set -eux; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        git \
        unzip \
        wget \
        sudo \
    ; \
    rm -rf /var/lib/apt/lists/*

# Install Android command-line tools into the canonical
# $ANDROID_HOME/cmdline-tools/latest layout expected by sdkmanager.
RUN set -eux; \
    mkdir -p "${ANDROID_HOME}/cmdline-tools"; \
    cd /tmp; \
    wget -q -O cmdline-tools.zip "${CMDLINE_TOOLS_URL}"; \
    unzip -q cmdline-tools.zip; \
    mv cmdline-tools "${ANDROID_HOME}/cmdline-tools/latest"; \
    rm -f cmdline-tools.zip

ENV PATH="${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools:${PATH}"

# Accept licenses and install only the SDK components we need.
# Strip sources/emulator/docs to keep the image small.
RUN set -eux; \
    yes | sdkmanager --licenses >/dev/null; \
    sdkmanager \
        "platform-tools" \
        "platforms;${ANDROID_PLATFORM}" \
        "build-tools;${ANDROID_BUILD_TOOLS}" \
    ; \
    rm -rf "${ANDROID_HOME}/.cache" /root/.android

# Create a non-root builder user whose uid matches the host caller so
# files written into the bind-mounted /repo are owned correctly.
RUN set -eux; \
    groupadd -g "${HOST_GID}" builder || groupadd builder; \
    useradd -m -u "${HOST_UID}" -g builder -s /bin/bash builder; \
    mkdir -p /home/builder/.gradle /repo; \
    chown -R builder:builder /home/builder /repo "${ANDROID_HOME}"

USER builder
WORKDIR /repo

ENTRYPOINT ["bash", "/repo/docker/gb-builder/inside.sh"]
