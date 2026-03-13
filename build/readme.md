# Building

Current information regarding building, compatability, and libraries used.  
I have included the .vscode subdir for building with the vscode [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) and MSVC.  

## Operating System

Should work on most operating systems due to the current lack of non-standard libraries,
but read below.

## Compiler

So far I've only used **MSVC v17.10**, as it's justifiably simple for this project.  
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
