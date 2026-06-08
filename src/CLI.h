#ifndef CLI_H
#define CLI_H
#if __linux__
#include <stdbool.h>
#endif

extern int CLIargs(int argc, char *argv[], _Bool *hasOutput, _Bool *hasLogging, char inFPath[], char outSPath[], char outLPath[]);

#endif