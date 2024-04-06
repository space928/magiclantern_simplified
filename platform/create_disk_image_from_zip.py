#!/usr/bin/env python3

import os
import sys
import argparse
import subprocess
import shutil
import zipfile
import lzma
import hashlib
from time import sleep


def main():
    args = parse_args()

    qemu_mount_dir = "qemu_disk_mount"
    if os.path.isdir(qemu_mount_dir):
        # rmdir will fail if still mounted, this is good
        # as it indicates some problem we want to be aware of
        for root, dirs, files in os.walk(qemu_mount_dir, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
            for name in dirs:
                os.rmdir(os.path.join(root, name))
        os.rmdir(qemu_mount_dir)
    os.mkdir(qemu_mount_dir)

    qcow_name = "sd.qcow2"
    cf_name = "cf.qcow2"
    img_name = os.path.splitext(qcow_name)[0] + ".img"
    pid = 0

    # delete old disk images to ensure builds either give you
    # a current image, or nothing.
    if os.path.isfile(cf_name):
        os.remove(cf_name)
    if os.path.isfile(qcow_name):
        os.remove(qcow_name)
    if os.path.isfile(qcow_name + ".xz"):
        os.remove(qcow_name + ".xz")
    if os.path.isfile(img_name):
        os.remove(img_name)

    # decompress the base image
    with open(qcow_name, "wb") as outf:
        qcow_data = lzma.open(os.path.join("..", "sd.qcow2.xz")).read()
        outf.write(qcow_data)

    # I couldn't get guestmount to work for me on wsl/ubuntu
    # So, here's my janky solution (can you tell I'm not a linux user...)
    # Derived from:
    # https://foss.heptapod.net/magic-lantern/magic-lantern/-/blob/branch/qemu/contrib/qemu/scripts/mtools_copy_ml.sh
    # sd.qcow2 -> qemu-img -> sd.img -> mcopy -> qemu-img -> sd.qcow2
    try:
        subprocess.run(["qemu-img", "convert", qcow_name, img_name])

        # Copy files
        old_autoexec = os.path.join(qemu_mount_dir, "autoexec.bin")
        if os.path.isfile(old_autoexec):
            os.remove(old_autoexec)

        old_ml = os.path.join(qemu_mount_dir, "ML")
        if os.path.isdir(old_ml):
            shutil.rmtree(old_ml, ignore_errors=True)

        with zipfile.ZipFile(args.build_zip, 'r') as z:
            z.extractall(qemu_mount_dir)

        # Use mcopy to copy the files to the image, annoyingly we need to 
        # specify a constant offset into the image.
        # Derived from: 
        # https://foss.heptapod.net/magic-lantern/magic-lantern/-/blob/aa5fc66014270504f6ba83de1e541f0e0a53dca1/contrib/qemu/scripts/mtools_setup.sh#L10
        subprocess.run(["mcopy", "-o", "-i", img_name + "@@50688", 
                        os.path.join(qemu_mount_dir, "autoexec.bin"), 
                        "::"])
        subprocess.run(["mcopy", "-o", "-s", "-i", img_name + "@@50688", 
                        os.path.abspath(os.path.join(qemu_mount_dir, "ML")), 
                        "::"])

        # Now convert the image back to a qcow2
        subprocess.run(["qemu-img", "convert", 
                        "-O", "qcow2", 
                        img_name, qcow_name])
    except Exception as ex:
        print("Exception encountered while copying build to '{0}':\n\t{1}".format(qcow_name, ex))
    finally:
        # Clean up the temp dir
        for root, dirs, files in os.walk(qemu_mount_dir, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
            for name in dirs:
                os.rmdir(os.path.join(root, name))
        os.rmdir(qemu_mount_dir)
        if os.path.isfile(img_name):
            os.remove(img_name)

    # clone to the CF image if things went okay
    if os.path.isfile(qcow_name):
        shutil.copyfile(qcow_name, cf_name)

    # If we got the guestmount waiting logic wrong earlier,
    # these files can differ!  Final sanity check:
    with open(qcow_name, "rb") as f:
        sd_hash = hashlib.sha1(f.read())
    with open(cf_name, "rb") as f:
        cf_hash = hashlib.sha1(f.read())
    if sd_hash.digest() != cf_hash.digest():
        print("CF and SD qcow2 images didn't match hashes, failing!")
        sys.exit(-1)


def is_pid(pid):        
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    return True


def parse_args():
    description = """
    Given a path to a magiclantern platform build zip, e.g.
    magiclantern-Nightly.blah.200D.zip, use guestmount to
    create a qemu disk image with the contents, ready for emulation.

    Expected only to be called by the build system, via 'make disk_image'
    """
    parser = argparse.ArgumentParser(description=description)

    parser.add_argument("build_zip",
                        help="Path to ML build zip, required")

    args = parser.parse_args()

    if not os.path.isfile(args.build_zip):
        print("Couldn't access build zip")
        sys.exit(-1)

    return args


if __name__ == "__main__":
    main()
