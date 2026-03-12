#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

const char letters[14][3] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "??" }; // # = 0x23
const char keyboard[9][13][2] = {
    {""},
    {""},
    {"1", "!", "2", "@", "3", "4", "$", "5", "%", "6", "^", "7"}, /*C2-B2*/
    {"8", "*", "9", "(", "0", "q", "Q", "w", "W", "e", "E", "r"}, /*C3-B3*/
    {"t", "T", "y", "Y", "u", "i", "I", "o", "O", "p", "P", "a"}, /*C4-B4*/
    {"s", "S", "d", "D", "f", "g", "G", "h", "H", "j", "J", "k"}, /*C5-B5*/
    {"l", "L", "z", "Z", "x", "c", "C", "v", "V", "b", "B", "n"}, /*C6-B6*/
    {"m"}                                                         /*C7*/
};

// need to initialize the vars to 0 because the "OR assignment" or "Accumulator" (|=) turns bits on without clearing
// definitely not how structs are meant to be used generally but YOLO XD
struct midiHeader {
    uint32_t length;
    uint16_t format;
    uint16_t nTrks;
    uint16_t division; // ticks per quarter note
    uint16_t divFormat;
    int8_t FPS;  // for divFormat = 1
    uint8_t TPF; // for divFormat = 1
} header;

struct midiTrack {
    uint32_t length;    // fixed at 4 bytes
    uint32_t microsPQN; // microseconds per quarter note
    int32_t microsPT;   // microseconds per tick
    struct event {
        uint32_t deltaTime; // VLQ
        int32_t deltaTimeMS;
        int32_t absTick;
        uint8_t type;
        uint8_t subtype;
        uint32_t length; // VLQ
    } event;
    // notes[Ch][Key] = AbsTick of Key
    // AbsTick >= 0: key is ON
    // AbsTick == -1: key is OFF (init memset & event passing)
    int32_t notes[16][128];
} track;

struct VpRec {
    uint8_t channel;  // 1-16
    uint8_t keyValue; // 0-127 MIDI key number
    uint8_t keyNum;   // 0-11 key index in octave
    uint8_t octave;   // 0-8
    int32_t startTick;
    int32_t endTick;
    int32_t durationTick;
    int32_t durationMS;
    _Bool closed;
};

struct TsRec {
    int32_t absTick;
    int8_t num;
    int8_t denum;
};

struct VLQ {
    int VLQi;
    uint8_t cByte;
    uint32_t VLQresult;
} VLQ;

_Bool hasOutput;
_Bool hasLogging;

const char* options[] = { "-o", "-l" };
static int isOption(const char* argv) {
    for (int i = 0; i < sizeof(options) / sizeof(options[0]); i++)
    {
        if (strcmp(argv, options[i]) == 0)
            return 0;
    }
    return 1;
}

static int wFiles(FILE*, FILE*, uint8_t*, long);
static _Bool parseMThd(FILE*, FILE*, uint8_t*, int);
static _Bool parseMTrk(FILE*, FILE*, uint8_t*, int,
    struct VpRec**, size_t*, size_t*,
    struct TsRec**, size_t*, size_t*);
static _Bool calcVLQ(uint8_t*, int);
static _Bool appendRec(void**, size_t*, size_t*, size_t, const void*);
static int compVpRec(const void*, const void*);
static int compTsRec(const void*, const void*);
static void numToKey(uint8_t, uint8_t*, uint8_t*);

/************************ START OF MAIN ******************************/
int main(int argc, char* argv[]) {
    /***********************************/
    /*           CLI LOGIC             */
    /***********************************/
    // <inputPath> [-o] [-l] [<outputPath>]
    char outputSheetPath[200];
    char outputLogPath[200];
    char inputFilePath[200];

    if (argc >= 2 && argc <= 5)
    {
        snprintf(inputFilePath, sizeof(inputFilePath), "%s", argv[1]);
        int i = 0;
        while (*++argv)
            // *argv is a pointer to a pointer of a string array,
            // so it's basically the index of the current pointer to the array of the argument
            // if i do *++argv i move the index by 1 to the next pointer
        {
            i++;
            // so here *argv is the same as argv[0]
            if (strcmp(*argv, "-o") == 0 && i < argc) // compare strings
                hasOutput = 1;
            if (strcmp(*argv, "-l") == 0 && i < argc)
                hasLogging = 1;

            if (*argv[0] != '-' && (hasOutput || hasLogging))
            {
                if (hasOutput)
                {
                    snprintf(outputSheetPath, sizeof(outputSheetPath), "%s\\VirtualMIDI.txt", *argv);
                    printf("Output file at: %s\n", outputSheetPath);
                }
                if (hasLogging)
                {
                    snprintf(outputLogPath, sizeof(outputLogPath), "%s\\VirtualMIDI.log", *argv);
                    printf("Log file at: %s\n", outputLogPath);
                }
            }
            if (*argv[0] == '-' && isOption(*argv) != 0)
            {
                printf("unknown option: %s", *argv);
                return 1;
            }

            if (i == argc - 1 && (hasOutput || hasLogging) && *argv[0] == '-') // at end of args
            {
                if (hasOutput)
                {
                    snprintf(outputSheetPath, sizeof(outputSheetPath), "output\\VirtualMIDI.txt");
                    printf("Output file at: %s\n", outputSheetPath);
                }
                if (hasLogging)
                {
                    snprintf(outputLogPath, sizeof(outputLogPath), "output\\VirtualMIDI.log");
                    printf("Log file at: %s\n", outputLogPath);
                }
            }
        }
    }
    else
    {
        printf("VirtualMIDI: <inputPath> [-o] [-l] [<outputPath>]\n");
        return 0;
    }

    long fileLength;
    uint8_t* buffer;

    FILE* inputMidiFile = fopen(inputFilePath, "rb");
    if (!inputMidiFile) // Check if file was opened
    {
        printf("inputMidiFile stream not opened");
        return 1;
    }

    fseek(inputMidiFile, 0, SEEK_END); // Jump to the end of the file
    fileLength = ftell(inputMidiFile); // Get the current byte offset in the file
    // printf("\nFile length is %i bytes", fileLength);          // Print the byte offset as byte size
    buffer = (uint8_t*)malloc(fileLength * sizeof(uint8_t)); // Allocate enough memory in buffer for the file
    if (!buffer)
    {
        printf("\ncouldn't allocate memory to buffer");
        return 1;
    }
    rewind(inputMidiFile);                       // Jump back to the start of the file
    fread(buffer, 1, fileLength, inputMidiFile); // Read into buffer the entire file

    if (fclose(inputMidiFile) != 0) // free the file stream pointer & check if success
    {
        printf("\ninputMidiFile stream not closed");
        return 1;
    }
    inputMidiFile = (void*)0; // prevent dangling pointer

    FILE* sheetStream = (void*)0;
    FILE* logStream = (void*)0;

    if (hasOutput)
        sheetStream = fopen(outputSheetPath, "w");
    if (hasLogging)
        logStream = fopen(outputLogPath, "w");

    if (wFiles(sheetStream, logStream, buffer, fileLength) != 0)
        return 1;

    if (sheetStream)
        fclose(sheetStream);
    if (logStream)
        fclose(logStream);
    sheetStream = (void*)0;
    logStream = (void*)0;

    free(buffer);       // free buffer malloc
    buffer = (void*)0; // free pointer
    if (buffer)
    {
        printf("\ncouldn't free memory from buffer");
        return 1;
    }

    printf("\n ");
    return 0;
}
/************************* END OF MAIN *******************************/

