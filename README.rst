libaosd-xinerama
================

:Description: libaosd (Atheme OSD) with Xinerama support and aosd_cat enhancements
:License: see LICENSE for details
:AUR: https://aur.archlinux.org/packages/libaosd-xinerama-git/

This is a fork of https://github.com/atheme/libaosd with following enhancements.

``libaosd`` / ``libaosd-text``:

* Xinerama support added
* renders Pango color markup when configured for full foreground opacity

``aosd_cat``:

* shows input immediately by default (see ``-W`` option for old behavior)
* cycles window on receiving line containing ``^G`` as single character
* new options added (see summary below)

``aosd_cat`` new options::

    -O, --output=-1         Sets the Xinerama output (0-N), -1 for whole X screen.
    -A, --alignment=0       Sets the text alignment (0=left, 1=center, 2=right).
    -W, --wait              Wait until the display is clear.
    -k, --keep-reading      Reopen input on EOF (doesn't have any effect when reading from stdin).
    -m, --markup            Enable Pango markup language.
    -T, --input-timeout=0   Sets the input timeout (in seconds).
    -P, --spacing=0         Sets the line-height adjustment (in pixels)


Usage
-----

Based on ``aosd_cat`` you can devise simple OSD daemon, such as::

    aosd_cat -O 0 -p 2 -x -10 -y 25 -e 0 -d 5 \
             -t 0 -n local_osd -A 2 \
             -B black -R yellow \
             -f 0 -u 1000 -o 0 \
             -l 1 \
             -k -m -i "$pipe"

where:

* ``-O 0`` - use output 0 (which corresponds to primary one if it is defined)
* ``-A 2`` - right alignment of text (doesn't make difference here since there is only one line)
* ``-k``   - if all readers exit ``$pipe`` will EOF so let's reopen it in that case
* ``-m``   - interpret markup

Above is a part of a my daemon script called ``osd`` / ``osdd``
(`source <https://github.com/mkoskar/homefiles/blob/master/bin/osd>`_).

I use it for notifications of system changes as a result of trigger
(usually window manager shortcut) such as:

* audio settings (volume, mute on/off, dock on/off)
* dpms on/off
* rfkill
* xkb layouts


Implementation Notes
--------------------

Basic mode of ``aosd_cat`` operation has been modified such that there is a
separate thread for reading an input stream. After OSD window is rendered
in full opacity, main thread repeatedly (in interval of 25ms) checks for
"data ready" flag. If set, the window is kept shown, data retrieved,
window updated and rendering continues on full opacity with ``--fade-full``
interval reset.

It is possible to improve on this, such that reader thread will send X client
message to inform main thread (while in X processing loop) of new data and thus
prevent unnecessary checks.
