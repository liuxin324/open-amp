/*-
 * Copyright (c) 2011, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <string.h>
#include <openamp/virtio.h>
#include <openamp/virtqueue.h>
#include <metal/atomic.h>
#include <metal/log.h>
#include <metal/alloc.h>

/* Prototype for internal functions. */
static void vq_ring_init(struct virtqueue *, void *, int);
static void vq_ring_update_avail(struct virtqueue *, uint16_t);
static uint16_t vq_ring_add_buffer(struct virtqueue *, struct vring_desc *,
				   uint16_t, struct virtqueue_buf *, int, int);
static int vq_ring_enable_interrupt(struct virtqueue *, uint16_t);
static void vq_ring_free_chain(struct virtqueue *, uint16_t);
static int vq_ring_must_notify(struct virtqueue *vq);
static void vq_ring_notify(struct virtqueue *vq);
#ifndef VIRTIO_DEVICE_ONLY //（仅在宿主设备上使用）
static int virtqueue_nused(struct virtqueue *vq);
#endif
#ifndef VIRTIO_DRIVER_ONLY // （仅在客户机设备上使用）
static int virtqueue_navail(struct virtqueue *vq);
#endif

/* Default implementation of P2V based on libmetal */
/*此函数将物理地址转换为对应的虚拟地址*/
static inline void *virtqueue_phys_to_virt(struct virtqueue *vq,
					   metal_phys_addr_t phys)
{
	struct metal_io_region *io = vq->shm_io;

	return metal_io_phys_to_virt(io, phys);
}

/* Default implementation of V2P based on libmetal */
//此函数将虚拟地址转换为对应的物理地址
static inline metal_phys_addr_t virtqueue_virt_to_phys(struct virtqueue *vq,
						       void *buf)
{
	struct metal_io_region *io = vq->shm_io;

	return metal_io_virt_to_phys(io, buf);
}

/**
 * @internal
 *
 * @brief Creates new VirtIO queue
 *	创建一个新的 VirtIO 队列
 * @param device	Pointer to VirtIO device --指向 struct virtio_device 的指针，代表要创建队列的 VirtIO 设备
 * @param id		VirtIO queue ID , must be unique  --VirtIO 队列的唯一标识符（ID）
 * @param name		Name of VirtIO queue --VirtIO 队列的名称
 * @param ring		Pointer to vring_alloc_info control block--指向 struct vring_alloc_info 的指针，这是一个控制块，包含用于分配 vring 的信息
 * @param callback	Pointer to callback function, invoked --指向回调函数的指针，当 VirtIO 队列上有消息可用时调用
 *			when message is available on VirtIO queue 
 * @param notify	Pointer to notify function, used to notify --用于通知另一方（设备或驱动程序）有工作可用的函数的指针
 *			other side that there is job available for it
 * @param vq		Created VirtIO queue. --指向新创建的 VirtIO 队列的指针
 *
 * @return Function status
 */
int virtqueue_create(struct virtio_device *virt_dev, unsigned short id,
		     const char *name, struct vring_alloc_info *ring,
		     void (*callback)(struct virtqueue *vq),
		     void (*notify)(struct virtqueue *vq),
		     struct virtqueue *vq)
{
	int status = VQUEUE_SUCCESS;
	/*函数首先验证输入参数是否有效使用宏 VQ_PARAM_CHK 检查 ring、vq 是否为 NULL，ring 的 num_descs（描述符数量）是否为零，以及是否为 2 的幂（对齐检查）*/
	VQ_PARAM_CHK(ring == NULL, status, ERROR_VQUEUE_INVLD_PARAM);
	VQ_PARAM_CHK(ring->num_descs == 0, status, ERROR_VQUEUE_INVLD_PARAM);
	VQ_PARAM_CHK(ring->num_descs & (ring->num_descs - 1), status,
		     ERROR_VRING_ALIGN);
	VQ_PARAM_CHK(vq == NULL, status, ERROR_NO_MEM);
	//初始化 vq 结构体
	if (status == VQUEUE_SUCCESS) {
		vq->vq_dev = virt_dev;
		vq->vq_name = name;
		vq->vq_queue_index = id;
		vq->vq_nentries = ring->num_descs;
		vq->vq_free_cnt = vq->vq_nentries;
		vq->callback = callback;
		vq->notify = notify;

		/* Initialize vring control block in virtqueue. */
		//初始化 vring
		vq_ring_init(vq, ring->vaddr, ring->align);
	}

	/*
	 * CACHE: nothing to be done here. Only desc.next is setup at this
	 * stage but that is only written by driver, so no need to flush it.
	 */

	return status;
}
/**
 * @internal
 *
 * @brief Enqueues new buffer in vring for consumption by other side. Readable
 * buffers are always inserted before writable buffers
 * 向VirtIO 队列中添加一个新的缓冲区，以便另一方（通常是设备）消费这个函数处理可读和可写缓冲区，确保它们按正确的顺序添加到队列中
 * @param vq		Pointer to VirtIO queue control block. 指向 VirtIO 队列控制块的指针
 * @param buf_list	Pointer to a list of virtqueue buffers. 指向一个虚拟队列缓冲区列表的指针
 * @param readable	Number of readable buffers 可读缓冲区的数量
 * @param writable	Number of writable buffers 可写缓冲区的数量
 * @param cookie	Pointer to hold call back data 用于回调数据的指针通常用于在缓冲区被设备处理后，识别和处理相应的缓冲区
 *
 * @return Function status
 */
