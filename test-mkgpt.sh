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
# TODO --uuid "all zero" is taken to mean "random uuid"?
./mkgpt -o ${tmpdir}/bla.img -s 131072 --disk-guid 1ABC2ABC-1111-2222-3333-1ABC2ABC3ABC \
	--part ${tmpdir}/a.img --type system --name part_system_a --uuid 33333333-3333-3333-3333-333333333333 \
	--part ${tmpdir}/b.img --type fat32 --name part_fat32_b --uuid 11111111-1111-1111-1111-111111111111 \
	--part ${tmpdir}/c.img --type linux --name X --uuid 01234567-89AB-CDEF-0123-456789ABCDEF \
	--part ${tmpdir}/d.img --type 0x82 --name 123456789012345678901234567890123456 --uuid 22222222-2222-2222-2222-222222222222 \
	--part ${tmpdir}/e.img --type 21686148-6449-6E6F-744E-656564454649 --uuid 44444444-4444-4444-4444-444444444444

fdisk -l ${tmpdir}/bla.img
sfdisk --verify ${tmpdir}/bla.img
sfdisk --dump ${tmpdir}/bla.img
head -c 512 ${tmpdir}/bla.img | xxd -s 446

checksum=$(md5sum ${tmpdir}/bla.img | cut -c1-32)
if [ ! "$checksum" = "a4abc130196a29288ab9af1c4489fe24" ]; then
	echo "checksum didn't match, regression!"
	exit 1
fi

rm -rfv ${tmpdir}
