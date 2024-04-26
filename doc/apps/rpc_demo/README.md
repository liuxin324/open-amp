
# rpc_demo 

这个自述文件介绍了 OpenAMP 的 rpc_demo 演示项目。rpc_demo 演示了一个处理器如何使用另一个处理器的 UART，并在另一个处理器的文件系统上通过文件操作创建文件。

目前，它实现了运行通用（裸机）应用的处理器访问 Linux 上的设备.

## Compilation

### 裸机编译

选项 WITH_RPC_DEMO 用于控制是否构建应用程序。默认情况下，当 WITH_APPS 开启时，此选项为 ON.

这里是一个示例:

```
$ cmake ../open-amp -DCMAKE_TOOLCHAIN_FILE=zynq7_generic -DWITH_OBSOLETE=on -DWITH_APPS=ON
```

### Linux 编译

#### Linux Kernel Compilation

你需要手动编译以下内核模块与你的 Linux 内核（请参考 Linux 内核文档了解如何添加内核模块）:

* 你机器的 remoteproc 内核驱动
* `obsolete/apps/rpc_demo/system/linux/kernelspace/rpmsg_proxy_dev_driver`

#### Linux Userspace Compliation
* 将 `obsolete/apps/rpc_demo/system/linux/userspace/proxy_app` 编译进你的 Linux 操作系统.
* 将构建好的通用 `rpc_demo` 可执行文件添加到你的 Linux 操作系统的固件中.

## Run the Demo
Linux 启动后，如下运行 `proxy_app`:
```
# proxy_app [-m REMOTEPROC_MODULE] [-f PATH_OF_THE_RPC_DEMO_FIRMWARE]
```

演示应用程序将加载 remoteproc 模块，然后是 proxy rpmsg 模块，将从另一个处理器发送的消息输出，将控制台输入返回给另一个处理器。当演示应用程序退出时，它将卸载内核模块.
