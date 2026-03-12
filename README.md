# VirtualMIDI

Converts any Format0 or Format1 MIDI file into a music sheet that may be played according to the common synthax for virtual pianos.
MIDI files that hold a header that indicates Format2 tracks are not supported and it may either not work at all or produce weird results.  

## Usage

`<inputPath> [-o] [-l] [<outputPath>]`  

`<inputPath>` is always required, while  `[<outputPath>]` may be omitted only as long as  both  `-o` and `-l` are omitted too.  
The `-o` flag instructs to create a .txt output file for the sheet.  
The `-l` flag instructs to create a .log output file.  
In the case where no output path is specified, `-o` and `-l` will default the path to the 'output' subdir  

The resulting sheet is always printed regardless.  

For building be sure to check out the readme inside the build subdir!  

## Footnote

Although VirtualMIDI seems to mostly work for its inteded purpose, this was mostly a learning experience for me over the course of a week; the project developed in a very erratic and "patchworky" way.
You *will* find crude initial logic that branches into more complex paths, possibly inefficiently, with some additional features currently missing.  

<sub>Any PRs are welcome :D</sub>
