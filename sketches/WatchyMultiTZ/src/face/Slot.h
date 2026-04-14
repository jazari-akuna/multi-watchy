#pragma once
// The four card rectangles of the watchface. Laid out with a 1-px black
// border around the whole screen and 1-px gaps between cards so every card
// gets its own clean frame on the e-paper panel.
//
//   y=  0         screen edge (1-px gutter)
//   y=  1..49     top-left alt TZ card   (100 x 49)
//                 top-right alt TZ card  (100 x 49, x=101)
//   y= 50         gap
//   y= 52..121    main TZ card           (198 x 70)
//   y=122..123    gap
//   y=124..198    event card             (198 x 75)
//   y=199         screen edge

#include "../hal/Types.h"

namespace wmt {

static constexpr Rect SLOT_ALT_LEFT  = {   1,   1,  99, 49 };
static constexpr Rect SLOT_ALT_RIGHT = { 101,   1,  99, 49 };
static constexpr Rect SLOT_MAIN      = {   1,  52, 198, 70 };
static constexpr Rect SLOT_EVENT     = {   1, 124, 198, 75 };

} // namespace wmt
