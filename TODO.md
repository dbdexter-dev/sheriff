# Sheriff.c
* SIGWINCH does not refresh the views for some reason (even though show\_listing has a wrefresh() at the end)
* Figure out a way to change A\_REVERSE on a line without changing/losing the colors
* Implement directory navigation
* Show file size on the right side of the window
* Implement directory preview in the right pane
* Implement bottom status bar (permissions, owner id, last modified, overall directory size)
* Implement preview window
* Implement config.h keybinding configuration
* Reduce the number of #define directives in sheriff.c

# Dir.c
* Figure out what the hell is going on with list\_xchg when the two swaps are changed, and why it doesn't work in that case
* Sort directories and filenames alphabetically, case insensitive (should be a & ~0x40 to get all uppercase)

# Makefile
* Add release target (-O2 and strip and stuff)
