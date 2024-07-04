void umkaRuntimeCompile(const char *path);

// System functions.
void umkaRuntimeRegister(const char *symbol, void *addr);
void umkaRuntimeInit();
void umkaRuntimeHandleIRQ(unsigned char irq);
void umkaRuntimeSchedule();
