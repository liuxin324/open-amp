/*
 * Copyright (c) 2022 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <metal/device.h>
#include <openamp/open_amp.h>
#include <openamp/virtio.h>
#include <openamp/virtio_mmio.h>
#include <openamp/virtqueue.h>
#include <stdbool.h>

void virtio_mmio_isr(struct virtio_device *vdev);

typedef void (*virtio_mmio_vq_callback)(void *);

static int virtio_mmio_create_virtqueues(struct virtio_device *vdev, unsigned int flags,
					 unsigned int nvqs, const char *names[],
					 vq_callback callbacks[], void *callback_args[]);

/**
 * @description: 用于向指定的 MMIO 寄存器写入一个 32 位的值
 * @param {virtio_device} *vdev 一个指向 VirtIO 设备的指针
 * @param {int} offset  要写入的寄存器的偏移量
 * @param {uint32_t} value 要写入的值
 * @return {*}
 */
static inline void virtio_mmio_write32(struct virtio_device *vdev, int offset, uint32_t value)
{
	struct virtio_mmio_device *vmdev = metal_container_of(vdev,
							      struct virtio_mmio_device, vdev);

	metal_io_write32(vmdev->cfg_io, offset, value);
}
/**
 * @description: 用于从指定的 MMIO 寄存器读取一个 32 位的值
 * @param {virtio_device} *vdev
 * @param {int} offset
 * @return {*}
 */
static inline uint32_t virtio_mmio_read32(struct virtio_device *vdev, int offset)
{
	struct virtio_mmio_device *vmdev = metal_container_of(vdev,
							      struct virtio_mmio_device, vdev);

	return metal_io_read32(vmdev->cfg_io, offset);
}

static inline uint8_t virtio_mmio_read8(struct virtio_device *vdev, int offset)
{
	struct virtio_mmio_device *vmdev = metal_container_of(vdev,
							      struct virtio_mmio_device, vdev);

	return metal_io_read8(vmdev->cfg_io, offset);
}
/**
 * @description: 设置 VirtIO 设备的状态寄存器
 * @param {virtio_device} *vdev
 * @param {uint8_t} status
 * @return {*}
 */
static inline void virtio_mmio_set_status(struct virtio_device *vdev, uint8_t status)
{
	virtio_mmio_write32(vdev, VIRTIO_MMIO_STATUS, status);
}
/**
 * @description: 获取 VirtIO 设备的当前状态
 * @param {virtio_device} *vdev
 * @return {*}
 */
static uint8_t virtio_mmio_get_status(struct virtio_device *vdev)
{
	return virtio_mmio_read32(vdev, VIRTIO_MMIO_STATUS);
}
/**
 * @description: 将数据写入 VirtIO 设备的配置空间，但在这个实现中，它不执行任何操作，只是打印一条不支持的警告信息
 * @param {virtio_device} *vdev
 * @param {uint32_t} offset
 * @param {void} *dst
 * @param {int} length
 * @return {*}
 */
static void virtio_mmio_write_config(struct virtio_device *vdev,uint32_t offset, void *dst, int length)
{
	(void)(vdev);
	(void)(offset);
	(void)(dst);
	(void)length;

	metal_log(METAL_LOG_WARNING, "%s not supported\n", __func__);
}
/**
 * @description: 此函数从 VirtIO 设备的配置空间读取数据
 * @param {virtio_device} *vdev
 * @param {uint32_t} offset
 * @param {void} *dst
 * @param {int} length  连续读取 length 字节的数据到 dst 指向的缓冲区
 * @return {*}
 */
static void virtio_mmio_read_config(struct virtio_device *vdev,uint32_t offset, void *dst, int length)
{
	int i;
	uint8_t *d = dst;
	(void)(offset);

	for (i = 0; i < length; i++)
		d[i] = virtio_mmio_read8(vdev, VIRTIO_MMIO_CONFIG + i);
}
/**
 * @description: 内部函数-用于查询 VirtIO 设备支持的特性集
 * @param {virtio_device} *vdev
 * @param {int} idx
 * @return {*}
 */
