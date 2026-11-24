/* Standard GUI entry point for the optional MinGW-w64 build. */
void start(void);

int WinMain(void *instance, void *previous, char *command, int display) {
    (void)instance;
    (void)previous;
    (void)command;
    (void)display;
    start();
    return 0;
}