int virtqueue_add_buffer(struct virtqueue *vq, struct virtqueue_buf *buf_list,
			 int readable, int writable, void *cookie)
{
	struct vq_desc_extra *dxp = NULL;
	int status = VQUEUE_SUCCESS;
	uint16_t head_idx;
	uint16_t idx;
	int needed;

	needed = readable + writable;

	VQ_PARAM_CHK(vq == NULL, status, ERROR_VQUEUE_INVLD_PARAM);
	VQ_PARAM_CHK(needed < 1, status, ERROR_VQUEUE_INVLD_PARAM);
	//检查队列空间: 验证虚拟队列是否有足够的空间来添加新的缓冲区如果空间不足，函数返回错
	VQ_PARAM_CHK(vq->vq_free_cnt < needed, status, ERROR_VRING_FULL);

	// 标记队列为忙: 使用 VQUEUE_BUSY 宏来标记队列正在被使用，防止同时对同一个队列进行操作
	VQUEUE_BUSY(vq);

	if (status == VQUEUE_SUCCESS) {
		VQASSERT(vq, cookie != NULL, "enqueuing with no cookie");

		head_idx = vq->vq_desc_head_idx; // 使用 head_idx 获取队列中当前空闲的第一个描述符的索引
		VQ_RING_ASSERT_VALID_IDX(vq, head_idx);
		dxp = &vq->vq_descx[head_idx];

		VQASSERT(vq, dxp->cookie == NULL,
			 "cookie already exists for index");

		dxp->cookie = cookie;
		dxp->ndescs = needed;

		/* Enqueue buffer onto the ring. */
		//将缓冲区添加到 vring 中这个过程包括设置描述符的物理地址、长度和标志等信息
		idx = vq_ring_add_buffer(vq, vq->vq_ring.desc, head_idx,
					 buf_list, readable, writable);

		vq->vq_desc_head_idx = idx;
		vq->vq_free_cnt -= needed;

		if (vq->vq_free_cnt == 0) {
			VQ_RING_ASSERT_CHAIN_TERM(vq);
		} else {
			VQ_RING_ASSERT_VALID_IDX(vq, idx);
		}

		/*
		 * Update vring_avail control block fields so that other
		 * side can get buffer using it.
		 */
		//更新可用环: 使用 vq_ring_update_avail 函数更新队列的可用环，以通知另一方新的缓冲区已经可用
		vq_ring_update_avail(vq, head_idx);
	}
	//标记队列为空闲: 操作完成后，使用 VQUEUE_IDLE 宏将队列标记为不忙
	VQUEUE_IDLE(vq);

	return status;
}
/**
 * @internal
 *
 * @brief Returns used buffers from VirtIO queue
 *	从 VirtIO 队列中返回已被设备使用（消费）的缓冲区
 * @param vq	Pointer to VirtIO queue control block 指向 VirtIO 队列控制块的指针
 * @param len	Length of conumed buffer 用于存储消费缓冲区长度的指针
 * @param idx	Index of the buffer 用于存储缓冲区索引的指针
 *
 * @return Pointer to used buffer 返回指向被消费缓冲区关联数据的 cookie 指针
 */
void *virtqueue_get_buffer(struct virtqueue *vq, uint32_t *len, uint16_t *idx)
{
	struct vring_used_elem *uep;
	void *cookie;
	uint16_t used_idx, desc_idx;

	/* Used.idx is updated by the virtio device, so we need to invalidate */
	// VRING_INVALIDATE 宏来确保 used->idx 的缓存一致性，因为这个值由设备更新
	VRING_INVALIDATE(&vq->vq_ring.used->idx, sizeof(vq->vq_ring.used->idx));
	// 检查是否有新的已使用缓冲区（通过比较 vq_used_cons_idx 和 vq->vq_ring.used->idx）
	if (!vq || vq->vq_used_cons_idx == vq->vq_ring.used->idx)
		return NULL;

	VQUEUE_BUSY(vq);
	// 获取下一个要处理的已使用缓冲区的索引（used_idx），并根据该索引从 used 环中获取相应的 vring_used_elem（uep）
	used_idx = vq->vq_used_cons_idx++ & (vq->vq_nentries - 1);
	uep = &vq->vq_ring.used->ring[used_idx];

	atomic_thread_fence(memory_order_seq_cst);

	/* Used.ring is written by remote, invalidate it */
	VRING_INVALIDATE(&vq->vq_ring.used->ring[used_idx],
			 sizeof(vq->vq_ring.used->ring[used_idx]));

	desc_idx = (uint16_t)uep->id;
	if (len)
		*len = uep->len;
	// 从 uep 中获取描述符索引和长度，通过 vq_ring_free_chain 释放相关的描述符链
	vq_ring_free_chain(vq, desc_idx);
	// 获取与描述符相关联的 cookie（回调数据）
	cookie = vq->vq_descx[desc_idx].cookie;
	vq->vq_descx[desc_idx].cookie = NULL;

	if (idx)
		*idx = used_idx;
	VQUEUE_IDLE(vq);

	return cookie;
}
/**
 * @description: 获取指定缓冲区的长度
 * @param {virtqueue} *vq 指向 VirtIO 队列控制块的指针
 * @param {uint16_t} idx 缓冲区的索引
 * @return {*} 返回指定缓冲区的长度
 */
