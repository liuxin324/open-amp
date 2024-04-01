/*-
 * Copyright (c) 2011, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <openamp/virtio.h>

static const char *virtio_feature_name(unsigned long feature,
				       const struct virtio_feature_desc *);

/*
 * TODO :
 * This structure may change depending on the types of devices we support.
 */
static const struct virtio_ident {
	unsigned short devid; 	 // 设备ID 唯一标识 VirtIO 设备的类型
	const char *name;   	 //这是一个指向字符串的指针，用于描述设备的名称
} virtio_ident_table[] = {   //该数组包含了多个 virtio_ident 结构体的实例，每个实例对应一种 VirtIO 设备类型
	{
	VIRTIO_ID_NETWORK, "Network"}, {
	VIRTIO_ID_BLOCK, "Block"}, {
	VIRTIO_ID_CONSOLE, "Console"}, {
	VIRTIO_ID_ENTROPY, "Entropy"}, {
	VIRTIO_ID_BALLOON, "Balloon"}, {
	VIRTIO_ID_IOMEMORY, "IOMemory"}, {
	VIRTIO_ID_SCSI, "SCSI"}, {
	VIRTIO_ID_9P, "9P Transport"}, {
	VIRTIO_ID_MAC80211_WLAN, "MAC80211 WLAN"}, {
	VIRTIO_ID_RPROC_SERIAL, "Remoteproc Serial"}, {
	VIRTIO_ID_GPU, "GPU"}, {
	VIRTIO_ID_INPUT, "Input"}, {
	VIRTIO_ID_VSOCK, "Vsock Transport"}, {
	VIRTIO_ID_SOUND, "Sound"}, {
	VIRTIO_ID_FS, "File System"}, {
	VIRTIO_ID_MAC80211_HWSIM, "MAC80211 HWSIM"}, {
	VIRTIO_ID_I2C_ADAPTER, "I2C Adapter"}, {
	VIRTIO_ID_BT, "Bluetooth"}, {
	VIRTIO_ID_GPIO, "GPIO" }, {
	0, NULL}  //数组的最后一个元素 {0, NULL} 作为哨兵值，标识数组的结束
};

/* Device independent features. */
/* VirtIO 设备的一些通用特性描述 */
static const struct virtio_feature_desc virtio_common_feature_desc[] = {
	{VIRTIO_F_NOTIFY_ON_EMPTY, "NotifyOnEmpty"}, //表示设备在其队列变空时（即，所有的缓冲区都已被设备处理）将会发送通知，即使之前对中断进行了抑制
	{VIRTIO_RING_F_INDIRECT_DESC, "RingIndirect"}, // 表示设备支持间接描述符
	{VIRTIO_RING_F_EVENT_IDX, "EventIdx"},  //表示设备支持通过事件索引来控制中断的发送
	{VIRTIO_F_BAD_FEATURE, "BadFeature"}, //用于开发和调试，帮助识别错误地协商了不应该被支持的特性

	{0, NULL}
};

/**
 * @brief Get the name of a virtio device.
 *		  根据 VirtIO 设备的 ID 返回该设备的名称
 * @param devid Id of the device.
 *
 * @return pointer to the device name string if found, otherwise null.
 */

const char *virtio_dev_name(unsigned short devid)
{
	const struct virtio_ident *ident;

	for (ident = virtio_ident_table; ident->name; ident++) {  //循环的继续条件是 ident->name 不为 NULL
		if (ident->devid == devid)
			return ident->name;
	}

	return NULL;
}
/**
 * @description: 查找和返回与给定特性值 val 相匹配的特性名称
 * @param {unsigned long} val  要查找名称的特性值
 * @param {virtio_feature_desc} *desc 
 * @return {*}
 */
static const char *virtio_feature_name(unsigned long val,const struct virtio_feature_desc *desc)
{
	int i, j;
	const struct virtio_feature_desc *descs[2] = { desc,
		virtio_common_feature_desc
	};

	for (i = 0; i < 2; i++) {
		if (!descs[i])
			continue;

		for (j = 0; descs[i][j].vfd_val != 0; j++) {
			if (val == descs[i][j].vfd_val)
				return descs[i][j].vfd_str;
		}
	}

	return NULL;
}

