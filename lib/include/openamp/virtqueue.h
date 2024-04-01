#ifndef VIRTQUEUE_H_
#define VIRTQUEUE_H_

/*-
 * Copyright (c) 2011, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * $FreeBSD$
 */

#include <stdbool.h>
#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

#include <openamp/virtio_ring.h>
#include <metal/alloc.h>
#include <metal/io.h>
#include <metal/cache.h>

/* Error Codes */
#define VQ_ERROR_BASE                                 -3000
#define ERROR_VRING_FULL                              (VQ_ERROR_BASE - 1) //尝试向已经没有剩余空间的虚拟环添加缓冲区时返回此错误
#define ERROR_INVLD_DESC_IDX                          (VQ_ERROR_BASE - 2) //当提供给虚拟环的描述符索引无效返回此错误
#define ERROR_EMPTY_RING                              (VQ_ERROR_BASE - 3) // 尝试从空环中获取缓冲区时返回此错误
#define ERROR_NO_MEM                                  (VQ_ERROR_BASE - 4) // 表示没有足够的内存来完成请求的操作，如分配新的虚拟环
#define ERROR_VRING_MAX_DESC                          (VQ_ERROR_BASE - 5) // 表示虚拟环的描述符数量超过了最大限制
#define ERROR_VRING_ALIGN                             (VQ_ERROR_BASE - 6) // 表示虚拟环的地址对齐不正确
#define ERROR_VRING_NO_BUFF                           (VQ_ERROR_BASE - 7) // 表示没有足够的缓冲区来完成请求的操作
#define ERROR_VQUEUE_INVLD_PARAM                      (VQ_ERROR_BASE - 8) // 表示提供给虚拟环的参数无效

#define VQUEUE_SUCCESS                                0

/* The maximum virtqueue size is 2^15. Use that value as the end of
 * descriptor chain terminator since it will never be a valid index
 * in the descriptor table. This is used to verify we are correctly
 * handling vq_free_cnt.
 */
/*由于 VirtIO 虚拟队列的最大大小是 2^15，这个值永远不会是有效的描述符索引，
因此可以安全地用作链终结符。这主要用于验证虚拟队列中空闲描述符的计数（vq_free_cnt）是否正确处理*/
#define VQ_RING_DESC_CHAIN_END                         32768

/* Support for indirect buffer descriptors. */
/*此特性表示设备支持间接缓冲区描述符。间接描述符允许一个描述符指向一个描述符表，而不是直接指向数据这可以减少需要在虚拟队列中直接管理的描述符数量，提高效率*/
#define VIRTIO_RING_F_INDIRECT_DESC    (1 << 28)

/* Support to suppress interrupt until specific index is reached. */
// 允许使用 used_event 和 avail_event 字段来抑制中断，直到达到特定的索引。
#define VIRTIO_RING_F_EVENT_IDX        (1 << 29)

/* cache invalidation helpers */
// 缓存刷新（写回）和缓存失效
#define CACHE_FLUSH(x, s)		metal_cache_flush(x, s)
#define CACHE_INVALIDATE(x, s)		metal_cache_invalidate(x, s)

#ifdef VIRTIO_CACHED_VRINGS
#warning "VIRTIO_CACHED_VRINGS is deprecated, please use VIRTIO_USE_DCACHE"
#endif
#if defined(VIRTIO_CACHED_VRINGS) || defined(VIRTIO_USE_DCACHE)
#define VRING_FLUSH(x, s)		CACHE_FLUSH(x, s)
#define VRING_INVALIDATE(x, s)		CACHE_INVALIDATE(x, s)
#else
#define VRING_FLUSH(x, s)		do { } while (0)
#define VRING_INVALIDATE(x, s)		do { } while (0)
#endif /* VIRTIO_CACHED_VRINGS || VIRTIO_USE_DCACHE */

/** @brief Buffer descriptor. */
/*表示一个缓冲区的描述符，用于 VirtIO 队列传输的数据缓冲区*/
struct virtqueue_buf {
	/** Address of the buffer. */
	void *buf; 

	/** Size of the buffer. */
	int len;
};

/** @brief Vring descriptor extra information for buffer list management. */
//提供 VirtIO 队列中某个缓冲区的额外信息，主要用于缓冲区列表管理
struct vq_desc_extra {
	/** Pointer to first descriptor. */
	//指向第一个描述符的指针，通常用于跟踪和识别缓冲区
	void *cookie;