static uint32_t _virtio_mmio_get_features(struct virtio_device *vdev, int idx)
{
	uint32_t hfeatures;

	/* Writing selection register VIRTIO_MMIO_DEVICE_FEATURES_SEL. In pure AMP
	 * mode this needs to be followed by a synchronization w/ the device
	 * before reading VIRTIO_MMIO_DEVICE_FEATURES
	 */
	/* 写入选择寄存器VIRTIO_MMIO_DEVICE_FEATURES_SEL。 在纯 AMP 模式下，在读取 VIRTIO_MMIO_DEVICE_FEATURES 之前需要先与设备同步 */
	/*通过 VIRTIO_MMIO_DEVICE_FEATURES_SEL 寄存器设置要查询的特性集索引（idx）*/
	virtio_mmio_write32(vdev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, idx);
	/*VIRTIO_MMIO_DEVICE_FEATURES 寄存器读取设备支持的特性，并与设备当前激活的特性 (vdev->features) 进行按位与操作，以确定双方都支持的特性集*/
	hfeatures = virtio_mmio_read32(vdev, VIRTIO_MMIO_DEVICE_FEATURES);
	return hfeatures & vdev->features;
}
/**
 * @description: 此函数是获取设备支持特性的公共接口，它调用_virtio_mmio_get_features 函数查询索引为 0 的特性集，即主特性集
 * @param {virtio_device} *vdev
 * @return {*}
 */
static uint32_t virtio_mmio_get_features(struct virtio_device *vdev)
{
	return _virtio_mmio_get_features(vdev, 0);
}

/* This is more like negotiate_features */
/**
 * @description: 这是一个内部函数，用于设置驱动程序希望激活的特性.
 * @param {virtio_device} *vdev
 * @param {uint32_t} features
 * @param {int} idx
 * @return {*}
 */
static void _virtio_mmio_set_features(struct virtio_device *vdev,uint32_t features, int idx)
{
	uint32_t hfeatures;

	/* Writing selection register VIRTIO_MMIO_DEVICE_FEATURES_SEL. In pure AMP
	 * mode this needs to be followed by a synchronization w/ the device
	 * before reading VIRTIO_MMIO_DEVICE_FEATURES
	 */
	/*通过 VIRTIO_MMIO_DEVICE_FEATURES_SEL 寄存器指定特性集索引，然后读取设备支持的特性，
	将这些特性与驱动程序希望激活的特性 (features) 进行按位与操作，
	以确定最终要激活的特性集。之后，通过 VIRTIO_MMIO_DRIVER_FEATURES 寄存器写入这个特性集*/
	virtio_mmio_write32(vdev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, idx);
	hfeatures = virtio_mmio_read32(vdev, VIRTIO_MMIO_DEVICE_FEATURES);
	features &= hfeatures;
	virtio_mmio_write32(vdev, VIRTIO_MMIO_DRIVER_FEATURES, features);
	vdev->features = features;
}

static void virtio_mmio_set_features(struct virtio_device *vdev, uint32_t features)
{
	_virtio_mmio_set_features(vdev, features, 0);
}
/**
 * @description: 此函数用于重置 VirtIO 设备。它通过将 0 写入 VIRTIO_MMIO_STATUS 寄存器来实现设备的重置操作，清除所有先前的状态和配置
 * @param {virtio_device} *vdev
 * @return {*}
 */
static void virtio_mmio_reset_device(struct virtio_device *vdev)
{
	virtio_mmio_set_status(vdev, 0);
}
/**
 * @description: 于通知设备某个虚拟队列 (virtqueue) 上有新的任务需要处理。它通过将队列索引 (vq->vq_queue_index) 写入 VIRTIO_MMIO_QUEUE_NOTIFY 寄存器来实现
 * @param {virtqueue} *vq
 * @return {*}
 */
static void virtio_mmio_notify(struct virtqueue *vq)
{
	/* VIRTIO_F_NOTIFICATION_DATA is not supported for now */
	virtio_mmio_write32(vq->vq_dev, VIRTIO_MMIO_QUEUE_NOTIFY, vq->vq_queue_index);
}
/*提供了一个函数指针表，这些函数指针定义了驱动程序与VirtIO MMIO设备交互所需的一系列操作。
这允许驱动程序在运行时调用这些函数来执行特定的任务，
例如创建虚拟队列、获取和设置设备状态、读写设备配置、重置设备以及通知设备处理新的任务*/
const struct virtio_dispatch virtio_mmio_dispatch = {
	.create_virtqueues = virtio_mmio_create_virtqueues,
	.get_status = virtio_mmio_get_status,
	.set_status = virtio_mmio_set_status,
	.get_features = virtio_mmio_get_features,
	.set_features = virtio_mmio_set_features,
	.read_config = virtio_mmio_read_config,
	.write_config = virtio_mmio_write_config,
	.reset_device = virtio_mmio_reset_device,
	.notify = virtio_mmio_notify,
};
/**
 * @description: 初始化并获取用于访问VirtIO MMIO设备的metal_io_region结构体。这些结构体提供了物理内存映射到虚拟地址空间的抽象，使得驱动程序可以通过虚拟地址操作设备的寄存器和共享内存
 * @param {virtio_device} *vdev
 * @param {uintptr_t} virt_mem_ptr
 * @param {uintptr_t} cfg_mem_ptr
 * @return {*}
 */
