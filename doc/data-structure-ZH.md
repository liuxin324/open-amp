Remoteproc data struct

===========================
remoteproc 结构体表示一个远程处理器实例，包含了用于管理和通信的各种元素，如互斥锁、资源表地址和长度、内存列表、虚拟设备列表等。这些元素使得主处理器能够管理远程处理器的启动、操作和通信。

remoteproc_virtio 结构体表示一个远程处理器中的 virtio 设备。virtio 是一种标准化的设备虚拟化技术，允许虚拟机与宿主机之间高效地交换数据。在这个上下文中，它被用于远程处理器和主处理器之间的高效数据交换。这个结构体包含了设备的私有数据、资源地址、I/O 区域、通知函数以及与之关联的 virtio 设备

* Representation of the remote processor instance:

```
struct remoteproc {
 metal_mutex_t lock;                /**< 互斥锁 */
 void *rsc_table;                   /**< 资源表的地址 */
 size_t rsc_len;                    /**< 资源表的长度 */
 struct metal_io_region *rsc_io;    /**< 资源表的 metal I/O 区域 */
 struct metal_list mems;            /**< remoteproc 内存 */
 struct metal_list vdevs;           /**< remoteproc 虚拟设备 */
 unsigned long bitmap;              /**< 用于 remoteproc 子设备的通知 ID 的位图 */
 const struct remoteproc_ops *ops;  /**< 指向 remoteproc 操作的指针 */
 metal_phys_addr_t bootaddr;        /**< 启动地址 */
 const struct loader_ops *loader;   /**< 图像加载器操作 */
 unsigned int state;                /**< 远程处理器状态 */
 void *priv;                        /**< remoteproc 私有数据 */
};

```
* Representation of the remote processor virtio device:
```

struct remoteproc_virtio {
 void *priv;                           /**< 私有数据 */
 void *vdev_rsc;                       /**< vdev 资源的地址 */
 struct metal_io_region *vdev_rsc_io;  /**< vdev_info 的 metal I/O 区域，可以为 NULL */
 rpvdev_notify_func notify;            /**< 通知函数 */
 struct virtio_device vdev;            /**< 关联的 virtio 设备 */
 struct metal_list node;               /**< remoteproc vdevs 列表的节点 */
};


```

Virtio Data struct
===========================
* Representation of a virtio device:
```
struct virtio_dev {
 uint32_t notifyid;                       /**< 在 virtio 总线上的唯一标识 */
 struct virtio_device_id id;              /**< 设备类型标识（用于将其与驱动程序匹配） */
 uint64_t features;                       /**< 设备和驱动程序双方都支持的特性。 */
 unsigned int role;                       /**< 它是 virtio 后端还是前端。 */
 virtio_dev_reset_cb reset_cb;            /**< 用户注册的设备回调 */
 const struct virtio_dispatch *func;      /**< 设备需要被重置时调用的回调函数-指向虚拟设备操作函数表的指针，这些操作函数定义了设备与驱动程序交互的具体方法 */
 void *priv;                              /**< 指向 virtio_device 私有数据的指针-用于存储设备运行时的状态信息或其他重要数据 */
 unsigned int vrings_num;                 /**< vrings 的数量 */
 struct virtio_vring_info *vrings_info;   /**< 与 virtio 设备关联的 vrings的信息 */
};

```

