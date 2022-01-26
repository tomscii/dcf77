void backlight_on_tick ();

char backlight_is_on ();

// return previous 'on' state, equivalent to calling backlight_is_on ()
char backlight_set (char on);
