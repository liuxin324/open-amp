/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $FreeBSD$
 */

#ifndef _VIRTIO_H_
#define _VIRTIO_H_

#include <openamp/virtqueue.h>
#include <metal/errno.h>
#include <metal/spinlock.h>

#if defined __cplusplus
extern "C" {
#endif

/* VirtIO device IDs. */
#define VIRTIO_ID_NETWORK        1UL  /* 网络设备 */
#define VIRTIO_ID_BLOCK          2UL  /* 块设备 */
#define VIRTIO_ID_CONSOLE        3UL  /* 控制台设备，用于虚拟机的输入/输出操作 */
#define VIRTIO_ID_ENTROPY        4UL  /* 熵源设备，提供随机数生成服务 */
#define VIRTIO_ID_BALLOON        5UL  /* 内存气球（balloon）设备，用于动态调整虚拟机的内存大小 */
#define VIRTIO_ID_IOMEMORY       6UL  /* 表示用于直接访问或共享 I/O 内存的设备 */
#define VIRTIO_ID_RPMSG          7UL  /* remote processor messaging */
#define VIRTIO_ID_SCSI           8UL  /* 虚拟 SCSI 设备 */
#define VIRTIO_ID_9P             9UL  /* 9P 文件系统，用于实现宿主机和虚拟机之间的文件共享 */
#define VIRTIO_ID_MAC80211_WLAN  10UL /* 表示虚拟的无线局域网设备，模拟 MAC802.11 协议的 WLAN 接口*/
#define VIRTIO_ID_RPROC_SERIAL   11UL /* 远程处理器串行通信设备，用于处理器间的串行通信 */
#define VIRTIO_ID_CAIF           12UL /* CAIF （Connected Application Interface）设备，用于移动通信中的应用层接口*/
#define VIRTIO_ID_MEMORY_BALLOON 13UL /* 另一种内存气球设备，用于调整和优化虚拟机内存使用*/
#define VIRTIO_ID_GPU            16UL /* 虚拟图形处理单元，用于虚拟环境中的图形渲染和加速 */
#define VIRTIO_ID_CLOCK          17UL /* 虚拟时钟设备，用于管理和同步虚拟机内部时间*/
#define VIRTIO_ID_INPUT          18UL /* 虚拟输入设备，如键盘和鼠标 */
#define VIRTIO_ID_VSOCK          19UL /* 虚拟套接字设备，用于宿主机和虚拟机间的通信 */
#define VIRTIO_ID_CRYPTO         20UL /* 虚拟加密设备，提供加密和哈希计算功能*/
#define VIRTIO_ID_SIGNAL_DIST    21UL /* 信号分发设备，用于在虚拟设备间分发信号*/
#define VIRTIO_ID_PSTORE         22UL /*  提供持久化存储功能的虚拟设备*/
#define VIRTIO_ID_IOMMU          23UL /* 输入输出内存管理单元，用于虚拟地址空间管理和设备隔离*/
#define VIRTIO_ID_MEM            24UL /* 代表通用内存设备，用于内存共享和管理*/
#define VIRTIO_ID_SOUND          25UL /* 虚拟声音设备，用于音频输入输出*/
#define VIRTIO_ID_FS             26UL /* 虚拟文件系统设备*/
#define VIRTIO_ID_PMEM           27UL /* 表示持久内存设备，用于访问 NVDIMM 类存储*/
#define VIRTIO_ID_RPMB           28UL /* 可重写只读内存块设备，用于安全存储操作*/
#define VIRTIO_ID_MAC80211_HWSIM 29UL /* 无线硬件模拟设备，用于模拟无线网络环境*/
#define VIRTIO_ID_VIDEO_ENCODER  30UL /* 视频编码器，用于视频压缩*/
#define VIRTIO_ID_VIDEO_DECODER  31UL /* 视频解码器，用于视频解压缩*/
#define VIRTIO_ID_SCMI           32UL /* 系统控制和管理接口设备，用于系统级控制和管理*/
#define VIRTIO_ID_NITRO_SEC_MOD  33UL /* Nitro 安全模块，用于提高虚拟环境的安全性*/
#define VIRTIO_ID_I2C_ADAPTER    34UL /* I2C 适配器，用于模拟 I2C 通信*/
#define VIRTIO_ID_WATCHDOG       35UL /* 看门狗计时器*/
#define VIRTIO_ID_CAN            36UL /* 控制器局域网络设备*/
#define VIRTIO_ID_PARAM_SERV     38UL /* 参数服务设备，用于配置和参数交换*/
#define VIRTIO_ID_AUDIO_POLICY   39UL /* 音频策略设备*/
#define VIRTIO_ID_BT             40UL /* 蓝牙设备*/
#define VIRTIO_ID_GPIO           41UL /* 通用输入输出设备，用于模拟 GPIO 接口*/
#define VIRTIO_ID_RDMA           42UL /*远程直接内存访问设备，用于高速数据传输*/
#define VIRTIO_DEV_ANY_ID        -1UL /*特殊的标识符，用于表示任何或未指定的 VirtIO 设备类型*/

/* Status byte for guest to report progress. */
/* Device Status Field -设备状态字段 */
// 2.1 Device Status Field
#define VIRTIO_CONFIG_STATUS_RESET       0x00  /* 设备处于重置状态，即设备尚未启动或被明确地重置到初始状态  */
#define VIRTIO_CONFIG_STATUS_ACK         0x01  /* ACKNOWLEDGE (1) -指示Guest OS已找到该设备并将其识别为有效的虚拟设备 */
#define VIRTIO_CONFIG_STATUS_DRIVER      0x02  /* DRIVER (2) -表示Guest OS知道如何驱动设备 */
#define VIRTIO_CONFIG_STATUS_DRIVER_OK   0x04  /* DRIVER_OK (4) - 指示驱动程序已设置并准备好驱动设备*/
#define VIRTIO_CONFIG_FEATURES_OK        0x08  /* FEATURES_OK (8) -表示驱动程序已经确认了它所理解的所有特征，并且特征协商完成 */
#define VIRTIO_CONFIG_STATUS_NEEDS_RESET 0x40  /* DEVICE_NEEDS_RESET (64) - 指示设备发生了无法恢复的错误 */
#define VIRTIO_CONFIG_STATUS_FAILED      0x80 /* FAILED (128) - 指示客户机出了问题，并且它已经放弃了该设备.这可能是内部错误，或者驱动程序由于某种原因不喜欢设备，甚至是设备操作期间的致命错误*/

/* Virtio device role */
#define VIRTIO_DEV_DRIVER	0UL /* 指定设备作为驱动方，通常是指宿主系统中负责管理和控制虚拟设备的部分 */
#define VIRTIO_DEV_DEVICE	1UL /* 指定设备作为被驱动方，通常是指虚拟机中模拟出来的设备 */

/*为了向后兼容，也定义了两个宏 VIRTIO_DEV_MASTER 和 VIRTIO_DEV_SLAVE，这两个宏分别被标记为 deprecated，
并分别映射到新的 VIRTIO_DEV_DRIVER 和 VIRTIO_DEV_DEVICE。这意味着，开发者应该使用新的宏定义，
而旧的宏定义保留是为了兼容已有的代码，但在将来可能会被移除。*/
#define VIRTIO_DEV_MASTER	deprecated_virtio_dev_master()
#define VIRTIO_DEV_SLAVE	deprecated_virtio_dev_slave()

/**
 * @description: __deprecated 是一个编译器特定的属性，用于标记函数、变量或类型声明为已弃用。
 * 当其他代码尝试使用被标记为 __deprecated 的函数或变量时，
 * 编译器会发出警告，通知开发者该函数或变量不再推荐使用，
 * 并可能在将来的版本中被移除。这有助于在保持向后兼容的同时，
 * 引导开发者转向使用新的、更优的接口或方法
 * static 关键字指定这个函数只能在定义它的文件内部被见到或调用。这有助于限制函数的作用域，避免在其他文件中的同名函数之间产生冲突。
 * inline 关键字建议编译器尝试内联该函数的实现，而不是通过常规的函数调用机制调用它。内联函数意味着函数的代码在每个调用点上直接展开，
 * 可以减少函数调用的开销，但会增加编译后的代码大小。编译器可以选择忽略此建议.
 */
__deprecated static inline int deprecated_virtio_dev_master(void)
{
	/* "VIRTIO_DEV_MASTER is deprecated, please use VIRTIO_DEV_DRIVER" */
	return VIRTIO_DEV_DRIVER;
}

__deprecated static inline int deprecated_virtio_dev_slave(void)
{
	/* "VIRTIO_DEV_SLAVE is deprecated, please use VIRTIO_DEV_DEVICE" */
	return VIRTIO_DEV_DEVICE;
}

#ifdef VIRTIO_MASTER_ONLY
#define VIRTIO_DRIVER_ONLY
#warning "VIRTIO_MASTER_ONLY is deprecated, please use VIRTIO_DRIVER_ONLY"
#endif

#ifdef VIRTIO_SLAVE_ONLY
#define VIRTIO_DEVICE_ONLY
#warning "VIRTIO_SLAVE_ONLY is deprecated, please use VIRTIO_DEVICE_ONLY"
#endif

/** @brief Virtio device identifier. */
/* 唯一标识一个 VirtIO 设备 */
struct virtio_device_id {
	/** Virtio subsystem device ID. */
	uint32_t device;
	/** Virtio subsystem vendor ID. */
	/*供应商ID，用于标识制造该设备的供应商*/
	uint32_t vendor;

	/** Virtio subsystem device version. */
	/*设备版本，表示特定供应商提供的设备的版本*/
	uint32_t version;
};


// ------------------------- 6.3 Legacy Interface: Reserved Feature Bits----
/*
 * Generate interrupt when the virtqueue ring is
 * completely used, even if we've suppressed them.
 */

/*
 * 这个特性位用于控制虚拟队列（virtqueue）的行为。当设置了这个特性位时，
 * 如果队列变为空（即所有的缓冲区都已经被设备处理完毕）
 * 即便之前对中断进行了抑制，设备也会生成一个中断
 */

#define VIRTIO_F_NOTIFY_ON_EMPTY (1 << 24)

/*
 * The guest should never negotiate this feature; it
 * is used to detect faulty drivers.
 */
/*这个特性位用作调试和错误检测。它不应该被虚拟机（guest）negotiate，如果设备看到这个特性被请求，
它可能表明驱动程序存在问题。*/
#define VIRTIO_F_BAD_FEATURE (1 << 30)

//---------------------------Transport Feature Bits ------
/*
 * Some VirtIO feature bits (currently bits 28 through 31) are
 * reserved for the transport being used (eg. virtio_ring), the
 * rest are per-device feature bits.
 */
/*
 * 定义了保留给传输层使用的特性位的范围，当前是从第 29 位到第 32 位（不包括第 32 位）。
 * 这些位对于特定的传输机制（如 virtio over PCI, virtio over MMIO）是保留的，
 * 用于表明传输层特有的能力和需求
*/
#define VIRTIO_TRANSPORT_F_START      28
#define VIRTIO_TRANSPORT_F_END        32

#ifdef VIRTIO_DEBUG
#include <metal/log.h>
/*
定义了一个断言宏，用于检查表达式（_exp）是否为真。如果不为真，使用 metal_log 打印一条紧急日志，
并调用 metal_assert 断言失败。这个宏在非调试模式下简化为直接调用 metal_assert，省略了日志记录的步骤
* 
*/
#define VIRTIO_ASSERT(_exp, _msg) do { \
		int exp = (_exp); \
		if (!(exp)) { \
			metal_log(METAL_LOG_EMERGENCY, \
				  "FATAL: %s - " _msg, __func__); \
			metal_assert(exp); \
		} \
	} while (0)
