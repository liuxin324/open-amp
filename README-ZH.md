# open-amp

这个仓库是开放异构多处理（Open Asymmetric Multi Processing，OpenAMP）框架项目的主页。
OpenAMP 框架提供了软件组件，使得开发者能够为异构多处理系统（AMP）开发软件应用。
该框架提供以下关键功能：

1. 为远程计算资源及其关联的软件上下文提供生命周期管理和处理器间通信能力。
2. 提供一个独立的库，可以用于实时操作系统（RTOS）和裸机（Baremetal）软件环境。
3. 与上游 Linux remoteproc 和 rpmsg 组件兼容。
4. 支持以下 AMP 配置：
  a. Linux 主机/通用（裸机）远程
  b. 通用（裸机）主机/Linux 远程
5. 代理基础设施和提供的示例演示了主机上的代理处理来自基于裸机的远程上下文的 printf、scanf、open、close、read、write 调用的能力。

## OpenAMP Source Structure
```
|- lib/
|  |- virtio/     # virtio 实现
|  |- rpmsg/      # rpmsg 实现
|  |- remoteproc/ # remoteproc 实现
|  |- proxy/      # 在另一处理器上实现对某一处理器设备的文件操作访问
|  |
|- apps/        # 演示/测试应用程序
|  |- examples/ # 使用 OpenAMP 框架的应用示例
|  |- machine/  # 应用程序可以共享的机器通用文件
|  |            # 每个应用程序可以自行决定是否使用这些文件.
|  |- system/   # 应用程序可以共享的系统通用文件
|               # 每个应用程序可以自行决定是否使用这些文件.
|- cmake        # CMake 文件
|- script       # 辅助脚本（例如 checkpatch）供贡献者使用.
```

OpenAMP 库 libopen_amp 由 ` lib/` 下的以下目录组成：
*   `virtio/`
*   `rpmsg/`
*   `remoteproc/`
*   `proxy/`

OpenAMP 的系统/机器支持已经移动到 libmetal，即 `apps/` 目录中的系统/机器层用于系统应用程序初始化和资源表定义.

### libmetal APIs used in OpenAMP

以下是 OpenAMP 使用的 libmetal APIs，如果你想将 OpenAMP 移植到你的系统，你需要在 libmetal 的 `lib/system/<SYS>`目录中实现以下 libmetal APIs：

* alloc，用于内存分配和释放
* cache，用于刷新缓存和使缓存无效
* io，用于内存映射。OpenAMP 需要内存映射以访问 vrings 和划分出的内存。
* irq，用于 IRQ 处理函数注册、IRQ 禁用/启用和全局 IRQ 处理。
* mutex
* shmem（对于 RTOS，你通常可以使用 lib/system/generic/ 中的实现）
* sleep，目前，OpenAMP 只需要微秒级别的睡眠，当 OpenAMP 无法获得发送消息的缓冲区时，它会调用此函数睡眠，然后再试一次。
* time，用于时间戳
* init，用于 libmetal 初始化。
* atomic

当你为你的系统移植 libmetal 时，请参考 `lib/system/generic`.

如果你使用的编译器不是 GNU gcc，请参考 `lib/compiler/gcc/` 目录下的内容，以将 libmetal 移植到你的编译器。目前，OpenAMP 需要在 `lib/compiler/gcc/atomic.h` 中定义的原子操作。


## OpenAMP Compilation

OpenAMP 使用 CMake 来编译库和演示应用程序。OpenAMP 需要 libmetal 库。目前，你需要在编译 OpenAMP 库之前单独下载并编译 libmetal 库。将来，我们将尝试将 libmetal 作为 OpenAMP 的子模块，以简化这一流程.

一些 CMake 选项可用于允许用户自定义 OpenAMP 库以适应其项目:

