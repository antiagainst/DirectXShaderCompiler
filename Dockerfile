FROM alpine:3.7

MAINTAINER DXC Team

RUN apk add --no-cache build-base git cmake python libexecinfo-dev

WORKDIR /dxc
RUN git clone --recurse-submodules https://github.com/Microsoft/DirectXShaderCompiler /dxc
WORKDIR build
RUN cmake $(cat /dxc/utils/cmake-predefined-config-params) \
          -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local .. \
    && make -j`nproc` install
WORKDIR /
RUN rm -rf dxc

ENTRYPOINT ["/usr/local/bin/dxc"]