uint32_t virtqueue_get_buffer_length(struct virtqueue *vq, uint16_t idx)
{
	//使用 VRING_INVALIDATE 确保 desc[idx].len 的缓存一致性
	VRING_INVALIDATE(&vq->vq_ring.desc[idx].len,
			 sizeof(vq->vq_ring.desc[idx].len));
	return vq->vq_ring.desc[idx].len;
}
/**
 * @description: 获取指定缓冲区的虚拟地址
 * @param {virtqueue} *vq 指向 VirtIO 队列控制块的指针
 * @param {uint16_t} idx 缓冲区的索引
 * @return {*}
 */
void *virtqueue_get_buffer_addr(struct virtqueue *vq, uint16_t idx)
{
	// 使用 VRING_INVALIDATE 确保 desc[idx].addr 的缓存一致性
	VRING_INVALIDATE(&vq->vq_ring.desc[idx].addr,
			 sizeof(vq->vq_ring.desc[idx].addr));
	// 使用 virtqueue_phys_to_virt 将物理地址转换为虚拟地址并返回
	return virtqueue_phys_to_virt(vq, vq->vq_ring.desc[idx].addr);
}
/**
 * @internal
 *
 * @brief Frees VirtIO queue resources 释放一个 VirtIO 队列的资源
 *
 * @param vq	Pointer to VirtIO queue control block 指向 VirtIO 队列控制块的指针
 */
void virtqueue_free(struct virtqueue *vq)
{
	if (vq) {
		/*如果队列在释放时不为空（vq_free_cnt 不等于 vq_nentries），则记录一条警告日志，指出正在释放一个非空的虚拟队列*/
		if (vq->vq_free_cnt != vq->vq_nentries) {
			metal_log(METAL_LOG_WARNING,
				  "%s: freeing non-empty virtqueue\r\n",
				  vq->vq_name);
		}

		metal_free_memory(vq);// 使用 metal_free_memory 释放 vq 指向的内存
	}
}
/**
 * @internal
 *
 * @brief Returns buffer available for use in the VirtIO queue
 * 用于获取一个可用的缓冲区，该缓冲区之后可以用于在 VirtIO 队列中存储数据
 * @param vq		Pointer to VirtIO queue control block -指向 VirtIO 队列控制块的指针
 * @param avail_idx	Pointer to index used in vring desc table -用于存储在 vring 描述符表中的索引的指针
 * @param len		Length of buffer -用于存储缓冲区长度的指针
 *
 * @return Pointer to available buffer -返回指向可用缓冲区的指针
 */
void *virtqueue_get_available_buffer(struct virtqueue *vq, uint16_t *avail_idx,
				     uint32_t *len)
{
	uint16_t head_idx = 0;
	void *buffer;
	// 使用内存屏障确保之前的内存操作完成
	atomic_thread_fence(memory_order_seq_cst);

	/* Avail.idx is updated by driver, invalidate it */
	/*验证是否有新的可用缓冲区（通过比较 vq_available_idx 和 vq->vq_ring.avail->idx）*/
	VRING_INVALIDATE(&vq->vq_ring.avail->idx, sizeof(vq->vq_ring.avail->idx));
	if (vq->vq_available_idx == vq->vq_ring.avail->idx) {
		return NULL;
	}

	VQUEUE_BUSY(vq);

	head_idx = vq->vq_available_idx++ & (vq->vq_nentries - 1);

	/* Avail.ring is updated by driver, invalidate it */
	/*计算头索引并从 avail->ring 中获取对应的缓冲区索引*/
	VRING_INVALIDATE(&vq->vq_ring.avail->ring[head_idx],
			 sizeof(vq->vq_ring.avail->ring[head_idx]));
	*avail_idx = vq->vq_ring.avail->ring[head_idx];

	/* Invalidate the desc entry written by driver before accessing it */
	/*使用 virtqueue_phys_to_virt 将缓冲区的物理地址转换为虚拟地址，并通过 len 返回缓冲区长度*/
	VRING_INVALIDATE(&vq->vq_ring.desc[*avail_idx],
			 sizeof(vq->vq_ring.desc[*avail_idx]));
	buffer = virtqueue_phys_to_virt(vq, vq->vq_ring.desc[*avail_idx].addr);
	*len = vq->vq_ring.desc[*avail_idx].len;

	VQUEUE_IDLE(vq);

	return buffer;
}
/**
 * @internal
 *
 * @brief Returns consumed buffer back to VirtIO queue
 *	此函数用于将一个已被消费的缓冲区返回到 VirtIO 队列中，以便重新使用
 * @param vq		Pointer to VirtIO queue control block -指向 VirtIO 队列控制块的指针
 * @param head_idx	Index of vring desc containing used buffer -包含已使用缓冲区的 vring 描述符的索引
 * @param len		Length of buffer -缓冲区的长度
 *
 * @return Function status
 */
