
#include "../include/keyboard.h"

/* VGA text-mode configuration */
#define VIDEO_MEMORY ((volatile unsigned short *)0xB8000)
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define SCREEN_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)
#define COLOR 0x0F

/* Directory configuration */
#define MAX_DIRECTORIES 32
#define MAX_NAME_LENGTH 32

/* Console state */
static int cursor = 0;

/* In-memory directory storage */
static char directories[MAX_DIRECTORIES][MAX_NAME_LENGTH];
static int directory_count = 0;

/* String length */
static int string_length(const char *str)
{
    int length = 0;

    while (str[length])
        length++;

    return length;
}

/* Compare strings */
static int string_equal(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return 0;

        a++;
        b++;
    }

    return (*a == '\0' && *b == '\0');
}

/* Copy string */
static void string_copy(char *dest, const char *src)
{
    int i = 0;

    while (src[i] && i < MAX_NAME_LENGTH - 1)
    {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

/* Clear screen */
static void console_clear(void)
{
    for (int i = 0; i < SCREEN_SIZE; i++)
    {
        VIDEO_MEMORY[i] =
            ((unsigned short)COLOR << 8) | ' ';
    }

    cursor = 0;
}

/* Scroll screen */
static void console_scroll(void)
{
    for (int i = 0; i < SCREEN_SIZE - SCREEN_WIDTH; i++)
    {
        VIDEO_MEMORY[i] = VIDEO_MEMORY[i + SCREEN_WIDTH];
    }

    for (int i = SCREEN_SIZE - SCREEN_WIDTH; i < SCREEN_SIZE; i++)
    {
        VIDEO_MEMORY[i] =
            ((unsigned short)COLOR << 8) | ' ';
    }

    cursor = SCREEN_SIZE - SCREEN_WIDTH;
}

/* Print character */
static void console_putc(char c)
{
    if (c == '\n')
    {
        cursor = ((cursor / SCREEN_WIDTH) + 1) * SCREEN_WIDTH;
    }
    else if (c == '\r')
    {
        cursor = (cursor / SCREEN_WIDTH) * SCREEN_WIDTH;
    }
    else if (c == '\b')
    {
        if (cursor > 0)
        {
            cursor--;

            VIDEO_MEMORY[cursor] =
                ((unsigned short)COLOR << 8) | ' ';
        }
    }
    else
    {
        if (cursor >= SCREEN_SIZE)
            console_scroll();

        VIDEO_MEMORY[cursor++] =
            ((unsigned short)COLOR << 8) | (unsigned char)c;
    }

    if (cursor >= SCREEN_SIZE)
        console_scroll();
}

/* Print string */
static void console_write(const char *str)
{
    while (*str)
        console_putc(*str++);
}

/* Print number */
static void console_write_number(int number)
{
    char buffer[12];
    int i = 0;

    if (number == 0)
    {
        console_putc('0');
        return;
    }

    while (number > 0 && i < 11)
    {
        buffer[i++] = '0' + (number % 10);
        number /= 10;
    }

    while (i > 0)
        console_putc(buffer[--i]);
}

/* Help command */
static void shell_help(void)
{
    console_write("\nAvailable commands:\n");
    console_write("  help       - Show commands\n");
    console_write("  clear      - Clear screen\n");
    console_write("  about      - About CampusOS\n");
    console_write("  version    - Show OS version\n");
    console_write("  echo TEXT  - Display a message\n");
    console_write("  mkdir NAME - Create directory\n");
    console_write("  ls         - List directories\n");
    console_write("  shutdown   - Shut down OS\n");
}

/* Echo command */
static void shell_echo(const char *command)
{
    if (command[4] == ' ')
    {
        console_write("\n");
        console_write(command + 5);
        console_write("\n");
    }
    else
    {
        console_write("\n");
    }
}

/* Create directory */
static void shell_mkdir(const char *name)
{
    if (*name == '\0')
    {
        console_write("\nUsage: mkdir NAME\n");
        return;
    }

    if (string_length(name) >= MAX_NAME_LENGTH)
    {
        console_write("\nDirectory name too long.\n");
        return;
    }

    for (int i = 0; i < directory_count; i++)
    {
        if (string_equal(directories[i], name))
        {
            console_write("\nDirectory already exists.\n");
            return;
        }
    }

    if (directory_count >= MAX_DIRECTORIES)
    {
        console_write("\nDirectory storage is full.\n");
        return;
    }

    string_copy(directories[directory_count], name);
    directory_count++;

    console_write("\nDirectory created: ");
    console_write(name);
    console_write("\n");
}

/* List directories */
static void shell_ls(void)
{
    console_write("\nDirectories:\n");

    if (directory_count == 0)
    {
        console_write("  No directories found.\n");
        return;
    }

    for (int i = 0; i < directory_count; i++)
    {
        console_write("  ");
        console_write(directories[i]);
        console_write("/\n");
    }
}

/* Shutdown computer */
static void shell_shutdown(void)
{
    console_write("\nShutting down CampusOS...\n");

    /*
     * QEMU ACPI shutdown port.
     * Requires compatible QEMU/ACPI configuration.
     */
    __asm__ volatile (
        "outw %0, %1"
        :
        : "a"((unsigned short)0x2000),
          "Nd"((unsigned short)0x0604)
    );

    /* Alternative QEMU/Bochs shutdown port */
    __asm__ volatile (
        "outw %0, %1"
        :
        : "a"((unsigned short)0x2000),
          "Nd"((unsigned short)0xB004)
    );

    /* Halt if shutdown was not accepted */
    while (1)
    {
        __asm__ volatile ("cli; hlt");
    }
}

/* Execute shell command */
static void shell_execute(const char *command)
{
    if (string_equal(command, "help"))
    {
        shell_help();
    }
    else if (string_equal(command, "clear"))
    {
        console_clear();
    }
    else if (string_equal(command, "about"))
    {
        console_write("\nCampusOS\n");
        console_write("A small educational operating system.\n");
        console_write("Built with C, Assembly, and GRUB.\n");
    }
    else if (string_equal(command, "version"))
    {
        console_write("\nCampusOS version 0.1\n");
    }
    else if (command[0] == 'e' &&
             command[1] == 'c' &&
             command[2] == 'h' &&
             command[3] == 'o' &&
             (command[4] == ' ' || command[4] == '\0'))
    {
        shell_echo(command);
    }
    else if (command[0] == 'm' &&
             command[1] == 'k' &&
             command[2] == 'd' &&
             command[3] == 'i' &&
             command[4] == 'r' &&
             command[5] == ' ')
    {
        shell_mkdir(command + 6);
    }
    else if (string_equal(command, "mkdir"))
    {
        console_write("\nUsage: mkdir NAME\n");
    }
    else if (string_equal(command, "ls"))
    {
        shell_ls();
    }
    else if (string_equal(command, "shutdown"))
    {
        shell_shutdown();
    }
    else if (command[0] == '\0')
    {
        /* Ignore empty commands */
    }
    else
    {
        console_write("\nUnknown command: ");
        console_write(command);
        console_write("\nType 'help' to see available commands.\n");
    }
}

/* Display prompt */
static void shell_prompt(void)
{
    console_write("\nCampusOS> ");
}

/* Kernel entry point */
void kernel_main(void)
{
    char command[128];
    int command_length = 0;

    console_clear();

    console_write("================================\n");
    console_write("       Welcome to CampusOS\n");
    console_write("================================\n");
    console_write("Kernel booted successfully!\n");
    console_write("CampusOS version 0.1\n");
    console_write("Type 'help' to see available commands.\n");

    keyboard_init();

    shell_prompt();

    while (1)
    {
        char c = keyboard_read_char();

        if (c == '\0')
            continue;

        if (c == '\n' || c == '\r')
        {
            command[command_length] = '\0';

            shell_execute(command);

            command_length = 0;

            shell_prompt();
        }
        else if (c == '\b')
        {
            if (command_length > 0)
            {
                command_length--;
                console_putc('\b');
            }
        }
        else if (c >= 32 && c <= 126)
        {
            if (command_length < (int)sizeof(command) - 1)
            {
                command[command_length++] = c;
                console_putc(c);
            }
        }
    }
}
