# overview

This solution provides a method for compressing embedded C/C++ debug information. Encode the debug information and generate the coded record, store the coded number in the generated code, output the debug coded number and debug parameters to the PC host software at runtime, the host software finds the format string according to the coded record, and according to the format string formats debug parameters, generate debug information and output.

# Current solutions

In the embedded field, debug information is often needed to be added to C/C++ code in order to locate errors and causes or obtain relevant information. At present, the commonly used method is to format the format string through printf or similar functions, generate debug information, and then output it through the serial port or other peripherals.

1. A long format string must be saved in the program, which will consume a certain amount of memory space for a microcontroller with limited resources. For example, a simple printf("Errno %d\n", 3) takes 10 words for the format string. 

2. The generated debug information is long. Take the previous printf("Errno %d\n", 3) as an example, the generated debug information "Errno 3\n" occupies at least 9 bytes. If the debug information needs to be saved inside the device , it also requires more resources.

3. At present, uart is commonly used as a peripheral for outputting debug information. It's speed is low. Longer debug information will take more time to output. When the debug print is frequently called, it is easy to cause blocking.

The disadvantages of this solution are that more memory is occupied a bit more due to the long debug information, and the generated debug information is long and the transmission time of the debug information is long.

Could we just save the raw data of the print parameters and then parse those datas on PC with corresponding print format?

# Fastlog gcc plugin

This gcc plugin could find the locations to call 'fastlog', and record the information of line number, size/type of print paramaters and other informations, it output those informations to two files '*.c.inc' and '*.c.macro.h'.

...

## Run

Before the program runs, run the PC host software and the terminal display program on the PC side. The PC host software is used to open the debug interface connected to the PC, and connect the terminal display program through the TCP port. Putty can be used as a terminal display program. The PC host software reads the information in the 'fastlog' call record file and waits for the input of the debug interface.

When the program runs to the place where 'fastlog' is called, the 'fastlog' address value and the debug parameter value are output by the output interface in the form of a byte stream.

After receiving the byte stream, the PC host software analyzes the byte stream, first obtains the 'fastlog' address, retrieves the 'fastlog' call record file, and finds the corresponding record. Then, according to the format string, debug parameter type, and debug parameter bit width information in the record, the debug parameter data is converted into debug information, and output to the terminal display program.

Run 'tools/test.py' to test the sample.

## Repository

Git repository:  
[github.com/novumdun/FastLog-gcc-plugin](git@github.com:novumdun/FastLog-gcc-plugin.git)  