WITH_PROXY（默认关闭）：在库中包含代理支持。
WITH_APPS（默认关闭）：与示例应用程序一起构建。
WITH_PROXY_APPS（默认关闭）：与代理示例应用程序一起构建。
WITH_VIRTIO_DRIVER（默认开启）：启用 virtio 驱动时构建。
如果仅实现远程模式，此选项可以设置为关闭。
WITH_VIRTIO_DEVICE（默认开启）：启用 virtio 设备时构建。
如果仅实现驱动模式，此选项可以设置为关闭。
WITH_STATIC_LIB（默认开启）：以静态库的形式构建。
WITH_SHARED_LIB（默认开启）：以共享库的形式构建。
WITH_ZEPHYR（默认关闭）：作为 zephyr 库构建 open-amp。在 Zephyr 环境中，此选项是必需的。
WITH_DCACHE_VRINGS（默认关闭）：在 vrings 上启用数据缓存操作时构建。
WITH_DCACHE_BUFFERS（默认关闭）：在缓冲区上启用数据缓存操作时构建。
WITH_DCACHE_RSC_TABLE（默认关闭）：在资源表上启用数据缓存操作时构建。
WITH_DCACHE（默认关闭）：启用所有缓存操作时构建。当设置为开启时，vrings、缓冲区和资源表的缓存操作将被启用。
RPMSG_BUFFER_SIZE（默认 512）：调整 RPMsg 缓冲区的大小。
RPMsg 大小的默认值与 Linux 内核硬编码值兼容。如果你的 AMP 配置是 Linux 内核主机/ OpenAMP 远程，不应使用此选项。


