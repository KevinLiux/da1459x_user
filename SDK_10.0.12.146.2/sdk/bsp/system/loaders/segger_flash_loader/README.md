Segger Flash loader {#segger_flash_loader}
==========================================

## Introduction

This project is used to generate a flash loader according to SEGGER's Open Flashloader framework.

The loader allows programming the flash memory of the device using SEGGER's J-link SW tools, instead
of using the flash programming tools provided by the SDK (cli_programmer etc.).

It is therefore required for using the `QSPI_DA1469x_segger_flasher` Debug launcher in SmartSnippets
Studio (which - in contrary to the other provided QSPI launcher - also programs the flash with the new
image, by using J-Link GDBServer) and it is also needed in case of directly using J-Link DLL based
applications externally (e.g. J-Flash).

In order to build an application capable to be flashed with the Segger Flash Loader, it is necessary
to define `dg_configUSE_SEGGER_FLASH_LOADER=1`. Otherwise the binary file will not include the
corresponding product headers, image headers etc, causing the booter to get stuck and the application
will not boot.

So, provided that the application has been built properly, the user can then use the 
`QSPI_DA1469x_segger_flasher` launcher in order to flash it and start a debug session.

In the latest versions, the J-Link SW tools come with built-in support for the different DA1469x device
variants. So, unless using an older J-Link version, or unless a custom device implementation/variant
is used which requires a different flash loader, the following sections are not needed.

It should also be noted that, in case of creating a custom flash loader, the performance (with
respect to the speed of the flash programming process) may not be the same. 

## Setting up the Segger Flash loader

The project provides two build configurations:

- The `Debug_RAM` build configuration allows to debug the flash algorithm during development. The
configuration includes a `main.c` file containing the typical function call order, executed by the
J-Link DLL during flash programming. It behaves as a typical RAM build configuration.

- The `Release` build configuration does not allow debugging, yet it is the one that creates the
actual `*.elf` file of the loader and also copies it under `{workspace_loc}/binaries/da1469x/qflash`.
This is basically the elf file that is referenced from within the JLinkDevices.xml file as "Loader".

Thus, the following steps should be followed for generating the Segger flash loader .elf file (steps
2 - 3 are useful just for validating that everything has been configured properly):

 1. Compile the Release build configuration of the corresponding device.

 2. Make sure that the `segger_flash_loader.elf` file has been built under the
    `{workspace_loc}/binaries/da1469x/qflash/` path.

 3. Make sure that the valid XML Tags and Attributes have been added to the
    `{workspace_loc}/config/segger/DA1469x/JLinkDevices.xml` so that the SEGGER tools can use the
    `segger_flash_loader.elf` properly.

## Integrating the Segger flash launcher in the SDK

The QSPI_DA1469x_segger_flasher launcher uses by default J-Link's built-in flash loader.

In case using a custom segger flash loader (using this project) is required, the user should open
the configuration settings for the `QSPI_DA1469x_segger_flasher` debug launcher, go to the 'Debugger'
tab and set `Device name` to 'DA1469x' (instead of 'DA14699').

For easier maintenance, the SDK integrates its own `JLinkDevices.xml` using the  J-Link command
`-JLinkDevicesXMLPath <path to JLinkDevices.xml>` according to
[SEGGER's suggestion](https://forum.segger.com/index.php/Thread/4209-Add-a-new-flash-device-to-the-JLInkDevices-database/?postID=15141#post15141).

To have a better understanding how the JLinkDevices.xml is integrated by the launcher, open the
`Debugger` Tab of `QSPI_DA1469x_segger_flasher` and have a look at the field `Other options` where
the next attribute is included:

`-JLinkDevicesXMLPath ${workspace_loc}/config/segger/DA1469x/JLinkDevices.xml`

Note that the J-Link DLL first searches the xml files for an entry that matches the Device name.
If there isn't such an entry, then it searches in its internal database. Thus, even in case of an
officially supported device, such as DA14699, it is possible (using the xml file) to override the
internal flash loader to be used and use a custom flash loader instead. This can be done by
modifying the `Name` attribute for xml as follows:

~~~~{.xml}
<DataBase>
  <!--                 -->
  <!-- DIALOG (DA14699)-->
  <!--                 -->
  <Device>
    <ChipInfo Vendor="Dialog Semiconductor" Name="DA14699" WorkRAMAddr="0x810000" WorkRAMSize="0x10000" Core="JLINK_CORE_CORTEX_M33" />
    <FlashBankInfo Name="QSPI Flash" BaseAddr="0x16000000" MaxSize="0x2000000" Loader="../../../binaries/da1469x/qflash/segger_flash_loader.elf" LoaderType="FLASH_ALGO_TYPE_OPEN" />
  </Device>
</DataBase>
~~~~

This way, the `QSPI_DA1469x_segger_flasher` launcher will actually use the custom loader instead of
the built-in one.

For more info, please refer to [Open Flashloader wiki page](https://wiki.segger.com/Open_Flashloader.
