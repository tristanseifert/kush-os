#include <stdint.h>

void kernel_init();
void kernel_main();

static void *get_pc () { return __builtin_return_address(0); }

static const char *msg = "Welcome to the kush zone.   PC: $";

/**
 * Early kernel initialization
 */
void kernel_init() {
    char *video = (char*) 0xC00B8000;

    // clear video
    for(int i = 0; i < 25*80; i++) {
        video[2*i] = 0;
    }

    // write message
    int i;
    for(i = 0; msg[i]; i++) {
        video[2*i] = msg[i];
        video[2*i + 1] = 0x07;
    }

    // write PC
    uint32_t pc = (uint32_t) get_pc();

    for(int x = 0; x < 8; x++) {
        uint32_t mask = 0xF << ((7 - x) * 4);
        uint32_t nybble = (pc & mask) >> ((7 - x) * 4);

        if(nybble <= 0x09) {
            video[2*i] = '0' + nybble;
            video[2*i+1] = 0x70;
        } else {
            video[2*i] = 'A' + nybble - 0x0A;
            video[2*i+1] = 0x70;
        }

        i++;
    }
}

/**
 * After all systems have been initialized, we'll jump here. Higher level kernel initialization
 * takes place and we go into the scheduler.
 */
void kernel_main() {

}
