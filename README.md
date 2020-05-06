# entdump
Entity and Texture dump program for Quake 2 BSPs
// From http://old.r1ch.net/stuff/r1q2/ - thanks R1ch.
//GPL etc.
//
// entdump is used for extracting entities from quake2 bsp files in text
// format for usage with the added ent file support in Xatrix+
//
// Build like: gcc -o entdump entdump.c
// Use like:  # ./entdump map.bsp > map.ent
// Nick I fixed so that the ent.file didn't have a double newline at the end.

// January 11, 2010, QwazyWabbit added texture file name output, usage info
// July 18, 2019, QwazyWabbit add missing texture flagging.
// Process wild-card filenames.
//

/* May 6th 2020
*  MrG{DRGN} added 64 bit version options
*/
