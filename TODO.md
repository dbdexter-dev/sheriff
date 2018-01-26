# Sheriff.c
* Implement preview window
* Implement window scrolling
  * Update on resize
  * Fix indexes not updating when entering a directory
  * Also, what the hell is going on with the desyncs >.<
* Reduce the number of #define directives in sheriff.c
* Enable top bar truncation a la fish
* Implement directory list caching
* Fix screen not updating correctly after executing past the execlp block
* Rewrite resize\_handler to resize as init\_windows does

# Makefile
* Add release target (-O2 and strip and stuff)

Also:
* The UI shits itself when unicode is involved
