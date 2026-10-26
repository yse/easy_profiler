FROM 51test2003/freebsd-cross-i386:latest
#FROM alpine:3.8
RUN apk add --no-cache cmake
RUN wget https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.1/clang+llvm-13.0.1-i386-unknown-freebsd13.tar.xz
RUN tar -xf clang+llvm-13.0.1-i386-unknown-freebsd13.tar.xz 
