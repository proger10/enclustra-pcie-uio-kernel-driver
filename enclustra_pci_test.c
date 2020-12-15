#include <linux/device.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>

#include <asm/io.h>

#define DMA_BASE 0x4000

static irqreturn_t irq_handler(int irq, struct uio_info *dev_info) {
    // Interrupt for DMA
    u_int32_t *regs = dev_info->mem[1].internal_addr + DMA_BASE; // Get registers memory

    // Check if interrupt really raised
    if (!(regs[0] & 1)){
        return IRQ_NONE;
    }

    // handle
    regs[0] = regs[0] & (~1);
    __asm volatile("sfence" ::: "memory"); // Not sure it is neccessary. Synchronize buffer and physical memory

    return IRQ_HANDLED;
}

static int enclustra_pci_probe(struct pci_dev *dev, const struct pci_device_id *id) {
    // info-structure for Userspace I/O
    struct uio_info *info;
    info = kzalloc(sizeof(struct uio_info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;

    if (pci_enable_device(dev))
        goto out_free;

    // Not sure it is neccessary
    // TODO: error handling
    pci_set_master(dev);

    if (pci_request_regions(dev, "enclustra_pci_test"))
        goto out_disable;

    // Set appropriate DMA mask
    // Not sure what happens here
    if ( !pci_set_dma_mask(dev, DMA_BIT_MASK(64)) ) {
        pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(64));
        printk(KERN_DEBUG "Using a 64-bit DMA mask.\n");
    } else  
    if ( !pci_set_dma_mask(dev, DMA_BIT_MASK(32)) ) {
        pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(32));
        printk(KERN_DEBUG "Using a 32-bit DMA mask.\n");
    } else {
        printk(KERN_DEBUG "pci_set_dma_mask() fails for both 32-bit and 64-bit DMA!\n");
        // TODO: Error handling
    }


    // BAR0 direct access to on-chip-memory
    info->mem[0].addr = pci_resource_start(dev, 0);
    if (!info->mem[0].addr)
        goto out_release;
    info->mem[0].size = pci_resource_len(dev, 0); // this function returns strange size: 0x400000
    // info->mem[0].size = 131072;
    info->mem[0].internal_addr = pci_iomap(dev, 0, info->mem[0].size);
    if (!info->mem[0].internal_addr)
        goto out_release;
    info->mem[0].memtype = UIO_MEM_PHYS;
    printk(KERN_DEBUG "BAR0 done!\n");


    // BAR2 access to control registers: DMA and PCI-E
    info->mem[1].addr = pci_resource_start(dev, 2);
    if (!info->mem[1].addr)
        goto out_release;
    info->mem[1].size = pci_resource_len(dev, 2);
    // info->mem[1].size = 0x5000;
    info->mem[1].internal_addr = pci_iomap(dev, 2, info->mem[1].size);
    if (!info->mem[1].internal_addr)
        goto out_release;
    info->mem[1].memtype = UIO_MEM_PHYS;
    printk(KERN_DEBUG "BAR2 done!\n");

    // Some driver information
    info->name = "enclustra_pci_test";
    info->version = "0.0.1";
    info->irq = dev->irq;
    info->irq_flags = IRQF_SHARED; // Not sure about this
    // info->irq_flags = 0;
    info->handler = irq_handler;

    printk(KERN_DEBUG "Going to register UIO device!\n");
    if (uio_register_device(&dev->dev, info))
        goto out_unmap;

    printk(KERN_DEBUG "Going to pci_set_drvdata()!\n");
    pci_set_drvdata(dev, info);

    // Allow PCI-E interrupts
    *((u_int32_t *)(info->mem[1].internal_addr+0x50)) = 1;
    __asm volatile("sfence" ::: "memory"); // Not sure it is neccessary. Synchronize buffer and physical memory
    return 0;
out_unmap:
    printk(KERN_DEBUG "out_unmap() error!\n");
    iounmap(info->mem[0].internal_addr);
    iounmap(info->mem[1].internal_addr);
out_release:
    printk(KERN_DEBUG "out_release() error!\n");
    pci_release_regions(dev);
out_disable:
    printk(KERN_DEBUG "out_disable() error!\n");
    pci_disable_device(dev);
out_free:
    printk(KERN_DEBUG "out_free() error!\n");
    kfree (info);
    return -ENODEV;
}

static void enclustra_pci_remove(struct pci_dev *dev) {
    struct uio_info *info = pci_get_drvdata(dev);

    uio_unregister_device(info);
    pci_release_regions(dev);
    pci_disable_device(dev);
    iounmap(info->mem[0].internal_addr);
    iounmap(info->mem[1].internal_addr);
    kfree (info);
}

static struct pci_device_id enclustra_pci_ids[] = { 
    {
        .vendor =   0x1172,
        .device =   0xe001,
        .subvendor =    0x0000,
        .subdevice =    0x0000,
    },
    { 0, }
};

static struct pci_driver enclustra_pci_driver = {
    .name = "enclustra_pci_test",
    .id_table = enclustra_pci_ids,
    .probe = enclustra_pci_probe,
    .remove = enclustra_pci_remove,
};

module_pci_driver(enclustra_pci_driver);
MODULE_DEVICE_TABLE(pci, enclustra_pci_ids);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Vladimir Sitnov");
