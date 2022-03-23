#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>

#define F_BTN_MODE      1
#define F_BTN_SET       (1 << 1)
#define F_BTN_ADJ       (1 << 2)

/* Generated events follow this lifecycle:
 * - a TOUCH event immediately on button press
 * - a HOLD event every CNT_THRESH_HOLD jiffies, as long as button is held
 * - a RELEASE event immediately on button release
 */
#define F_EVT_TOUCH     (1 << 4)
#define F_EVT_HOLD      (1 << 5)
#define F_EVT_RELEASE   (1 << 6)

#define F_BTN_MASK      (F_BTN_MODE | F_BTN_SET | F_BTN_ADJ)
#define F_EVT_MASK      (F_EVT_TOUCH | F_EVT_HOLD | F_EVT_RELEASE)

// button holding period (in jiffies)
#define CNT_THRESH_HOLD      100

struct pbtn_flags_t
{
   uint8_t momentary: 3;      // bitwise F_BTN_* of momentary state
   uint8_t previous: 3;       // bitwise F_BTN_* of previous state
   uint8_t ack_event: 1;      // acknowledge pending_event
   uint8_t pending_event;     // bitwise F_* or zero if no event
};
extern volatile struct pbtn_flags_t pbtn_flags;

void pbtn_setup ();

void pbtn_on_tick ();
