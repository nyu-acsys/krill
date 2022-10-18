#
# Ubuntu
#
FROM ubuntu:22.04 as base
ARG DEBIAN_FRONTEND=noninteractive
ARG TZ=Etc/UTC
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
        clang \
        libc++1 \
        libc++-dev \
        libc++abi1 \
        libc++abi-dev \
        zsh \
        texlive-latex-base \
        texlive-latex-extra \
        libpgf6 \
    && rm -rf /var/lib/apt/lists/*
ENV CC clang
ENV CXX clang++

#
# Z3
#
FROM base as z3
RUN git clone --depth=1 --branch z3-4.8.7 https://github.com/Z3Prover/z3.git /artifact/z3_source/
WORKDIR /artifact/z3_source/build/
RUN cmake .. -DCMAKE_INSTALL_PREFIX=/usr/
RUN make -j$((`nproc`+1))
RUN make install

#
# Plankton & Krill
#
FROM z3 as release
COPY . /artifact/krill_source
WORKDIR /artifact/krill_source/build
RUN cmake .. -DCMAKE_INSTALL_PREFIX=/ -DINSTALL_FOLDER=artifact -DCMAKE_BUILD_TYPE=RELEASE
RUN make -j$((`nproc`+1))
RUN make install

#
# Start
#
FROM release
WORKDIR /artifact/
ENTRYPOINT [ "/bin/zsh" ]
