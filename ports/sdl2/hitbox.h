/**
 *    Name:        hitbox.h
 *    Description: the ships' hitboxes shown over the picture (a white glowing dot, the box the game itself tests the
 *                 enemies' bullets against), and the laser's aura (the box around the ship where the laser hits
 *                 enemies close up)
 *    Version:     0.1.0 (draft)
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    DonPachi Recomp's (ports/sdl2/hitbox.h), made again for DoDonPachi. The game's tests (func_0057EE, run for each
 *    ship from 005790): a bullet's point (the 44 bullets from $106CB6, $40 bytes each; 48 in the second loop) against
 *    the ship's box, from its record (player 1 at $102C8C, player 2 at $102CCC): position +06 (along the board's x: up
 *    the cabinet's screen) and +08 (the board's y: across it), in 64ths of a pixel; the box reaches +10 above +06 and
 *    +12 below it, +14 above +08 and +16 below it (narrower as the ship banks). A ship is in play while bit 15 of its
 *    word +00 is set.
 *    The aura: a hit record of its own for each ship (player 1's at $103BAE, player 2's at $103ECE: the same layout,
 *    in play (bit 15 of +00) while the laser is fired), at the ship's position: on the owner's PC's type A shot it
 *    reaches 56 pixels ahead of the ship, 16 behind and 16 to each side; its own sizes are read each frame.
 *    The sprites shown are two frames behind the game's memory (three with arcade timing: measured, the ship's sprite
 *    against its record), so what is drawn is the boxes of the frame the ship shown is from. A box is drawn only while
 *    the ship's own sprite (its code, the record's +04) is in the sprite bank shown, near it, during play.
 */
#ifndef DDONPACH_HITBOX_H
#define DDONPACH_HITBOX_H

#include <stdbool.h>
#include <stdint.h>

/* After each of the game's frames (its sprites latched): both ships' boxes and auras, kept for a few frames */
void hitbox_record(void);

/* The boxes of 'frames_ago' frames ago drawn on 'picture' (240 x 320, as the cabinet shows it): the hitboxes, the
   auras, as asked */
void hitbox_draw(uint32_t *picture, int frames_ago, bool hitboxes, bool auras);

#endif
