# Yousician Transcoder

[Yousician](http://yousician.com/) is a great application for learning and
practicing playing guitar and other instruments. Yousician provides access to
large library of songs, some provided by the Yousician creators, other
user-uploaded. While the songs from Yousician use backing tracks in the OGG
Vorbis format, most of the user-uploaded songs use MP3. Unfortunately Yousician
cannot play that on Linux (probably due to Unity 3D engine limitations).

This project implements a hack which allows the Linux version of Yousician to
play MP3 backing tracks.

## Requirements

This should work on any Linux system that provides a suitable command-line
utility for transcoding from MP3 to OGG. By default the 'sox' utility is used,
but it can be replaced with 'ffmpeg' or anything else.

## Usage

Compile the wrapper library with:
```
make
```

Or just use the provided binary package.

Put the '`YousicianT`' and `'yousician_transcode.so'` files in the Yousician
base directory (the folder when the 'Yousician Launcher' executable is).

Use 'YousicianT' instead of 'Yousician Launcher' to start the application.

To use 'ffmpeg' (or other command-line tool) instead of 'sox' for file
conversion, edit the 'YousicianT' script.
