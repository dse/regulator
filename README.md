# regulator

A somewhat quick-and-dirty C program for calibrating (regulating)
mechanical clocks and watches.

The goal is to do two things:

1.  Guess the number of ticks per hour.

2.  Guess how fast or slow the clock is, in terms of ±seconds per day.

It's in C, and uses the PulseAudio API.

## Build Requirements

1.  [pulseaudio](https://www.freedesktop.org/wiki/Software/PulseAudio/)

2.  [libsndfile](http://www.mega-nerd.com/libsndfile/), used by
    audacity, does not support mp3

    I really only chose libsndfile over libavcodec because
    libavcodec's API is confusing.

## Got macOS?

```
brew install pulseaudio libsndfile
```

## Got Linux?

```
sudo apt-get install libpulse-dev
```
