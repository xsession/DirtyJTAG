# Clean-room protocol policy

This project implements behavior from public vendor programming specifications and independently published/open-source host protocol documentation. It does not copy MPLAB/PICkit firmware, proprietary PIC scripting bytecode, leaked code, or undocumented vendor binaries.

PICkit 4's publicly documented AVR mode uses Microchip's CMSIS-DAP/EDBG command envelope. The universal firmware uses its own DJP2 USB transport so PIC/dsPIC, AVR, STM8, MSP430, C2, RL78, SWD and JTAG backends share one stable host API. A future compatibility shim may map documented EDBG vendor commands onto these backends without embedding Microchip's undisclosed PIC scripting language.
