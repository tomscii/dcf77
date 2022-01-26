#include "common.h"
#include "clock.h"
#include "pwm.h"
#include "backlight.h"

static const int IDLE_ON_TIME_TICKS = (F_TICK * 8);
static const int BRIGHTNESS = 96;

enum backlight_state_t
{
   OFF, RAMP_UP, ON, RAMP_DOWN
};
static enum backlight_state_t state = OFF;
static int state_ticks = 0; // ticks since state transition

void
backlight_on_tick ()
{
   switch (state)
   {
   case OFF:
      return;
   case ON:
      if (++state_ticks == IDLE_ON_TIME_TICKS)
      {
         state = RAMP_DOWN;
         state_ticks = 0;
      }
      return;
   case RAMP_UP:
      if (pwm_get_led (IDX_LED_R) < BRIGHTNESS)
         pwm_nudge_led (IDX_LED_R, 8);
      else
         state = ON;
      return;
   case RAMP_DOWN:
      if (pwm_get_led (IDX_LED_R) > 0)
         pwm_nudge_led (IDX_LED_R, -8);
      else
         state = OFF;
      return;
   }
}

void
backlight_on_second ()
{
}

char backlight_is_on ()
{
   switch (state)
   {
   case OFF:
   case RAMP_DOWN:
      return 0;
   case ON:
   case RAMP_UP:
      return 1;
   }

   __builtin_unreachable ();
}

char backlight_set (char on)
{
   char was_on = backlight_is_on ();
   if (was_on == (on ? 1 : 0))
   {
      if (state == ON || state == OFF)
         state_ticks = 0;

      return was_on;
   }

   if (on)
   {
      state = RAMP_UP;
      state_ticks = 0;
   }
   else
   {
      state = RAMP_DOWN;
      state_ticks = 0;
   }

   return was_on;
}