#else
#define VIRTIO_ASSERT(_exp, _msg) metal_assert(_exp)
#endif /* VIRTIO_DEBUG */

//定义了 VirtIO MMIO 环形队列的内存对齐要求，设置为 4096 字节
#define VIRTIO_MMIO_VRING_ALIGNMENT           4096
/*
定义了一个函数指针类型，指向的函数用于重置 VirtIO 设备。
这种类型的函数通常在设备需要被重置到初始状态时被调用，
例如在发生错误或重新初始化设备时。struct virtio_device *vdev 参数是指向需要被重置的 VirtIO 设备的指针
*/
typedef void (*virtio_dev_reset_cb)(struct virtio_device *vdev);

struct virtio_dispatch;

/** @brief Device features. */
/* 设备特性 */
struct virtio_feature_desc {
	/** Unique feature ID, defined in the virtio specification. */
	/*这是特性的唯一标识符，根据 VirtIO 规范定义。每个特性标识符代表了一种设备能力或配置选项，例如支持某种加速、某种高级数据处理能力等*/
	uint32_t vfd_val;

	/** Name of the feature (for debug). */
	/*这是特性名称的字符串表示*/
	const char *vfd_str;
};

/** @brief Virtio vring data structure */
struct virtio_vring_info {
	/** Virtio queue */
	/*指向 VirtIO queue的指针。*/
	struct virtqueue *vq;

