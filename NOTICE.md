# Notices

This project is derived from an STM32N6 hand-landmark demonstration project for the ALIENTEK STM32N647 development board.

## Custom Additions

The following files are newly added for the gesture-control layer:

- `Appli/Core/Inc/app_gesture.h`
- `Appli/Core/Src/app_gesture.c`

Integration changes were also made in:

- `Appli/Core/Src/app.c`
- `STM32CubeIDE/Appli/.project`

## Third-Party Material

Files originating from STMicroelectronics, ALIENTEK, Microsoft Azure RTOS ThreadX, model-generation tools, middleware packages, BSP code, generated neural-network code, and model artifacts remain subject to their respective original licenses, notices, and redistribution terms.

This repository does not relicense third-party components. Keep original copyright headers and license notices intact.

Before using this project in a commercial product or redistributing binary/model artifacts, review the licenses for:

- STM32Cube firmware package
- STM32N6 HAL and CMSIS components
- STM32 AI/NPU runtime
- STM32 middleware packages
- Azure RTOS ThreadX
- ALIENTEK BSP and demo code
- bundled `.tflite`, generated model code, and network data
