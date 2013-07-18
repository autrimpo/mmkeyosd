mmkeyosd
========

mmkeyosd means Multimedia key OSD. It allows you
to use your multimedia keys (and any other keys) to display an OSD.

TODO
----
* xinerama

Configuration
-------------
Keys are configured in ~/.mmkeyosd/keys. The syntax is easy; each line 
is a keybinding. First field specifies which keys to use. Syntax of key field is:

	[Any of Super, Alt, Control+]Key
	ControlAlt+m
	AudioRaiseVolume

The second field tells which draw function to use. Possible values are `text_with_text` and `text_with_bar`.
The third field must always be surrounded by quotes (but it may contain spaces). This 
text is the heading of the OSD. The last field is the command which changes some state
and produces a one line output, either a text for `text_with_text` or an integer between
0 and 100 for `text_with_bar` (that is, the percentage of the bar).

You can break a line into multiple lines by typing a `\` as the last character on the line.

Superficial and behaviorial settings are configured in ~/.mmkeyosd/settings. Syntax is:

	key = value
	fgcolor = green

Possible settings are:

	fontbig          string (Helvetica-15)
	fontsmall        string
	fgcolor          string (#ffffff, white)
	bgcolor          string
	errcolor         string
	borderwidth      unsigned int
	windowwidth      unsigned int
	windowheight     unsigned int
	barwidth         unsigned int
	barheight        unsigned int
	opacity          float (0.0 - 1.0)
	windowtimeout    unsigned int (milliseconds)
	shell            string

Everything else, empty lines and lines starting with `#` are ignored.
See keys.example and settings.example for more info.

Thanks to
---------
Thanks to the authors of dwm and yeahconsole