	/** Vring alloc info */
	/*包含了分配给该环形队列的配置信息，如队列大小、内存布局等*/
	struct vring_alloc_info info;

	/** Vring notify id */
	/*通知标识符，用于标识当数据被放入队列时如何通知设备。每个环形队列都有一个唯一的通知ID*/
	uint32_t notifyid;

	/** Metal I/O region of the vring memory, can be NULL */
	/* 指向环形队列内存的 Metal I/O 区域的指针。metal_io_region 是一种抽象，用于表示设备的内存映射区域，可以为 NULL，表示某些情况下队列不需要特定的内存区域 */
	struct metal_io_region *io;
};

/** @brief Structure definition for virtio devices for use by the applications/drivers */
struct virtio_device {
	/** Unique position on the virtio bus */
	/*设备在 VirtIO 总线上的唯一位置或通知标识符，用于标识设备本身*/
	uint32_t notifyid;

	/** The device type identification used to match it with a driver */
	/*设备类型标识，用于匹配设备与相应的驱动程序。包含设备ID、供应商ID和设备版本，这有助于驱动程序识别并正确处理设备*/
	struct virtio_device_id id;

	/** The features supported by both ends. */
	/*表示双方支持的特性位掩码。这是设备和驱动程序协商过程中，双方共同支持特性的记录*/
	uint64_t features;

