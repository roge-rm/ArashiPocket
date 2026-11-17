/* Code segments on Palm. A segment holds at most 32 KB. A function goes in
   a segment when its definition and every declaration carry the segment's
   attribute (declare it in the header, and put the same macro on static
   helpers and their forward declarations). A call through a declaration
   without the attribute jumps to the wrong place; tools/segcheck.py finds
   those.
   Everything is empty outside the Palm build. */
#ifndef CORE_SEG_H
#define CORE_SEG_H

#ifdef PALM_SEGMENTS
#define SEG_MATH   __attribute__((section("math")))
#define SEG_SCRIPT __attribute__((section("interp")))
#define SEG_DEV    __attribute__((section("device")))
#define SEG_PAT    __attribute__((section("pat")))
#define SEG_VA     __attribute__((section("render")))
#define SEG_GAME   __attribute__((section("game")))
#define SEG_ENEMY  __attribute__((section("enemy")))
#else
#define SEG_MATH
#define SEG_SCRIPT
#define SEG_DEV
#define SEG_PAT
#define SEG_VA
#define SEG_GAME
#define SEG_ENEMY
#endif

#endif
