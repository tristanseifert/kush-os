#!/bin/sh
###############################################################################
### This builds an LLVM toolchain.
#
# I'm not really sure where this script originated, but I've been using it for
# a while to build cross toolchains.
###############################################################################

echo "===================================================================="
echo "I will try to fetch and build everything needed for a freestanding"
echo "cross-compiler toolchain. This includes llvm, clang, and lld"
echo "and may take quite a while to build. Play some tetris and check back"
echo "every once in a while. The process is largely automatic and should"
echo "not require any manual intervention. Fingers crossed!"
echo
echo "You'll need UNIX tools git, cmake, and ninja."
echo
echo "Specify LIBCXX_TRIPLE if you're not on mac"
echo "===================================================================="
echo

# *** USER-ADJUSTABLE SETTINGS ***

#X86;ARM;AArch64;Mips;
#export LLVM_TARGETS="X86;ARM;AArch64;Mips;PowerPC"
export LLVM_TARGETS="X86;ARM;AArch64"

export LLVM_REVISION="llvmorg-12.0.0-rc1"
if [ -z $LIBCXX_TRIPLE ]; then
    export LIBCXX_TRIPLE=-apple-
fi

# END OF USER-ADJUSTABLE SETTINGS

which git || (echo "Install git: brew install git"; exit)
which cmake || (echo "Install cmake: brew install cmake"; exit)
which ninja || (echo "Install ninja: brew install ninja"; exit)

mkdir -p toolchain/{build/llvm-project,sources,build/llvm2}
pushd toolchain/

export TOOLCHAIN_DIR=`pwd`

REPOBASE=https://github.com/llvm

#echo "===================================================================="
#echo "Checking out llvm-project [$LLVM_REVISION]..."
#echo "===================================================================="

#if [ ! -d sources/llvm-project ]; then
#    git clone --depth 1 --shallow-submodules --no-tags -b $LLVM_REVISION $REPOBASE/llvm-project.git sources/llvm-project
#else
#    (cd sources/llvm-project; git fetch; git checkout $LLVM_REVISION)
#fi

echo "===================================================================="
echo "Configuring llvm..."
echo "===================================================================="
if [ ! -f build/llvm-project/.config.succeeded ]; then
    pushd build/llvm-project && \
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$TOOLCHAIN_DIR/llvm \
        -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;libcxx;libcxxabi;libunwind;lldb;compiler-rt;lld;polly" \
        -DLLVM_TARGETS_TO_BUILD=$LLVM_TARGETS \
        -DLLVM_USE_SPLIT_DWARF=True -DLLVM_OPTIMIZED_TABLEGEN=True \
        -DLLVM_BUILD_TESTS=False -DLLVM_INCLUDE_TESTS=False -DLLDB_INCLUDE_TESTS=False \
        -DLLVM_BUILD_DOCS=False -DLLVM_INCLUDE_DOCS=False \
        -DLLVM_ENABLE_OCAMLDOC=False -DLLVM_ENABLE_BINDINGS=False \
        -DLLDB_USE_SYSTEM_DEBUGSERVER=True \
        ../../sources/llvm-project/llvm && \
    touch .config.succeeded && \
    popd || exit 1
else
    echo "build/llvm-project/.config.succeeded exists, NOT reconfiguring llvm!"
fi

echo "===================================================================="
echo "Building llvm... this may take a long while"
echo "===================================================================="

#if [ ! -f build/llvm-project/.build.succeeded ]; then
    pushd build/llvm-project && \
    cmake --build . && \
    touch .build.succeeded && \
    popd || exit 1
#else
#    echo "build/llvm-project/.build.succeeded exists, NOT rebuilding llvm!"
#fi

echo "===================================================================="
echo "Installing llvm and all tools..."
echo "===================================================================="

#if [ ! -f build/llvm-project/.install.succeeded ]; then
    pushd build/llvm-project && \
    cmake --build . --target install && \
    touch .install.succeeded && \
    popd || exit 1
#else
#    echo "build/llvm-project/.install.succeeded exists, NOT reinstalling llvm!"
#fi

popd
exit 0