static int virtio_mmio_get_metal_io(struct virtio_device *vdev, uintptr_t virt_mem_ptr,uintptr_t cfg_mem_ptr)
{
	struct metal_device *device;
	int32_t err;
	struct virtio_mmio_device *vmdev = metal_container_of(vdev,
							      struct virtio_mmio_device, vdev);

	/* Setup shared memory device */
	/* 首先设置共享内存（用于数据传输）和设备配置空间（用于存储设备特定的配置信息）的物理地址、虚拟地址和大小 */
	vmdev->shm_device.regions[0].physmap = (metal_phys_addr_t *)&vmdev->shm_mem.base;
	vmdev->shm_device.regions[0].virt = (void *)virt_mem_ptr;
	vmdev->shm_device.regions[0].size = vmdev->shm_mem.size;

	VIRTIO_ASSERT((METAL_MAX_DEVICE_REGIONS > 1),
		      "METAL_MAX_DEVICE_REGIONS must be greater that 1");

	vmdev->shm_device.regions[1].physmap = (metal_phys_addr_t *)&vmdev->cfg_mem.base;
	vmdev->shm_device.regions[1].virt = (void *)cfg_mem_ptr;
	vmdev->shm_device.regions[1].size = vmdev->cfg_mem.size;
	/*通过调用metal_register_generic_device函数注册一个通用设备，并随后通过metal_device_open打开该设备，这使得驱动程序能够访问由Metal库提供的设备I/O服务*/
	err = metal_register_generic_device(&vmdev->shm_device);
	if (err) {
		metal_log(METAL_LOG_ERROR, "Couldn't register shared memory device: %d\n", err);
		return err;
	}

	err = metal_device_open("generic", vmdev->shm_device.name, &device);
	if (err) {
		metal_log(METAL_LOG_ERROR, "metal_device_open failed: %d", err);
		return err;
	}
	/*利用metal_device_io_region函数获取共享内存和配置空间对应的I/O区域（metal_io_region结构体）。这些I/O区域用于后续的内存访问操作，例如读取或写入配置数据*/
	vmdev->shm_io = metal_device_io_region(device, 0);
	if (!vmdev->shm_io) {
		metal_log(METAL_LOG_ERROR, "metal_device_io_region failed to get region 0");
		return err;
	}

	vmdev->cfg_io = metal_device_io_region(device, 1);
	if (!vmdev->cfg_io) {
		metal_log(METAL_LOG_ERROR, "metal_device_io_region failed to get region 1");
		return err;
	}

	return 0;
}
/**
 * @description: 查询指定虚拟队列（virtqueue）所能支持的最大元素（即描述符）数量
 * @param {virtio_device} *vdev
 * @param {int} idx
 * @return {*}
 */
uint32_t virtio_mmio_get_max_elem(struct virtio_device *vdev, int idx)
{
	/* Select the queue we're interested in by writing selection register
	 * VIRTIO_MMIO_QUEUE_SEL. In pure AMP mode this needs to be followed by a
	 * synchronization w/ the device before reading VIRTIO_MMIO_QUEUE_NUM_MAX
	 */
	/*它是通过写入特定的队列选择寄存器VIRTIO_MMIO_QUEUE_SEL来指定想要查询的队列，然后从VIRTIO_MMIO_QUEUE_NUM_MAX寄存器读取该队列能够支持的最大元素数量。*/
	virtio_mmio_write32(vdev, VIRTIO_MMIO_QUEUE_SEL, idx);
	return virtio_mmio_read32(vdev, VIRTIO_MMIO_QUEUE_NUM_MAX);
}

