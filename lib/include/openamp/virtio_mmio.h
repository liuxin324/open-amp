/*
 * Copyright (c) 2022 Wind River Systems, Inc.
 * Based on Virtio PCI driver by Anthony Liguori, copyright IBM Corp. 2007
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef OPENAMP_VIRTIO_MMIO_H
#define OPENAMP_VIRTIO_MMIO_H

#include <metal/device.h>
#include <openamp/virtio.h>
#include <openamp/virtqueue.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Enable support for legacy devices */
#define VIRTIO_MMIO_LEGACY

/* Control registers -控制寄存器*/

/* Magic value ("virt" string) - Read Only */
/*一个固定的魔数值，用来验证内存映射的地址是否真的对应一个 VirtIO 设备。这个值应该读取为 ASCII 字符串 "virt"*/
#define VIRTIO_MMIO_MAGIC_VALUE		0x000

#define VIRTIO_MMIO_MAGIC_VALUE_STRING ('v' | ('i' << 8) | ('r' << 16) | ('t' << 24))

/* Virtio device version - Read Only */
/*VirtIO 设备的版本号，用于确定设备和驱动兼容的特性集*/
#define VIRTIO_MMIO_VERSION		0x004

/* Virtio device ID - Read Only */
/*标识设备类型的唯一ID。不同的值指示不同类型的设备（如网络、块设备等）*/
#define VIRTIO_MMIO_DEVICE_ID		0x008

/* Virtio vendor ID - Read Only */
/*设备制造商的唯一ID*/
#define VIRTIO_MMIO_VENDOR_ID		0x00c

/*
 * Bitmask of the features supported by the device (host)
 * (32 bits per set) - Read Only
 */
/* 设备（宿主）支持的特性位掩码，每组32位。这些特性包括各种优化和设备能力 */
#define VIRTIO_MMIO_DEVICE_FEATURES	0x010

/* Device (host) features set selector - Write Only */
/* 用于选择读取的特性位掩码集的寄存器 */
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL	0x014

/*
 * Bitmask of features activated by the driver (guest)
 * (32 bits per set) - Write Only
 */
/* 驱动（客户端）激活的特性位掩码，每组32位 */
#define VIRTIO_MMIO_DRIVER_FEATURES	0x020

/* Activated features set selector - Write Only */
/* 用于选择写入的特性位掩码集的寄存器 */
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL	0x024

#ifndef VIRTIO_MMIO_NO_LEGACY /* LEGACY DEVICES ONLY! */
/* Guest's memory page size in bytes - Write Only */
#define VIRTIO_MMIO_GUEST_PAGE_SIZE	0x028
#endif

/* Queue selector - Write Only */
/*选择当前操作的队列*/
#define VIRTIO_MMIO_QUEUE_SEL		0x030

/* Maximum size of the currently selected queue - Read Only */
/*当前选中队列的最大大小（即可容纳的描述符数）*/
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034

/* Queue size for the currently selected queue - Write Only */
/*设置当前选中队列的实际大小*/
#define VIRTIO_MMIO_QUEUE_NUM		0x038

#ifdef VIRTIO_MMIO_LEGACY
/* Used Ring alignment for the currently selected queue - Write Only */
/*设置当前选中队列的 Used Ring 对齐*/
#define VIRTIO_MMIO_QUEUE_ALIGN		0x03c
/* Guest's PFN for the currently selected queue - Read Write */
/*设置当前选中队列的页面帧号*/
#define VIRTIO_MMIO_QUEUE_PFN		0x040
#endif

/* Ready bit for the currently selected queue - Read Write */
/*标记当前选中队列为就绪状态*/
#define VIRTIO_MMIO_QUEUE_READY		0x044

/* Queue notifier - Write Only */
/*用于通知设备处理特定队列*/
#define VIRTIO_MMIO_QUEUE_NOTIFY	0x050

/* Interrupt status - Read Only */
/*中断状态寄存器，指示是否有未处理的中断*/
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060

/* Interrupt acknowledge - Write Only */
/*用于确认和清除中断*/
#define VIRTIO_MMIO_INTERRUPT_ACK	0x064

/* Device status register - Read Write */
/*设备状态寄存器，用于管理设备的生命周期*/
#define VIRTIO_MMIO_STATUS		0x070

/* Selected queue's Descriptor Table address, 64 bits in two halves */
/*指定队列描述符表的低/高32位地址*/
#define VIRTIO_MMIO_QUEUE_DESC_LOW	0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH	0x084

/* Selected queue's Available Ring address, 64 bits in two halves */
/*指定队列可用环的低/高32位地址*/
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW	0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH	0x094

/* Selected queue's Used Ring address, 64 bits in two halves */
/*指定队列已用环的低/高32位地址*/
#define VIRTIO_MMIO_QUEUE_USED_LOW	0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH	0x0a4

/* Shared memory region id */
/*选择当前操作的共享内存区域*/
#define VIRTIO_MMIO_SHM_SEL             0x0ac

/* Shared memory region length, 64 bits in two halves */
/*这两个共同定义了选定共享内存区域的长度，使用 64 位值以支持大内存区域*/
#define VIRTIO_MMIO_SHM_LEN_LOW         0x0b0
#define VIRTIO_MMIO_SHM_LEN_HIGH        0x0b4

/* Shared memory region base address, 64 bits in two halves */
/*这两个共同定义了选定共享内存区域的基地址，使用 64 位地址以支持大地址空间*/
#define VIRTIO_MMIO_SHM_BASE_LOW        0x0b8
#define VIRTIO_MMIO_SHM_BASE_HIGH       0x0bc

