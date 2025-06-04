Using test environment by Docker
-----------------------------------
Customers also can use docker to build the testing environment by following the next steps.

Before starting to build it, we recommand following the content of :doc:`安装libsophon <1_install>` to build it on the host firstly!

Build test environment on Docker
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Install Docker
^^^^^^^^^^^^^^

Please read the instructions on Docker official website and install docker step by step:

.. code-block:: shell

    # uninstall docker
    sudo apt-get remove docker docker.io containerd runc

    # install dependencies
    sudo apt-get update
    sudo apt-get install \
            ca-certificates \
            curl \
            gnupg \
            lsb-release

    # get the PIN
    sudo mkdir -p /etc/apt/keyrings
    curl -fsSL \
        https://download.docker.com/linux/ubuntu/gpg | \
        gpg --dearmor -o docker.gpg && \
        sudo mv -f docker.gpg /etc/apt/keyrings/

    # add docker libraries to system
    echo \
        "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
        https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | \
        sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

    # install docker
    sudo apt-get update
    sudo apt-get install docker-ce docker-ce-cli containerd.io docker-compose-plugin

Make the Dockerfile and create the docker image
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Make the dockerfile
"""""""""""""""""""

Using dockerfile to build the docker image with testing environment. The dockerfile is in the libsophon directory. Before building the image, please check whether the system time is accurate, as an incorrect system time can result in certificate verification failure during the process of updating the package list.

.. code-block:: shell

    # convert the following path to the path where the actual libsophon is
    cd path/to/libsophon
    sudo docker build -t image_name:image_version -f libsophon_dockerfile .

You can named ``image_name`` and ``image_version`` as you wish.

After all the above work have been done, checking if the docker image created sucessfully by executing the command `docker images`:

.. code-block:: shell

    $ sudo docker images
    REPOSITORY   TAG               IMAGE ID     CREATED          SIZE
    image_name      image_version  image_id        create_time        1.74GB


The ``image_name`` and ``image_version`` should be the same as what customer sets up. ``image_id`` and ``create_time`` \
should be when and what image customer uses.


Create the docker image
""""""""""""""""""""""""

libsophon dynamic library is necessary to make a test in the docker. There are two ways to install it:

1. Install sophon-libsophon and  sophon-libsophon-dev on host. Then map them into docker.

.. code-block:: shell

    # sophon-libsophon and sophon-libsophon-dev are installed on host
    sudo docker run \
        -td \
        --privileged=true \
        -v /opt/sophon:/opt/sophon \
        -v /etc/profile.d:/etc/profile.d \
        -v /etc/ld.so.conf.d:/etc/ld.so.conf.d \
        --name container_name image_name:image_version bash

2. Directly install them in docker

.. code-block:: shell

    # Directly install sophon-libsophon and sophon-libsophon-dev in docker
    sudo docker run \
        -td \
        --privileged=true \
        --name container_name image_name:image_version bash


Please read the related content in :doc:`安装libsophon <1_install>`, such as ``instll sophon-driver``, ``sophon-libsophon`` and ``sophon-libsophon-dev`` before starting install them in docker

The ``image_name`` and ``image_versio`` is what users decided before.  The name of the image is customize, either.

Checkout the environment
~~~~~~~~~~~~~~~~~~~~~~~~~~

To make sure libsophon is working in docker, run the following commands in the docker:

.. code-block:: shell

    # start the docker image and get into it
    sudo docker exec -it container_name bash

    # run this command to let libsophon can be found
    ldconfig

    # update environment variables so that libsophon tools can be straightly to use
    for f in /etc/profile.d/*sophon*; do source $f; done

After that, opening ``bm-smi`` to check if the output is the same as the one in :doc:`bm-smi使用说明 <3_1_bmsmi_description>`.