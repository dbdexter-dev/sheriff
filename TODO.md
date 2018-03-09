# Sheriff.c
* Implement preview window
* Reduce the number of #define directives
* Enable top bar truncation a la fish
* Allow traversing links if they link to directories
* Allow for multiple files to be opened simultaneously
* Keep the directory contents updated

# Config.h
* Add highlighting options

# Dir.c/Backend.c
* Implement directory list caching

# Clipboard.c
* Rewrite a custom copy-paste function that reports the progress

Also:
* Go forward, highlight a file, go forward, then back twice. The highlighted
  element in the center row is not the one logically selected.