	/** Number of chained descriptors. */
	//链式描述符的数量，表示当前缓冲区涉及的描述符数量
	uint16_t ndescs;
};

/** @brief Local virtio queue to manage a virtio ring for sending or receiving. */
// 表示一个局部的 VirtIO 队列，用于管理 VirtIO 环（vring）以发送或接收数据
struct virtqueue {
	/** Associated virtio device. */
	//关联的 VirtIO 设备
	struct virtio_device *vq_dev;

	/** Name of the virtio queue. */
	//VirtIO 队列的名称
	const char *vq_name;

	/** Index of the virtio queue. */
	//VirtIO 队列的索引
	uint16_t vq_queue_index;

	/** Max number of buffers in the virtio queue. */
	//VirtIO 队列中的最大缓冲区数量
	uint16_t vq_nentries;

	/** Function to invoke, when message is available on the virtio queue. */
	//当 VirtIO 队列中有消息可用时调用的函数
	void (*callback)(struct virtqueue *vq);

	/** Private data associated to the virtio queue. */
	//关联到 VirtIO 队列的私有数据
	void *priv;

	/** Function to invoke, to inform the other side about an update in the virtio queue. */
	//通知另一端 VirtIO 队列中有消息可用的函数
	void (*notify)(struct virtqueue *vq);

	/** Associated virtio ring. */
	//关联的 VirtIO 环
	struct vring vq_ring;

	/** Number of free descriptor in the virtio ring. */
	//VirtIO 环中的空闲描述符数量
	uint16_t vq_free_cnt;

	/** Number of queued buffer in the virtio ring. */
	//VirtIO 环中排队缓冲区的数量
	uint16_t vq_queued_cnt;

	/**
	 * Metal I/O region of the vrings and buffers.
	 * This structure is used for conversion between virtual and physical addresses.
	 */
	//用于虚拟地址和物理地址转换的 Metal I/O 区域
	void *shm_io;

	/**
	 * Head of the free chain in the descriptor table. If there are no free descriptors,
	 * this will be set to VQ_RING_DESC_CHAIN_END.
	 */
	//描述符表中空闲链的头部，如果没有空闲描述符，将设置为 VQ_RING_DESC_CHAIN_END
	uint16_t vq_desc_head_idx;

	/** Last consumed descriptor in the used table, trails vq_ring.used->idx. */
	//已使用表中的最后一个消耗的描述符，跟踪 vq_ring.used->idx
	uint16_t vq_used_cons_idx;

	/** Last consumed descriptor in the available table, used by the consumer side. */
	//可用表中的最后一个消耗的描述符，由消费者端使用
	uint16_t vq_available_idx;

#ifdef VQUEUE_DEBUG
	/** Debug counter for virtqueue reentrance check. */
	bool vq_inuse;
#endif

	/**
	 * Used by the host side during callback. Cookie holds the address of buffer received from
	 * other side. Other fields in this structure are not used currently.
	 */
	/**
	* 回调时主机端使用。 Cookie 保存从接收到的缓冲区的地址
	* 对方。 该结构中的其他字段当前未使用。
	*/
	struct vq_desc_extra vq_descx[0];
};

/** @brief Virtio ring specific information. */
// 含分配 VirtIO 环所需的特定信息
struct vring_alloc_info {
	/** Vring address. */
	//VirtIO 环的地址
	void *vaddr;

	/** Vring alignment. */
	//VirtIO 环的地址对齐
	uint32_t align;

	/** Number of descriptors in the vring. */
	//VirtIO 环中描述符的数量
	uint16_t num_descs;

	/** Padding */
	//填充，保证结构体的整体对齐
	uint16_t pad;
};
//当虚拟队列中有消息可用时被调用
typedef void (*vq_callback)(struct virtqueue *);
//通知另一方（设备或驱动程序）虚拟队列中有数据更新或有工作需要处理
typedef void (*vq_notify)(struct virtqueue *);

#ifdef VQUEUE_DEBUG
#include <metal/log.h>
#include <metal/assert.h>
/*用于在条件不满足时记录错误并断言。
它接收三个参数：_vq（虚拟队列指针），_exp（要检查的表达式），和 _msg（错误消息）。如果 _exp 为假（即条件不满足），
它会使用 metal_log 记录一条紧急日志，并使用 metal_assert 触发断言。*/
#define VQASSERT(_vq, _exp, _msg) \
	do { \
		if (!(_exp)) { \
			metal_log(METAL_LOG_EMERGENCY, \
				  "%s: %s - "_msg, __func__, (_vq)->vq_name); \
			metal_assert(_exp); \
		} \
	} while (0)

