# RPMsg Design Document

RPMsg 是一个允许两个处理器之间进行通信的框架。OpenAMP库中的RPMsg实现基于virtio。它遵循RPMsg Linux内核实现的规范。它定义了在两个处理器上运行的应用程序之间建立和拆除通信的握手协议。

## RPMsg 用户API流程图
### RPMsg Static Endpoint
![Static Endpoint](img/coprocessor-rpmsg-static-ep.png)
### Binding Endpoint Dynamically with Name Service
![Binding Endpoint Dynamically with Name Service](img/coprocessor-rpmsg-ns.png)
### Creating Endpoint Dynamically with Name Service
![Creating Endpoint Dynamically with Name Service](img/coprocessor-rpmsg-ns-dynamic.png)

## RPMsg 用户APIs
* 初始化共享缓冲池的RPMsg virtio驱动（RPMsg virtio设备不需要使用此API）：
  ```
  void rpmsg_virtio_init_shm_pool(struct rpmsg_virtio_shm_pool *shpool,
				  void *shbuf, size_t size)
  ```
* 初始化RPMsg virtio设备:
  ```
  int rpmsg_init_vdev(struct rpmsg_virtio_device *rvdev,
		      struct virtio_device *vdev,
		      rpmsg_ns_bind_cb ns_bind_cb,
		      struct metal_io_region *shm_io,
		      struct rpmsg_virtio_shm_pool *shpool)
  ```
* 反初始化RPMsg virtio设备:
  ```
  void rpmsg_deinit_vdev(struct rpmsg_virtio_device *rvdev)`
  ```
* 从RPMsg virtio设备获取RPMsg设备:
  ```
  struct rpmsg_device *rpmsg_virtio_get_rpmsg_device(struct rpmsg_virtio_device *rvdev)
  ```
### RPMsg virtio端点APIs
* 创建RPMsg端点:
  ```
  int rpmsg_create_ept(struct rpmsg_endpoint *ept,
		       struct rpmsg_device *rdev,
		       const char *name, uint32_t src, uint32_t dest,
		       rpmsg_ept_cb cb, rpmsg_ns_unbind_cb ns_unbind_cb)
  ```
* 销毁RPMsg端点:
  ```
  void rpmsg_destroy_ept(struct rpsmg_endpoint *ept)
  ```
* 检查本地RPMsg端点是否已绑定到远程，并准备发送消息:
  ```
  int is_rpmsg_ept_ready(struct rpmsg_endpoint *ept)
  ```
### RPMsg 消息APIs
* 使用RPMsg端点默认绑定发送消息:
  ```
  int rpmsg_send(struct rpmsg_endpoint *ept, const void *data, int len)
  ```
* 使用RPMsg端点发送消息，指定目的地址:
  ```
  int rpmsg_sendto(struct rpmsg_endpoint *ept, void *data, int len,
		   uint32_t dst)
  ```
* 使用RPMsg端点发送消息，明确指定源地址和目的地址:
  ```
  int rpmsg_send_offchannel(struct rpmsg_endpoint *ept,
			    uint32_t src, uint32_t dst,
			    const void *data, int len)
  ```
* 如果没有可用的缓冲区，尝试使用RPMsg端点默认绑定发送消息，返回:
  ```
  int rpmsg_trysend(struct rpmsg_endpoint *ept, const void *data,
		    int len)
  ```
* 如果没有可用的缓冲区，尝试使用RPMsg端点发送消息，指定目的地址，返回:
  ```
  int rpmsg_trysendto(struct rpmsg_endpoint *ept, void *data, int len,
		      uint32_t dst)
  ```
* 如果没有可用的缓冲区，尝试使用RPMsg端点发送消息，明确指定源地址和目的地址，返回:
  ```
  int rpmsg_trysend_offchannel(struct rpmsg_endpoint *ept,
			       uint32_t src, uint32_t dst,
			       const void *data, int len)`
  ```

* 保留接收回调外部使用的rx缓冲区:
  ```
  void rpmsg_hold_rx_buffer(struct rpmsg_endpoint *ept, void *rxbuf)
  ```

* 释放由rpmsg_hold_rx_buffer()函数保留的rx缓冲区:
  ```
  void rpmsg_release_rx_buffer(struct rpmsg_endpoint *ept, void *rxbuf)

  ```
* 获取消息有效载荷的tx缓冲区.
  ```
  void *rpmsg_get_tx_payload_buffer(struct rpmsg_endpoint *ept,
          uint32_t *len, int wait)
  ```

* 使用通过调用rpmsg_get_tx_payload_buffer()函数获得的缓冲区，使用RPMsg端点默认绑定发送消息:
  ```
  int rpmsg_send_nocopy(struct rpmsg_endpoint *ept,
            const void *data, int len)
  ```

* 使用通过调用rpmsg_get_tx_payload_buffer()函数获得的缓冲区，使用RPMsg端点发送消息，指定目的地址:
  ```
  int rpmsg_sendto_nocopy(struct rpmsg_endpoint *ept,
              const void *data, int len, uint32_t dst)
  ```

* 使用通过调用rpmsg_get_tx_payload_buffer()函数获得的缓冲区，使用RPMsg端点发送消息，明确指定源地址和目的地址:
  ```
  int rpmsg_send_offchannel_nocopy(struct rpmsg_endpoint *ept, uint32_t src,
          uint32_t dst, const void *data, int len)
  ```

* 释放由rpmsg_get_tx_payload_buffer()函数保留的未使用的Tx缓冲区:
  ```
  int rpmsg_release_tx_buffer(struct rpmsg_endpoint *ept, void *txbuf)
  ```

## RPMsg 用户定义回调
* RPMsg端点消息接收回调:
  ```
  int (*rpmsg_ept_cb)(struct rpmsg_endpoint *ept, void *data,
		      size_t len, uint32_t src, void *priv)
  ```
* RPMsg名称服务绑定回调。如果用户定义了此类回调，当有名称服务公告到达时，如果没有找到注册的端点来绑定到此名称服务，它将调用此回调。如果未定义此回调，将丢弃此名称服务.:
  ```
  void (*rpmsg_ns_bind_cb)(struct rpmsg_device *rdev,
			   const char *name, uint32_t dest)
  ```
* RPMsg名称服务解绑回调。如果用户定义了此类回调，当有名称服务销毁到达时，将调用此回调.:
  ```
  void (*rpmsg_ns_unbind_cb)(struct rpmsg_device *rdev,
			   const char *name, uint32_t dest)
  ```
* RPMsg端点名称服务解绑回调。如果用户定义了此类回调，当有名称服务销毁到达时，将调用此回调，以通知用户应用程序远程已销毁服务.:
  ```
  void (*rpmsg_ns_unbind_cb)(struct rpmsg_endpoint *ept)
  ```