int virtqueue_add_consumed_buffer(struct virtqueue *vq, uint16_t head_idx,
				  uint32_t len)
{
	struct vring_used_elem *used_desc = NULL;
	uint16_t used_idx;
	//验证 head_idx 是否有效
	if (head_idx >= vq->vq_nentries) {
		return ERROR_VRING_NO_BUFF;
	}

	VQUEUE_BUSY(vq);

	/* CACHE: used is never written by driver, so it's safe to directly access it */
	//计算 used 环中的下一个索引，并获取对应的 vring_used_elem 结构体指针
	used_idx = vq->vq_ring.used->idx & (vq->vq_nentries - 1);
	used_desc = &vq->vq_ring.used->ring[used_idx];
	//设置 used_desc 的 id 和 len，表明缓冲区已被使用
	used_desc->id = head_idx;
	used_desc->len = len;

	/* We still need to flush it because this is read by driver */
	//更新 vq->vq_ring.used->idx 以指示有新的已使用缓冲区
	VRING_FLUSH(&vq->vq_ring.used->ring[used_idx],
		    sizeof(vq->vq_ring.used->ring[used_idx]));

	atomic_thread_fence(memory_order_seq_cst);

	vq->vq_ring.used->idx++;

	/* Used.idx is read by driver, so we need to flush it */
	// 通过 VRING_FLUSH 确保 used 环的更新对设备可见
	VRING_FLUSH(&vq->vq_ring.used->idx, sizeof(vq->vq_ring.used->idx));

	/* Keep pending count until virtqueue_notify(). */
	// 增加 vq_queued_cnt 计数，跟踪队列中待处理的缓冲区数量
	vq->vq_queued_cnt++;

	VQUEUE_IDLE(vq);
	// 返回 VQUEUE_SUCCESS 表示成功将缓冲区添加到已使用队列中
	return VQUEUE_SUCCESS;
}
/**
 * @internal
 *
 * @brief Enables callback generation
 *	用于启用给定的 VirtIO 队列上的回调（或中断）生成
 * @param vq	Pointer to VirtIO queue control block 指向 VirtIO 队列控制块的指针
 *
 * @return Function status
 */
int virtqueue_enable_cb(struct virtqueue *vq)
{
	//函数通过调用 vq_ring_enable_interrupt 实现，将中断启用阈值设置为 0，这意味着设备在有任何新的已用缓冲区时都将生成中断
	return vq_ring_enable_interrupt(vq, 0);
}
/**
 * @internal
 *
 * @brief Disables callback generation
 *	此函数用于禁用给定的 VirtIO 队列上的回调（或中断）生成
 * @param vq	Pointer to VirtIO queue control block
 */
void virtqueue_disable_cb(struct virtqueue *vq)
{
	// 标记队列为忙状态，防止在修改配置时的并发访问
	VQUEUE_BUSY(vq);
	/*根据 VirtIO 设备的特性（vq->vq_dev->features）和角色（驱动程序或设备），采用不同的方式来禁用回调*/
	if (vq->vq_dev->features & VIRTIO_RING_F_EVENT_IDX) { //如果启用了 VIRTIO_RING_F_EVENT_IDX 特性，函数通过设置 vring_used_event 或 vring_avail_event 来调整中断生成的条件
#ifndef VIRTIO_DEVICE_ONLY  
		if (vq->vq_dev->role == VIRTIO_DEV_DRIVER) {
			vring_used_event(&vq->vq_ring) =
			    vq->vq_used_cons_idx - vq->vq_nentries - 1;
			VRING_FLUSH(&vring_used_event(&vq->vq_ring),
				    sizeof(vring_used_event(&vq->vq_ring)));
		}
#endif /*VIRTIO_DEVICE_ONLY*/
#ifndef VIRTIO_DRIVER_ONLY
		if (vq->vq_dev->role == VIRTIO_DEV_DEVICE) {
			vring_avail_event(&vq->vq_ring) =
			    vq->vq_available_idx - vq->vq_nentries - 1;
			VRING_FLUSH(&vring_avail_event(&vq->vq_ring),
				    sizeof(vring_avail_event(&vq->vq_ring)));
		}
#endif /*VIRTIO_DRIVER_ONLY*/
	} else { //如果未启用 VIRTIO_RING_F_EVENT_IDX 特性，函数通过直接设置 VRING_AVAIL_F_NO_INTERRUPT 或 VRING_USED_F_NO_NOTIFY 标志来禁用回调
#ifndef VIRTIO_DEVICE_ONLY
		if (vq->vq_dev->role == VIRTIO_DEV_DRIVER) {
			vq->vq_ring.avail->flags |= VRING_AVAIL_F_NO_INTERRUPT;
			VRING_FLUSH(&vq->vq_ring.avail->flags,
				    sizeof(vq->vq_ring.avail->flags));
		}
#endif /*VIRTIO_DEVICE_ONLY*/
#ifndef VIRTIO_DRIVER_ONLY
		if (vq->vq_dev->role == VIRTIO_DEV_DEVICE) {
			vq->vq_ring.used->flags |= VRING_USED_F_NO_NOTIFY;
			VRING_FLUSH(&vq->vq_ring.used->flags,
				    sizeof(vq->vq_ring.used->flags));
		}
#endif /*VIRTIO_DRIVER_ONLY*/
	}
	// 通过 VQUEUE_IDLE 宏将队列标记为闲置状态
	VQUEUE_IDLE(vq);
}
/**
 * @internal
 *
 * @brief Notifies other side that there is buffer available for it.
 *	此函数用于通知另一端（通常是设备）虚拟队列中有新的可用缓冲区这个操作通常称为“kick”或“notify”
 * @param vq	Pointer to VirtIO queue control block
 */
void virtqueue_kick(struct virtqueue *vq)
{
	VQUEUE_BUSY(vq); //标记队列为忙，防止并发访问

	/* Ensure updated avail->idx is visible to host. */
	//确保 avail->idx 的更新对设备可见
	atomic_thread_fence(memory_order_seq_cst);
	//判断是否需要通知设备
	if (vq_ring_must_notify(vq))
		vq_ring_notify(vq);//如果需要通知，则调用 vq_ring_notify 实际执行通知操作

	vq->vq_queued_cnt = 0;//将 vq->vq_queued_cnt 重置为 0，表示已通知设备所有队列中的缓冲区

	VQUEUE_IDLE(vq); //使用 VQUEUE_IDLE 宏将队列标记为空闲
}
/**
 * @internal
 *
 * @brief Dumps important virtqueue fields , use for debugging purposes
 *	此函数用于调试目的，打印一个虚拟队列的关键字段状态
 * @param vq	Pointer to VirtIO queue control block
 */