/*检查给定的索引 _idx 是否在虚拟队列的条目数范围内。如果索引无效（即大于等于队列的最大条目数），它会触发 VQASSERT*/
#define VQ_RING_ASSERT_VALID_IDX(_vq, _idx)            \
	VQASSERT((_vq), (_idx) < (_vq)->vq_nentries, "invalid ring index")
/*验证虚拟队列描述符链的终结条件是否满足。
特别是，它检查虚拟队列的 vq_desc_head_idx 是否等于 VQ_RING_DESC_CHAIN_END，
这是描述符链终结的标志。如果不等，说明链终结的方式有误，触发 VQASSERT*/
#define VQ_RING_ASSERT_CHAIN_TERM(_vq)                \
	VQASSERT((_vq), (_vq)->vq_desc_head_idx ==            \
	VQ_RING_DESC_CHAIN_END, \
	"full ring terminated incorrectly: invalid head")
/*用于检查条件并设置状态变量。如果给定的条件为真，并且状态变量 status_var 当前为 0，则将 status_var 设置为 status_err*/
#define VQ_PARAM_CHK(condition, status_var, status_err) \
	do {						\
		if (((status_var) == 0) && (condition)) { \
			status_var = status_err;        \
		}					\
	} while (0)
/*检查虚拟队列是否已被使用。如果 vq_inuse 标志为假（表示队列当前未被使用），则将其设置为真，表示队列现在正忙。如果 vq_inuse 已经是真（队列已在使用中），则触发 VQASSERT，指出虚拟队列被重复使用。*/
#define VQUEUE_BUSY(vq) \
	do {						     \
		if (!(vq)->vq_inuse)                 \
			(vq)->vq_inuse = true;               \
		else                                         \
			VQASSERT(vq, !(vq)->vq_inuse,\
				"VirtQueue already in use");  \
	} while (0)
//将虚拟队列的 vq_inuse 标志设置为假，表示队列现在空闲
#define VQUEUE_IDLE(vq)            ((vq)->vq_inuse = false)

#else

#define VQASSERT(_vq, _exp, _msg)
#define VQ_RING_ASSERT_VALID_IDX(_vq, _idx)
#define VQ_RING_ASSERT_CHAIN_TERM(_vq)
#define VQ_PARAM_CHK(condition, status_var, status_err)
#define VQUEUE_BUSY(vq)
#define VQUEUE_IDLE(vq)

#endif

/**
 * @internal
 *
 * @brief Creates new VirtIO queue
 *
 * @param device	Pointer to VirtIO device
 * @param id		VirtIO queue ID , must be unique
 * @param name		Name of VirtIO queue
 * @param ring		Pointer to vring_alloc_info control block
 * @param callback	Pointer to callback function, invoked
 *			when message is available on VirtIO queue
 * @param notify	Pointer to notify function, used to notify
 *			other side that there is job available for it
 * @param vq		Created VirtIO queue.
 *
 * @return Function status
 */
int virtqueue_create(struct virtio_device *device, unsigned short id,
		     const char *name, struct vring_alloc_info *ring,
		     void (*callback)(struct virtqueue *vq),
		     void (*notify)(struct virtqueue *vq),
		     struct virtqueue *vq);

/*
 * virtqueue_set_shmem_io
 *
 * set virtqueue shared memory I/O region
 * 设置虚拟队列的共享内存 I/O 区域。共享内存 I/O 区域是一块内存区域，
 * 用于在设备和驱动程序之间交换数据，其内容可以直接映射到物理内存地址，从而实现高效的数据访问
 * @vq - virt queue 表示要设置共享内存 I/O 区域的虚拟队列
 * @io - pointer to the shared memory I/O region 表示共享内存的 I/O 区域
 */
static inline void virtqueue_set_shmem_io(struct virtqueue *vq,
					  struct metal_io_region *io)
{
	vq->shm_io = io;
}

/**
 * @internal
 *
 * @brief Enqueues new buffer in vring for consumption by other side. Readable
 * buffers are always inserted before writable buffers
 *
 * @param vq		Pointer to VirtIO queue control block.
 * @param buf_list	Pointer to a list of virtqueue buffers.
 * @param readable	Number of readable buffers
 * @param writable	Number of writable buffers
 * @param cookie	Pointer to hold call back data
 *
 * @return Function status
 */
