APU(Audio Processing Unit) demo application {#apu_demo}
======================================================

## Overview

The application demonstrates how various APU data paths can be used.

Features:

- The application prints the path configuration of each demo used via UART2,
- The application checks the usage of PDM input to I2S output and I2S input to I2S output paths.

There are four demos supplied as examples:

1. PDM-SRC-PCM configuration for voice/audio streaming.
2. PDM-SRC-MEMORY-> MEMORY-SRC-PCM configuration for voice recording for ~12 seconds and audio playback.
3. PCM-SRC-PCM configuration for voice/audio streaming.
4. PCM-SRC-MEMORY-> MEMORY-SRC-PCM configuration for voice recording for ~12 seconds and audio playback.
5. MEMORY-SRC1-MEMORY configuration that changes the sample rate of a tone in the memory.

  **Note**:

  - Demos are implemented with the specific configuration (sample rate, bit depth etc.).
  Further configurability is not supported.
  - Demos that include PCM interface can be ran using either DIVN or DIV1(all supported clocks) system clock by setting:

~~~{.c}
        #define SYS_CLK_DIV1                             (1)
~~~
  - Demos that include PCM or MEMORY can have sample rate conversion between two pairs of sample rates:

        1. 16KHz to/from 8KHz by setting:

~~~{.c}
        #define SR1_16_SR2_8                             (1)
        #define SR1_48_SR2_32                            (0)
~~~

         2. 48KHz to/from 32KHz by setting:

~~~{.c}
         #define SR1_16_SR2_8                            (0)
         #define SR1_48_SR2_32                           (1)
~~~
        when PCM is used as output the sample rate is 16/48KHz respectively.

### Folder structure

| Top level folders | Description
|-------------------|-----------------------------------
| config/           | Configuration of demo applications
| sdk/		        | SDK layer
| startup/          | Startup files


| Includes folder contents  | Description
|---------------------------|------------------------------------------------------------------------------------------------------------
| includes/DA7218_driver.h  | Driver for DA7218
| includes/audio_task.h     | Default application configuration (see Suggested configurable parameters)
| includes/periph_setup.h   | Default GPIO pin assignment (see GPIO assignments)
| includes/DA7218_regs.h    | Registers file of Codec DA7218 (should not be modified directly, modify audio_codec_driver.h above instead)
| includes/demo_helpers.h   | Construction of sin and cos tones

###Demos

Using the apu_demo it is possible to check how APU framework works.
The features depend on the selected demos which are described below.

All demos use:
- DA7218_driver.c:             functions for Codec DA7218 
- audio_task.c :               Demos implementation
- demo_helpers.c:              functions for construction of tones

#### PDM-SRC-PCM configuration

- <b>Demonstrated path</b>: PDM-SRC-PCM

- <b>Source file</b>: audio_task.c

- <b>Description</b>:<br>

The demo allows the user to listen from PDM Mic input (SPK0838HT4H-B digital microphone),
using a headset, hooked in a DA7218 module jack.

<b>Hardware Setup</b>:

   - Dialog DA7218 mikrobus module
   - SPK0838HT4H-B digital microphone

<b>Run steps</b>:<br>
To enable the demo go to the `includes/audio_task.h` file. The respective macro must be set to (1). 

~~~{.c}
#define DEMO_PDM_MIC               (1)
#define DEMO_PDM_RECORD_PLAYBACK   (0)
#define DEMO_PCM_MIC               (0)
#define DEMO_PCM_RECORD_PLAYBACK   (0)
#define DEMO_MEM_TO_MEM            (0)
~~~

####  PDM-SRC-MEMORY-> MEMORY-SRC-PCM configuration 
- <b>Demonstrated path</b>: PDM-SRC-MEMORY-> MEMORY-SRC-PCM

- <b>Source file</b>: audio_task.c

- <b>Description</b>:<br>
The demo allows the user to listen from PDM Mic input (SPK0838HT4H-B digital microphone), using a headset hooked in a DA7218 module jack. More specifically:
	- The PDM-SRC-Memory configuration allows the user to record PDM input to QSPI (Log partition) by  pressing  K1 button. The recording duration is ~12 seconds with the default sample rate and bit depth.
	- The Memory-SRC-PCM configuration allows the user to listen to recorded audio, using a headset hooked in a DA7218 module jack.

<b>Hardware Setup</b>:

- Dialog DA7218 mikrobus module
- SPK0838HT4H-B digital microphone

<b>Run steps</b>:<br>
To enable the demo go to the `includes/audio_task.h` file. The respective macro must be set to (1). 

~~~{.c}
#define DEMO_PDM_MIC               (0)
#define DEMO_PDM_RECORD_PLAYBACK   (1)
#define DEMO_PCM_MIC               (0)
#define DEMO_PCM_RECORD_PLAYBACK   (0)
#define DEMO_MEM_TO_MEM            (0)
~~~

####   PCM-SRC-PCM configuration
- <b>Demonstrated path</b>:  PCM-SRC-PCM

- <b>Source file</b>: audio_task.c

- <b>Description</b>:<br>
The demo  allows the user to listen from a PCM Mic input, using a headset hooked in a DA7218 module jack.

<b>Hardware Setup</b>:

- Dialog DA7218 mikrobus module

<b>Run steps</b>:<br>
To enable the demo go to the `includes/audio_task.h` file. The respective macro must be set to (1). 

~~~{.c}
#define DEMO_PDM_MIC               (0)
#define DEMO_PDM_RECORD_PLAYBACK   (0)
#define DEMO_PCM_MIC               (1)
#define DEMO_PCM_RECORD_PLAYBACK   (0)
#define DEMO_MEM_TO_MEM            (0)
~~~

####  PCM-SRC-MEMORY-> MEMORY-SRC-PCM configuration
- <b>Demonstrated path</b>:  PCM-SRC-MEMORY-> MEMORY-SRC-PCM

- <b>Source file</b>: audio_task.c

- <b>Description</b>:<br>
The demo  allows the user to listen from a PCM Mic input, using a headset hooked in a DA7218 module jack. More specifically:
	- The PCM-SRC-Memory configuration allows the  user to record PCM input to QSPI (Log partition) by  pressing  K1 button. The recording duration is ~12 seconds with the default sample rate and bit depth.
	- The Memory-SRC-PCM configuration allows the user to listen to recorded audio, using a headset hooked in a DA7218 module jack.

<b>Hardware Setup</b>:

- Dialog DA7218 mikrobus module

<b>Run steps</b>:<br>
To enable the demo go to the `includes/audio_task.h` file. The respective macro must be set to (1). 

~~~{.c}
#define DEMO_PDM_MIC               (0)
#define DEMO_PDM_RECORD_PLAYBACK   (0)
#define DEMO_PCM_MIC               (0)
#define DEMO_PCM_RECORD_PLAYBACK   (1)
#define DEMO_MEM_TO_MEM            (0)
~~~

#### MEMORY-SRC1-MEMORY configuration
- **Demonstrated path**:  MEMORY-SRC1-MEMORY

- **Source file**: audio_task.c

- **Description**:<br>
- The demo  converts the sample rate of 16/48 KHz of a tone of 1 KHz in memory to 8/32 KHz sample rate, respectively.
Using memory browser, the tone in the input memory buffer must be the same with the one in the output memory buffer when it is depicted in an audio analyzer such as audacity.

- **Run steps**:<br>
To enable the demo go to the `includes/audio_task.h` file. The respective macro must be set to (1).

~~~{.c}
#define DEMO_PDM_MIC               (0)
#define DEMO_PDM_RECORD_PLAYBACK   (0)
#define DEMO_PCM_MIC               (0)
#define DEMO_PCM_RECORD_PLAYBACK   (0)
#define DEMO_MEM_TO_MEM            (1)
~~~

## Installation procedure

The project is located in the `projects/dk_apps/demos/apu_demo` folder.

To install the project follow the [General Installation and Debugging Procedure](@ref install_and_debug_procedure).

## File structure

The following file structure will be created:

* projects/dk_apps/demos/apu_demo/

    * config
        * custom_config_qspi.h
        * custom_config_ram.h
    * includes
        * audio_task.h
        * DA7218_driver.h
        * DA7218_regs.h
        * demo_helpers.h
        * periph_setup.h
    * sdk
    * startup
    * audio_task.c
    * config_audio_demo.c
    * DA7218_driver.c
    * demo_helpers.c
    * main.c

## Existing build configurations

The template contains build configurations for executing it from RAM or QSPI.

- `DA1469X-00-Debug_RAM`. The project is built to be run from RAM. The executable is built with debug (-Og) information.
- `DA1469X-00-Debug_QSPI`. The project is built to be run from QSPI. The executable is built with debug (-Og) information.
- `DA1469X-00-Release_RAM`. The project is built to be run from RAM. The executable is built with no debug information and size optimization (-Os).
- `DA1469X-00-Release_QSPI`. The project is built to be run from QSPI. The executable is built with no debug information and size optimization (-Os).

