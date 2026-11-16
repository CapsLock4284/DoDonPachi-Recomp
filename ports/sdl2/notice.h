/**
 *    Name:        notice.h
 *    Description: a line of text over the game for a moment ("STATE SAVED (SLOT 1)"), in the port's own font
 *    Version:     0.1.0 (draft)
 *    Created:     16-11-2026
 *
 *    license:BSD-3-Clause
 *    (DonPachi Recomp's menu writes its notices in that game's own font; DoDonPachi's port has no menu yet, so its
 *    notices use font5x7.h.)
 */
#ifndef DDONPACH_NOTICE_H
#define DDONPACH_NOTICE_H

#include <stdint.h>

/* 'text' shown for about two seconds of frames (a new one replaces it) */
void notice_show(const char *text);

/* The picture (240 x 320, as the cabinet shows it) with the notice over it while there is one, else the picture
   itself: once a shown frame (it counts the notice's time down) */
const uint32_t *notice_over(const uint32_t *picture);

#endif
