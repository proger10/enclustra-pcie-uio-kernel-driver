

## Requirements
Current kernel sources, UIO

## Build
```
export KDIR=/lib/modules/$(uname -r)/build
sudo make
```

## Usage
Make sure Userspace I/O driver `uio` is loaded (`lsmod | grep uio`). If not you must `sudo modprobe uio` first
Now you can use this driver. `sudo insmod enclustra_pci_test.ko`