int virtqueue_add_buffer(struct virtqueue *vq, struct virtqueue_buf *buf_list,
			 int readable, int writable, void *cookie);

/**
 * @internal
 *
 * @brief Returns used buffers from VirtIO queue
 *
 * @param vq	Pointer to VirtIO queue control block
 * @param len	Length of conumed buffer
 * @param idx	Index of the buffer
 *
 * @return Pointer to used buffer
 */
void *virtqueue_get_buffer(struct virtqueue *vq, uint32_t *len, uint16_t *idx);

/**
 * @internal
 *
 * @brief Returns buffer available for use in the VirtIO queue
 *
 * @param vq		Pointer to VirtIO queue control block
 * @param avail_idx	Pointer to index used in vring desc table
 * @param len		Length of buffer
 *
 * @return Pointer to available buffer
 */
void *virtqueue_get_available_buffer(struct virtqueue *vq, uint16_t *avail_idx,
				     uint32_t *len);

/**
 * @internal
 *
 * @brief Returns consumed buffer back to VirtIO queue
 *
 * @param vq		Pointer to VirtIO queue control block
 * @param head_idx	Index of vring desc containing used buffer
 * @param len		Length of buffer
 *
 * @return Function status
 */
int virtqueue_add_consumed_buffer(struct virtqueue *vq, uint16_t head_idx,
				  uint32_t len);

/**
 * @internal
 *
 * @brief Disables callback generation
 *
 * @param vq	Pointer to VirtIO queue control block
 */
void virtqueue_disable_cb(struct virtqueue *vq);

/**
 * @internal
 *
 * @brief Enables callback generation
 *
 * @param vq	Pointer to VirtIO queue control block
 *
 * @return Function status
 */
int virtqueue_enable_cb(struct virtqueue *vq);

/**
 * @internal
 *
 * @brief Notifies other side that there is buffer available for it.
 *
 * @param vq	Pointer to VirtIO queue control block
 */
void virtqueue_kick(struct virtqueue *vq);

/**
 * @description: 此函数用于分配和初始化一个新的虚拟队列结构体
 * @param {unsigned int} num_desc_extra  额外描述符信息结构（struct vq_desc_extra）的数量
 * @return {*}
 */
static inline struct virtqueue *virtqueue_allocate(unsigned int num_desc_extra)
{
	struct virtqueue *vqs;
	uint32_t vq_size = sizeof(struct virtqueue) + num_desc_extra * sizeof(struct vq_desc_extra);

	vqs = (struct virtqueue *)metal_allocate_memory(vq_size);
	if (vqs) {
		memset(vqs, 0x00, vq_size);
	}

	return vqs;
}

/**
 * @internal
 *
 * @brief Frees VirtIO queue resources
 *
 * @param vq	Pointer to VirtIO queue control block
 */
void virtqueue_free(struct virtqueue *vq);

/**
 * @internal
 *
 * @brief Dumps important virtqueue fields , use for debugging purposes
 *
 * @param vq	Pointer to VirtIO queue control block
 */
void virtqueue_dump(struct virtqueue *vq);

void virtqueue_notification(struct virtqueue *vq);

/**
 * @internal
 *
 * @brief Returns vring descriptor size
 *
 * @param vq	Pointer to VirtIO queue control block
 *
 * @return Descriptor length
 */
uint32_t virtqueue_get_desc_size(struct virtqueue *vq);

uint32_t virtqueue_get_buffer_length(struct virtqueue *vq, uint16_t idx);
void *virtqueue_get_buffer_addr(struct virtqueue *vq, uint16_t idx);

/**
 * @brief Test if virtqueue is empty
 *	此函数检测一个虚拟队列是否为空
 * @param vq	Pointer to VirtIO queue control block
 *
 * @return 1 if virtqueue is empty, 0 otherwise
 */
static inline int virtqueue_empty(struct virtqueue *vq)
{
	//虚拟队列被认为是空的，当其空闲描述符数量 (vq_free_cnt) 等于队列中的条目数 (vq_nentries) 时
	return (vq->vq_nentries == vq->vq_free_cnt);
}

/**
 * @brief Test if virtqueue is full
 *	此函数检测一个虚拟队列是否已满
 * @param vq	Pointer to VirtIO queue control block
 *
 * @return 1 if virtqueue is full, 0 otherwise
 */
static inline int virtqueue_full(struct virtqueue *vq)
{
	return (vq->vq_free_cnt == 0);
}

#if defined __cplusplus
}
#endif

#endif				/* VIRTQUEUE_H_ */
