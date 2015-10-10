## Trekkie — Pebble Watchface

![Screenshot](https://raw.githubusercontent.com/orenbenkiki/trekkie/master/screenshot.png)

An LCARS-inspired Pebble watchface, intended for my personal usage, so you
won't find it on the pebble store. Feel free to clone and adapt for your needs.

*I hold no responsibility if it breaks your pebble. May contain nuts. YMMV.*
It works fine for me, though.

This is a heavily modified version of [Trekkie](https://github.com/remixz/trekkie):

* Only works on pebble time (uses color, etc.).

* Shows a possibly too blatant bluetooth connection status image.

* Texts on the left show (from top to bottom):

  * Name of day.

  * Name of month.

  * Compass heading (N / NE / E / SE / S / SW / W / NW). This takes a bit of
    time to calibrate after installation or recharging.

  * Week number in year (I work in Intel and they live by "work week" numbers).
    There are many ways to compute week number in year and Intel
    (unsurprisingly) uses whatever Outlook does, which seems to be unrelated to
    any standard. I abuse the 12/24 clock setting to optionally add 1 to the
    work week to make it show what I want in each year. The clock itself is
    always at 24 hours (hey, it is consistent with the LCARS theme :-). The
    right thing to would be to add a proper configuration screen, and a nice
    icon for the watch faces list, but I don't care much about all that.

* Bottom bar shows battery status with a prediction of time left (D+HH). This
  takes a bit of time to calibrate after installation. It uses a moving average
  to learn the discharge rate so it should adapt to your usage pattern. The
  code "should" also show the time left for a full charge when charging, but
  somehow this doesn't work (and is impossible to debug on CloudPebble). Nobody
  looks at their phone while charging anyway.
