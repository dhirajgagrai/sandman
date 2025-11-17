# In-kernel sandbox

This documentation contains guide to build the project and run it on a custom Linux kernel.

## Pre-requisites

We need some packages for compilation process:

- Build tools for project and kernel compilation
```sh
sudo apt-get install build-essential libncurses-dev bison flex libssl-dev xz-utils cmake
```

- Build tools for eBPF
```sh
sudo apt-get install libbpf-dev libelf-dev
```

- The kernel uses `pahole` to read the DWARF debug info and convert it into BTF.
```sh
sudo apt-get install dwarves
```

## Getting Started

Following tools were used during development and are required for build process:
- LLVM 21.1.3
- Clang 21.1.3
- Linux 6.8.0
- Ubuntu 24.04.2
- QEMU for testing

### LLVM and Clang

Download the LLVM release 21.1.3 from [here](https://github.com/llvm/llvm-project/releases/tag/llvmorg-21.1.3).
For Linux system, the file name starts with `LLVM-` followed by the platform name.
For example, `LLVM-21.1.3-Linux-ARM64.tar.xz` contains LLVM binaries for Arm64 Linux.

During development, I built LLVM and clang from source but pre-compiled binaries can also be used.

Extract the files and set `PATH` and `LLVM_DIR` variables for your shell (using `.bashrc` here):
```sh
tar xf LLVM-21.1.3-Linux-ARM64.tar.xz
mkdir -p ~/.local
mv LLVM-21.1.3-Linux-ARM64 ~/.local/llvm
touch ~/.bashrc
echo 'export LLVM_DIR=~/.local/llvm' >> ~/.bashrc
echo 'export PATH="$LLVM_DIR/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### Linux

For convenience, I have hosted a fork of Linux kernel 6.8.0 [here](https://github.com/dhirajgagrai/linux-4-sandman.git)
which is modified to have support for `dummy` syscall. This repo can be cloned and compilation can be easily done.

We also need an Ubuntu image for `.config` file so that kernel compatibility can be maintained.
I am using 24.04.2 release which can be found at https://old-releases.ubuntu.com/releases/24.04.2/.

Boot up the image in QEMU and copy the config file to the host system. It can be found at `/boot/config-<version>-generic`.
Rename the copied file as `.config` and place it under the Linux kernel source directory that was cloned above.

Configure:
```sh
make olddefconfig
make menuconfig
```

In config menu, navigate to `Kernel hacking` > `Compile-time checks and compiler options` >
`Debug information` > `Generate DWARF Version 5 debuginfo`.
Then enable `Kernel hacking` > `Compile-time checks and compiler options` > Enable `Generate BTF typeinfo`.

Save the config and kernel is setup for compilation.

Install bpftool (required for eBPF compilation):
```sh
make -C tools/bpf/bpftool/
sudo make -C tools/bpf/bpftool/ install
```

## Compilation

### Build LLVM Pass

Navigate into root of the project directory and run the following scripts to build the pass:
```sh
./scripts/clean.sh
./scripts/dump-libc.sh
./scripts/build.sh
```

### Build eBPF loader

In the root of the project directory, run the below script.
This will generate a executable called `ebpf-loader`:
```sh
./scripts/ebpf-build.sh
```

### Build Linux Kernel

Ensure the setup is done properly as given in [Getting Started](#getting-started).

There might be an error regarding certificates, so run following:
```sh
scripts/config --disable SYSTEM_TRUSTED_KEYS
scripts/config --disable SYSTEM_REVOCATION_KEYS
```

Navigate into Linux kernel directory and run:
```sh
make -j8
```

Build the modules under a temporary directory:
```sh
mkdir -p ~/tmp_modules
make modules_install INSTALL_MOD_PATH=~/tmp_modules/
```

Note the kernel version in tmp_modules/lib/modules/<kernel-version>. We need this version while copying other files.

## Installation

Copy the `tmp_modules` directory from host to Ubuntu running in QEMU (Let's assume files are copied under `/home/user` or `~/`).
Copy the new `.config` file which was generated under kernel source code directory.
Also copy the new image file which is generated at `arch/<arch>/boot/Image` of kernel source code directory.

Rename the `.config` and `Image` files with kernel version noted above:
```sh
mv .config config-<kernel-version>
mv Image vmlinuz-<kernel-version>
```

Now copy the files to the system:
```sh
sudo mv ~/tmp_modules/lib/modules/<kernel-version> /lib/modules/
sudo cp ~/vmlinuz-<kernel-version> /boot/
sudo cp ~/config-<kernel-version> /boot/
```

Generate `initrd`:
```sh
sudo update-initramfs -c -k <kernel-version>
```

Make changes to the GRUB. Comment out GRUB_TIMEOUT_STYLE and set timeout to some positive value GRUB_TIMEOUT = 5.
```sh
sudo vi /etc/default/grub
sudo update-grub
```

## Execution

To compile C programs, use the scripts provided:
```sh
./scripts/compile.sh <program.c>
```
This will generate a file called `nfa.dat` for the `program.c` in the root of the project directory
and also an exectutable `program.out` in the same directory as `program.c`.

Copy the `ebpf-loader` , `nfa.dat` and `program.c` to QEMU and run it:
```sh
sudo ./ebpf-loader nfa.dat
```
Monitor will start and when `program.out` is executed it will enforce the NFA according to `nfa.dat`.

## Multiple C Files Compilation

To compile multiple file, use the multi-compile script:
```sh
sudo ./scripts/multi-compile.sh <file1.c> <file2.c>
```

This will generate an executable `final-build.out` under project root directory.

To compile programs in mbedtls repo, first run the `make` and build regularly as given in the mbedtls README.md.
This will generate the required binaries for mbedtls programs to work.
Then from the root directory of this project, compile the programs using following format:
```sh
clang -fpass-plugin=./build/pass/SandmanPlugin.so -I<path-to-mbedtls-project>/include -L<path-to-mbedtls-project>/library <path-to-mbedtls-project>/programs/<sub-program-directory>/<program>.c -lmbedtls -lmbedx509 -lmbedcrypto -o <program>.out
```