#pragma region parseFunctions

/*****************************************************************************/
/*                            WRITE OUTPUT SHEET                             */
/*****************************************************************************/
static int wFiles(FILE* sheetStream, FILE* logStream, uint8_t* buffer, long fileLength) {
    if (logStream == (void*)0 && hasLogging) // Check if file was opened
    {
        printf("\noutputLogFile stream not opened");
        return 1;
    }
    if (sheetStream == (void*)0 && hasOutput) // Check if file was opened
    {
        printf("\noutputSheetFile stream not opened");
        return 1;
    }

    struct VpRec* allVpRecs = (void*)0;
    size_t vpRecsCount = 0;
    size_t vpRecsCapacity = 0;

    struct TsRec* allTsRecs = (void*)0;
    size_t tsRecsCount = 0;
    size_t tsRecsCapacity = 0;

    /***********************************************************/
    /*                       PARSING                           */
    /***********************************************************/
    if (logStream)
        fprintf(logStream, "File length is %i bytes", fileLength);

    for (int i = 0; i < fileLength; i++)
    {
        // identifies chunk starters & types
        if (i + 3 < fileLength)
            if (buffer[i] == 0x4D && buffer[i + 1] == 0x54) // "MT"
            {
                if (buffer[i + 2] == 0x68 && buffer[i + 3] == 0x64) // "hd"
                {
                    parseMThd(sheetStream, logStream, buffer, i + 4);
                    if (logStream)
                        fprintf(logStream, "\n");
                }
                else if (buffer[i + 2] == 0x72 && buffer[i + 3] == 0x6B) // "rk"
                {
                    if (parseMTrk(sheetStream, logStream, buffer, i + 4,
                        &allVpRecs, &vpRecsCount, &vpRecsCapacity,
                        &allTsRecs, &tsRecsCount, &tsRecsCapacity) != 0)
                    {
                        free(allVpRecs); // cuz it goes to next track
                        allVpRecs = (void*)0;
                        free(allTsRecs);
                        allTsRecs = (void*)0;
                        return 1;
                    }
                    if (logStream)
                        fprintf(logStream, "\n");
                }
            }
        // fprintf(fileStream, " %02X ", buffer[i]);
    }

    /***********************************************************/
    /*                     AFTER PARSING                       */
    /***********************************************************/
    if (vpRecsCount > 1)
        qsort(allVpRecs, vpRecsCount, sizeof(struct VpRec), compVpRec);
    if (tsRecsCount > 1)
        qsort(allTsRecs, tsRecsCount, sizeof(struct TsRec), compTsRec);

    // Auto-transpose into C2-C7 to avoid invalid keys.
    const int32_t vpPlayableMin = 48; // C2
    const int32_t vpPlayableMax = 84; // C7
    int32_t minKeyValue = 127;
    int32_t maxKeyValue = 0;
    _Bool hasClosedNotes = 0;
    for (size_t i = 0; i < vpRecsCount; i++)
    {
        if (!allVpRecs[i].closed)
            continue;
        hasClosedNotes = 1;
        if (allVpRecs[i].keyValue < minKeyValue)
            minKeyValue = allVpRecs[i].keyValue;
        if (allVpRecs[i].keyValue > maxKeyValue)
            maxKeyValue = allVpRecs[i].keyValue;
    }

    /***********************************/
    /*         TRANSPOSITION           */
    /***********************************/
    int32_t autoTranspose = 0;
    if (hasClosedNotes)
    {
        int32_t minShift = vpPlayableMin - minKeyValue;
        int32_t maxShift = vpPlayableMax - maxKeyValue;
        if (minShift <= maxShift)
        {
            // pick the shift closest to zero inside the valid interval
            if (0 < minShift)
                autoTranspose = minShift;
            else if (0 > maxShift)
                autoTranspose = maxShift;
            else
                autoTranspose = 0;
        }
        else
        {
            // if song range is wider than VP range pick center
            int32_t sourceMid = (minKeyValue + maxKeyValue) / 2;
            int32_t targetMid = (vpPlayableMin + vpPlayableMax) / 2;
            autoTranspose = targetMid - sourceMid;
        }
    }
    printf("\nTranspose: %+d\n", autoTranspose);
    if (hasClosedNotes)
        printf("MIDI range from %d-%d to %d-%d\n",
            minKeyValue,
            maxKeyValue,
            minKeyValue + autoTranspose,
            maxKeyValue + autoTranspose);
    printf("Sheet:\n");
    if (sheetStream)
    {
        if (hasClosedNotes)
            fprintf(sheetStream, "# MIDI range from %d-%d to %d-%d\n",
                minKeyValue,
                maxKeyValue,
                minKeyValue + autoTranspose,
                maxKeyValue + autoTranspose);
        fprintf(sheetStream, "# Transpose: %+d\n", autoTranspose);
        fprintf(sheetStream, "# Sheet:\n");
    }

    /***********************************/
    /*           FORMATTING            */
    /***********************************/
    size_t tsIndex = 0;
    for (size_t i = 0; i < vpRecsCount;)
    {
        if (!allVpRecs[i].closed)
        {
            i++;
            continue;
        }
        /******************* TIME SIGNATURE LOGIC ********************/
        int8_t activeTimeSigN = 4;
        int8_t activeTimeSigD = 2;
        if (tsRecsCount > 0)
        {
            while (tsIndex + 1 < tsRecsCount && allTsRecs[tsIndex + 1].absTick <= allVpRecs[i].startTick)
                tsIndex++;

            if (allTsRecs[tsIndex].absTick <= allVpRecs[i].startTick)
            {
                activeTimeSigN = allTsRecs[tsIndex].num;
                activeTimeSigD = allTsRecs[tsIndex].denum;
            }
        }

        // ticksPerBeat = TPQN * (4 / denum)
        int32_t timeSigDenum = 4;
        if (activeTimeSigD >= 0 && activeTimeSigD <= 30)
            timeSigDenum = 1 << activeTimeSigD;
        if (timeSigDenum <= 0)
        {
            timeSigDenum = 4;
            printf("\none timeSignature denominator for track %zu was equal or less than 0 and was set to 4\n", i);
        }

        int32_t ticksPerBeat = (int32_t)((header.division * 4) / timeSigDenum);
        if (ticksPerBeat <= 0)
        {
            ticksPerBeat = header.division;
            printf("\none calculated ticksPerBeat for track %zu was equal or less than 0 and was set to default header division\n", i);
        }

        if (activeTimeSigN <= 0)
        {
            activeTimeSigN = 4;
            printf("\none timeSignature numerator for track %zu was equal or less than 0 and was set to 4\n", i);
        }

        int32_t ticksPerBar = ticksPerBeat * activeTimeSigN;

        int32_t chordTicks = ticksPerBeat / 8; // notes started within 1/8 beat we consider simultaneous
        if (chordTicks < 1)
        {
            chordTicks = 1;
            printf("\none calculated chordTicks for track %zu was less than 1 and was set to default of 1\n", i);
        }

        int32_t noPauseMax = ticksPerBeat / 4;    // up to 1/16 note
        int32_t shortPauseMax = ticksPerBeat / 2; // up to 1/8 note
        int32_t mediumPauseMax = ticksPerBeat;    // up to 1 beat
        int32_t longPauseMax = ticksPerBeat * 2;  // up to 2 beats
        int32_t barPauseMax = ticksPerBar;        // up to 1 bar

        /******************* CLUSTERING LOGIC ************************/
        // Find one simultaneous cluster
        size_t clusterEnd = i;
        while (clusterEnd + 1 < vpRecsCount)
        {
            size_t cand = clusterEnd + 1;
            while (cand < vpRecsCount && !allVpRecs[cand].closed)
                cand++;
            if (cand >= vpRecsCount)
                break;

            int32_t deltaCluster = allVpRecs[cand].startTick - allVpRecs[clusterEnd].startTick;
            if (deltaCluster > chordTicks)
                break;
            clusterEnd = cand;
        }

        // memset cluster & only unique keys inside [].
        _Bool isCluster = (clusterEnd > i);
        unsigned char usedKeys[256];
        if (isCluster)
        {
            memset(usedKeys, 0, sizeof(usedKeys));
            printf("[");
            if (sheetStream)
                fprintf(sheetStream, "[");
        }

        for (size_t k = i; k <= clusterEnd; k++)
        {
            int32_t transposedKeyValue = (int32_t)allVpRecs[k].keyValue + autoTranspose;
            // to keep a valid vp key when exceedss range.
            while (transposedKeyValue < vpPlayableMin)
                transposedKeyValue += 12;
            while (transposedKeyValue > vpPlayableMax)
                transposedKeyValue -= 12;
            if (transposedKeyValue < 0)
                transposedKeyValue = 0;
            else if (transposedKeyValue > 127)
                transposedKeyValue = 127;

            uint8_t trKeyNum;
            uint8_t trOctave;
            numToKey((uint8_t)transposedKeyValue, &trKeyNum, &trOctave);

            const char* vpKey = "";
            if (trOctave <= 8 && trKeyNum <= 11 && keyboard[trOctave][trKeyNum][0] != '\0')
                vpKey = keyboard[trOctave][trKeyNum];

            unsigned char keyChar = (unsigned char)vpKey[0];
            if (isCluster)
            {
                if (keyChar == '\0')
                    continue;
                if (usedKeys[keyChar])
                    continue;
                usedKeys[keyChar] = 1;
            }
            if (sheetStream)
                fprintf(sheetStream, "%s", vpKey);
            printf("%s", vpKey);
        }

        if (isCluster)
        {
            if (sheetStream)
                fprintf(sheetStream, "]");
            printf("]");
        }
        // compute gap to next closed record and pause separator.
        size_t nextI = clusterEnd + 1;
        while (nextI < vpRecsCount && !allVpRecs[nextI].closed)
            nextI++;

        int32_t closeDelta = INT32_MAX;
        if (nextI < vpRecsCount)
            closeDelta = allVpRecs[nextI].startTick - allVpRecs[clusterEnd].startTick;

        if (closeDelta <= noPauseMax)
        {
            if (sheetStream)
                fprintf(sheetStream, "");
            printf("");
        }

        else if (closeDelta <= shortPauseMax)
        {
            if (sheetStream)
                fprintf(sheetStream, " ");
            printf(" ");
        }
        else if (closeDelta <= mediumPauseMax)
        {
            if (sheetStream)
                fprintf(sheetStream, "|");
            printf("|");
        }
        else if (closeDelta <= longPauseMax)
        {
            if (sheetStream)
                fprintf(sheetStream, " |");
            printf(" |");
        }
        else
        {
            if (sheetStream)
                fprintf(sheetStream, "||");
            printf("||");
        }

        i = clusterEnd + 1;
    }
    free(allVpRecs);
    allVpRecs = (void*)0;
    free(allTsRecs);
    allTsRecs = (void*)0;
    if (sheetStream)
        fprintf(sheetStream, "\n");
    printf("\n");
    /************************************************************/
    return 0;
}