### 为 Zephyr 编译 OpenAMP 的示例

 [Zephyr open-amp repo](https://github.com/zephyrproject-rtos/open-amp) 仓库 为 Zephyr 项目实现了 open-amp 库。它主要是这个仓库的一个分支，增加了一些用于在 Zephyr 项目中集成的附加功能。编译 Zephyr 项目的 OpenAMP 的标准方式是使用 Zephyr 构建环境。请参考 [Zephyr OpenAMP samples](https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/subsys/ipc)了解示例，并参考[Zephyr documentation](https://docs.zephyrproject.org/latest/) 了解构建过程

The [Zephyr open-amp repo](https://github.com/zephyrproject-rtos/open-amp)

### 为 Linux 进程间通信编译 OpenAMP 的示例:

* 在你的 Linux 主机上安装 libsysfs devel 和 libhugetlbfs devel 包.
* 如下在你的主机上构建 libmetal 库:

    ```
        $ mkdir -p build-libmetal
        $ cd build-libmetal
        $ cmake <libmetal_source>
        $ make VERBOSE=1 DESTDIR=<libmetal_install> install
    ```

* 如下在你的主机上构建 OpenAMP 库:

        $ mkdir -p build-openamp
        $ cd build-openamp
        $ cmake <openamp_source> -DCMAKE_INCLUDE_PATH=<libmetal_built_include_dir> \
              -DCMAKE_LIBRARY_PATH=<libmetal_built_lib_dir> [-DWITH_APPS=ON]
        $ make VERBOSE=1 DESTDIR=$(pwd) install

OpenAMP 库将被生成到 `build/usr/local/lib` 目录，头文件将被生成到 `build/usr/local/include`目录，应用程序可执行文件将被生成到 `build/usr/local/bin`目录

* cmake 选项`-DWITH_APPS=ON`是为了构建演示应用程序.
* 如果你使用 `-DWITH_APPS=ON`构建了演示程序，你可以如下在你的 Linux 主机上尝试它们

  * rpmsg echo demo:
    ```
    # Start echo test server to wait for message to echo
    $ sudo LD_LIBRARY_PATH=<openamp_built>/usr/local/lib:<libmetal_built>/usr/local/lib \
       build/usr/local/bin/rpmsg-echo-shared
    # Run echo test to send message to echo test server
    $ sudo LD_LIBRARY_PATH=<openamp_built>/usr/local/lib:<libmetal_built>/usr/local/lib \
       build/usr/local/bin/rpmsg-echo-ping-shared 1
    ```

  * rpmsg echo demo with the nocopy API:
    ```
    # Start echo test server to wait for message to echo
    $ sudo LD_LIBRARY_PATH=<openamp_built>/usr/local/lib:<libmetal_built>/usr/local/lib \
       build/usr/local/bin/rpmsg-nocopy-echo-shared
    # Run echo test to send message to echo test server
    $ sudo LD_LIBRARY_PATH=<openamp_built>/usr/local/lib:<libmetal_built>/usr/local/lib \
       build/usr/local/bin/rpmsg-nocopy-ping-shared 1
    ```

###  Example to compile Zynq UltraScale+ MPSoC R5 generic(baremetal) remote:
* build libmetal library on your host as follows:
  * Create your on cmake toolchain file to compile libmetal for your generic
    (baremetal) platform. Here is the example of the toolchain file:

    ```
        set (CMAKE_SYSTEM_PROCESSOR "arm"              CACHE STRING "")
        set (MACHINE "zynqmp_r5" CACHE STRING "")

        set (CROSS_PREFIX           "armr5-none-eabi-" CACHE STRING "")
        set (CMAKE_C_FLAGS          "-mfloat-abi=soft -mcpu=cortex-r5 -Wall -Werror -Wextra \
           -flto -Os -I/ws/xsdk/r5_0_bsp/psu_cortexr5_0/include" CACHE STRING "")

        SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
        SET(CMAKE_AR  "gcc-ar" CACHE STRING "")
        SET(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
        SET(CMAKE_C_ARCHIVE_FINISH   true)

        include (cross-generic-gcc)
    ```

  * Compile libmetal library:

    ```
        $ mkdir -p build-libmetal
        $ cd build-libmetal
        $ cmake <libmetal_source> -DCMAKE_TOOLCHAIN_FILE=<toolchain_file>
        $ make VERBOSE=1 DESTDIR=<libmetal_install> install
    ```

* build OpenAMP library on your host as follows:
  * Create your on cmake toolchain file to compile openamp for your generic
    (baremetal) platform. Here is the example of the toolchain file:
    ```
        set (CMAKE_SYSTEM_PROCESSOR "arm" CACHE STRING "")
        set (MACHINE                "zynqmp_r5" CACHE STRING "")
        set (CROSS_PREFIX           "armr5-none-eabi-" CACHE STRING "")
        set (CMAKE_C_FLAGS          "-mfloat-abi=soft -mcpu=cortex-r5 -Os -flto \
          -I/ws/libmetal-r5-generic/usr/local/include \
          -I/ws/xsdk/r5_0_bsp/psu_cortexr5_0/include" CACHE STRING "")
        set (CMAKE_ASM_FLAGS        "-mfloat-abi=soft -mcpu=cortex-r5" CACHE STRING "")
        set (PLATFORM_LIB_DEPS      "-lxil -lc -lm" CACHE STRING "")
        SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
        SET(CMAKE_AR  "gcc-ar" CACHE STRING "")
        SET(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
        SET(CMAKE_C_ARCHIVE_FINISH   true)
        set (CMAKE_FIND_ROOT_PATH /ws/libmetal-r5-generic/usr/local/lib \
            /ws/xsdk/r5_bsp/psu_cortexr5_0/lib )

        include (cross_generic_gcc)
    ```

  * We use cmake `find_path` and `find_library` to check if libmetal includes
    and libmetal library is in the includes and library search paths. However,
    for non-linux system, it doesn't work with `CMAKE_INCLUDE_PATH` and
    `CMAKE_LIBRARY_PATH` variables, and thus, we need to specify those paths
    in the toolchain file with `CMAKE_C_FLAGS` and `CMAKE_FIND_ROOT_PATH`.
* Compile the OpenAMP library:

    ```
    $ mkdir -p build-openamp
    $ cd build-openamp
    $ cmake <openamp_source> -DCMAKE_TOOLCHAIN_FILE=<toolchain_file>
    $ make VERBOSE=1 DESTDIR=$(pwd) install
    ```

The OpenAMP library will be generated to `build/usr/local/lib` directory,
headers will be generated to `build/usr/local/include` directory, and the
applications executable will be generated to `build/usr/local/bin`
directory.


### Example to compile OpenAMP Linux Userspace for Zynq UltraScale+ MPSoC
We can use yocto to build the OpenAMP Linux userspace library and application.
open-amp and libmetal recipes are in this yocto layer:
https://github.com/OpenAMP/meta-openamp
* Add the `meta-openamp` layer to your layers in your yocto build project's `bblayers.conf` file.
* Add `libmetal` and `open-amp` to your packages list. E.g. add `libmetal` and `open-amp` to the
  `IMAGE_INSTALL_append` in the `local.conf` file.
* You can also add OpenAMP demos Linux applications packages to your yocto packages list. OpenAMP
  demo examples recipes are also in `meta-openamp`:
  https://github.com/OpenAMP/meta-openamp/tree/master/recipes-openamp/rpmsg-examples

In order to user OpenAMP(RPMsg) in Linux userspace, you will need to have put the IPI device,
  vring memory and shared buffer memory to your Linux kernel device tree. The device tree example
  can be found here:
  https://github.com/OpenAMP/open-amp/blob/main/apps/machine/zynqmp/openamp-linux-userspace.dtsi

## Version
The OpenAMP version follows the set of rule proposed in
[Semantic Versioning specification](https://semver.org/).

## Supported System and Machines
For now, it supports:
* Zynq generic remote
* Zynq UltraScale+ MPSoC R5 generic remote
* Linux host OpenAMP between Linux userspace processes
* Linux userspace OpenAMP RPMsg host
* Linux userspace OpenAMP RPMsg remote
* Linux userspace OpenAMP RPMsg and MicroBlaze bare metal remote

## Known Limitations:
1. In case of OpenAMP on Linux userspace for inter processors communication,
   it only supports static vrings and shared buffers.
2. `sudo` is required to run the OpenAMP demos between Linux processes, as
   it doesn't work on some systems if you are normal users.

## How to contribute:
As an open-source project, we welcome and encourage the community to submit patches directly to the
project. As a contributor you  should be familiar with common developer tools such as Git and CMake,
and platforms such as GitHub.
Then following points should be rescpected to facilitate the review process.

### Licencing
Code is contributed to the Linux kernel under a number of licenses, but all code must be compatible
with version the [BSD License](https://github.com/OpenAMP/open-amp/blob/main/LICENSE.md), which is
the license covering the OpenAMP distribution as a whole. In practice, use the following tag
instead of the full license text in the individual files:

    ```
    SPDX-License-Identifier:    BSD-3-Clause
    SPDX-License-Identifier:    BSD-2-Clause
    ```
### Signed-off-by
Commit message must contain Signed-off-by: line and your email must match the change authorship
information. Make sure your .gitconfig is set up correctly:

    ```
    git config --global user.name "first-name Last-Namer"
    git config --global user.email "yourmail@company.com"
    ```
### gitlint
Before you submit a pull request to the project, verify your commit messages meet the requirements.
The check can be  performed locally using the the gitlint command.

Run gitlint locally in your tree and branch where your patches have been committed:

      ```gitlint```
Note, gitlint only checks HEAD (the most recent commit), so you should run it after each commit, or
use the --commits option to specify a commit range covering all the development patches to be
submitted.

### Code style
In general, follow the Linux kernel coding style, with the following exceptions:

* Use /**  */ for doxygen comments that need to appear in the documentation.

The Linux kernel GPL-licensed tool checkpatch is used to check coding style conformity.Checkpatch is
available in the scripts directory.

To check your \<n\> commits in your git branch:
   ```
   ./scripts/checkpatch.pl --strict  -g HEAD-<n>

   ```
### Send a pull request
We use standard github mechanism for pull request. Please refer to github documentation for help.

## Communication and Collaboration
[Subscribe](https://lists.openampproject.org/mailman3/lists/openamp-rp.lists.openampproject.org/) to
the OpenAMP mailing list(openamp-rp@lists.openampproject.org).

For more details on the framework please refer to the
[OpenAMP Docs](https://openamp.readthedocs.io/en/latest/).
