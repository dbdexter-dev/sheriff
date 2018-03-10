# Guidelines for editing code

All the C code follows the
[suckless coding style](https://suckless.org/coding_style),
except for headers, where include guards are still present.
That's because I find it a lot clearer if a file using a certain function
directly includes the header where the prototype is defined, regardless of where
else that header is included.

I tried to keep everything as modular as possible, so that it's (hopefully)
easier to understand what a certain function is doing, and the complexity of any
given function is reduced. Here's a tl;dr of what each .c file contains:

* **backend.c**: functions that operate on PaneCtx structs.
* **clipboard.c**: functions that deal with operating on files in a clipboard,
  e.g. moving, copying, deleting, linking, and the like.
* **dir.c**: functions that deal with the Direntry backend, populating Fileentry
  arrays and updating values inside a Direntry struct.
* **ncutils.c**: auxiliary functions for some common ncurses tasks, like
  changing the highlighted line.
* **sheriff.c**: main(), keybinding functions and generally any function that
  must have access to basically everything, or to elements on different
  abstraction levels.
* **tabs.c**: functions that add, change or remove whole tab contexts
* **ui.c**: functions that handle drawing things on the ncurses windows,
  translating the data inside a PaneCtx struct into panes, bars and text lines.
  These functions operate on Direntry structs.
* **utils.c**: simple, random auxiliary functions that manipulate primitive C
  data types.
