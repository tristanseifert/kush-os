void init(void) {
    char *video = (char*) 0xB8000;
    const char *msg = "Welcome to the kush zone.";

    // clear video
    for(int i = 0; i < 25*80; i++) {
        video[2*i] = 0;
    }

    // write message
    for(int i = 0; msg[i]; i++) {
        video[2*i] = msg[i];
        video[2*i + 1] = 0x07;
    }
}