/*****************************************************************************/
/*                            PARSE MIDI HEADER                              */
/*****************************************************************************/
static _Bool parseMThd(FILE* sheetStream, FILE* logStream, uint8_t* buffer, int startOffset) {
    header.length = 0;
    header.format = 0;
    header.nTrks = 0;
    header.division = 0; // ticks per quarter note (time signature 4/4)
    header.FPS = 0;
    header.TPF = 0;
    for (int i = 3; i >= 0; i--)
    {
        header.length |= buffer[startOffset + 3 - i] << (8 * i);
    }
    unsigned int dataEndOffset = startOffset + 3 + header.length;
    for (int i = 1; i >= 0; i--)
    {
        header.format |= buffer[dataEndOffset - 4 - i] << (8 * i);
        header.nTrks |= buffer[dataEndOffset - 2 - i] << (8 * i);
        header.division |= buffer[dataEndOffset - 0 - i] << (8 * i); // ticks per quarter note (time signature 4/4)
    }

    _Bool divisionBits[16];
    for (int i = 15; i >= 0; i--)
    {
        divisionBits[15 - i] = (header.division >> i) & 0x01; // MSB first & shift by i & masks all bits except last one
    }

    /*
    If bit 15 of <division> is a one, delta-times in a file correspond to subdivisions of a second, in
    a way consistent with SMPTE and MIDI time code. Bits 14 thru 8 contain one of the four
    values -24, -25, -29, or -30, corresponding to the four standard SMPTE and MIDI time code
    formats (-29 corresponds to 30 drop frame), and represents the number of frames per second.
    These negative numbers are stored in two's complement form. The second byte (stored
    positive) is the resolution within a frame: typical values may be 4 (MIDI time code resolution),
    8, 10, 80 (bit resolution), or 100. This system allows exact specification of time-code-based
    tracks, but also allows millisecond-based tracks by specifying 25 frames/sec and a resolution of
    40 units per frame. If the events in a file are stored with bit resolution of thirty-frame time
    code, the division word would be 0xE250.
    [FROM MIDI DOCUMENTATION]
    */

    // check the kind of delta-time
    if (divisionBits[15] == 0) // metrical time
    {
        header.divFormat = 0;
    }
    else if (divisionBits[15] == 1) // time-code-based time
    {
        header.divFormat = 1;
        header.FPS = (int8_t)header.division >> 8;
        header.TPF = header.division & 0xFF;
    }
    if (logStream)
    {
        fprintf(logStream, "\nfor parsing, bytes start from location 0\n");
        fprintf(logStream, "\n║ MThd start(@B%i)", startOffset - 4);
        fprintf(logStream, " | MThd data length (from B%i): %iB", (startOffset + 3), header.length);
        fprintf(logStream, " | Format: %hi", header.format);
        fprintf(logStream, " | nTrks: %hi", header.nTrks);
        fprintf(logStream, " | Division: 0x%04X(%hi) - bit15: %i", header.division, header.division, header.divFormat);
        fprintf(logStream, " | MThd end(@B%i)\n", dataEndOffset);
    }
    return 0;
}

