# Sheriff.c
* Implement preview window
* Reduce the number of #define directives
* Enable top bar truncation a la fish
* Allow traversing links if they link to directories
* Implement file moving/deleting/yanking
* Implement file selection
* Add tabbed navigation

# Dir.c/Backend.c
* Implement directory list caching

Also:
* The UI shits itself when unicode is involved
* How about this: instead of freeing and allocating memory every time the directory changes, keep a max\_nodes count in the struct direntry*, and allocate nodes as needed when the directory structure changes. Never free any of the tree nodes, right until the very end. This increases the total memory usage at any point, but the overall alloc'd space is significantly lower, and so is the time wasted in system calls.
