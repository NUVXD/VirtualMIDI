# VirtualMIDI

![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/NUVXD/VirtualMIDI) ![GitHub commit activity](https://img.shields.io/github/commit-activity/t/NUVXD/VirtualMIDI) [![GitHub License](https://img.shields.io/github/license/NUVXD/VirtualMIDI)](https://github.com/NUVXD/VirtualMIDI/blob/main/LICENSE)

Converts any [Format0 or Format1](https://midi.org/standard-midi-files-specification) MIDI file into a music sheet that may be played according to the common syntax for virtual pianos.  
MIDI files that hold a header that indicates Format2 tracks are not supported and it may either not work at all or produce weird results.  
VirtualMIDI uses only standard **[libraries](https://github.com/NUVXD/VirtualMIDI/tree/main/build#libraries)** and should therefore be completely portable, but this may change in the future.

## Usage

`<inputPath> [-o] [-l] [<outputPath>]`  

`<inputPath>` is always required, while  `[<outputPath>]` may be omitted only as long as  both  `-o` and `-l` are omitted too.  
The `-o` flag instructs to create a .txt output file for the sheet.  
The `-l` flag instructs to create a .log output file.  
In the case where no output path is specified, `-o` and `-l` will default the path to the 'output' subdir  

The resulting sheet is always printed in-console regardless.  

## Footnote

Although VirtualMIDI works just fine for its intended purpose, this was mostly a learning experience for me over the course of the first week; the project developed in a very erratic and "patchworky" way.
You *will* find crude initial logic that branches into more complex paths, possibly inefficiently, with some additional features initialized but currently missing.  

<sub>Any PRs are welcome :D</sub>

---
