# Sheriff

Sheriff is a [Ranger](https://ranger.github.io/) clone written in C, developed
with the "suckless philosophy" in mind.
You can learn more about suckless [here](https://suckless.org/).

## Configuration

As with other suckless projects, changing configuration requires editing the
src/config.h header and recompiling.

* **Keybinds**: Stored in the *keys* array. Each element has 3 fields: a key, a
  function, and, optionally, an argument to said function.  The array has to be
  terminated with an element with a 0 in every field.
  There's a special function, *chain*, which allows chaining multiple keys
  together. Basically, you can pass another array of keybinds to it, and create
  multi-character bindings that way.
* **File associations**: Stored in the *associations* array. Here you can
  specify a default program to open files with a certain extension. Each element
  of the array has 2 fields: an extension, and the name of the executable to
  launch.

## Compiling and installing

To compile and install sheriff, run:
```
make && sudo make install
```

If you want to disable compiler optimizations and keep all debug symbols, a
`debug` target is available.
