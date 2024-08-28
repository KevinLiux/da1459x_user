USB CDC and Virtual MSD application {#usb_cdc_vmsd}
======================================================================
## Overview
This application is an example app about how to implement USB CDC and VirtualMSD multi-interface USB-Device.

## VirtualMSD information
VirtualMSD is used as a wrapper for handling files without implementing the full MSD functionality through VirtualMSD's abstraction layer by retrieving and manipulating the content of specific files without the need to implement a filesystem in the device.
VirtualMSD is not a replacement of the MSD, just a wrapper on top of the MSD. MSD functionality and API is also available with VMSD on the created volume.

**Notes:**
   - New files created are properly passed through the _cbOnWrite() and _cbOnRead() enabling the application to save their content and retrieve 
     it from permanent storage locations or redirect the contents through services while the device is plugged. 
   - Constant files can be used to extract information from the device by adding dynamically a constant file in the array of constant files 
     with the pData = NULL and the FileSize=\<size to return to host\> in the USB_VMSD_CONST_FILE array of constant files before the enumeration.
   - For dynamic addition of constant files, the application should add the entries in the USB_VMSD_CONST_FILE array of constant files and then 
     call the USBD_VMSD_ReInit(); to re-initialize the Virtual Storage area.
   - To have generic storage capabilities and store any file in the device, use the MSD class. In this case it is suggested to use the NVMS as low level medium access interface 
     and a FAT-FS implementation for accessing the files in the the device's storage for this purpose.

For details on the use of the VirtualMSD and MSD please refer to SEGGER documentation and SEGGER support for the emUSB-Device USB stack at https://www.segger.com/emusb.html
## USB feature

- To enable the USB-Data functionality it is mandatory to define the next macro in custom_config_qspi.h: 
~~~{.c}
  #define dg_configUSE_USB_ENUMERATION 1
~~~
- The USB stack framework included in the SDK is the Segger emUSB-Device. 
  Please refer to SEGGER emUSB documentation for more information at link:
  https://www.segger.com/emusb.html

**Notes:**
   - The charger functionality can be also enabled along with the USB-Data functionality.
     To enable the USB-Charger functionality define the next macro in custom_config_qspi.h: 
~~~{.c}
     #define dg_configUSE_SYS_CHARGER 1
~~~
   - The emUSB stack framework is provided in object code in the 'sdk/interfaces/usb' library.
     It is required the library to be included in the project to be able to build the project with USB-Data functionality.
   - The emUSB stack framework is NOT needed for USB-Charger only functionality.


## Configurable parameters
- The default values for usb PID/VID can be used during development period. It should be changed to your company's values.
	- USB PID/VID and com port name in usb_cdc_vmsd.c.
	- Windows driver(dialog_usb.inf).
- VirtualMSD is an emulated FAT file system. 
  The data will be stored to FLASH if the following macro (in usb_cdc_vmsd.c) is defined, otherwise they will be stored in RAM.
~~~{.c}
  #define VMSD_USE_NVMS
~~~
- Sector size of FAT can be adjusted.
~~~{.c}
  #define VIRTUALMSD_NUM_SECTORS (64)
~~~
## Operation of VirtualMSD
- Connect the USB1 on a proDK motherboard
- Write the application to the device QSPI FLASH.
- Run the example application by pressing once the K2(RST) button on the daughterboard.
- Connect the USB on the daughterboard to Host-PC (Windows, Linux, OS X, Android, etc).
- For the CDC, confirm that a new COMxx port appears at the Host-PC device manager.
	* For MS Windows, if asked for a driver, use the `utilities/windows/cdc/dialog_usb.inf` to install it.
	* `USB CDC serial port emulation (COMxx)` in Ports(COM & LPT) of device manager should appear.- No special driver is required.
- For the Mass-Storage device confirm that a removable volume appeared in the file manager application (e.g. Windows File Explorer).
- One file is shown at the portable storage in Host with the name 'README.txt' and is readable.
- Create a file named ``FW.BIN`` (case sensitive) and copy it to the VMSD storage.
  Its contents will be written to the LOG partition of the device FLASH and will be accessible from the application code 
  without any need for a file system implementation.
  This is the starting point for example for updating the NVMS_PART though a file write, or even performing a FW update by just drag 'n' drop the new FW to the VMSD device.
- After creating/copy the ``FW.BIN``file, eject/disconnect the USB device properly, using the O/S infrastructure (click to: "Safely Remove Hardware and Eject Media")
- Reconnect the USB device and check if the ``README.txt`` file is only appeared.

## Operation of USB CDC
-  Refer to README.md in [usb_cdc] (@ref usb_cdc) project.
