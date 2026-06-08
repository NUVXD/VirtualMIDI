# Building

Current information regarding building, compatability, and libraries used.  
I have included the .vscode subdir for building with the vscode [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) and MSVC. 
A basic GCC compile task is also present. 

## Operating System

Should work on most operating systems due to the current lack of non-standard libraries,
but read below.

## Compiler

So far I've only used **MSVC v17.10** myself, as it's justifiably simple for this project.  
I have also compiled using **GCC** and the tasks inside .vscode, but I'm quite unfamiliar with GCC and there may be some problems.  
There shouldn't be many - if any - compatibility issues across compilers I believe, but
check the libraries down below to be sure.  
As of writing this I only use standard libraries and, while it is the current direction,
I can't guarantee that will continue to remain the case in the future.

## Libraries

|    Library   |    Type    | Description                            |
|:------------:|:----------:|----------------------------------------|
|  `<stdio.h>` | `Standard` | Input and Output                       |
| `<stdlib.h>` | `Standard` | General Functions and Dynamic Memory   |
| `<string.h>` | `Standard` | String and Memory manipulation         |
| `<stdint.h>` | `Standard` | Exact-width Types                      |
| `<stdbool.h>`| `Standard` | (Only on Linux systems) _Bools macro   |
