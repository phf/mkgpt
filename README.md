# mkgpt: create GPT disk images from partition images

A simple tool for creating disk images with a GPT header and including the
contents of provided partition images.

## How to build

Just say `make` followed by `make install` and most likely you'll be happy.
If you want a compact, statically linked executable make sure you have the
`musl-gcc` wrapper installed and say `make musl-static` (or just `make static`
if you have a full musl toolchain) followed by `make install` instead.

## How to use

### Program options

- `-o <output_file>`
  specify output filename
- `--sector-size <size>`
  size of a sector (defaults to 512)
- `--minimum-image-size <size>`
  minimum size of the image in sectors (defaults to 2048)
- `--disk-guid <guid>`
  GUID of the entire disk (see GUID format below - defaults to random)
- `--part <file> <options>`
  begin a partition entry containing the specified image as its data and
  options as below

### Partition options

- `--name <name>`
  set the name of the entry in the GPT
- `--type <type>`
  set the type of the entry, either a GUID, MBR partition ID _in decimal_ or
  one of the known paritition types
- `--uuid <guid>`
  specify the UUID of the partition in the GPT (defaults to a random uuid)

### Known partition types

- `system` EFI sytem partition
- `bios` BIOS boot partition
- FAT types: `fat12`, `fat16`, `fat16b`, `fat32`, `fat16x`, `fat32x`, `fat16+`,
  `fat32+`
- NTFS types: `ntfs`
- Linux types: `linux`

### GUID format

GUIDs/UUIDs should be in the canonical representation as per
https://en.wikipedia.org/wiki/Universally_unique_identifier#Format
without any surrounding braces.
An example is:
`123E4567-E89B-12D3-A456-426655440000`
Optionally, the string `random` can be used to generate a random GUID.

## Why fork?

- the original build process seemed bloated for a tool this simple
- no way to make room for larger boot loaders or to create swap partitions
- some overly specific code (Linux, Windoze) without a strong need for it
- some strange code (static info in oversized, dynamically populated array?)
- some rather broken code (required calls in asserts, misaligned pointers)