void virtqueue_dump(struct virtqueue *vq)
{
	if (!vq)
		return;
	//使用 VRING_INVALIDATE 刷新 avail 和 used 结构体的缓存，确保读取到最新的状态
	VRING_INVALIDATE(&vq->vq_ring.avail, sizeof(vq->vq_ring.avail));
	VRING_INVALIDATE(&vq->vq_ring.used, sizeof(vq->vq_ring.used));
	//使用 metal_log 打印虚拟队列的名称、大小、空闲缓冲区数量、已入队缓冲区数量、头索引、可用索引、已使用消费索引、avail 和 used 的标志等关键信息
	metal_log(METAL_LOG_DEBUG,
		  "VQ: %s - size=%d; free=%d; queued=%d; desc_head_idx=%d; "
		  "available_idx=%d; avail.idx=%d; used_cons_idx=%d; "
		  "used.idx=%d; avail.flags=0x%x; used.flags=0x%x\r\n",
		  vq->vq_name, vq->vq_nentries, vq->vq_free_cnt,
		  vq->vq_queued_cnt, vq->vq_desc_head_idx, vq->vq_available_idx,
		  vq->vq_ring.avail->idx, vq->vq_used_cons_idx,
		  vq->vq_ring.used->idx, vq->vq_ring.avail->flags,
		  vq->vq_ring.used->flags);
}
/**
 * @internal
 *
 * @brief Returns vring descriptor size
 * 此函数返回虚拟队列中一个指定描述符的缓冲区大小
 * @param vq	Pointer to VirtIO queue control block
 *
 * @return Descriptor length
 */
uint32_t virtqueue_get_desc_size(struct virtqueue *vq)
{
	uint16_t head_idx = 0;
	uint16_t avail_idx = 0;
	uint32_t len = 0;

	/* Avail.idx is updated by driver, invalidate it */
	// 使用 VRING_INVALIDATE 刷新 avail->idx 的缓存，确保读取到最新的状态
	VRING_INVALIDATE(&vq->vq_ring.avail->idx, sizeof(vq->vq_ring.avail->idx));
	/*如果 vq->vq_available_idx 与 vq->vq_ring.avail->idx 相等，表示没有新的可用缓冲区，返回 0*/
	if (vq->vq_available_idx == vq->vq_ring.avail->idx) {
		return 0;
	}

	VQUEUE_BUSY(vq);//使用 VQUEUE_BUSY 宏标记队列为忙
	//根据 vq->vq_available_idx 计算头索引，并从 avail->ring 获取实际的缓冲区索引
	head_idx = vq->vq_available_idx & (vq->vq_nentries - 1);

	/* Avail.ring is updated by driver, invalidate it */
	//刷新对应描述符的 len 字段的缓存，然后读取缓冲区长度
	VRING_INVALIDATE(&vq->vq_ring.avail->ring[head_idx],
			 sizeof(vq->vq_ring.avail->ring[head_idx]));
	avail_idx = vq->vq_ring.avail->ring[head_idx];

	/* Invalidate the desc entry written by driver before accessing it */
	VRING_INVALIDATE(&vq->vq_ring.desc[avail_idx].len,
			 sizeof(vq->vq_ring.desc[avail_idx].len));

	len = vq->vq_ring.desc[avail_idx].len;

	VQUEUE_IDLE(vq);

	return len;
}

/**************************************************************************
 *                            Helper Functions                            *
 **************************************************************************/

/**
 * @description: 此函数用于将一个或多个缓冲区添加到 VirtIO 队列的 vring 中
 * @param {virtqueue} *vq 指向 VirtIO 队列控制块的指针
 * @param {vring_desc} *desc 指向描述符数组的指针
 * @param {uint16_t} head_idx 第一个要使用的描述符的索引
 * @param {virtqueue_buf} *buf_list 包含要添加的缓冲区信息的数组
 * @param {int} readable 可读缓冲区的数量
 * @param {int} writable 可写缓冲区的数量
 * @return {*}
 */
static uint16_t vq_ring_add_buffer(struct virtqueue *vq,struct vring_desc *desc, uint16_t head_idx,struct virtqueue_buf *buf_list, int readable,int writable)
{
	struct vring_desc *dp;
	int i, needed;
	uint16_t idx;

	(void)vq;
	// 计算总共需要的描述符数量（needed），包括可读和可写缓冲区
	needed = readable + writable;
	// 遍历 buf_list，为每个缓冲区设置对应的描述符
	for (i = 0, idx = head_idx; i < needed; i++, idx = dp->next) {
		VQASSERT(vq, idx != VQ_RING_DESC_CHAIN_END,
			 "premature end of free desc chain");

		/* CACHE: No need to invalidate desc because it is only written by driver */
		dp = &desc[idx];
		dp->addr = virtqueue_virt_to_phys(vq, buf_list[i].buf); // 将缓冲区的虚拟地址转换为物理地址
		dp->len = buf_list[i].len; //描述符的长度和标志
		dp->flags = 0;

		/*如果不是最后一个缓冲区，设置 VRING_DESC_F_NEXT 标志，指示还有后续的描述符*/
		if (i < needed - 1)
			dp->flags |= VRING_DESC_F_NEXT;

		/*
		 * Readable buffers are inserted  into vring before the
		 * writable buffers.
		 */
		if (i >= readable)
			dp->flags |= VRING_DESC_F_WRITE; //设置缓冲区可写

		/*
		 * Instead of flushing the whole desc region, we flush only the
		 * single entry hopefully saving some cycles
		 */
		/*仅刷新修改过的单个描述符条目，而不是整个描述符区域*/
		VRING_FLUSH(&desc[idx], sizeof(desc[idx]));

	}

	return idx;
}

