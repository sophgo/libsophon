Install libsophon
------------------

The libsophon offers different types of installations on different Linux versions. 
Please select the corresponding method according to your system. 
Do not mix different installations on the same machine. 
Depending on the current situation, the installed version may change. 
"0.4.1" in the following description is only an example.

**Please uninstall the old BM1684 SDK driver first:**

.. code-block:: shell

  #Enter the SDK installation directory and execute:
  sudo ./remove_driver_pcie.sh
  #or:
  sudo rmmod bmsophon
  sudo rm -f /lib/modules/$(uname -r)/kernel/drivers/pci/bmsophon.ko


**If the Debian/Ubuntu system is used:**

the installation package consists of three files:

 - sophon-driver_0.4.1_amd64.deb
 - sophon-libsophon_0.4.1_amd64.deb
 - sophon-libsophon-dev_0.4.1_amd64.deb

Where: sophon-driver contains the PCIe accelerator card driver; 
sophon-libsophon contains the runtime environment (library files, tools, etc.); 
sophon-libsophon-dev contains the development environment (header files, etc.). 
If the system is only installed on a deployment environment, 
the sophon-libsopho-dev does not need to be installed.

The installation may be done by following steps:


.. code-block:: shell

  
  #To install the dependency library, execute the following commands only once:
  sudo apt install dkms libncurses5
  #Install libsophon:
  sudo dpkg -i sophon-*.deb
  #Bm-smi can be used after executing the following command in the terminal, 
  #or after logging out and then logging in to the current user:
  source /etc/profile



Installation position:

.. parsed-literal::

  /opt/sophon/
  ├── driver-0.4.1
  ├── libsophon-0.4.1
  │    ├── bin
  │    ├── data
  │    ├── include
  │    └── lib
  └── libsophon-current -> /opt/sophon/libsophon-0.4.1

The deb package installation method does not allow you to install multiple different versions of the same package, but you may have placed several different versions under /opt/sophon in other ways. 
When using the deb package for installation, /opt/sophon/libsophon-current will point to the last version installed. After uninstallation, it will point to the remaining latest version (if any).

When using the deb package to install, if the following prompt message appears:

.. parsed-literal::

  modprobe: FATAL: Module bmsophon is in use.

You need to manually execute the following command to install the driver in the new deb package after manually stopping the program that is using the driver.

.. code-block:: shell

  sudo modprobe -r bmsophon
  sudo modprobe bmsophon


Uninstallation method:

.. code-block:: shell
  
  sudo apt remove sophon-driver sophon-libsophon
  #or:
  sudo dpkg -r sophon-driver
  sudo dpkg -r sophon-libsophon-dev
  sudo dpkg -r sophon-libsophon

If there is any trouble in uninstallation, you can try the following operations:

.. code-block:: shell

  #Uninstall the driver manually:
  dkms status
  #Check the output, usually as follows:
  bmsophon, 0.4.1, 5.15.0-41-generic, x86_64: installed
  #Remember the first two fields and apply them to the following command:
  sudo dkms remove -m bmsophon -v 0.4.1 --all
  #Then uninstall the driver again:
  sudo apt remove sophon-driver
  sudo dpkg --purge sophon-driver
  #Completely clear libsophon:
  sudo dpkg --purge sophon-libsophon

**If another Linux system is used:**

the installation package consist of one file:

  - libsophon_0.4.1_x86_64.tar.gz

The installation may be done by following steps:

.. code-block:: shell

  tar -xzvf libsophon_0.4.1_x86_64.tar.gz
  sudo cp -r libsophon_0.4.1_x86_64/* /
  sudo ln -s /opt/sophon/libsophon-0.4.1 /opt/sophon/libsophon-current


Next, please build the driver compilation environment according to the requirements of the Linux Releases you are using, and then do the following operations:



.. code-block:: shell

  sudo ln -s /opt/sophon/driver-0.4.1/$bin /lib/firmware/bm1684x_firmware.bin
  sudo ln -s /opt/sophon/driver-0.4.1/$bin /lib/firmware/bm1684_ddr_firmware.bin
  sudo ln -s /opt/sophon/driver-0.4.1/$bin /lib/firmware/bm1684_tcm_firmware.bin
  cd /opt/sophon/driver-0.4.1

Here “$bin” is the full name of the bin file with version number, for bm1684x, e.g. **bm1684x.bin_v3.1.0-9734c1da-220802**, for bm1684, e.g. **bm1684_ddr.bin_v3.1.1-63a8614d-220906** and **bm1684_tcm.bin_v3.1.1-63a8614d-220906**.

After that you can compile the driver (here it does not depend on dkms):

.. code-block:: shell

  sudo make SOC_MODE=0 PLATFORM=asic SYNC_API_INT_MODE=1 \
            TARGET_PROJECT=sg_x86_pcie_device FW_SIMPLE=0 \
            PCIE_MODE_ENABLE_CPU=1
  sudo cp ./bmsophon.ko /lib/modules/$(uname -r)/kernel/
  sudo depmod
  sudo modprobe bmsophon

Finally, some configuration work should be done:

.. code-block:: shell

  # Add library and executable file path:
  sudo cp /opt/sophon/libsophon-current/data/libsophon.conf /etc/ld.so.conf.d/
  sudo ldconfig
  sudo cp /opt/sophon/libsophon-current/data/libsophon-bin-path.sh /etc/profile.d/
  source /etc/profile
  # Add cmake config files:
  sudo mkdir -p /usr/lib/cmake/libsophon
  sudo cp /opt/sophon/libsophon-current/data/libsophon-config.cmake /usr/lib/cmake/libsophon/

Uninstallation method:
  
.. code-block:: shell
  
  sudo rm -f /etc/ld.so.conf.d/libsophon.conf
  sudo ldconfig
  sudo rm -f /etc/profile.d/libsophon-bin-path.sh
  sudo rm -rf /usr/lib/cmake/libsophon
  sudo rmmod bmsophon
  sudo rm -f /lib/modules/$(uname -r)/kernel/bmsophon.ko
  sudo depmod
  sudo rm -f /lib/firmware/bm1684x_firmware.bin
  sudo rm -f /lib/firmware/bm1684_ddr_firmware.bin
  sudo rm -f /lib/firmware/bm1684_tcm_firmware.bin
  sudo rm -f /opt/sophon/libsophon-current
  sudo rm -rf /opt/sophon/libsophon-0.4.1
  sudo rm -rf /opt/sophon/driver-0.4.1

