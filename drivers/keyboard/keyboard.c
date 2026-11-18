#include "../../include/keyboard.h"

static inline unsigned char inb(unsigned short port)
{
    unsigned char result;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(result)
        : "Nd"(port)
    );

    return result;
}

void keyboard_init(void)
{
}

int keyboard_try_read_char(char *out)
{
    if (!(inb(0x64) & 1))
        return 0;

    unsigned char sc = inb(0x60);

    if (sc & 0x80)
        return 0;

    if (sc >= 0x02 && sc <= 0x0B) {
        static const char keys[] = "1234567890";
        *out = keys[sc - 0x02];
    }
    else if (sc >= 0x10 && sc <= 0x19) {
        static const char keys[] = "qwertyuiop";
        *out = keys[sc - 0x10];
    }
    else if (sc >= 0x1E && sc <= 0x26) {
        static const char keys[] = "asdfghjkl";
        *out = keys[sc - 0x1E];
    }
    else if (sc >= 0x2C && sc <= 0x32) {
        static const char keys[] = "zxcvbnm";
        *out = keys[sc - 0x2C];
    }
    else if (sc == 0x39)
        *out = ' ';
    else if (sc == 0x1C)
        *out = '\n';
    else if (sc == 0x0E)
        *out = '\b';
    else
        return 0;

    return 1;
}