/**
 * @description: 此函数用于释放虚拟队列的 vring 中由一个描述符索引指向的缓冲区链 
 * @param {virtqueue} *vq 指向 VirtIO 队列控制块的指针
 * @param {uint16_t} desc_idx 要开始释放的描述符链的头部索引
 * @return {*}
 */
static void vq_ring_free_chain(struct virtqueue *vq, uint16_t desc_idx)
{
	struct vring_desc *dp;
	struct vq_desc_extra *dxp;

	/* CACHE: desc is never written by remote, no need to invalidate */
	/*验证 desc_idx 是否有效*/
	VQ_RING_ASSERT_VALID_IDX(vq, desc_idx);
	dp = &vq->vq_ring.desc[desc_idx];
	dxp = &vq->vq_descx[desc_idx];

	if (vq->vq_free_cnt == 0) {
		VQ_RING_ASSERT_CHAIN_TERM(vq);
	}
	/*遍历由 desc_idx 指定的描述符链，更新 vq->vq_free_cnt 来反映新增的空闲描述符数量*/
	vq->vq_free_cnt += dxp->ndescs;
	dxp->ndescs--;

	if ((dp->flags & VRING_DESC_F_INDIRECT) == 0) {
		while (dp->flags & VRING_DESC_F_NEXT) { //对于每个描述符，如果它有 VRING_DESC_F_NEXT 标志，继续遍历到下一个描述符，直到链的末尾
			VQ_RING_ASSERT_VALID_IDX(vq, dp->next);
			dp = &vq->vq_ring.desc[dp->next];
			dxp->ndescs--;
		}
	}
	//确保整个描述符链都已被遍历并释放
	VQASSERT(vq, dxp->ndescs == 0,
		 "failed to free entire desc chain, remaining");

	/*
	 * We must append the existing free chain, if any, to the end of
	 * newly freed chain. If the virtqueue was completely used, then
	 * head would be VQ_RING_DESC_CHAIN_END (ASSERTed above).
	 *
	 * CACHE: desc.next is never read by remote, no need to flush it.
	 */
	/*
	* 我们必须将现有的空闲链（如果有）附加到新释放的链的末尾。 如果 virtqueue 被完全使用，则 head 将为 VQ_RING_DESC_CHAIN_END（上面断言）。
	* CACHE：desc.next 永远不会被远程读取，无需刷新它。
	*/
	dp->next = vq->vq_desc_head_idx;
	vq->vq_desc_head_idx = desc_idx;
}

/**
 * @description: 此函数初始化一个 VirtIO 队列的 vring 结构
 * @param {virtqueue} *vq  指向 VirtIO 队列控制块的指针
 * @param {void} *ring_mem 指向为 vring 分配的内存的指针
 * @param {int} alignment vring 中元素的对齐要求
 * @return {*}
 */
static void vq_ring_init(struct virtqueue *vq, void *ring_mem, int alignment)
{
	struct vring *vr;
	int size;

	size = vq->vq_nentries; //获取队列的大小（size），即队列中的描述符数量
	vr = &vq->vq_ring;
	/*使用 vring_init 函数和提供的参数初始化 vring。这包括设置描述符表、可用（avail）和已使用（used）环的位置和大小*/
	vring_init(vr, size, ring_mem, alignment);

#ifndef VIRTIO_DEVICE_ONLY
	/*对于驱动程序角色（VIRTIO_DEV_DRIVER），初始化描述符链，使得每个描述符的 next 指向下一个描述符，
	最后一个描述符的 next 设置为 VQ_RING_DESC_CHAIN_END 表示链的末端。这是为了在队列中快速添加和处理缓冲区*/
	if (vq->vq_dev->role == VIRTIO_DEV_DRIVER) {
		int i;

		for (i = 0; i < size - 1; i++)
			vr->desc[i].next = i + 1;
		vr->desc[i].next = VQ_RING_DESC_CHAIN_END;
	}
#endif /*VIRTIO_DEVICE_ONLY*/
}

/**
 * @description: 此函数用于在队列的可用环中添加一个新的描述符，并使其对宿主可用
 * @param {virtqueue} *vq 指向 VirtIO 队列控制块的指针
 * @param {uint16_t} desc_idx 要添加到可用环的描述符索引
 * @return {*}
 */