* Representation of a virtqueue local context:
```
struct virtqueue {
	struct virtio_device *vq_dev;            /**< 指向 virtio 设备的指针 */
	const char *vq_name;                     /**< virtqueue name */
	uint16_t vq_queue_index;                 /**< virtqueue index */
	uint16_t vq_nentries;                    /**< virtqueue 条目数 */
	void (*callback)(struct virtqueue *vq);  /**< virtqueue 回调函数 */
	void (*notify)(struct virtqueue *vq);    /**< 通知 virtqueue 远程函数 */
	struct vring vq_ring;                    /**< virtqueue 的环 */
	uint16_t vq_free_cnt;                    /**< 空闲计数 */
	uint16_t vq_queued_cnt;                  /**< 排队计数 */
	void *shm_io;                            /**< 指向共享缓冲区 I/O 区域的指针 */

	/*
  	* 描述符表中空闲链的头。如果没有空闲描述符，
  	* 这将设置为 VQ_RING_DESC_CHAIN_END。
  	*/
	uint16_t vq_desc_head_idx;

	/*
	* 在 used 表中最后消费的描述符，
	* 落后于 vq_ring.used->idx。
	*/
	uint16_t vq_used_cons_idx;

	/*
	* 在 available 表中最后消费的描述符 -
	* 由消费者端使用。
	*/
	uint16_t vq_available_idx;

	/*
	* 在回调期间由宿主端使用。Cookie
	* 保存从另一端收到的缓冲区的地址。
	* 此结构中的其他字段当前未使用。
	* 我们需要吗?
	*/
	struct vq_desc_extra {
		void *cookie;
		struct vring_desc *indirect;
		uint32_t indirect_paddr;
		uint16_t ndescs;
	} vq_descx[0];
};
```

* 在虚拟I/O设备 (VIRTIO) 版本1.1中定义的共享virtqueue结构的表示:
```
struct vring {
	unsigned int num;           /**< vring 的缓冲区数量 */
	struct vring_desc *desc;    /**< 指向缓冲区描述符的指针 */
	struct vring_avail *avail;  /**< 指向可用描述符头环的指针*/
	struct vring_used *used;    /**< 指向已用描述符头环的指针 */
};
```
RPMsg virtio Data struct
===========================
* Representation of a RPMsg virtio device:
```
struct rpmsg_virtio_device {
	struct rpmsg_device rdev;               /**< 关联的 rpmsg 设备 */
	struct rpmsg_virtio_config config;      /**< 包含 virtio 配置的结构体 */
	struct virtio_device *vdev;             /**< 指向 virtio 设备的指针 */
	struct virtqueue *rvq;                  /**< 指向接收 virtqueue 的指针 */
	struct virtqueue *svq;                  /**< 指向发送 virtqueue 的指针 */
	struct metal_io_region *shbuf_io;       /**< 指向共享缓冲区 I/O 区域的指针 */
	struct rpmsg_virtio_shm_pool *shpool;   /**< 指向共享缓冲区池的指针 */
};

```

RPMsg Data struct
===========================
* RPMsg设备的表示:
```
struct rpmsg_device {
	struct metal_list endpoints;                                   /**< 端点列表 */
	struct rpmsg_endpoint ns_ept;                                  /**< 名称服务端点 */
	unsigned long bitmap[metal_bitmap_longs(RPMSG_ADDR_BMP_SIZE)]; /**< 位图：端点地址分配表 */
	metal_mutex_t lock;                                            /**< rpmsg 管理的互斥锁 */
	rpmsg_ns_bind_cb ns_bind_cb;                                   /**< 没有本地端点等待绑定时，名称服务通告的回调处理程序 */
	struct rpmsg_device_ops ops;                                   /**< RPMsg 设备操作 */
	bool support_ns;                                               /**< 创建/销毁命名空间消息 */
};


* Representation of a local RPMsg endpoint associated to an unique address:
* RPMsg端点，它是通信的基本单元，每个端点都有一个本地地址和目标地址。端点可以关联一个回调函数，用于处理接收到的消息。此外，端点还可以包含用户定义的私有数据
struct rpmsg_endpoint {
	char name[SERVICE_NAME_SIZE];                                                     /**< 关联的名称服务 */
	struct rpmsg_virtio_dev *rvdev;                                                   /**< 指向 RPMsg virtio 设备的指针 */
	uint32_t addr;                                                                    /**< 端点本地地址 */
	uint32_t dest_addr;                                                               /**< 端点默认目标地址 */
	int (*cb)(struct rpmsg_endpoint *ept, void *data, size_t len, uint32_t addr);     /**< 端点回调 */
	void (*ns_unbind_cb)(struct rpmsg_endpoint *ept);                                 /**< 远程端点销毁回调 */
	struct metal_list node;                                                           /**< rpmsg_device 端点列表的节点 */
	void *priv;                                                                       /**< 用户私有数据 */
};

```