// man i gotta chunk-ify this function rofl
/*****************************************************************************/
/*                             PARSE MIDI TRACK                              */
/*****************************************************************************/
static _Bool parseMTrk(
    FILE* sheetStream, FILE* logStream, uint8_t* buffer, int lengthStart,
    struct VpRec** allVpRecs, size_t* vpRecsCount, size_t* vpRecsCapacity,
    struct TsRec** allTsRecs, size_t* tsRecsCount, size_t* tsRecsCapacity) {
    /*************************************** INITIALS ****************************************/
    // it took me 5 days and i already have no clue what the frick i wrote down here but it seems to work
    //
    // any variable that starts with "after" (for example afterEventLength) refers to the index of the byte immediately after:
    // if track.event.length occupies the bytes 20 and 25 then afterEventLength is 26 (cuz of how I did the VLQ function)
    // writing this for my mental sanity because i keep forgetting and whenever i try to change the var names it only makes it worse

    track.length = 0;
    track.event.deltaTime = 0;
    track.event.absTick = 0;
    track.event.type = 0;
    track.event.subtype = 0;
    track.event.length = 0;

    // Calc Track Length
    for (int i = 3; i >= 0; i--)
    {
        track.length |= buffer[lengthStart + 3 - i] << (8 * i); // managed to use bitwise logic instead from down here :DD
    }

    int dataStart = lengthStart + 4;
    int trackEnd = dataStart + (int)track.length;
    int i = dataStart;
    uint8_t runningStatus = 0;
    int32_t noteRecIndex[16][128];

    struct VpRec* VpRecs = (void*)0;
    struct VpRec vpRec;
    struct TsRec tsRec;

    size_t VpRecCount = 0;
    size_t VpRecCapacity = 0;

    int8_t timeSigN = 4;  // Numerator (default 4/4)
    int8_t timeSigD = 2;  // Denominator exponent (2^2 = 4, default 4/4)
    int8_t timeSigC = 24; // Number of Midi Clocks in a metronome click <-- unused for now
    int8_t timeSigB = 8;  // Number of notated 32nd notes per quarter-note <-- unused for now

    /* sf = -7: 7 flats
     * sf = -1: 1 flat
     * sf = 0: key of C
     * sf = 1: 1 sharp
     * sf = 7: 7 sharps */
    int8_t keySigSF = 0; // <-- unused for now
    /* mi = 0: major key
     * mi = 1: minor key */
    _Bool keySignMI = 0; // <-- unused for now

    // as default tempo if it's not later specified by meta events
    if (!track.microsPQN)
    {
        track.microsPQN = 500000;                             // 120 BPM
        track.microsPT = (track.microsPQN / header.division); // 1041us
    }

    memset(track.notes, -1, sizeof(track.notes));
    memset(noteRecIndex, -1, sizeof(noteRecIndex));
    /*
    for(int i = 0; i < 16; i++)
    {
        for(int j = 0; j < 128; j++)
        printf("Ch%iKey%i Set to %i\n", i + 1, j, track.notes[i][j]);
    }
    */

    if (logStream)
        fprintf(logStream, "║ MTrk start(@B%i) | MTrk data length(from B%i): %iB\n", lengthStart - 4, dataStart, track.length);
    /************************************ HANDLE EVENTS **************************************/
    while (i < trackEnd)
    {
        calcVLQ(buffer, i);
        track.event.deltaTime = VLQ.VLQresult;
        track.event.deltaTimeMS = (track.event.deltaTime * track.microsPT) / 1000;
        track.event.absTick += track.event.deltaTime;
        int afterDeltaTime = VLQ.VLQi;
        int deltaTimeBytes = afterDeltaTime - i;

        if (afterDeltaTime >= trackEnd)
        {
            if (logStream)
                fprintf(logStream, "\n | event starts outside track(@B%i)", afterDeltaTime);
            break;
        }

        uint8_t rawStatus = buffer[afterDeltaTime];
        _Bool hasStatusByte = (rawStatus & 0x80) != 0;
        uint8_t status = 0;
        int cursor = afterDeltaTime;

        if (hasStatusByte)
        {
            status = rawStatus;
            cursor++;
            if ((status & 0xF0) >= 0x80 && (status & 0xF0) <= 0xE0)
                runningStatus = status;
        }
        else
        {
            if (runningStatus == 0)
            {
                if (logStream)
                    fprintf(logStream, "\n | running status missing(@B%i)", afterDeltaTime);
                break;
            }
            status = runningStatus;
        }
        track.event.type = status;
        track.event.subtype = 0;
        track.event.length = 0;

        /*****************************************************************************/
        /*                              META    EVENTS                               */
        /*****************************************************************************/
        if (status == 0xFF) // Meta: FF tt len data
        {
            if (cursor >= trackEnd)
            {
                if (logStream)
                    fprintf(logStream, "\n | meta subtype outside track(@B%i)", cursor);
                break;
            }
            track.event.subtype = buffer[cursor++];
            calcVLQ(buffer, cursor);
            track.event.length = VLQ.VLQresult;
            int afterEventLength = VLQ.VLQi;
            int eventLenBytes = afterEventLength - cursor;
            int dataEnd = afterEventLength + (int)track.event.length;
            if (dataEnd > trackEnd)
            {
                if (logStream)
                    fprintf(logStream, "\n | meta data outside track(@B%i)", dataEnd);
                break;
            }

            if (logStream)
            {
                fprintf(logStream, "\n║ META");
                fprintf(logStream, " | deltaTime: 0x%06X(%luticks)(%lims)(@B%i-B%i)", track.event.deltaTime, track.event.deltaTime, track.event.deltaTimeMS, i, afterDeltaTime - 1);
                fprintf(logStream, " | absTick: 0x%06X(%luticks)", track.event.absTick, track.event.absTick);
                fprintf(logStream, " | type: 0x%02X(@B%i) | subtype: 0x%02X(@B%i)", track.event.type, afterDeltaTime, track.event.subtype, cursor - 1);
            }

            /***********************************/
            /*           SET TEMPO             */
            /***********************************/
            if (track.event.subtype == 0x51) // "Set Tempo" Meta Event
            {
                if (logStream)
                {
                    fprintf(logStream, " | SET_TEMPO");
                    fprintf(logStream, " | eventLen: 0x%06X(%lu)(@B%i-B%i)", track.event.length, track.event.length, cursor, afterEventLength - 1);
                }
                track.microsPQN = 0;
                track.microsPT = 0;
                if (header.divFormat == 0)
                {
                    for (int j = 0; j < track.event.length; j++)
                    {
                        track.microsPQN |= buffer[(afterEventLength + track.event.length - 1) - j] << (8 * j);
                    }
                    track.microsPT = (track.microsPQN / header.division);
                    track.event.deltaTimeMS = (track.event.deltaTime * track.microsPT / 1000);
                } // need to add else (for divFormat == 1)

                // printf("| set tempo value: 0x%06X -> %ius(%.1fms) ", track.microsPQN, track.microsPQN, (float)track.microsPQN / 1000);
                // printf("| tick duration: %ius(%.2fms) |\n", track.microsPT, (float)track.microsPT / 1000);
            }
            /***********************************/
            /*         TIME SIGNATURE          */
            /***********************************/
            else if (track.event.subtype == 0x58) // "Time Signature" Meta Event
            {
                timeSigN = buffer[dataEnd - 4];
                timeSigD = buffer[dataEnd - 3];
                timeSigC = buffer[dataEnd - 2];
                timeSigB = buffer[dataEnd - 1];
                int8_t timeSigDR = 1; // Denumerator result
                timeSigDR = 1 << timeSigD;

                tsRec.absTick = track.event.absTick;
                tsRec.num = timeSigN;
                tsRec.denum = timeSigD;
                if (appendRec((void**)allTsRecs, tsRecsCount, tsRecsCapacity, sizeof(**allTsRecs), &tsRec))
                {
                    if (logStream)
                        fprintf(logStream, "\n | failed to allocate tsRec buffer");
                    return 1;
                }

                if (logStream)
                {
                    fprintf(logStream, " | TIME_SIGNATURE");
                    fprintf(logStream, " | eventLen: 0x%06X(%lu)(@B%i-B%i)", track.event.length, track.event.length, cursor, afterEventLength - 1);
                    fprintf(logStream, " | Time: %i/%i | %i MIDI CPC | %i 32nd notes/24 MIDI clocks", timeSigN, timeSigDR, timeSigC, timeSigB);
                }
                // CPC = Clocks Per Click
            }
            /***********************************/
            /*          KEY SIGNATURE          */
            /***********************************/
            else if (track.event.subtype == 0x59) // "Key Signature" Meta Event
            {
                keySigSF = buffer[dataEnd - 2];
                keySignMI = buffer[dataEnd - 1];

                if (logStream)
                {
                    fprintf(logStream, " | KEY_SIGNATURE");
                    fprintf(logStream, " | eventLen: 0x%06X(%lu)(@B%i-B%i)", track.event.length, track.event.length, cursor, afterEventLength - 1);

                    if (keySigSF < 0)
                        fprintf(logStream, " | Flats: %i", -1 * keySigSF);
                    else if (keySigSF == 0)
                        fprintf(logStream, " | Key of C");
                    else
                        fprintf(logStream, " | Sharps: %i", keySigSF);

                    if (keySignMI)
                        fprintf(logStream, " | Minor");
                    else
                        fprintf(logStream, " | Major");
                }
            }
            /***********************************/
            /*          TEXT  EVENT            */
            /***********************************/
            else if (track.event.subtype == 0x01) // "Text Event" Meta Event
            {
                char text[sizeof(track.event.length) + 1];
                for (int j = 0; j <= track.event.length; j++)
                    text[j] = buffer[afterEventLength + j];

                if (logStream)
                {
                    fprintf(logStream, " | TEXT_EVENT");
                    fprintf(logStream, " | eventLen: 0x%06X(%lu)(@B%i-B%i)", track.event.length, track.event.length, cursor, afterEventLength - 1);
                    fprintf(logStream, " | Text %s", text);
                }
            }
            /***********************************/
            /*        COPYRIGHT NOTICE         */
            /***********************************/
            else if (track.event.subtype == 0x02) // "Copyright Notice" Meta Event
            {
                char text[sizeof(track.event.length) + 1];
                for (int j = 0; j <= track.event.length; j++)
                    text[j] = buffer[afterEventLength + j];

                if (logStream)
                {
                    fprintf(logStream, " | COPYRIGHT_NOTICE");
                    fprintf(logStream, " | eventLen: 0x%06X(%lu)(@B%i-B%i)", track.event.length, track.event.length, cursor, afterEventLength - 1);
                    fprintf(logStream, " | Text %s", text);
                }
            }
            /***********************************/
            /*          SEQUENCE NAME          */
            /***********************************/
            else if (track.event.subtype == 0x03) // "Sequence/Track Name" Meta Event
            {
                char text[sizeof(track.event.length) + 1];
                for (int j = 0; j <= track.event.length; j++)
                    text[j] = buffer[afterEventLength + j];

                if (logStream)
                {
                    fprintf(logStream, " | SEQUENCE_NAME");
                    fprintf(logStream, " | eventLen: 0x%06X(%lu)(@B%i-B%i)", track.event.length, track.event.length, cursor, afterEventLength - 1);
                    fprintf(logStream, " | Text %s", text);
                }
            }
            /***********************************/
            /*         END OF TRACK            */
            /***********************************/
            else if (track.event.subtype == 0x2F) // "End Of Track" Meta Event
            {
                if (logStream)
                    fprintf(logStream, " | END_OF_TRACK");
            }
            /***********************************/
            /*      UNKNOWN META EVENTS        */
            /***********************************/
            else
            {
                if (logStream)
                {
                    fprintf(logStream, " | UNKNOWN_EVENT");
                    fprintf(logStream, " | eventLen: 0x%06X(%lu)(@B%i-B%i)", track.event.length, track.event.length, cursor, afterEventLength - 1);
                }
            }

            i = dataEnd;
            continue;
        }
        /*****************************************************************************/
        /*                             CHANNEL  EVENTS                               */
        /*****************************************************************************/
        if ((status & 0xF0) >= 0x80 && (status & 0xF0) <= 0xE0) // Channel events
        {
            int channelDataBytes = ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) ? 1 : 2;
            int dataStartByte = cursor;
            int dataEnd = dataStartByte + channelDataBytes;
            if (logStream)
                if (dataEnd > trackEnd)
                {
                    fprintf(logStream, "\n | channel data outside track(@B%i)", dataEnd);
                    break;
                }
            track.event.length = (uint32_t)channelDataBytes;
            track.event.subtype = buffer[dataStartByte];

            if (logStream)
            {
                fprintf(logStream, "\n║ CHAN");
                fprintf(logStream, " | deltaTime: 0x%06X(%luticks)(%lims)(@B%i-B%i)", track.event.deltaTime, track.event.deltaTime, track.event.deltaTimeMS, i, afterDeltaTime - 1);
                fprintf(logStream, " | absTick: 0x%06X(%luticks)", track.event.absTick, track.event.absTick);
                if (hasStatusByte)
                    fprintf(logStream, " | type: 0x%02X(@B%i)", status, afterDeltaTime);
                else
                    fprintf(logStream, " | type: 0x%02X(running)", status);
            }

#define isNoteOn (status >= 0x90 && status <= 0x9F)  // 0x9# Note ON
#define isNoteOff (status >= 0x80 && status <= 0x8F) // 0x8# Note OFF
#define hasVel (buffer[dataStartByte + 1])
            /***********************************/
            /*             NOTE_ON             */
            /***********************************/
            if (isNoteOn && hasVel)
            {
                uint8_t keyNum;
                uint8_t octave;
                uint8_t channel;
                uint8_t keyValue;
                channel = (status & 0x0F);
                keyValue = buffer[dataStartByte];

                numToKey(keyValue, &keyNum, &octave);

                track.notes[channel][keyValue] = track.event.absTick;
                vpRec.channel = channel + 1;
                vpRec.keyValue = keyValue;
                vpRec.keyNum = keyNum;
                vpRec.octave = octave;
                vpRec.startTick = track.event.absTick;
                vpRec.endTick = -1;
                vpRec.durationTick = -1;
                vpRec.durationMS = -1;

                /*
                printf("VpRec | channel: %u | keyValue: %u | keyNum: %u | octave: %u | startTick: %li | endTick: %li | durationTick: %li | durationMS: %li | timeSignature: %i/2^%i\n",
                    vpRec.channel, vpRec.keyValue, vpRec.keyNum, vpRec.octave, vpRec.startTick, vpRec.endTick, vpRec.durationTick, vpRec.durationMS, timeSigN, timeSigD);
                */

                vpRec.closed = 0;
                if (appendRec((void**)&VpRecs, &VpRecCount, &VpRecCapacity, sizeof(*VpRecs), &vpRec))
                {
                    if (logStream)
                        fprintf(logStream, "\n | failed to allocate vpRec buffer");
                    break;
                }
                noteRecIndex[channel][keyValue] = (int32_t)(VpRecCount - 1);
                // printf("Ch%iKey%i Set to %i\n", channel, keyValue, track.notes[channel][keyValue]);

                if (logStream)
                {
                    fprintf(logStream, " | NOTE_ON");
                    fprintf(logStream, " | Ch%u", channel + 1);
                    fprintf(logStream, " | eventLen: %lu(@B%i-B%i)", track.event.length, dataStartByte, dataEnd - 1);
                    fprintf(logStream, " | noteNum %s%u(%i)", letters[keyNum], octave, buffer[dataStartByte]);
                    fprintf(logStream, " | noteVel %i(0-127)", buffer[dataStartByte + 1]);
                }
            }
            /***********************************/
            /*            NOTE_OFF             */
            /***********************************/
            else if (isNoteOff || (isNoteOn && !hasVel))
            {
                uint8_t keyNum;
                uint8_t octave;
                uint8_t channel;
                uint8_t keyValue;
                channel = (status & 0x0F);
                keyValue = buffer[dataStartByte];

                numToKey(keyValue, &keyNum, &octave);

                int32_t startTick;
                int32_t endTick;
                int32_t noteDuration;
                int32_t noteDurationMS;
                if (track.notes[channel][keyValue] != -1)
                {
                    startTick = track.notes[channel][keyValue];
                    endTick = track.event.absTick;
                    noteDuration = endTick - startTick;
                    track.notes[channel][keyValue] = -1;
                    noteDurationMS = (noteDuration * track.microsPT) / 1000;
                    int32_t recIndex = noteRecIndex[channel][keyValue];
                    noteRecIndex[channel][keyValue] = -1;
                    if (recIndex >= 0 && (size_t)recIndex < VpRecCount)
                    {
                        VpRecs[recIndex].endTick = endTick;
                        VpRecs[recIndex].durationTick = noteDuration;
                        VpRecs[recIndex].durationMS = noteDurationMS;
                        VpRecs[recIndex].closed = 1;
                    }

                    // fprintf(sheetStream, "%s ",  keyboard[octave][keyNum]);
                }
                else
                {
                    if (logStream)
                        fprintf(logStream, " | (!) DUPLICATE NOTE_OFF (!)");
                }

                if (logStream)
                {
                    fprintf(logStream, " | NOTE_OFF");
                    fprintf(logStream, " | Ch%u", (status & 0x0F) + 1);
                    fprintf(logStream, " | eventLen: %lu(@B%i-B%i)", track.event.length, dataStartByte, dataEnd - 1);
                    fprintf(logStream, " | noteNum %s%u(%i)", letters[keyNum], octave, buffer[dataStartByte]);
                    fprintf(logStream, " | noteVel %i(0-127)", buffer[dataStartByte + 1]);
                }
            }
            /***********************************/
            /*      UNKNOWN CHAN EVENTS        */
            /***********************************/
            else
            {
                if (logStream)
                {
                    fprintf(logStream, " | (unknown event)");
                    fprintf(logStream, " | eventLen: %lu(@B%i-B%i)", track.event.length, dataStartByte, dataEnd - 1);
                }
            }

            i = dataEnd;
            continue;
        }
        /*****************************************************************************/
        /*                              SYSEX   EVENTS                               */
        /*****************************************************************************/
        if (status == 0xF0 || status == 0xF7) // SysEx: F0/F7 len data
        {
            calcVLQ(buffer, cursor);
            track.event.length = VLQ.VLQresult;
            int afterEventLength = VLQ.VLQi;
            int eventLenBytes = afterEventLength - cursor;
            int dataEnd = afterEventLength + (int)track.event.length;
            if (dataEnd > trackEnd)
            {
                if (logStream)
                    fprintf(logStream, "\n | sysex data outside track(@B%i)", dataEnd);
                break;
            }

            if (logStream)
            {
                fprintf(logStream, "\n║ SYSEX");
                fprintf(logStream, " | deltaTime: 0x%06X(%luticks)(%lims)(@B%i-B%i)", track.event.deltaTime, track.event.deltaTime, track.event.deltaTimeMS, i, afterDeltaTime - 1);
                fprintf(logStream, " | absTick: 0x%06X(%luticks)", track.event.absTick, track.event.absTick);
                fprintf(logStream, " | type: 0x%02X(@B%i)", track.event.type, afterDeltaTime);
                fprintf(logStream, " | eventLen: 0x%06X(%lu)(@B%i-B%i)", track.event.length, track.event.length, cursor, afterEventLength - 1);
            }
            i = dataEnd;
            continue;
        }
        if (logStream)
            fprintf(logStream, "\n | unsupported status 0x%02X(@B%i)", status, afterDeltaTime);
        break;
    }
    /**************************** MERGE THIS TRACK INTO GLOBAL VP BUFFER ****************************/
    for (size_t recI = 0; recI < VpRecCount; recI++)
    {
        if (!VpRecs[recI].closed)
            continue;
        if (appendRec((void**)allVpRecs, vpRecsCount, vpRecsCapacity, sizeof(**allVpRecs), &VpRecs[recI]))
        {
            free(VpRecs);
            VpRecs = (void*)0;
            if (logStream)
                fprintf(logStream, "\n | failed to allocate merged VP buffer");
            return 1;
        }
    }
    free(VpRecs);
    VpRecs = (void*)0;

    if (logStream)
        fprintf(logStream, "\n");
    return 0;
}
/*
Think of a track in the MIDI file in the same way that you normally think of a track in a sequencer.
A sequencer track contains a series of events. For example, the first event in the track may be to sound a middle C note.
The second event may be to sound the E above middle C. These two events may both happen at the same time.
The third event may be to release the middle C note.
This event may happen a few musical beats after the first two events (ie, the middle C note is held down for a few musical beats).
Each event has a "time" when it must occur, and the events are arranged within a "chunk" of memory in the order that they occur.

In a MIDI file, an event's "time" precedes the data bytes that make up that event itself.
In other words, the bytes that make up the event's time-stamp come first.
A given event's time-stamp is referenced from the previous event.
For example, if the first event occurs 4 clocks after the start of play, then its "delta-time" is 04.
If the next event occurs simultaneously with that first event, its time is 00.
So, a delta-time is the duration (in clocks) between an event and the preceding event.

NOTE: Since all tracks start with an assumed time of 0, the first event's delta-time is referenced from 0.
[https://web.archive.org/web/20051129113105/http://www.borg.com/~jglatt/tech/midifile/vari.htm]
*/
#pragma endregion parseFunctions

