#!/usr/bin/env python3
import os, struct, glob, sys

SECTOR_SIZE = 512
TOTAL_SECTORS = 2880  # 1.44MB
BYTES_PER_SECTOR = SECTOR_SIZE
SECTORS_PER_CLUSTER = 1
RESERVED_SECTORS = 1
NUM_FATS = 2
ROOT_ENTRIES = 224
SECTORS_PER_FAT = 9
ROOT_DIR_SECTORS = (ROOT_ENTRIES * 32 + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR
DATA_START_SECTOR = RESERVED_SECTORS + NUM_FATS * SECTORS_PER_FAT + ROOT_DIR_SECTORS
CLUSTER_SIZE = SECTORS_PER_CLUSTER * BYTES_PER_SECTOR
IMAGE_SIZE = TOTAL_SECTORS * BYTES_PER_SECTOR

# FAT12 end-of-chain marker
EOC = 0xFFF

# Offsets inside boot sector for partition table
PART_START_OFFSET = 0x1BE + 8
PART_SIZE_OFFSET = 0x1BE + 12


def set_fat_entry(fat: bytearray, index: int, value: int):
    # FAT12 uses 12-bit entries
    offset = (index * 3) // 2
    if index & 1:
        fat[offset] = (fat[offset] & 0x0F) | ((value << 4) & 0xF0)
        fat[offset + 1] = (value >> 4) & 0xFF
    else:
        fat[offset] = value & 0xFF
        fat[offset + 1] = (fat[offset + 1] & 0xF0) | ((value >> 8) & 0x0F)


def write_files(image: bytearray, fat: bytearray, root_dir: bytearray, files):
    next_cluster = 2
    root_pos = 0
    for name, ext, path in files:
        with open(path, 'rb') as f:
            data = f.read()
        size = len(data)
        clusters = (size + CLUSTER_SIZE - 1) // CLUSTER_SIZE
        start = next_cluster
        for i in range(clusters):
            set_fat_entry(fat, next_cluster, EOC if i == clusters - 1 else next_cluster + 1)
            next_cluster += 1
        data_offset = (DATA_START_SECTOR + (start - 2) * SECTORS_PER_CLUSTER) * BYTES_PER_SECTOR
        image[data_offset:data_offset + size] = data

        entry = bytearray(32)
        entry[0:8] = name.encode('ascii')[:8].ljust(8, b' ')
        entry[8:11] = ext.encode('ascii')[:3].ljust(3, b' ')
        entry[11] = 0x20  # archive
        struct.pack_into('<H', entry, 26, start)
        struct.pack_into('<I', entry, 28, size)
        root_dir[root_pos:root_pos + 32] = entry
        root_pos += 32


def create_image(img_path: str, boot_bin: str):
    image = bytearray(IMAGE_SIZE)
    boot_sector = bytearray(512)
    boot_sector[0:3] = b'\xeb\x3c\x90'
    boot_sector[3:11] = b'MSWIN4.1'
    struct.pack_into('<H', boot_sector, 11, BYTES_PER_SECTOR)
    boot_sector[13] = SECTORS_PER_CLUSTER
    struct.pack_into('<H', boot_sector, 14, RESERVED_SECTORS)
    boot_sector[16] = NUM_FATS
    struct.pack_into('<H', boot_sector, 17, ROOT_ENTRIES)
    struct.pack_into('<H', boot_sector, 19, TOTAL_SECTORS)
    boot_sector[21] = 0xF8
    struct.pack_into('<H', boot_sector, 22, SECTORS_PER_FAT)
    struct.pack_into('<H', boot_sector, 24, 32)
    struct.pack_into('<H', boot_sector, 26, 64)
    struct.pack_into('<I', boot_sector, 28, 0)
    struct.pack_into('<I', boot_sector, 32, 0)
    boot_sector[36] = 0
    boot_sector[37] = 0
    boot_sector[38] = 0x29
    struct.pack_into('<I', boot_sector, 39, 0x12345678)
    boot_sector[43:54] = b'VCORE      '
    boot_sector[54:62] = b'FAT12   '
    boot_sector[510] = 0x55
    boot_sector[511] = 0xAA
    image[0:512] = boot_sector

    fat = bytearray(SECTORS_PER_FAT * BYTES_PER_SECTOR)
    fat[0:3] = b'\xf8\xff\xff'
    root_dir = bytearray(ROOT_DIR_SECTORS * BYTES_PER_SECTOR)

    files = []
    files.append(('KERNEL', 'ELF', 'kernel.elf'))
    files.append(('USER', 'ELF', 'user/user1/user.elf'))
    for path in glob.glob('user/*/*.elf'):
        base = os.path.basename(path)
        if base.lower() == 'user.elf' and 'user1' in path:
            continue
        name = os.path.splitext(base)[0].upper()[:8]
        files.append((name, 'ELF', path))

    write_files(image, fat, root_dir, files)

    fat_start = RESERVED_SECTORS * BYTES_PER_SECTOR
    root_start = (RESERVED_SECTORS + NUM_FATS * SECTORS_PER_FAT) * BYTES_PER_SECTOR
    image[fat_start:fat_start + len(fat)] = fat
    image[fat_start + len(fat):fat_start + len(fat) * 2] = fat
    image[root_start:root_start + len(root_dir)] = root_dir

    with open(img_path, 'wb') as f:
        f.write(image)

    # Patch partition table in boot sector
    size_sectors = len(image) // BYTES_PER_SECTOR
    with open(boot_bin, 'r+b') as f:
        f.seek(PART_START_OFFSET)
        f.write(struct.pack('<I', 63))
        f.seek(PART_SIZE_OFFSET)
        f.write(struct.pack('<I', size_sectors))


def main():
    if len(sys.argv) != 3:
        print('usage: mkfs.py <boot.bin> <fs.img>')
        return 1
    create_image(sys.argv[2], sys.argv[1])
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
