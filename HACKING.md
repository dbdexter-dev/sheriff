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

* **sheriff.c**: main(), keybinding functions and generally any function that
  *must* access the global Dirview array (for instance, functions that refresh
  the whole screen and keep the Direntry pointers synchronized between different
  windows).
* **backend.c**: functions that don't really have to access any global
  variables, but do manipulate both the ncurses frontend and the Direntry
  backend.
* **ncutils.c**: functions that deal with the ncurses frontend on a window
  basis, so refreshing a listing, redrawing a bar, that sort of thing.
* **dir.c**: functions that deal with the Direntry backend, populating Fileentry
  arrays and updating values inside the Direntry struct.
* **clipboard.c**: functions that deal with operating on files in a clipboard,
  e.g. moving, copying, deleting, linking, and the like.
* **utils.c**: simple, random utility functions that manipulate primitive C data
  types.

