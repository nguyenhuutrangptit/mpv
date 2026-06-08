#!/bin/bash -e

. ../../include/path.sh

build=_build$ndk_suffix

if [ "$1" == "build" ]; then
	true
elif [ "$1" == "clean" ]; then
	rm -rf $build
	exit 0
else
	exit 255
fi

unset CC CXX # meson wants these unset

meson setup $build --cross-file "$prefix_dir"/crossfile.txt \
	--default-library shared \
	-Diconv=disabled -Dlua=disabled -Dlibass=disabled \
	-Dlibarchive=disabled -Dlibbluray=disabled -Ddvdnav=disabled \
	-Dcaca=disabled -Dsdl2-video=disabled -Dwayland=disabled \
	-Dx11=disabled -Dgl=disabled -Dvulkan=disabled \
	-Dvaapi=disabled -Dvdpau=disabled -Dplain-gl=disabled \
	-Dcoreaudio=disabled -Dalsa=disabled -Dpulse=disabled -Dpipewire=disabled \
	-Dlibmpv=true -Dcplayer=false \
	-Dmanpage-build=disabled

ninja -C $build -j$cores
if [ -f $build/libmpv.a ]; then
	echo >&2 "Meson fucked up, forcing rebuild."
	$0 clean
	exec $0 build
fi
DESTDIR="$prefix_dir" ninja -C $build install