static void vq_ring_update_avail(struct virtqueue *vq, uint16_t desc_idx)
{
	uint16_t avail_idx;

	/*
	 * Place the head of the descriptor chain into the next slot and make
	 * it usable to the host. The chain is made available now rather than
	 * deferring to virtqueue_notify() in the hopes that if the host is
	 * currently running on another CPU, we can keep it processing the new
	 * descriptor.
	 *
	 * CACHE: avail is never written by remote, so it is safe to not invalidate here
	 */
	/*
	* 将描述符链的头部放入下一个槽中，并使其可供主机使用。 该链条现在可用，而不是
	* 推迟到 virtqueue_notify()，希望如果主机当前正在另一个 CPU 上运行，我们可以让它继续处理新描述符。
	* CACHE：avail永远不会被远程写入，所以这里不失效是安全的
	*/
	avail_idx = vq->vq_ring.avail->idx & (vq->vq_nentries - 1); //可 用环的当前索引（avail_idx）
	vq->vq_ring.avail->ring[avail_idx] = desc_idx;  // 在可用环的当前位置放置新的描述符索引（desc_idx），以通知宿主新的缓冲区已准备好处理

	/* We still need to flush the ring */
	//刷新（flush）更改过的可用环的部分，确保宿主可以看到更新
	VRING_FLUSH(&vq->vq_ring.avail->ring[avail_idx],
		    sizeof(vq->vq_ring.avail->ring[avail_idx]));
	//使用内存屏障（atomic_thread_fence）确保上述更新对宿主立即可见
	atomic_thread_fence(memory_order_seq_cst);
	// 增加可用环的索引（vq->vq_ring.avail->idx）并再次刷新以确保宿主看到这一变化
	vq->vq_ring.avail->idx++;

	/* And the index */
	VRING_FLUSH(&vq->vq_ring.avail->idx, sizeof(vq->vq_ring.avail->idx));

	/* Keep pending count until virtqueue_notify(). */
	/*增加队列的已入队缓冲区计数（vq->vq_queued_cnt），直到调用 virtqueue_notify 函数*/
	vq->vq_queued_cnt++;
}

/**
 * @description: 此函数用于启用 VirtIO 队列上的中断
 * @param {virtqueue} *vq 指向 VirtIO 队列控制块的指针
 * @param {uint16_t} ndesc 阈值，指示在触发中断前可以消费多少描述符
 * @return {*}
 */
static int vq_ring_enable_interrupt(struct virtqueue *vq, uint16_t ndesc)
{
	/*
	 * Enable interrupts, making sure we get the latest index of
	 * what's already been consumed.
	 */
	if (vq->vq_dev->features & VIRTIO_RING_F_EVENT_IDX) {/*如果支持 VIRTIO_RING_F_EVENT_IDX，则根据设备角色（驱动或设备）更新 vring_used_event 或 vring_avail_event，以指定触发中断的索引阈值，并确保该更改被刷新到内存*/
#ifndef VIRTIO_DEVICE_ONLY
		if (vq->vq_dev->role == VIRTIO_DEV_DRIVER) {
			vring_used_event(&vq->vq_ring) =
				vq->vq_used_cons_idx + ndesc;
			VRING_FLUSH(&vring_used_event(&vq->vq_ring),
				    sizeof(vring_used_event(&vq->vq_ring)));
		}
#endif /*VIRTIO_DEVICE_ONLY*/
#ifndef VIRTIO_DRIVER_ONLY
		if (vq->vq_dev->role == VIRTIO_DEV_DEVICE) {
			vring_avail_event(&vq->vq_ring) =
				vq->vq_available_idx + ndesc;
			VRING_FLUSH(&vring_avail_event(&vq->vq_ring),
				    sizeof(vring_avail_event(&vq->vq_ring)));
		}
#endif /*VIRTIO_DRIVER_ONLY*/
	} else { /*如果不支持 VIRTIO_RING_F_EVENT_IDX，则清除 VRING_AVAIL_F_NO_INTERRUPT 或 VRING_USED_F_NO_NOTIFY 标志，允许中断的产生，并确保标志更改被刷新到内存*/
#ifndef VIRTIO_DEVICE_ONLY
		if (vq->vq_dev->role == VIRTIO_DEV_DRIVER) {
			vq->vq_ring.avail->flags &= ~VRING_AVAIL_F_NO_INTERRUPT;
			VRING_FLUSH(&vq->vq_ring.avail->flags,
				    sizeof(vq->vq_ring.avail->flags));
		}
#endif /*VIRTIO_DEVICE_ONLY*/
#ifndef VIRTIO_DRIVER_ONLY
		if (vq->vq_dev->role == VIRTIO_DEV_DEVICE) {
			vq->vq_ring.used->flags &= ~VRING_USED_F_NO_NOTIFY;
			VRING_FLUSH(&vq->vq_ring.used->flags,
				    sizeof(vq->vq_ring.used->flags));
		}
#endif /*VIRTIO_DRIVER_ONLY*/
	}
	/*使用 atomic_thread_fence 确保所有更改对其他处理器可见*/
	atomic_thread_fence(memory_order_seq_cst);

	/*
	 * Enough items may have already been consumed to meet our threshold
	 * since we last checked. Let our caller know so it processes the new
	 * entries.
	 */
	/*检查是否已经有足够数量的描述符被消费或者可用，如果是，则返回 1，表示调用方应该处理新的条目*/
#ifndef VIRTIO_DEVICE_ONLY
	if (vq->vq_dev->role == VIRTIO_DEV_DRIVER) {
		if (virtqueue_nused(vq) > ndesc) {
			return 1;
		}
	}
#endif /*VIRTIO_DEVICE_ONLY*/
#ifndef VIRTIO_DRIVER_ONLY
	if (vq->vq_dev->role == VIRTIO_DEV_DEVICE) {
		if (virtqueue_navail(vq) > ndesc) {
			return 1;
		}
	}
#endif /*VIRTIO_DRIVER_ONLY*/

	return 0;
}

/**
 * @description: 此函数用于当 VirtIO 队列有数据时调用设定的回调函数，通常是设备向驱动程序发送通知
 * @param {virtqueue} *vq
 * @return {*}
 */
void virtqueue_notification(struct virtqueue *vq)
{
	atomic_thread_fence(memory_order_seq_cst); //使用 atomic_thread_fence 确保所有内存操作完成
	if (vq->callback)
		vq->callback(vq);// 调用回调函数
}

