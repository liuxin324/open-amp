
# matrix_multiply 

这个自述文件介绍了 OpenAMP 的 matrix_multiply 演示项目。matrix_multiply 演示了一个处理器生成两个矩阵并将它们发送给另一个处理器，另一个处理器计算矩阵乘法并返回结果矩阵。

目前，它实现了 Linux 生成矩阵，裸机计算矩阵乘法并发送回结果
## Compilation

### Baremetal Compilation

选项 WITH_MATRIX_MULTIPLY 用于控制是否构建应用程序。
默认情况下，当 WITH_APPS 开启时，此选项为 ON.

这里是一个示例:

```
$ cmake ../open-amp -DCMAKE_TOOLCHAIN_FILE=zynq7_generic -DWITH_OBSOLETE=on -DWITH_APPS=ON
```

### Linux Compilation

#### Linux Kernel Compilation
你需要手动编译以下内核模块与你的 Linux 内核（请参考 Linux 内核文档了解如何添加内核模块）:

* 你机器的 remoteproc 内核驱动
* 如果你想在 Linux 用户空间运行 matrix_multiply 应用，编译 `obsolete/system/linux/kernelspace/rpmsg_user_dev_driver`
* 如果你想在 Linux 内核空间运行 matrix_multiply 应用，编译 `obsolete/apps/matrix_multiply/system/linux/kernelspace/rpmsg_mat_mul_kern_app`

#### Linux Userspace Compliation
* 将 `obsolete/apps/matrix_multiply/system/linux/userspace/mat_mul_demo` 编译进你的 Linux 操作系统.
* 如果你运行的是作为 remoteproc 远端的通用（裸机）系统，以及作为 remoteproc 宿主的 Linux，请也将构建好的通用 matrix_multiply 可执行文件添加到你的 Linux 操作系统的固件中.

## Run the Demo

### Load the Demo
Linux 启动后,
* 加载机器 remoteproc。如果 Linux 作为 remoteproc 宿主运行，你需要将另一个处理器的 matrix_multiply 二进制文件作为固件参数传递给 remoteproc 模块.
* 如果你运行 Linux 内核应用演示，加载 `rpmsg_mat_mul_kern_app` 模块，你将看到内核应用生成两个矩阵发送给另一个处理器，并输出另一个处理器返回的结果矩阵.
* 如果你运行用户空间应用演示，加载 `rpmsg_user_dev_driver` 模块.
* 如果你运行用户空间应用演示 `mat_mul_demo`，你将在控制台上看到类似的输出:
```
****************************************
Please enter command and press enter key
****************************************
1 - Generates random 6x6 matrices and transmits them to remote core over rpmsg
..
2 - Quit this application ..
CMD>
```
* Input `1` to run the matrix multiplication.
* Input `2` to exit the application.

After you run the demo, you will need to unload the kernel modules.

### Unload the Demo
* If you run the userspace application demo, unload the `rpmsg_user_dev_driver` module.
* If you run the kernelspace application demo, unload the `rpmsg_mat_mul_kern_app` module.
* Unload the machine remoteproc driver.