__deprecated void virtio_describe(struct virtio_device *dev, const char *msg,
				  uint32_t features, struct virtio_feature_desc *desc)
{
	(void)dev;
	(void)msg;
	(void)features;

	/* TODO: Not used currently - keeping it for future use*/
	virtio_feature_name(0, desc);
}

/**
 * @brief Create the virtio device virtqueue.
 *
 * @param vdev			Pointer to virtio device structure.  指向 virtio_device 结构的指针，代表要操作的 VirtIO 设备
 * @param flags			Create flag.  创建标志
 * @param nvqs			The virtqueue number. 要创建的 virtqueue 数量
 * @param names			Virtqueue names.  virtqueue 的名称
 * @param callbacks		Virtqueue callback functions. virtqueue 的回调函数，用于处理队列事件
 * @param callback_args	Virtqueue callback function arguments. virtqueue 回调函数参数
 *
 * @return 0 on success, otherwise error code.
 */

int virtio_create_virtqueues(struct virtio_device *vdev, unsigned int flags,
			     unsigned int nvqs, const char *names[],
			     vq_callback callbacks[], void *callback_args[])
{
	struct virtio_vring_info *vring_info; 
	struct vring_alloc_info *vring_alloc; 
	unsigned int num_vrings, i;
	int ret;
	(void)flags;
	// 1.检查 vdev 是否为 NULL
	if (!vdev)  //检查 vdev 是否为 NULL
		return -EINVAL;
	// 2.检查 vdev 结构中是否定义了 create_virtqueues 回调,如果定义了，直接调用该回调以便执行设备特定的队列创建逻辑
	if (vdev->func && vdev->func->create_virtqueues) {
		return vdev->func->create_virtqueues(vdev, flags, nvqs,
						     names, callbacks, callback_args);
	}
	// 3.如果没有定义 create_virtqueues 回调，比较请求创建的队列数量 (nvqs) 与设备支持的队列数量 (num_vrings)，如果大于，则返回错误
	num_vrings = vdev->vrings_num;
	if (nvqs > num_vrings)
		return ERROR_VQUEUE_INVLD_PARAM;
	/* Initialize virtqueue for each vring */
	/* 初始化每个虚拟队列 */
	/*通过索引 i 访问 vdev->vrings_info 数组，获取指向当前队列配置信息的指针。vrings_info 数组中的每个元素都是 virtio_vring_info 结构体，包含了队列的详细配置信息*/
	for (i = 0; i < nvqs; i++) { 
		vring_info = &vdev->vrings_info[i]; 
	//从 vring_info 结构体中取出队列的分配信息，准备用于创建队列。分配信息包括了队列的大小、对齐方式等重要参数
		vring_alloc = &vring_info->info;
#ifndef VIRTIO_DEVICE_ONLY  
		if (vdev->role == VIRTIO_DEV_DRIVER) {// 如果设备角色是驱动（VIRTIO_DEV_DRIVER），则对应的内存区域将被清零初始化 -宿主机
			size_t offset;  
			struct metal_io_region *io = vring_info->io; // 获取队列内存区域的 I/O 区域指针

			offset = metal_io_virt_to_offset(io,vring_alloc->vaddr);// 将虚拟地址转换为在 I/O 区域内的偏移量
			metal_io_block_set(io, offset, 0,vring_size(vring_alloc->num_descs,vring_alloc->align));//清零指定的内存区域，以确保队列在使用前处于已知的初始状态
		}
#endif
		/*调用 virtqueue_create 函数创建并初始化队列。这个函数需要设备指针、队列索引、队列名称、分配信息、回调函数、通知函数和队列指针作为参数*/
		ret = virtqueue_create(vdev, i, names[i], vring_alloc,
				       callbacks[i], vdev->func->notify,
				       vring_info->vq);
		if (ret)
			return ret;
	}
	return 0;
}

