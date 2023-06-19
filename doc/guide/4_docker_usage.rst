使用 Docker 搭建测试环境
------------------------

可使用 Docker 搭建 libsophon 的测试环境, 这里介绍环境的搭建流程和使用方法。

进行此部分操作前请参照 :doc:`安装libsophon <1_install>` 中内容在 host 端安装 sophon-driver！

Docker 测试环境搭建
~~~~~~~~~~~~~~~~~~~

安装 Docker
^^^^^^^^^^^^^^

参考 `官网教程 <https://docs.docker.com/engine/install/ubuntu/>`_ 进行安装:

.. code-block:: shell

    # 卸载 docker
    sudo apt-get remove docker docker.io containerd runc

    # 安装依赖
    sudo apt-get update
    sudo apt-get install \
            ca-certificates \
            curl \
            gnupg \
            lsb-release

    # 获取密钥
    sudo mkdir -p /etc/apt/keyrings
    curl -fsSL \
        https://download.docker.com/linux/ubuntu/gpg | \
        gpg --dearmor -o docker.gpg && \
        sudo mv -f docker.gpg /etc/apt/keyrings/

    # 添加 docker 软件包
    echo \
        "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
        https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | \
        sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

    # 安装 docker
    sudo apt-get update
    sudo apt-get install docker-ce docker-ce-cli containerd.io docker-compose-plugin

构建测试镜像及容器
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

构建镜像
""""""""

使用 dockerfile 构建测试所需的 Docker 镜像, dockerfile 位于 libsophon 文件夹下。构建镜像前请检查系统时间是否准确，错误的系统时间会导致构建过程中更新 package list 时证书验证失败。

.. code-block:: shell

    # 替换以下 path/to/libsophon 为实际 libsohon 路径
    cd path/to/libsophon
    sudo docker build -t image_name:image_version -f libsophon_dockerfile .

以上 ``image_name`` 和 ``image_version`` 可自定义。

运行完以上命令后, 可运行 ``docker images`` 命令核对构建结果, 命令输出应如下:

.. code-block:: shell

    $ sudo docker images
    REPOSITORY   TAG               IMAGE ID     CREATED          SIZE
    image_name      image_version  image_id        create_time        1.74GB

以上内容中 ``image_name`` 与 ``image_version`` 应与构建时设置内容一致, ``image_id`` 与 ``create_time`` 应与实际情况相符合。

创建容器
""""""""

在 Docker 容器中进行测试需要依赖 libsophon 动态库, 依赖可分以下两种方法建立:

1. 在host端安装sophon-libsophon和sophon-libsophon-dev, 然后通过文件映射将动态库映射进 Docker 容器中;

.. code-block:: shell

    # sophon-libsophon 和 sophon-libsophon-dev 安装在 host 端
    sudo docker run \
        -td \
        --privileged=true \
        -v /opt/sophon:/opt/sophon \
        -v /etc/profile.d:/etc/profile.d \
        -v /etc/ld.so.conf.d:/etc/ld.so.conf.d \
        --name container_name image_name:image_version bash

2. 直接在 Docker 容器中安装 sophon-libsophon 和 sophon-libsophon-dev。

.. code-block:: shell

    # 在 Docker 容器中安装 sophon-libsophon 和 sophon-libsophon-dev
    sudo docker run \
        -td \
        --privileged=true \
        --name container_name image_name:image_version bash

通过以上两种方法建立动态依赖前请首先前请参照 :doc:`安装libsophon <1_install>` 中内容在 host 端安装 sophon-driver, 安装 sophon-libsophon 和 sophon-libsophon-dev 参考 :doc:`安装libsophon <1_install>` 中对应内容。

以上 ``image_name`` 和 ``image_version`` 对应创建镜像时的内容, ``container_name`` 可自定义。

测试环境生效
~~~~~~~~~~~~

为确保已经构建的 Docker 环境能正常使用 libsophon, 进入 Docker 容器运行以下命令:

.. code-block:: shell

    # 进入 Docker 容器
    sudo docker exec -it container_name bash

    # 在 Docker 容器中运行此命令以确保 libsophon 动态库能被找到
    ldconfig

    # 在 Docker 容器中运行此命令以确保 libsophon 工具可使用
    for f in /etc/profile.d/*sophon*; do source $f; done

运行以上命令后可运行 ``bm-smi`` 命令以检查是否可正常使用 libsophon, 命令输出应与 :doc:`bm-smi使用说明 <3_1_bmsmi_description>` 中对应内容相符。