#pragma region helperFunctions
/*
if (buffer[dataStart] & 0x80) // (if FALSE)
// this wasn't the end byte of the VLQ
{
}
else // (if TRUE)
// this was the end byte of the VLQ
{
}
// 0x80 in bin is 10000000 so it basically masks off everything except MSB
// so if buffer[dataStart] = 0x79 (01111001) it becomes: 01111001 & 10000000 which is equal to 00000000 or just 0
// the if-statement always considers 0 as FALSE
// conversely, if buffer[dataStart] = 0xF9 (11111001) it becomes: 11111001 & 10000000 which is equal to 10000000
// C always treats any non-zero value as true
//
// this is so cool!!! i dont know how they came up with this in the 70s
*/
// ^^^ from that and deltaTime as uint32_t, with bit shifting i can do VVV
// handle VLQ
static _Bool calcVLQ(uint8_t* buffer, int index) {
    VLQ.VLQi = index;
    VLQ.cByte = 0;
    VLQ.VLQresult = 0;

    do
    {
        VLQ.cByte = buffer[VLQ.VLQi++];
        VLQ.VLQresult = (VLQ.VLQresult << 7) | (VLQ.cByte & 0x7F);
    } while (VLQ.cByte & 0x80);

    return 0;
}

static _Bool appendRec(void** recs, size_t* count, size_t* capacity, size_t eSize, const void* value) {
    if (*count == *capacity)
    {
        size_t newCapacity = (*capacity == 0) ? 256 : (*capacity * 2);
        void* newRecs = realloc(*recs, newCapacity * eSize);
        if (!newRecs)
            return 1;
        *recs = newRecs;
        *capacity = newCapacity;
    }

    memcpy((char*)(*recs) + (*count * eSize), value, eSize);
    (*count)++;
    return 0;
}