	/** If it is virtio backend or front end. */
	/*表示设备的角色，是作为 VirtIO 后端还是前端*/
	unsigned int role;

	/** User-registered device callback */
	/*用户注册的设备重置回调函数，用于在设备需要重置时执行*/
	virtio_dev_reset_cb reset_cb;

	/** Virtio dispatch table */
	/*指向 VirtIO 操作函数表的指针。这个分派表包含了设备操作所需的一系列函数，如队列创建、设备状态获取设置等*/
	const struct virtio_dispatch *func;

	/** Private data */
	/*指向私有数据的指针，可以被驱动程序用来存储任何设备特定的信息*/
	void *priv;

	/** Number of vrings */
	/*表示设备拥有的环形队列的数量*/
	unsigned int vrings_num;

	/** Pointer to the virtio vring structure */
	/*指向一个数组，包含每个环形队列的配置和状态信息。数组的大小由 vrings_num 确定 */
	struct virtio_vring_info *vrings_info;
};

/*
 * Helper functions.
 */

/**
 * @brief Get the name of a virtio device.
 *		  根据 VirtIO 设备的 ID 返回该设备的名称
 * @param devid Id of the device.
 *
 * @return pointer to the device name string if found, otherwise null.
 */
const char *virtio_dev_name(uint16_t devid);

/*已弃用-提供一个机制，用于描述一个 VirtIO 设备及其支持的特性*/
__deprecated void virtio_describe(struct virtio_device *dev, const char *msg,
				  uint32_t features,
				  struct virtio_feature_desc *feature_desc);

/**
 * @brief Virtio device dispatcher functions.
 *
 * Functions for virtio device configuration as defined in Rusty Russell's paper.
 * The virtio transport layers are expected to implement these functions in their respective codes.
 */
/* 
这个 virtio_dispatch 结构体定义了一系列回调函数，这些函数构成了 VirtIO 设备操作的接口。这个接口允许不同的 VirtIO 传输层（如 PCI、MMIO）以及设备类型（如网络、块存储设备）实现具体的操作逻辑
*/
struct virtio_dispatch {
	/** Create virtio queue instances. -创建 VirtIO 队列*/
	int (*create_virtqueues)(struct virtio_device *vdev,
				 unsigned int flags,
				 unsigned int nvqs, const char *names[],
				 vq_callback callbacks[],
				 void *callback_args[]);

	/** Delete virtio queue instances. -删除 VirtIO 队列 */
	void (*delete_virtqueues)(struct virtio_device *vdev);

	/** Get the status of the virtio device.- 获取 VirtIO 设备的当前状态*/
	uint8_t (*get_status)(struct virtio_device *dev);