/**
 * @brief VIRTIO MMIO device initialization.
 *	初始化VirtIO MMIO设备
 * @param vmdev			Pointer to virtio_mmio_device structure. 指向virtio_mmio_device结构体的指针
 * @param virt_mem_ptr	Guest virtio (shared) memory base address (virtual).  指向虚拟内存（共享内存）基地址的指针（以虚拟地址的形式给出）
 * @param cfg_mem_ptr	Virtio device configuration memory base address (virtual). Virtio设备配置内存基地址（虚拟地址）
 * @param user_data		Pointer to custom user data. 指向自定义用户数据的指针
 *
 * @return int 0 for success.
 */
int virtio_mmio_device_init(struct virtio_mmio_device *vmdev, uintptr_t virt_mem_ptr,
			    uintptr_t cfg_mem_ptr, void *user_data)
{
	// 1.设定设备角色和用户数据
	struct virtio_device *vdev = &vmdev->vdev;
	uint32_t magic, version, devid, vendor;

	vdev->role = vmdev->device_mode;
	vdev->priv = vmdev;
	vdev->func = &virtio_mmio_dispatch;
	vmdev->user_data = user_data;

	/* Set metal io mem ops */
	// 2.配置内存访问操作
	virtio_mmio_get_metal_io(vdev, virt_mem_ptr, cfg_mem_ptr);
	// 3.检查魔数（Magic Value）
	/*读取VIRTIO_MMIO_MAGIC_VALUE寄存器并验证其值是否为"virt"字符串的ASCII编码*/
	magic = virtio_mmio_read32(vdev, VIRTIO_MMIO_MAGIC_VALUE);
	if (magic != VIRTIO_MMIO_MAGIC_VALUE_STRING) {
		metal_log(METAL_LOG_ERROR, "Bad magic value %08x\n", magic);
		return -1;
	}
	// 4.读取设备版本号、设备ID和供应商ID
	version = virtio_mmio_read32(vdev, VIRTIO_MMIO_VERSION);
	devid = virtio_mmio_read32(vdev, VIRTIO_MMIO_DEVICE_ID);
	if (devid == 0) {
		/* Placeholder */
		return -1;
	}

	if (version != 1) {
		metal_log(METAL_LOG_ERROR, "Bad version %08x\n", version);
		return -1;
	}

	vendor = virtio_mmio_read32(vdev, VIRTIO_MMIO_VENDOR_ID);
	metal_log(METAL_LOG_DEBUG, "VIRTIO %08x:%08x\n", vendor, devid);

	vdev->id.version = version;
	vdev->id.device = devid;
	vdev->id.vendor = vendor;
	// 5.设置设备状态-VIRTIO_CONFIG_STATUS_ACK，表示驱动已识别设备
	virtio_mmio_set_status(vdev, VIRTIO_CONFIG_STATUS_ACK);
	// 6.设置页大小-4k
	virtio_mmio_write32(vdev, VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);

	return 0;
}

/**
 * @brief Register a VIRTIO device with the VIRTIO stack. 向 VIRTIO 堆栈注册 VIRTIO 设备
 *	将一个VirtIO设备注册到VirtIO框架中
 * @param dev		Pointer to device structure.
 * @param vq_num	Number of virtqueues the device uses.
 * @param vqs		Array of pointers to vthe virtqueues used by the device.
 * 指向virtqueue指针数组的指针，数组中的每个元素都是指向一个virtqueue结构体的指针。这个数组列出了设备使用的所有virtqueues
 */

void virtio_mmio_register_device(struct virtio_device *vdev, int vq_num, struct virtqueue **vqs)
{
	int i;
	/*根据传入的virtqueue数量vq_num分配足够的内存来存储virtqueue的信息（即virtio_vring_info结构体数组）*/
	vdev->vrings_info = metal_allocate_memory(sizeof(struct virtio_vring_info) * vq_num);
	/* TODO: handle error case */
	for (i = 0; i < vq_num; i++) {
		vdev->vrings_info[i].vq = vqs[i];
	}
	//更新vdev结构体中记录virtqueue数量的字段vrings_num，以反映设备使用的实际virtqueue数量
	vdev->vrings_num = vq_num;
}

