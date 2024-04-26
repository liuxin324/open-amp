
# echo_test
这个自述文件介绍了 OpenAMP 的 echo_test 演示项目。echo_test 演示了一个处理器向另一个处理器发送消息，另一个处理器将消息回显。发送消息的处理器将验证回显消息。

目前，它实现了 Linux 发送消息，裸机回显.

## Compilation

### Baremetal Compilation
选项 `WITH_ECHO_TEST `用于控制是否构建应用程序。
默认情况下，当 `WITH_APPS `开启时，此选项为`ON`.

Here is an example:

```
$ cmake ../open-amp -DCMAKE_TOOLCHAIN_FILE=zynq7_generic -DWITH_OBSOLETE=on -DWITH_APPS=ON
```

### Linux Compilation

#### Linux Kernel Compilation
You will need to manually compile the following kernel modules with your Linux kernel (Please refer to Linux kernel documents for how to add kernel module):

* Your machine's remoteproc kernel driver
* `obsolete/apps/echo_test/system/linux/kernelspace/rpmsg_user_dev_driver` if you want to run the echo_test app in Linux user space.
* `obsolete/system/linux/kernelspace/rpmsg_echo_test_kern_app` if you want to run the echo_test app in Linux kernel space.

#### Linux Userspace Compliation
* Compile `obsolete/apps/echo_test/system/linux/userspace/echo_test` into your Linux OS.
* 如果你运行的是作为 remoteproc 远端的通用（裸机）系统，以及作为 remoteproc 宿主的 Linux，请也将构建好的通用 echo_test 可执行文件添加到你的 Linux 操作系统的固件中.

## Run the Demo

### Load the Demo
After Linux boots,
* 加载机器 remoteproc。如果 Linux 作为 remoteproc 宿主运行，你需要将另一个处理器的 echo_test 二进制文件作为固件参数传递给 remoteproc 模块.
* 如果你运行 Linux 内核应用演示，加载 rpmsg_echo_test_kern_app 模块。你将看到内核应用向远端发送消息，远端回复消息，内核应用将验证结果.
* 如果你运行用户空间应用演示，加载 rpmsg_user_dev_driver 模块.
* 如果你运行用户空间应用演示，你将在控制台上看到类似的输出:
```
****************************************
 Please enter command and press enter key
 ****************************************
 1 - Send data to remote core, retrieve the echo and validate its integrity ..
 2 - Quit this application ..
 CMD>
```
* Input `1` to send packages.
* Input `2` to exit the application.

After you run the demo, you will need to unload the kernel modules.

### Unload the Demo
* If you run the userspace application demo, unload the `rpmsg_user_dev_driver` module.
* If you run the kernelspace application demo, unload the `rpmsg_echo_test_kern_app` module.
* Unload the machine remoteproc driver.