	/** Set the status of the virtio device. -设置 VirtIO 设备的当前状态*/
	void (*set_status)(struct virtio_device *dev, uint8_t status);

	/** Get the feature exposed by the virtio device. -获取设备公开的特性位*/
	uint32_t (*get_features)(struct virtio_device *dev);

	/** Set the supported feature (virtio driver only). -设置驱动支持的特性 */
	void (*set_features)(struct virtio_device *dev, uint32_t feature);

	/**
	 * Set the supported feature negotiate between the \ref features parameter and features
	 * supported by the device (virtio driver only).
	 */
	/*协商双方支持的特性*/
	uint32_t (*negotiate_features)(struct virtio_device *dev,
				       uint32_t features);

	/**
	 * Read a variable amount from the device specific (ie, network)
	 * configuration region.
	 */
	/*从设备特定的配置区域读取数据*/
	void (*read_config)(struct virtio_device *dev, uint32_t offset,
			    void *dst, int length);

	/**
	 * Write a variable amount from the device specific (ie, network)
	 * configuration region.
	 */
	/*向设备特定的配置区域写入数据*/
	void (*write_config)(struct virtio_device *dev, uint32_t offset,
			     void *src, int length);

	/** Request a reset of the virtio device. 请求重置 VirtIO 设备*/
	void (*reset_device)(struct virtio_device *dev);

	/** Notify the other side that a virtio vring as been updated. 通知另一方 VirtIO 队列已经更新*/
	void (*notify)(struct virtqueue *vq);

	/** Customize the wait, when no Tx buffer is available (optional) 自定义等待机制，用于在没有可用的发送缓冲区时等待*/
	int (*wait_notified)(struct virtio_device *dev, struct virtqueue *vq);
};

/**
 * @brief Create the virtio device virtqueue.
 *
 * @param vdev			Pointer to virtio device structure.
 * @param flags			Create flag.
 * @param nvqs			The virtqueue number.
 * @param names			Virtqueue names.
 * @param callbacks		Virtqueue callback functions.
 * @param callback_args	Virtqueue callback function arguments.
 *
 * @return 0 on success, otherwise error code.
 */
int virtio_create_virtqueues(struct virtio_device *vdev, unsigned int flags,
			     unsigned int nvqs, const char *names[],
			     vq_callback callbacks[], void *callback_args[]);

/**
 * @brief Delete the virtio device virtqueue.
 * 	删除指定 VirtIO 设备的所有虚拟队列（virtqueues）
 * @param vdev	Pointer to virtio device structure.
 *
 */
static inline void virtio_delete_virtqueues(struct virtio_device *vdev)
{
	/*1.首先检查 vdev 是否为 NULL，以及 vdev->func 和 vdev->func->delete_virtqueues 是否存在，确保调用是安全的*/
	if (!vdev || !vdev->func || !vdev->func->delete_virtqueues)
		return;
	/*2.调用 delete_virtqueues 函数，它是 virtio_dispatch 结构中定义的回调函数，负责删除和释放与设备关联的所有虚拟队列*/
	vdev->func->delete_virtqueues(vdev);
}

/**
 * @brief Get device ID.
 *	获取 VirtIO 设备的设备ID
 * @param dev Pointer to device structure.
 *
 * @return Device ID value.
 */
static inline uint32_t virtio_get_devid(const struct virtio_device *vdev)
{
	if (!vdev)
		return 0;
	return vdev->id.device;
}

/**
 * @brief Retrieve device status.
 *	检索 VirtIO 设备的当前状态
 * @param dev		Pointer to device structure.
 * @param status	Pointer to the virtio device status.
 *
 * @return 0 on success, otherwise error code.
 */

static inline int virtio_get_status(struct virtio_device *vdev, uint8_t *status)
{
	/*如果 vdev 或 status 为 NULL，返回 -EINVAL（无效的参数）*/
	if (!vdev || !status)
		return -EINVAL;
	/*如果设备函数指针 get_status 不存在，返回 -ENXIO（无此设备或地址）*/
	if (!vdev->func || !vdev->func->get_status)
		return -ENXIO;

	*status = vdev->func->get_status(vdev);
	return 0;
}

/**
 * @brief Set device status.
 *	设置 VirtIO 设备的当前状态
 * @param dev		Pointer to device structure.
 * @param status	Value to be set as device status.
 *
 * @return 0 on success, otherwise error code.
 */
