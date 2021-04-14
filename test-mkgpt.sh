#!/bin/sh
#
# Simplistic test case, but better than none.

tmpdir="/tmp/test-mkgpt"

if [ ! -x ./mkgpt ]; then
	echo "No ./mkgpt to run, no fun!"
	exit 1
fi

mkdir -pv ${tmpdir}
for name in a.img b.img c.img d.img; do
	truncate --size=16M ${tmpdir}/${name}
done

# TODO something goes wrong with --sector-size 1024 or 4096
./mkgpt -o ${tmpdir}/bla.img -s 131072 \
	--part ${tmpdir}/a.img --type system \
	--part ${tmpdir}/b.img --type fat32 \
	--part ${tmpdir}/c.img --type linux \
	--part ${tmpdir}/d.img --type swap

fdisk -l ${tmpdir}/bla.img

rm -rfv ${tmpdir}
