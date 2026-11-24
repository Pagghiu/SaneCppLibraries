#!/bin/sh
set -eu

target=${1:?Expected freebsd, netbsd, or openbsd.}
sysroot=${2:?Expected a sysroot directory.}
archives=${3:?Expected a release archive cache directory.}
mkdir -p "$sysroot" "$archives"

fetch()
{
    archive=$1
    algorithm=$2
    digest=$3
    if [ ! -f "$archives/$archive" ]; then
        curl --fail --silent --show-error --location --retry 3 \
            --output "$archives/$archive.part" "$url/$archive"
        printf '%s  %s\n' "$digest" "$archives/$archive.part" | "$algorithm" --check
        mv "$archives/$archive.part" "$archives/$archive"
    else
        printf '%s  %s\n' "$digest" "$archives/$archive" | "$algorithm" --check
    fi
}

case "$target" in
    freebsd)
        url=https://download.freebsd.org/releases/arm64/aarch64/15.1-RELEASE
        fetch base.txz sha256sum 5b7a46a0abfbe23a1d4454b5600e2efcfce16b705bc7e9c851c37470c035ef98
        tar -xf "$archives/base.txz" -C "$sysroot" ./lib ./usr/include ./usr/lib
        ;;
    netbsd)
        url=https://cdn.NetBSD.org/pub/NetBSD/NetBSD-11.0/evbarm-aarch64/binary/sets
        fetch base.tar.xz sha512sum d17b3253959e110edba1755f481e707fd79a1a4a35bd7096a305c2959d6a5964713d4c35f439d76ca03d9252f8652d73f521ea6fc07a27ffa5ca22a80df6e7c5
        fetch comp.tar.xz sha512sum 0719f3d94fd5cdedf83ad7418f1e254c898b388c320f28d3d39b8abf96fe6ea58b605be8dc9e02d4d76021f87e3f0cd2bc327560e887abc572fbae04742305fd
        # NetBSD ships the machine include symlink in the base set.
        tar -xf "$archives/base.tar.xz" -C "$sysroot" ./lib ./usr/include ./usr/lib
        tar -xf "$archives/comp.tar.xz" -C "$sysroot" ./usr/include ./usr/lib
        test -f "$sysroot/usr/include/machine/ansi.h"
        ;;
    openbsd)
        url=https://cdn.openbsd.org/pub/OpenBSD/8.0/arm64
        fetch base80.tgz sha256sum 92cfeda3da21d14f10f0687d194a3a6ec99fbf89140f927369e37f917ca35ed9
        fetch comp80.tgz sha256sum 7552eaf6ee6c607a3fb174e0a77efcd40aa6c62540884069cf2dffc7b8c0c10d
        tar -xf "$archives/base80.tgz" -C "$sysroot" ./usr/lib ./usr/libexec/ld.so
        tar -xf "$archives/comp80.tgz" -C "$sysroot" ./usr/include ./usr/lib
        # Upstream LLD needs unversioned linker names. The shared objects retain their versioned SONAMEs.
        for library in "$sysroot"/usr/lib/lib*.so.[0-9]*; do
            name=${library%.so.*}.so
            if [ ! -e "$name" ]; then
                ln -s "${library##*/}" "$name"
            fi
        done
        test -f "$sysroot/usr/include/machine/signal.h"
        ;;
    *) printf 'Unknown BSD target: %s\n' "$target" >&2; exit 1 ;;
esac