static inline int virtio_set_status(struct virtio_device *vdev, uint8_t status)
{
	if (!vdev)
		return -EINVAL;

	if (!vdev->func || !vdev->func->set_status)
		return -ENXIO;

	vdev->func->set_status(vdev, status);
	return 0;
}

/**
 * @brief Retrieve configuration data from the device.
 *	从 VirtIO 设备的配置区域读取数据
 * @param dev		Pointer to device structure.
 * @param offset	Offset of the data within the configuration area. 配置区域中的偏移量，指定从哪里开始读取数据
 * @param dst		Address of the buffer that will hold the data.  指向接收数据的缓冲区的指针
 * @param len		Length of the data to be retrieved.  要读取的数据长度
 *
 * @return 0 on success, otherwise error code.
 */
static inline int virtio_read_config(struct virtio_device *vdev,
				     uint32_t offset, void *dst, int len)
{
	if (!vdev || !dst)
		return -EINVAL;

	if (!vdev->func || !vdev->func->read_config)
		return -ENXIO;

	vdev->func->read_config(vdev, offset, dst, len);
	return 0;
}

/**
 * @brief Write configuration data to the device.
 *	向 VirtIO 设备的配置区写入数据
 * @param dev		Pointer to device structure.
 * @param offset	Offset of the data within the configuration area. 配置区域中的偏移量，指定从哪里开始写入数据
 * @param src		Address of the buffer that holds the data to write. 指向包含要写入数据的缓冲区的指针
 * @param len		Length of the data to be written. 
 *
 * @return 0 on success, otherwise error code.
 */
static inline int virtio_write_config(struct virtio_device *vdev,
				      uint32_t offset, void *src, int len)
{
	if (!vdev || !src)
		return -EINVAL;

	if (!vdev->func || !vdev->func->write_config)
		return -ENXIO;

	vdev->func->write_config(vdev, offset, src, len);
	return 0;
}

/**
 * @brief Get the virtio device features.
 *
 * @param dev		Pointer to device structure.
 * @param features	Pointer to features supported by both the driver and
 *			the device as a bitfield.
 *
 * @return 0 on success, otherwise error code.
 */
static inline int virtio_get_features(struct virtio_device *vdev,
				      uint32_t *features)
{
	if (!vdev || !features)
		return -EINVAL;

	if (!vdev->func || !vdev->func->get_features)
		return -ENXIO;

	*features = vdev->func->get_features(vdev);
	return 0;
}

/**
 * @brief Set features supported by the VIRTIO driver.
 *	设置VIRTIO驱动程序支持的特性
 * @param dev		Pointer to device structure.
 * @param features	Features supported by the driver as a bitfield.
 *
 * @return 0 on success, otherwise error code.
 */
static inline int virtio_set_features(struct virtio_device *vdev,
				      uint32_t features)
{
	if (!vdev)
		return -EINVAL;

	if (!vdev->func || !vdev->func->set_features)
		return -ENXIO;

	vdev->func->set_features(vdev, features);
	return 0;
}

/**
 * @brief Negotiate features between virtio device and driver.
 *	在virtio设备和驱动程序之间协商特性
 * @param dev			Pointer to device structure.
 * @param features		Supported features.
 * @param final_features	Pointer to the final features after negotiate.
 *
 * @return 0 on success, otherwise error code.
 */
static inline int virtio_negotiate_features(struct virtio_device *vdev,
					    uint32_t features,
					    uint32_t *final_features)
{
	if (!vdev || !final_features)
		return -EINVAL;

	if (!vdev->func || !vdev->func->negotiate_features)
		return -ENXIO;

	*final_features = vdev->func->negotiate_features(vdev, features);
	return 0;
}

/**
 * @brief Reset virtio device.
 *	重置 VirtIO 设备
 * @param vdev	Pointer to virtio_device structure.
 *
 * @return 0 on success, otherwise error code.
 */
static inline int virtio_reset_device(struct virtio_device *vdev)
{
	if (!vdev)
		return -EINVAL;

	if (!vdev->func || !vdev->func->reset_device)
		return -ENXIO;

	vdev->func->reset_device(vdev);
	return 0;
}

#if defined __cplusplus
}
#endif

#endif				/* _VIRTIO_H_ */
