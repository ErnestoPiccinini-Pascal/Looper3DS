#include <3ds.h>
#include <stdio.h>

int main()
{
    // Initialize the console
    consoleInit(GFX_TOP, NULL);

    // Print a message to the console
    printf("Hello, 3DS World!\n");

    // Main loop
    while (aptMainLoop())
    {
        // Scan for input
        hidScanInput();

        // Check if the A button is pressed
        if (hidKeysDown() & KEY_A)
            printf("A button pressed!\n");

        // Check if the B button is pressed
        if (hidKeysDown() & KEY_B)
            printf("B button pressed!\n");

        // Check if the Start button is pressed
        if (hidKeysDown() & KEY_START)
            printf("Start button pressed!\n");

        // Check if the D-Pad Up is pressed
        if (hidKeysDown() & KEY_UP)
            printf("D-Pad Up pressed!\n");

        // Check if the D-Pad Down is pressed
        if (hidKeysDown() & KEY_DOWN)
            printf("D-Pad Down pressed!\n");

        // Check if the D-Pad Left is pressed
        if (hidKeysDown() & KEY_LEFT)
            printf("D-Pad Left pressed!\n");

        // Check if the D-Pad Right is pressed
        if (hidKeysDown() & KEY_RIGHT)
            printf("D-Pad Right pressed!\n");

        // Wait for the next frame
        gspWaitForVBlank();
    }

    return 0;
}