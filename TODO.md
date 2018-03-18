# Sheriff.c
* Implement preview window
* Allow for multiple files to be opened simultaneously
* Chmod

# Clipboard.c
* Fix progress bar glitches when multiple file operations are carried out at the
  same time
* Preserve selection between directory redraws

# Config.h
* Add file extension based highlighting options

# Dir.c/Backend.c
* Implement directory list caching

Also:
* Go forward, highlight a file, go forward, then back twice. The highlighted
  element in the center row is not the one logically selected.
