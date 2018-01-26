# Sheriff.c
* Implement preview window
* Reduce the number of #define directives in sheriff.c
* Enable top bar truncation a la fish
* Implement directory list caching
* Fix screen not updating correctly after executing past the execlp block
* Rewrite resize\_handler to resize as init\_windows does
* Switch from insertion sort to something more efficient
* Implement quick search
* Allow traversing links if they link to directories

# Makefile
* Add release target (-O2 and strip and stuff)

Also:
* The UI shits itself when unicode is involved
