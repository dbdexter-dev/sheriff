# Sheriff.c
* Implement preview window
* Reduce the number of #define directives
* Enable top bar truncation a la fish
* Allow traversing links if they link to directories
* Implement file moving/deleting/yanking
* Implement file selection
* Add tabbed navigation
* Asynchronous directory updates (i.e. don't lock up when copying files)

# Config.h
* Add highlighting options

# Dir.c/Backend.c
* Implement directory list caching

Also:
* The UI shits itself when unicode is involved
* Kill the zombies ffs, and refresh after waitpid()
* Something's wrong with the search: highlight a directory, then search for a
  file. Third pane shits itself.
* Changing focus causes extraneous input when a file is opened with vim