/**
 * @description: 此函数决定是否需要向另一方（设备或宿主）发送通知
 * @param {virtqueue} *vq
 * @return {*} 0 or 1
 */
static int vq_ring_must_notify(struct virtqueue *vq)
{
	uint16_t new_idx, prev_idx, event_idx;

	if (vq->vq_dev->features & VIRTIO_RING_F_EVENT_IDX) {
		/*根据设备角色（驱动或设备）获取当前的 avail 或 used 索引，并计算是否达到了触发通知的条件*/
#ifndef VIRTIO_DEVICE_ONLY
		if (vq->vq_dev->role == VIRTIO_DEV_DRIVER) {
			/* CACHE: no need to invalidate avail */
			new_idx = vq->vq_ring.avail->idx;
			prev_idx = new_idx - vq->vq_queued_cnt;
			VRING_INVALIDATE(&vring_avail_event(&vq->vq_ring),
					 sizeof(vring_avail_event(&vq->vq_ring)));
			event_idx = vring_avail_event(&vq->vq_ring);
			return vring_need_event(event_idx, new_idx,
						prev_idx) != 0;
		}
#endif /*VIRTIO_DEVICE_ONLY*/
#ifndef VIRTIO_DRIVER_ONLY
		if (vq->vq_dev->role == VIRTIO_DEV_DEVICE) {
			/* CACHE: no need to invalidate used */
			new_idx = vq->vq_ring.used->idx;
			prev_idx = new_idx - vq->vq_queued_cnt;
			VRING_INVALIDATE(&vring_used_event(&vq->vq_ring),
					 sizeof(vring_used_event(&vq->vq_ring)));
			event_idx = vring_used_event(&vq->vq_ring);
			return vring_need_event(event_idx, new_idx,
						prev_idx) != 0;
		}
#endif /*VIRTIO_DRIVER_ONLY*/
	} else {
		/*如果不支持 VIRTIO_RING_F_EVENT_IDX，则检查 used 或 avail 标志位，确定是否已经禁用了通知*/
#ifndef VIRTIO_DEVICE_ONLY
		if (vq->vq_dev->role == VIRTIO_DEV_DRIVER) {
			VRING_INVALIDATE(&vq->vq_ring.used->flags,
					 sizeof(vq->vq_ring.used->flags));
			return (vq->vq_ring.used->flags &
				VRING_USED_F_NO_NOTIFY) == 0;
		}
#endif /*VIRTIO_DEVICE_ONLY*/
#ifndef VIRTIO_DRIVER_ONLY
		if (vq->vq_dev->role == VIRTIO_DEV_DEVICE) {
			VRING_INVALIDATE(&vq->vq_ring.avail->flags,
					 sizeof(vq->vq_ring.avail->flags));
			return (vq->vq_ring.avail->flags &
				VRING_AVAIL_F_NO_INTERRUPT) == 0;
		}
#endif /*VIRTIO_DRIVER_ONLY*/
	}

	return 0;
}

/**
 * @description: 通知 VirtIO 设备有一个或多个新的缓冲区已经准备好被设备处理
 * @param {virtqueue} *vq
 * @return {*}
 */
static void vq_ring_notify(struct virtqueue *vq)
{
	if (vq->notify)
		vq->notify(vq);
}


#ifndef VIRTIO_DEVICE_ONLY
/**
 * @description: 计算在驱动程序角度看到的，已经被设备使用（即处理完毕）的描述符数量
 * @param {virtqueue} *vq
 * @return {*}
 */
static int virtqueue_nused(struct virtqueue *vq)
{
	uint16_t used_idx, nused;

	/* Used is written by remote */
	/*刷新 used->idx 的值，确保从设备写回的值是最新的*/
	VRING_INVALIDATE(&vq->vq_ring.used->idx, sizeof(vq->vq_ring.used->idx));
	used_idx = vq->vq_ring.used->idx;
	/*计算已使用的描述符数量，这是通过当前的 used->idx 减去上一次记录的 vq_used_cons_idx（消费索引）得到的*/
	nused = (uint16_t)(used_idx - vq->vq_used_cons_idx);
	/*确保计算出的已使用数量不超过队列中的描述符总数*/
	VQASSERT(vq, nused <= vq->vq_nentries, "used more than available");

	return nused;
}
#endif /*VIRTIO_DEVICE_ONLY*/


#ifndef VIRTIO_DRIVER_ONLY
/**
 * @description: 用于计算在设备角度看到的，当前可用于处理的新描述符数量
 * @param {virtqueue} *vq
 * @return {*}
 */
static int virtqueue_navail(struct virtqueue *vq)
{
	uint16_t avail_idx, navail;

	/* Avail is written by driver */
	/*刷新 avail->idx 的值，确保从驱动程序写入的值是最新的*/
	VRING_INVALIDATE(&vq->vq_ring.avail->idx, sizeof(vq->vq_ring.avail->idx));

	avail_idx = vq->vq_ring.avail->idx;
	/*计算可用的描述符数量，这是通过当前的 avail->idx 减去上一次记录的 vq_available_idx（可用索引）得到的*/
	navail = (uint16_t)(avail_idx - vq->vq_available_idx);
	VQASSERT(vq, navail <= vq->vq_nentries, "avail more than available");//确保计算出的可用数量不超过队列中的描述符总数
	//返回可用的描述符数量
	return navail;
}
#endif /*VIRTIO_DRIVER_ONLY*/
