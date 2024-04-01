/*
 * Copyright Rusty Russell IBM Corporation 2007.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $FreeBSD$
 */

#ifndef VIRTIO_RING_H
#define	VIRTIO_RING_H

#include <metal/compiler.h>

#if defined __cplusplus
extern "C" {
#endif

/* This marks a buffer as continuing via the next field. */
/*这个标志表示当前的描述符（buffer descriptor）不是链中的最后一个，它通过 next 字段指向下一个描述符。
这允许将多个描述符链接在一起，形成一个描述符链，用于表示较大的数据缓冲区或复杂的数据结构*/
#define VRING_DESC_F_NEXT       1
/* This marks a buffer as write-only (otherwise read-only). */
#define VRING_DESC_F_WRITE      2
/* This means the buffer contains a list of buffer descriptors. */
/*设置此标志表示当前描述符指向的是一个间接描述符表，而非直接指向数据。
间接描述符表允许一个单一的描述符引用多个数据缓冲区，
可以减少必须在 virtqueue 中直接管理的描述符数量，从而提高效率*/
#define VRING_DESC_F_INDIRECT   4

/* The Host uses this in used->flags to advise the Guest: don't kick me
 * when you add a buffer.  It's unreliable, so it's simply an
 * optimization.  Guest will still kick if it's out of buffers.
 */
/*这个标志用于宿主（Host）在 used 环的 flags 中设置，
以通知客户机（Guest）在添加缓冲区到 used 环时不需要通知宿主。这是一种优化，旨在减少中断的次数。
这种通知机制是不可靠的，如果客户机耗尽了可用缓冲区，它仍然会发送通知
*/
#define VRING_USED_F_NO_NOTIFY  1
/* The Guest uses this in avail->flags to advise the Host: don't
 * interrupt me when you consume a buffer.  It's unreliable, so it's
 * simply an optimization.
 */
/*这个标志由客户机（Guest）在 avail 环的 flags 中设置，以通知宿主（Host）在消费 avail 环中的缓冲区时不要产生中断。
与 VRING_USED_F_NO_NOTIFY 类似，这也是一种优化，用来减少中断，提高效率。
同样，这种优化是不可靠的，宿主可能在某些情况下仍然会产生中断*/
#define VRING_AVAIL_F_NO_INTERRUPT      1

/**
 * @brief VirtIO ring descriptors.
 *
 * The descriptor table refers to the buffers the driver is using for the
 * device. addr is a physical address, and the buffers can be chained via \ref next.
 * Each descriptor describes a buffer which is read-only for the device
 * (“device-readable”) or write-only for the device (“device-writable”), but a
 * chain of descriptors can contain both device-readable and device-writable
 * buffers.
 */
/**
  * @brief VirtIO vring 描述符。
  *
  * 描述符表指的是驱动程序正在使用的缓冲区设备。 
  * addr 是一个物理地址，缓冲区可以通过 \ref next 链接起来。
  * 每个描述符描述一个设备只读的缓冲区
  *（“设备可读”）或设备只写（“设备可写”）
  * 但是描述符链可以包含设备可读和设备可写缓冲区。
  */
METAL_PACKED_BEGIN
struct vring_desc {    // VirtIO 环中的描述符结构体，代表一个数据缓冲区
	/** Address (guest-physical) */
	/*缓冲区的物理地址（在客户机内存中）。这是设备访问数据的位置*/
	uint64_t addr;

	/** Length */
	uint32_t len;

	/** Flags relevant to the descriptors */
	/*描述符的标志，控制描述符的行为（如是否是写操作、是否链到另一个描述符等）*/
	uint16_t flags;

	/** We chain unused descriptors via this, too */
	/*如果描述符链中有多个描述符，next 字段指示下一个描述符的索引。这允许将多个小缓冲区链接成一个大的数据块*/
	uint16_t next;
} METAL_PACKED_END;

/**
 * @brief Used to offer buffers to the device.
 *
 * Each ring entry refers to the head of a descriptor chain. It is only
 * written by the driver and read by the device.
 */
/**
  * @brief 用于向设备提供缓冲区。
  *
  * 每个ring entry指的是描述符链的头部。 
  * 这只是由驱动程序写入并由设备读取。
  */
METAL_PACKED_BEGIN
struct vring_avail {
	/** Flag which determines whether device notifications are required */
	/*标志位，用于控制设备的通知行为（例如，是否需要向设备发送中断）*/
	uint16_t flags;

	/**
	 * Indicates where the driver puts the next descriptor entry in the
	 * ring (modulo the queue size)
	 */
	/*环索引，指示驱动程序将下一个描述符放置在环的哪个位置*/
	uint16_t idx;

	/** The ring of descriptors */
	/*一个包含描述符索引的数组，这些描述符索引指向 struct vring_desc 中的条目*/
	uint16_t ring[0];
} METAL_PACKED_END;

/* uint32_t is used here for ids for padding reasons. */
METAL_PACKED_BEGIN
/*used ring 中的元素，设备用它来返回处理完毕的缓冲区给驱动*/
struct vring_used_elem {
	union {
		uint16_t event;
		/* Index of start of used descriptor chain. */
		uint32_t id;
	};
	/* Total length of the descriptor chain which was written to. */
	uint32_t len;
} METAL_PACKED_END;

/**
 * @brief The device returns buffers to this structure when done with them
 *
 * The structure is only written to by the device, and read by the driver.
 */
METAL_PACKED_BEGIN
//设备通过它将完成的缓冲区返回给驱动程序
struct vring_used {
	/** Flag which determines whether device notifications are required */
	uint16_t flags;

	/**
	 * Indicates where the driver puts the next descriptor entry in the
	 * ring (modulo the queue size)
	 */
	uint16_t idx;

	/** The ring of descriptors */
	/*一个包含 struct vring_used_elem 的数组，表示已使用的缓冲区*/
	struct vring_used_elem ring[0];
} METAL_PACKED_END;

/**
 * @brief The virtqueue layout structure
 *
 * Each virtqueue consists of; descriptor table, available ring, used ring,
 * where each part is physically contiguous in guest memory.
 *
 * When the driver wants to send a buffer to the device, it fills in a slot in
 * the descriptor table (or chains several together), and writes the descriptor
 * index into the available ring. It then notifies the device. When the device
 * has finished a buffer, it writes the descriptor index into the used ring,
 * and sends an interrupt.
 *
 * The standard layout for the ring is a continuous chunk of memory which
 * looks like this.  We assume num is a power of 2.
 *
 * struct vring {
 *      // The actual descriptors (16 bytes each)
 *      struct vring_desc desc[num];
 *
 *      // A ring of available descriptor heads with free-running index.
 *      __u16 avail_flags;
 *      __u16 avail_idx;
 *      __u16 available[num];
 *      __u16 used_event_idx;
 *
 *      // Padding to the next align boundary.
 *      char pad[];
 *
 *      // A ring of used descriptor heads with free-running index.
 *      __u16 used_flags;
 *      __u16 used_idx;
 *      struct vring_used_elem used[num];
 *      __u16 avail_event_idx;
 * };
 *
 * NOTE: for VirtIO PCI, align is 4096.
 */

/**
  * @brief virtqueue布局结构
  *
  * 每个virtqueue包含： 描述符表、可用环、已用环，其中每个部分在客户内存中物理上是连续的。
  *
  * 当驱动程序想要向设备发送缓冲区时，它会填充描述符表中的一个槽（或将多个槽链接在一起），并将描述符索引写入可用环中。 
  *  然后它会通知设备。当设备完成缓冲区后，它将描述符索引写入已使用的环中，并发送中断。
  *
  * 环的标准布局是一个连续的内存块，如下所示。 我们假设 num 是 2 的幂。
  *
  * struct  vring{
  * // 实际描述符（每个16字节）
  * struct vring_desc desc[num];
  *
  * // 具有自由运行索引的可用描述符头环。
  * __u16 avail_flags
  * __u16 avail_idx;
  * __u16 available[num];
  * __u16 used_event_idx;
  *
  * // 填充到下一个对齐边界。
  * char pad[];
  *
  * // 具有自由运行索引的已用描述符头环。
  * __u16 used_flags;
  * __u16 used_idx;
  * struct vring_used_elem used[num]；
  * __u16 avail_event_idx;
  * };
  *
  * 注意：对于 VirtIO PCI，对齐为 4096。
  */
struct vring {
	/**
	 * The maximum number of buffer descriptors in the virtqueue.
	 * The value is always a power of 2.
	 */
	/*虚拟队列中缓冲区描述符的最大数量。该值总是 2 的幂*/
	unsigned int num;

	/** The actual buffer descriptors, 16 bytes each */
	/*实际的缓冲区描述符，每个描述符 16 字节*/
	struct vring_desc *desc;

	/** A ring of available descriptor heads with free-running index */
	/*可用描述符头的环，带有自由运行索引，驱动程序用它来提供缓冲区给设备*/
	struct vring_avail *avail;

	/** A ring of used descriptor heads with free-running index */
	//已使用描述符头的环，带有自由运行索引，设备用它来返回处理完成的缓冲区给驱动程序
	struct vring_used *used;
};

/*
 * We publish the used event index at the end of the available ring, and vice
 * versa. They are at the end for backwards compatibility.
 */
/*两个宏用于获取事件索引。VirtIO 通过事件索引机制（VIRTIO_RING_F_EVENT_IDX 特性）优化通知机制，减少中断*/
#define vring_used_event(vr)	((vr)->avail->ring[(vr)->num]) //获取可用环的事件索引，用于通知设备驱动不需要再检查已用环
#define vring_avail_event(vr)	((vr)->used->ring[(vr)->num].event) //取已用环的事件索引，用于通知设备不需要发送中断给驱动

/**
 * @description: 计算给定数量描述符的 VirtIO 环所需的内存大小。考
 * 虑到对齐要求，函数首先计算描述符表、可用环和已用环所需的大小，
 * 然后根据对齐参数调整总大小以确保整个 VirtIO 环在物理内存中正确对齐
 * @param {unsigned int} num
 * @param {unsigned long} align
 * @return {*}
 */
static inline int vring_size(unsigned int num, unsigned long align)
{
	int size;

	size = num * sizeof(struct vring_desc);
	size += sizeof(struct vring_avail) + (num * sizeof(uint16_t)) +
	    sizeof(uint16_t);
	size = (size + align - 1) & ~(align - 1);
	size += sizeof(struct vring_used) +
	    (num * sizeof(struct vring_used_elem)) + sizeof(uint16_t);

	return size;
}
/**
 * @description:  初始化 VirtIO 环
 * @param {vring} *vr
 * @param {unsigned int} num 描述符数量描述符数量
 * @param {uint8_t} *p 一个内存块的起始地址 
 * @param {unsigned long} align  
 * @return {*}
 */
static inline void vring_init(struct vring *vr, unsigned int num, uint8_t *p, unsigned long align)
{
	vr->num = num;
	vr->desc = (struct vring_desc *)p; //设置描述符表的起始地址
	vr->avail = (struct vring_avail *)(p + num * sizeof(struct vring_desc)); //根据描述符表的大小计算可用环的起始地址
	vr->used = (struct vring_used *) 
	    (((unsigned long)&vr->avail->ring[num] + sizeof(uint16_t) +
	      align - 1) & ~(align - 1)); // 计算已用环的起始地址，需要考虑对齐要求，以确保已用环在内存中正确对齐
}

/*
 * The following is used with VIRTIO_RING_F_EVENT_IDX.
 *
 * Assuming a given event_idx value from the other size, if we have
 * just incremented index from old to new_idx, should we trigger an
 * event?
 */
/**
 * @description: 决定是否需要通知对方（设备或驱动）。当使用 VIRTIO_RING_F_EVENT_IDX 特性时，此函数帮助确定在更新索引（如从旧索引更新到新索引 new_idx）后是否需要向对方发送通知
 * @param {uint16_t} event_idx
 * @param {uint16_t} new_idx 对方期望接收通知的下一个索引
 * @param {uint16_t} old 更新前的索引
 * @return {*}
 */
static inline int vring_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old)
{
	/*如果 new_idx 在 event_idx 和 old 之间，或超过 event_idx，则返回真（1），表示需要发送通知*/
	return (uint16_t)(new_idx - event_idx - 1) <
	    (uint16_t)(new_idx - old);
}

#if defined __cplusplus
}
#endif

#endif				/* VIRTIO_RING_H */
