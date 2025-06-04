Instruction of Out_of_band interface
--------------------------------------

SMBUS protocol interface definition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Commands
^^^^^^^^^^^

Firstly write 1 byte of CMD to the i2c slave, then read n bytes data. \
Here is an instance of geting the temperature of CHIP0:

.. code-block:: shell

    # write 0x0 to 0x60 which is slave address
    i2c write 0x60 (slave addr) 0x0 (cmd)
    # then read 1 byte data from 0x60
    i2c read 0x60 (slave addr) 0x1 (n byte)


The out-of-band interface of SC5 series
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Instruction
^^^^^^^^^^^^
**Server manufacturer BMC control**

- The slave address of SC5 series card with multiple cpu are: CHIP0 -> 0x60, CHIP1 -> 0x61, CHIP2 -> 0x62

- While returning integer data, the higher bit is sent first (for example, integer 0x16841e30, return 0x16, 0x84, 0x1e, 0x30)

SC5+ MCU protocol command
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. table::
   :widths: 25 15 10 100

   ===================== ========== ========== =============================================
   Meaning               Address    Attribute     Description
   --------------------- ---------- ---------- ---------------------------------------------
   Chip temperature      0x00       RO            unsigned byte, unit: centigrade
   --------------------- ---------- ---------- ---------------------------------------------
   Card temperature      0x01       RO            unsigned byte, unit: centigrade
   --------------------- ---------- ---------- ---------------------------------------------
   Card power            0x02       RO            unsigned byte, unit:w
   --------------------- ---------- ---------- ---------------------------------------------
   Fan speed percentage  0x03       RO            unsigned byte, 0xff means no fan
   --------------------- ---------- ---------- ---------------------------------------------
   Vendor ID             0x10       RO            unsigned int;[31:16]:Device ID 0x1684;[15:0]:Vendor ID 0x1e30;
   --------------------- ---------- ---------- ---------------------------------------------
   Hardware Version      0x14       RO            unsigned byte
   --------------------- ---------- ---------- ---------------------------------------------
   Firmware Version      0x18       RO            unsigned int;[7:0]Minor version;[15:8]Major version;[31:16]chip version
   --------------------- ---------- ---------- ---------------------------------------------
   Kind of card          0x1c       RO            unsigned byte(Kind of card, SC5+ is 7)
   --------------------- ---------- ---------- ---------------------------------------------
   Sub Vendor ID         0x20       RO            unsigned int;[15:0]:sub Vendor ID 0x0;[31:16]:sub system ID 0x0
   ===================== ========== ========== =============================================

**Note:**

- Can only read from 0x02 of 0x60 for the power of each card
- unsigned int means this address has 4 bytes valid data
- unsigned byte means this address has 1 bytes valid data


The out-of-band interface of SC7 series
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Instruction
^^^^^^^^^^^^^^
**Server manufacturer BMC control**

- The slave address of SC7 series card with multiple cpu are: CHIP0 -> 0x60, CHIP1 -> 0x62, CHIP2 -> 0x72

- While returning integer data, the higher bit is sent first (for example, integer 0x16841f1c30, return 0x16, 0x84, 0x1f, 0x1c)

SC7Pro MCU protocol command
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. table::
   :widths: 25 15 10 100

   ==================== ========== ========== =============================================
   Meaning              Address    Attribute   Description
   -------------------- ---------- ---------- ---------------------------------------------
   Chip temperature     0x00       RO          unsigned byte, unit: centigrade
   -------------------- ---------- ---------- ---------------------------------------------
   Card temperature     0x01       RO          unsigned byte, unit: centigrade
   -------------------- ---------- ---------- ---------------------------------------------
   Card power           0x02       RO          unsigned byte, unit:w
   -------------------- ---------- ---------- ---------------------------------------------
   Fan speed percentage 0x03       RO          unsigned byte, 0xff means no fan
   -------------------- ---------- ---------- ---------------------------------------------
   Vendor ID            0x10       RO          unsigned int;[31:16]:Device ID 0x1686;[15:0]:Vendor ID 0x1f1c;
   -------------------- ---------- ---------- ---------------------------------------------
   Hardware Version     0x14       RO          unsigned byte
   -------------------- ---------- ---------- ---------------------------------------------
   Firmware Version     0x18       RO          unsigned int;[7:0]Minor version;[15:8]Major version;[31:16]chip version
   -------------------- ---------- ---------- ---------------------------------------------
   Kind of card         0x1c       RO          unsigned byte(Kind of card,sc7pro is 0x21)
   -------------------- ---------- ---------- ---------------------------------------------
   Sub Vendor ID        0x20       RO          unsigned int;[15:0]:sub Vendor ID 0x0;[31:16]:sub system ID 0x0
   -------------------- ---------- ---------- ---------------------------------------------
   SN ID                0x24       RO          product SN code
   -------------------- ---------- ---------- ---------------------------------------------
   MCU Version          0x36       RO          unsigned byte; MCU version:0
   ==================== ========== ========== =============================================

**Note:**

- Can only read from 0x02 of 0x60 for the power of each card
- unsigned int means this address has 4 bytes valid data
- unsigned byte means this address has 1 bytes valid data