#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "TiqiaaUsb.hpp"

static FILE *io_file = NULL;
static bool signal_received;
static const int MaxInputLength = 5;

static void test_callback(uint8_t * data, int size, class TiqiaaUsbIr * IrCls, void * context) {
    printf("INFO: Received data %d\n", size);
    fwrite(data, sizeof(char), size, io_file);
    fclose(io_file);
    signal_received = true;
}

static const char usage[] =
    "Usage: ir-usb [-s file_path] [-r file_path] [-r|-s ...]\n"
    "\n"
    "  -h   Show help message and quit\n"
    "  -r   Receive IR signal and store to file_path\n"
    "  -s   Send IR signal from file_path\n"
    "  -c   Continuous Rx of signal, no storage\n"
    "  -t   Continuous Tx from hex input in terminal, no storage\n"
    "  -y   Tx signals from text file in file_path\n";

int main(int argc, char *argv[]) {
    int err = 0;
    int c;

    while( (c = getopt(argc, argv, "hr:s:cty:")) != -1 ) {
        switch( c ) {
        case 'h':
            printf("%s", usage);
            return EXIT_SUCCESS;
        case 's':
        case 'r':
        case 'c':
        case 't':
        case 'y':
            break; // Just check it's ok
        case '?':
            if( isprint(optopt) )
              fprintf(stderr, "ERROR: Unknown option `-%c'.\n", optopt);
            else
              fprintf(stderr, "ERROR: Unknown option character `\\x%x'.\n", optopt);
            return 1;
        default:
            break;
        }
    }

    TiqiaaUsbIr Ir;
    Ir.IrRecvCallback = test_callback;
    if( Ir.Open() ) {
        fprintf(stderr, "INFO: Device opened\n");

        for( int i = 1; i < argc; i += 2 ) {
            bool send = argv[i][1] == 's';
            bool txr  = argv[i][1] == 't';
            bool rxr  = argv[i][1] == 'c';
            bool ytx  = argv[i][1] == 'y';
            if(send) {
                fprintf(stderr, "INFO: Reading signal from file: %s\n", argv[i+1]);
                io_file = fopen(argv[i+1], "rb");
            } else if(txr) {
                fprintf(stderr, "INFO: Transmitting signal from terminal\n");
            } else if(rxr) {
                fprintf(stderr, "INFO: Receiving signal continuously\n");
            } else if(ytx) {
                fprintf(stderr, "INFO: Sending signal from file: %s\n", argv[i+1]);
                io_file = fopen(argv[i+1], "r");
            } else {
                fprintf(stderr, "INFO: Writing signal to file: %s\n", argv[i+1]);
                io_file = fopen(argv[i+1], "wb");
            }
            if( (!txr & !rxr) & !io_file ) {
                fprintf(stderr, "ERROR: Unable to open file\n");
                return 1;
            }

            if(send) {
                uint8_t *buffer;
                long size;
                // Get file size
                fseek(io_file, 0, SEEK_END);
                size = ftell(io_file);
                rewind(io_file);

                buffer = (uint8_t*)malloc(sizeof(uint8_t)*size);
                fread(buffer, sizeof(uint8_t), size, io_file);
                fclose(io_file);

                if( Ir.SendIR(38000, buffer, size) ) {
                    fprintf(stderr, "INFO: Sent IR signal\n");
                } else
                    fprintf(stderr, "ERROR: Unable to send IR\n");

                free(buffer);
            } else if(txr) {
                char input[MaxInputLength];
                
                while(1) {
                    fprintf(stderr, ">:");
                    
                    // read input
                    if(fgets(input, sizeof(input), stdin) == NULL) break;

                    // remove trailing newline
                    input[strcspn(input, "\n")] = 0;
                    
                    // Check for quit
                    if (strcmp(input, "quit") == 0) {
                        break;
                    }

                    // validate length, 4 chars
                    int len = strlen(input);
                    if(len < 4 || len > 4) {
                        fprintf(stderr, "INFO: Must be 4 hex digits long\n");
                        continue;
                    }

                    // validate all chars are hex
                    int valid = 1;
                    for(int i = 0; i < len; i++) {
                        if(!isxdigit(input[i])) {
                            valid = 0;
                            break;
                        }
                    }

                    if(!valid) {
                        fprintf(stderr, "ERROR: non-hex chars used\n");
                        continue;
                    }

                    // convert and check range
                    char *endptr;
                    unsigned long value = strtoul(input, &endptr, 16);

                    if(*endptr != '\0') {
                        fprintf(stderr, "ERROR: invalid hex format\n");
                    } else if(value > 0xffff) {
                        fprintf(stderr, "ERROR: value exceeds 16 bits\n");
                    } else {
                        fprintf(stderr, "INFO: Valid input - 0x%04lX\n", value);
                    }

                    if( Ir.SendNecSignal(value) ) {
                        fprintf(stderr, "INFO: Sent IR signal\n");
                    } else
                        fprintf(stderr, "ERROR: Unable to send IR\n");
                    
                    //memset(input, 0, sizeof(input));
                    if (len == sizeof(input) - 1 && input[len - 1] != '\n') {
                        int c;
                        while ((c = getchar()) != '\n' && c != EOF);
                    }
                    
                }
            } else if(rxr) {
                
            } else if(ytx) {
                uint16_t *buffer;
                char line[256];
                int lineNum = 0;
                int count = 0;
                
                while(fgets(line, sizeof(line), io_file) != NULL && count < 64) {
                    lineNum++;
                    
                    // remove newline and trailing whitespace
                    line[strcspn(line, "\n")] = 0;
                    
                    // Check for quit
                    if (strcmp(line, "#quit") == 0) {
                        break;
                    }
                    
                    // Check for wait
                    if(strcmp(line, "#wait") == 0) {
                        sleep(1);
                        continue;
                    } else if(strcmp(line, "#wake") == 0) {
                        // this is a wakeup period so it takes up to 15s to be ready
                        sleep(15);
                        continue;
                    } else {
                        // need to sleep a little between each command otherwise too fast
                        usleep(500000);
                    }
                    
                    //fprintf(stderr, "Line: %X\n", line);

                    // validate length, 4 chars
                    int len = strlen(line);
                    if(len < 4 || len > 4) {
                        fprintf(stderr, "ERROR: Must be 4 hex digits long\n");
                        break;
                    }

                    // validate all chars are hex
                    int valid = 1;
                    for(int i = 0; i < len; i++) {
                        if(!isxdigit(line[i])) {
                            valid = 0;
                            break;
                        }
                    }

                    if(!valid) {
                        fprintf(stderr, "ERROR: non-hex chars used\n");
                        break;
                    }

                    // convert and check range
                    char *endptr;
                    unsigned long value = strtoul(line, &endptr, 16);

                    if(*endptr != '\0') {
                        fprintf(stderr, "ERROR: invalid hex format\n");
                    } else if(value > 0xffff) {
                        fprintf(stderr, "ERROR: value exceeds 16 bits\n");
                    } else {
                        //fprintf(stderr, "INFO: Valid input - 0x%04lX\n", value);
                    }

                    if( Ir.SendNecSignal(value) ) {
                        fprintf(stderr, "INFO: Sent IR signal 0x%04lX\n", value);
                    } else
                        fprintf(stderr, "ERROR: Unable to send IR\n");
                }
                
            } else {
                signal_received = false;
                if( Ir.StartRecvIR() ) {
                    fprintf(stderr, "INFO: Waiting for IR signal\n");
                    while( !signal_received )
                        usleep(1000);
                } else
                    fprintf(stderr, "ERROR: Unable to receive IR\n");
            }
        }
    } else
        fprintf(stderr, "ERROR: Unable to open the device\n");

    fprintf(stderr, "INFO: Closing device\n");
    Ir.Close();

    return err >= 0 ? err : -err;
}
