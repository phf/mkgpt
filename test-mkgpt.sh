#!/bin/sh
#
# Simplistic test case, but better than none.

tmpdir="/tmp/test-mkgpt"

if [ ! -x ./mkgpt ]; then
	echo "No ./mkgpt to run, no fun!"
	exit 1
fi

mkdir -pv ${tmpdir}
for name in a.img b.img c.img d.img e.img; do
	truncate --size=8M ${tmpdir}/${name}
done

# TODO something goes wrong with --sector-size 1024 or 4096
./mkgpt -o ${tmpdir}/bla.img -s 131072 \
	--part ${tmpdir}/a.img --type system --name part_system_a \
	--part ${tmpdir}/b.img --type fat32 --name part_fat32_b \
	--part ${tmpdir}/c.img --type linux --name X \
	--part ${tmpdir}/d.img --type 0x82 --name 123456789012345678901234567890123456 \
	--part ${tmpdir}/e.img --type 21686148-6449-6E6F-744E-656564454649

fdisk -l ${tmpdir}/bla.img
#xxd ${tmpdir}/bla.img | less

rm -rfv ${tmpdir}
