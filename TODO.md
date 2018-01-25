# Sheriff.c
* Implement directory navigation
* Show file size on the right side of the window
* Implement directory preview in the right pane
* Implement bottom status bar (last modified, overall directory size)
* Implement preview window
* Implement config.h keybinding configuration
* Reduce the number of #define directives in sheriff.c
* Enable top bar truncation a la fish
* Preserve highlighting through SIGWINCHes

# Dir.c
* Figure out what the hell is going on with list\_xchg when the two swaps are changed, and why it doesn't work in that case
* Sort directories and filenames alphabetically, case insensitive (should be a & ~0x40 to get all uppercase, but that's not unicode...)

# Makefile
* Add release target (-O2 and strip and stuff)

Also:
* The UI shits itself when unicode is involved
