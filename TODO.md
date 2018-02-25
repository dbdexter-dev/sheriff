# Sheriff.c
* Implement preview window
* Reduce the number of #define directives
* Enable top bar truncation a la fish
* Allow traversing links if they link to directories
* Add tabbed navigation
* Asynchronous directory updates (i.e. don't lock up when copying files)

# Config.h
* Add highlighting options

# Dir.c/Backend.c
* Implement directory list caching

Also:
* Changing focus causes extraneous input when a file is opened with vim
* TERM=xterm-256color messes with the highlighting
