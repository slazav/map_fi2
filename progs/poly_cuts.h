#ifndef POLY_CUTS_H
#define POLY_CUTS_H

#include <geom/multiline.h>
#include <geom/rect.h>

// Tools for handling polygon cuts.
// Cuts iMultiLine contains same or smaller number of segments when original line.
// Each segment contains (x,y) points where x - point number in the original segment, z - cut length

// create cuts array for a line cropped to a box
iMultiLine poly_cuts_make_box(const dMultiLine & crds, const dRect & box, const double acc);


// Split polygon segment using cut segment into 2*cseg.size() + 1 parts.
// Odd parts (cuts) have >= 2 points, even parts could be empty
dMultiLine poly_cuts_break_line(const dLine & pseg, const iLine & cseg);


// Merge parts produced by poly_cuts_break_line, update polygon segment and cut segment
void poly_cuts_merge_line(const dMultiLine & parts, dLine & pseg, iLine & cseg);

// convert polygon with cuts to lines
dMultiLine poly_cuts_get_lines(const dMultiLine & crds, const iMultiLine & cuts);

#endif
