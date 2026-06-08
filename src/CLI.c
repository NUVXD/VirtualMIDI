#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CLI.h"

static _Bool hasOut = 0;
static _Bool hasOutPath = 0;
static _Bool hasLog = 0;
static _Bool hasLogPath = 0;
static const char* options[] = { "-o", "-l" }; // require to be followed by a vlue
static const char* flags[] = { "--help" }; // just bools

static char* fixPaths(const char* argv) {
    char* fixedPath = malloc(strlen(argv) + 1);
    for (size_t i = 0; i < strlen(argv); i++)
        if (argv[i] == '\\')
            fixedPath[i] = '/';
        else
            fixedPath[i] = argv[i];
    fixedPath[strlen(argv)] = '\0';
    return fixedPath;
}
static _Bool isOption(const char* argv) { return (argv && argv[0] == '-' && argv[1] != '-'); }
static _Bool isFlag(const char* argv) { return (argv && argv[0] == '-' && argv[1] == '-'); }
static _Bool whatDoesOption(const char* argv) { // ooga booga
    for (int i = 0; i < sizeof(options) / sizeof(options[0]); i++)
        if (strcmp(argv, options[i]) == 0)
        {
            switch (i)
            {
            case 0: // "-o"
                hasOut = 1;
                break;
            case 1: // "-l"
                hasLog = 1;
                break;
            }
            return 1;
        }
    return 0;
}
static _Bool whatDoesFlag(const char* argv) { // ooga booga
    for (int i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
        if (strcmp(argv, flags[i]) == 0)
        {
            switch (i)
            {
            case 0: // "--help"
                printf("\nVirtualMIDI:\n");
                printf("<inputPath>: The disk path to your MIDI file. Example: 'C:/Users/John/Music/File.mid'.\n");
                printf("[-o]: Optional, tells the program to create a .txt file for the virtual piano notation, must be followed by a path or it will use the default output folder.\n");
                printf("[-l]: Optional, tells the program to create a .log file for additional information, must be followed by a path or it will use the default output folder.\n");
                printf("[<outputPath>]: Optional, output path for [-o] and/or [-l]\n\n");
                break;
            }
            return 1;
        }
    return 0;
}

int CLIargs(int argc, char *argv[], _Bool *hasOutput, _Bool *hasLogging, char inFPath[], char outSPath[], char outLPath[]) {
    snprintf(inFPath, 512, "%s", argv[1]);
    int i = 0;
    while (*++argv)
        // *argv is a pointer to a pointer of a string array,
        // so it's basically the index of the current pointer to the array of the argument
        // if i do *++argv i move the index by 1 to the next pointer
    {
        i++;
        if (isOption(*argv)) {
            if (!whatDoesOption(*argv))
            {
                printf("unknown option: %s", *argv);
                return 0;
            }
        }
        else if (isFlag(*argv)) {
            if (!whatDoesFlag(*argv))
                {
                    printf("unknown flag: %s", *argv);
                    return 0;
                }
        }
        else {
            if (hasOut)
            {
                char* fixedPath = fixPaths(*argv);
                snprintf(outSPath, 512, "%s/VirtualMIDI.txt", fixedPath);
                printf("Output file at: %s\n", outSPath);
                hasOutPath = 1;
                hasOut = 0;
                free(fixedPath);
            }
            if (hasLog)
            {
                char* fixedPath = fixPaths(*argv);
                snprintf(outLPath, 512, "%s/VirtualMIDI.log", fixedPath);
                printf("Log file at: %s\n", outLPath);
                hasLogPath = 1;
                hasLog = 0;
                free(fixedPath);
            }
        }

        if (i == argc - 1) {
            // defaulting
            if (hasOut && !hasOutPath)
            {
                snprintf(outSPath, 512, "output/VirtualMIDI.txt");
                printf("Output file defaulted at: %s\n", outSPath);
            }
            if (hasLog && !hasLogPath)
            {
                snprintf(outLPath, 512, "output/VirtualMIDI.log");
                printf("Log file defaulted at: %s\n", outLPath);
            }
        }
    }
    return 1;
}