/**
 * @brief Setup a virtqueue structure.
 * 配置并初始化指定的virtqueue（虚拟队列）
 * @param dev		Pointer to device structure. 指向VirtIO设备的指针，表示当前正在设置virtqueue的设备
 * @param idx		Index of the virtqueue. 要设置的virtqueue的索引。在设备中，每个virtqueue都有一个唯一的索引号
 * @param vq		Pointer to virtqueue structure. 指向预分配的virtqueue结构的指针。这里期望virtqueue已经被创建但尚未配置
 * @param cb		Pointer to virtqueue callback. Can be NULL. virtqueue的回调函数。当virtqueue有新的数据可用时，将调用此函数
 * @param cb_arg	Argument for the virtqueue callback. 回调函数的参数
 * @param vq_name   virtqueue的名称，用于标识和调试
 * @return pointer to virtqueue structure.
 */

struct virtqueue *virtio_mmio_setup_virtqueue(struct virtio_device *vdev,
					      unsigned int idx,
					      struct virtqueue *vq,
					      void (*cb)(void *),
					      void *cb_arg,
					      const char *vq_name)
{
	uint32_t maxq;
	struct virtio_vring_info _vring_info = {0};
	struct virtio_vring_info *vring_info = &_vring_info;
	struct vring_alloc_info *vring_alloc_info;
	struct virtio_mmio_device *vmdev = metal_container_of(vdev,
							      struct virtio_mmio_device, vdev);
	// 1.检查设备角色: 函数首先验证vdev的角色是否为VIRTIO_DEV_DRIVER（即设备驱动程序），目前只支持这种模式
	if (vdev->role != (unsigned int)VIRTIO_DEV_DRIVER) {
		metal_log(METAL_LOG_ERROR, "Only VIRTIO_DEV_DRIVER is currently supported\n");
		return NULL;
	}

	if (!vq) {
		metal_log(METAL_LOG_ERROR,
			  "Only preallocated virtqueues are currently supported\n");
		return NULL;
	}
	//检查VirtIO MMIO版本: 确保设备使用的是支持的VirtIO MMIO协议版本（版本1）
	if (vdev->id.version != 0x1) {
		metal_log(METAL_LOG_ERROR,
			  "Only VIRTIO MMIO version 1 is currently supported\n");
		return NULL;
	}

	vring_info->io = vmdev->shm_io;
	vring_info->info.num_descs = virtio_mmio_get_max_elem(vdev, idx);// 获取设备支持的最大virtqueue元素数
	vring_info->info.align = VIRTIO_MMIO_VRING_ALIGNMENT;

	/* Check if vrings are already configured */
	if (vq->vq_nentries != 0 && vq->vq_nentries == vq->vq_free_cnt &&
	    vq->vq_ring.desc) {
		vring_info->info.vaddr = vq->vq_ring.desc;
		vring_info->vq = vq;
	}
	vring_info->info.num_descs = vq->vq_nentries;

	vq->vq_dev = vdev;

	vring_alloc_info = &vring_info->info;

	unsigned int role_bk = vdev->role;
	/* Assign OA VIRTIO_DEV_DRIVER role to allow virtio guests to setup the vrings */
	// 为了设置virtqueue，将设备角色临时设置为VIRTIO_DEV_DRIVER
	vdev->role = (unsigned int)VIRTIO_DEV_DRIVER;
	// 创建virtqueue: 调用virtqueue_create函数，传入virtqueue配置信息，创建并初始化virtqueue
	if (virtqueue_create(vdev, idx, vq_name, vring_alloc_info, (void (*)(struct virtqueue *))cb,
			     vdev->func->notify, vring_info->vq)) {
		metal_log(METAL_LOG_ERROR, "virtqueue_create failed\n");
		return NULL;
	}
	vdev->role = role_bk;//恢复设备角色
	vq->priv = cb_arg;
	virtqueue_set_shmem_io(vq, vmdev->shm_io);// 设置共享内存IO区域: 为virtqueue设置共享内存IO区域，以便virtqueue可以访问共享内存

	/* Writing selection register VIRTIO_MMIO_QUEUE_SEL. In pure AMP
	 * mode this needs to be followed by a synchronization w/ the device
	 * before reading VIRTIO_MMIO_QUEUE_NUM_MAX
	 */
	virtio_mmio_write32(vdev, VIRTIO_MMIO_QUEUE_SEL, idx);
	maxq = virtio_mmio_read32(vdev, VIRTIO_MMIO_QUEUE_NUM_MAX);
	VIRTIO_ASSERT((maxq != 0),
		      "VIRTIO_MMIO_QUEUE_NUM_MAX cannot be 0");
	VIRTIO_ASSERT((maxq >= vq->vq_nentries),
		      "VIRTIO_MMIO_QUEUE_NUM_MAX must be greater than vqueue->vq_nentries");
	virtio_mmio_write32(vdev, VIRTIO_MMIO_QUEUE_NUM, vq->vq_nentries);
	virtio_mmio_write32(vdev, VIRTIO_MMIO_QUEUE_ALIGN, 4096);
	virtio_mmio_write32(vdev, VIRTIO_MMIO_QUEUE_PFN,
			    ((uintptr_t)metal_io_virt_to_phys(vq->shm_io,
			    (char *)vq->vq_ring.desc)) / 4096);

	vdev->vrings_info[vdev->vrings_num].vq = vq;
	vdev->vrings_num++;
	virtqueue_enable_cb(vq);// 启用virtqueue回调

	return vq;
}

