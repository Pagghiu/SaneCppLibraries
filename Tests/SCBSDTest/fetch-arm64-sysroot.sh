#!/bin/sh
set -eu

target=${1:?Expected freebsd or netbsd.}
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
    *) printf 'Unknown BSD target: %s\n' "$target" >&2; exit 1 ;;
esac