static int compVpRec(const void* a, const void* b) {
    const struct VpRec* ra = (const struct VpRec*)a;
    const struct VpRec* rb = (const struct VpRec*)b;

    if (ra->startTick < rb->startTick)
        return -1;
    if (ra->startTick > rb->startTick)
        return 1;

    if (ra->channel < rb->channel)
        return -1;
    if (ra->channel > rb->channel)
        return 1;

    if (ra->keyValue < rb->keyValue)
        return -1;
    if (ra->keyValue > rb->keyValue)
        return 1;
    return 0;
}
static int compTsRec(const void* a, const void* b) {
    const struct TsRec* ra = (const struct TsRec*)a;
    const struct TsRec* rb = (const struct TsRec*)b;

    if (ra->absTick < rb->absTick)
        return -1;
    if (ra->absTick > rb->absTick)
        return 1;

    if (ra->num < rb->num)
        return -1;
    if (ra->num > rb->num)
        return 1;

    if (ra->denum < rb->denum)
        return -1;
    if (ra->denum > rb->denum)
        return 1;

    return 0;
}

static void numToKey(uint8_t number, uint8_t* resultName, uint8_t* resultOctave) {
    char octave = 0;  // 0 to 8
    int minInOct = 0; // 24 (0)
    int maxInOct = 0; // 35 (11)
    int key = 0;

    for (int i = 0; i <= 8; i++)
    {
        minInOct = 24 + (12 * i);
        if (i < 8)
            maxInOct = 35 + (12 * i);
        else if (i == 8)
            maxInOct = 127;

        if ((number >= 24 + (12 * i)) && (number <= 35 + (12 * i)))
        {
            octave = i;
            key = (maxInOct - minInOct) - (maxInOct - number);
            // i didn't do 11 - (maxInOct - number) cuz for the last octave it's 7
            break;
        }
        else
        {
            // below/outside range
            // continuosly sets to -1 until it finds the correct values which is ok!
            octave = 0x3F;
            key = 12;
        }
    }
    // printf("| num>%i min>%i max>%i oct>%i key>%i|\n", number, minInOct, maxInOct, octave, key);

    *resultName = key;
    *resultOctave = octave;
}
#pragma endregion helperFunctions