/**
 * @brief VIRTIO MMIO interrupt service routine.
 * VirtIO MMIO设备的中断服务程序(ISR)，它是硬件中断的响应函数。当VirtIO设备产生中断时，该函数被调用
 * @param vdev Pointer to virtio_device structure.
 */
void virtio_mmio_isr(struct virtio_device *vdev)
{
	struct virtio_vring_info *vrings_info = vdev->vrings_info;
	// 读取中断状态: 通过读取VIRTIO_MMIO_INTERRUPT_STATUS寄存器来获取当前的中断状态
	uint32_t isr = virtio_mmio_read32(vdev, VIRTIO_MMIO_INTERRUPT_STATUS);
	struct virtqueue *vq;
	unsigned int i;
	// 处理Vring中断: 如果中断状态表明是Vring相关的中断(VIRTIO_MMIO_INT_VRING标志被设置)，则遍历所有virtqueue，并对于每个有注册回调函数的virtqueue，调用其回调函数
	if (isr & VIRTIO_MMIO_INT_VRING) {
		for (i = 0; i < vdev->vrings_num; i++) {
			vq = vrings_info[i].vq;
			if (vq->callback)
				vq->callback(vq->priv);
		}
	}

	if (isr & ~(VIRTIO_MMIO_INT_VRING)) //处理未知中断: 如果存在未处理的中断类型，输出警告信息
		metal_log(METAL_LOG_WARNING, "Unhandled interrupt type: 0x%x\n", isr);
	// 清除中断状态: 通过写入VIRTIO_MMIO_INTERRUPT_ACK寄存器来确认并清除已处理的中断
	virtio_mmio_write32(vdev, VIRTIO_MMIO_INTERRUPT_ACK, isr);
}
/**
 * @description: 为VirtIO设备创建一组virtqueues
 * @param {virtio_device} *vdev 指向VirtIO设备的指针
 * @param {unsigned int} flags 创建virtqueues时使用的标志
 * @param {unsigned int} nvqs 需要创建的virtqueues数量
 * @param {char} *names 各virtqueue的名称数组，用于标识和调试
 * @param {vq_callback} callbacks 各virtqueue接收到消息时调用的回调函数数组
 * @param {void} *callback_args 传递给回调函数的参数数组
 * @return {*}
 */
static int virtio_mmio_create_virtqueues(struct virtio_device *vdev, unsigned int flags,unsigned int nvqs, const char *names[],vq_callback callbacks[], void *callback_args[])
{
	struct virtqueue *vq;
	struct virtqueue *vring_vq;
	void (*cb)(void *);
	void *cb_arg;
	unsigned int i;

	(void)flags;
	// 参数校验: 检查传入的参数是否有效，确保vdev、names和vrings_info非空
	if (!vdev || !names || !vdev->vrings_info)
		return -EINVAL;

	for (i = 0; i < nvqs; i++) {
		vring_vq = NULL;
		cb = NULL;
		cb_arg = NULL;
		if (vdev->vrings_info[i].vq) //如果vdev->vrings_info[i].vq存在，则使用已有的virtqueue
			vring_vq = vdev->vrings_info[i].vq;
		if (callbacks) // 如果提供了callbacks和callback_args，则分别设置为virtqueue的回调函数和回调参数
			cb = (virtio_mmio_vq_callback)callbacks[i];
		if (callback_args)
			cb_arg = callback_args[i];
		//调用virtio_mmio_setup_virtqueue为每个virtqueue进行配置和初始化
		vq = virtio_mmio_setup_virtqueue(vdev, i, vring_vq, cb, cb_arg, names[i]);
		if (!vq)
			return -ENODEV;
	}

	return 0;
}