/* Configuration atomicity value */
/*配置空间的原子性值，用于确保配置空间的读取操作是一致的*/
#define VIRTIO_MMIO_CONFIG_GENERATION	0x0fc

/*
 * The config space is defined by each driver as
 * the per-driver configuration space - Read Write
 */
/*指定设备特定的配置空间开始的地址。每个驱动程序定义其自己的配置空间来存储设备特定的配置信息*/
#define VIRTIO_MMIO_CONFIG		0x100

/* Interrupt flags (re: interrupt status & acknowledge registers) */
/*指示与虚拟队列（vring）相关的中断。当设备完成数据处理，并且数据准备好被驱动程序收回时，此中断被触发*/
#define VIRTIO_MMIO_INT_VRING		(1 << 0)
/*指示与设备配置更改相关的中断。当设备的配置空间发生更改，需要驱动程序注意时，此中断被触发*/
#define VIRTIO_MMIO_INT_CONFIG		(1 << 1)

/* Data buffer size for preallocated buffers before vring */
/*定义在虚拟队列（vring）之前预分配的缓冲区的最大数据大小。这个值可能用于优化数据传输的内存分配*/
#define VIRTIO_MMIO_MAX_DATA_SIZE 128

/** @brief VIRTIO MMIO memory area */
/*VirtIO MMIO 设备相关的内存区域*/
struct virtio_mmio_dev_mem {
	/** Memory region physical address */
	void *base; // 指向内存区域物理地址的指针。这是内存映射的基础地址，设备的 MMIO 寄存器可以通过这个地址访问

	/** Memory region size */
	size_t size; // 内存区域的大小
};

/** @brief A VIRTIO MMIO device */
/*一个 VirtIO MMIO 设备，包含了设备所需的所有配置信息*/
struct virtio_mmio_device {
	/** Base virtio device structure */
	/*基础 VirtIO 设备结构。这是所有 VirtIO 设备共有的基本配置，包括功能标志、队列信息等*/
	struct virtio_device vdev;

	/** Device configuration space metal_io_region */
	/*分别指向设备配置空间和共享内存空间的 metal_io_region 结构体的指针*/
	struct metal_io_region *cfg_io;

	/** Pre-shared memory space metal_io_region */
	struct metal_io_region *shm_io;

	/** Shared memory device */
	/*表示与设备共享的内存设备。这是对共享内存硬件资源的抽象，允许设备和驱动程序之间高效地共享大量数据*/
	struct metal_device shm_device;

	/** VIRTIO device configuration space */
	/*分别表示设备的配置空间和预共享内存的 virtio_mmio_dev_mem 结构体实例*/
	struct virtio_mmio_dev_mem cfg_mem;

	/** VIRTIO device pre-shared memory */

	struct virtio_mmio_dev_mem shm_mem;

	/** VIRTIO_DEV_DRIVER or VIRTIO_DEV_DEVICE */
	/*指示设备是作为 VirtIO 设备端还是驱动端工作。它可以是 VIRTIO_DEV_DRIVER 或 VIRTIO_DEV_DEVICE，用于区分设备的角色*/
	unsigned int device_mode;

	/** Interrupt number */
	// 设备的中断号
	unsigned int irq;

	/** Custom user data */
	/*指向用户自定义数据的指针。这允许开发者将额外的数据或配置与 VirtIO MMIO 设备关联，为设备的使用和管理提供更大的灵活性*/
	void *user_data;
};

/**
 * @brief Register a VIRTIO device with the VIRTIO stack.
 *
 * @param dev		Pointer to device structure.
 * @param vq_num	Number of virtqueues the device uses.
 * @param vqs		Array of pointers to vthe virtqueues used by the device.
 */
void virtio_mmio_register_device(struct virtio_device *vdev, int vq_num, struct virtqueue **vqs);

/**
 * @brief Setup a virtqueue structure.
 *
 * @param dev		Pointer to device structure.
 * @param idx		Index of the virtqueue.
 * @param vq		Pointer to virtqueue structure.
 * @param cb		Pointer to virtqueue callback. Can be NULL.
 * @param cb_arg	Argument for the virtqueue callback.
 *
 * @return pointer to virtqueue structure.
 */
struct virtqueue *virtio_mmio_setup_virtqueue(struct virtio_device *vdev,
					      unsigned int idx,
					      struct virtqueue *vq,
					      void (*cb)(void *),
					      void *cb_arg,
					      const char *vq_name);

/**
 * @brief VIRTIO MMIO device initialization.
 *
 * @param vmdev			Pointer to virtio_mmio_device structure.
 * @param virt_mem_ptr	Guest virtio (shared) memory base address (virtual).
 * @param cfg_mem_ptr	Virtio device configuration memory base address (virtual).
 * @param user_data		Pointer to custom user data.
 *
 * @return int 0 for success.
 */
int virtio_mmio_device_init(struct virtio_mmio_device *vmdev, uintptr_t virt_mem_ptr,
			    uintptr_t cfg_mem_ptr, void *user_data);

/**
 * @brief VIRTIO MMIO interrupt service routine.
 *
 * @param vdev Pointer to virtio_device structure.
 */
void virtio_mmio_isr(struct virtio_device *vdev);

#ifdef __cplusplus
}
#endif

#endif /* OPENAMP_VIRTIO_MMIO_H */
