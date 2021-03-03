# mipsze  
A small prototype text editor with syntax highlighting for mips assembly that will eventually be rolled into mips2c to create a mips ide and emulator.    
Written in C using ncurses. Based mostly on `GNU nano`.  

Supports:
- macro and label highlighting
- instructions and pseudoinstruction highlighting
- some multicarat support
- saving / loading documents

Todo still:
- Custom colors
- Extended colors
- Multiline cut/paste
- Better error checking
- Lots of bug fixes
- Tests
  
This is still very much a work in progress, I wouldn't recommend using it on any data you like.  
  
To build: `make`  
To open a file for editing: `./mipsze filename.ext`  
compiled and tested with: `gcc v10.2.0` and `GNU make 4.1` on `ubuntu 16.04`. Tested with `Byobu terminal`, `XTerm`, and `GNOME terminal`.

<p align="center">
  <img src="https://github.com/tylerweston/eztxt/blob/main/mipszescreenshot.png?raw=true" />
</p>
