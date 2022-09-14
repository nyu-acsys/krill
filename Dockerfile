ARG COMPILER=gcc # allowed values: gcc, clang

#
# Ubuntu
#
FROM ubuntu:22.04 as base
ARG DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC
RUN apt update && apt -y --no-install-recommends install \
        make \
        cmake \
        git \
        default-jre-headless \
        python3 \
        python3-distutils \
        uuid \
        uuid-dev \
        pkg-config \
        gcc \
        g++ \
    && rm -rf /var/lib/apt/lists/*
ENV CC gcc
ENV CXX g++

#
# Z3
#
FROM base as z3
RUN git clone --depth=1 --branch z3-4.8.7 https://github.com/Z3Prover/z3.git /artifact/z3/
WORKDIR /artifact/z3/build/
RUN cmake .. -DCMAKE_INSTALL_PREFIX=/usr/
RUN make -j$((`nproc`+1))
RUN make install

#
# Compiler
#
FROM z3 as compiler-gcc
FROM z3 as compiler-clang
RUN apt update && apt -y --no-install-recommends install \
        clang \
        libc++1 \
        libc++-dev \
        libc++abi1 \
        libc++abi-dev \
    && rm -rf /var/lib/apt/lists/*
RUN apt -y --no-install-recommends install clang libc++1 libc++-dev libc++abi1 libc++abi-dev
RUN touch /usr/bin/stdclang
RUN chmod a+x /usr/bin/stdclang
RUN echo '#!/bin/bash' >> /usr/bin/stdclang
RUN echo 'clang++ -stdlib=libc++ $@' >> /usr/bin/stdclang
ENV CC clang
ENV CXX stdclang
FROM compiler-${COMPILER} as compiler

#
# Plankton
#
FROM compiler as debug
COPY . /artifact/plankton-debug/source
WORKDIR /artifact/plankton-debug/source/build
RUN cmake .. -DCMAKE_INSTALL_PREFIX=/artifact -DINSTALL_FOLDER=plankton-debug -DCMAKE_BUILD_TYPE=DEBUG
RUN make -j$((`nproc`+1))
RUN make install

FROM debug as release
COPY . /artifact/plankton/source
WORKDIR /artifact/plankton/source/build
RUN cmake .. -DCMAKE_INSTALL_PREFIX=/artifact -DINSTALL_FOLDER=plankton -DCMAKE_BUILD_TYPE=RELEASE
RUN make -j$((`nproc`+1))
RUN make install

#
# Start
#
FROM release
WORKDIR /artifact/plankton/
ENTRYPOINT [ "/bin/bash" ]
