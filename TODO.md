# Sheriff.c
* Implement directory navigation
* Implement preview window
* Implement config.h keybinding configuration
* Implement window scrolling
* Reduce the number of #define directives in sheriff.c
* Enable top bar truncation a la fish
* Preserve highlighting through SIGWINCHes

# Dir.c
* Figure out what the hell is going on with list\_xchg when the two swaps are changed, and why it doesn't work in that case

# Makefile
* Add release target (-O2 and strip and stuff)

Also:
* The UI shits itself when unicode is involved
