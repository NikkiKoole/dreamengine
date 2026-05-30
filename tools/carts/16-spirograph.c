#include "studio.h"

// spirograph — a loop with a fixed turn angle makes a pattern.
// try changing ANGLE to see different shapes:
//   91  = expanding square spiral
//   121 = rose  (triangle-based, default)
//   144 = five-pointed star
//  97.2 = nine-pointed rose

#define ANGLE 121.0f

static int n = 0;

void update() {
    n += 2;
    if (n > 360) n = 0;
}

void draw() {
    cls(CLR_BLACK);

    turtle_home();
    pen_down();

    for (int i = 0; i < n; i++) {
        pen_color(1 + (i * 3) % 15);
        turtle_move(70.0f);
        turtle_turn(ANGLE);
    }

    print("change ANGLE to explore", 4, SCREEN_H - 12, CLR_DARK_GREY);
}
