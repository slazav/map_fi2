#include "poly_cuts.h"

iMultiLine
poly_cuts_make_box(const dMultiLine & crds, const dRect & box, const double acc){

  iMultiLine cuts; // (x,y) points where x - point number, z - cut length

  for (const auto & seg: crds){
    iLine cut_seg;
    size_t N = seg.size();
    for (size_t n = 0; n+1 < N; n++){
      auto x1 = box.x, x2 = box.x + box.w;
      auto y1 = box.y, y2 = box.y + box.h;
      if ((fabs(seg[n].x - x1) < acc && fabs(seg[n+1].x - x1) < acc) ||
          (fabs(seg[n].x - x2) < acc && fabs(seg[n+1].x - x2) < acc) ||
          (fabs(seg[n].y - y1) < acc && fabs(seg[n+1].y - y1) < acc) ||
          (fabs(seg[n].y - y2) < acc && fabs(seg[n+1].y - y2) < acc))
        if (seg[n]!=seg[n+1]) cut_seg.emplace_back(n,1);
    }
    cuts.push_back(cut_seg);
  }

  // reduce length to last non-empty segment
  size_t i;
  for (i = cuts.size(); i>0 && cuts[i-1].size()==0; i--) {;}
  cuts.resize(i);
  return cuts;
}


dMultiLine
poly_cuts_break_line(const dLine & pseg, const iLine & cseg){
  dMultiLine parts;
  size_t N = pseg.size();
  if (N<2) return parts;
  size_t n0 = 0;
  for (auto & cut: cseg){
    if (cut.x < n0 || cut.y < 1 || cut.x + cut.y > N) throw Err() << "cuts: overflow";
    parts.emplace_back(pseg.begin() + n0, pseg.begin() + (cut.x>n0? cut.x+1: n0)); // 0,2,3,4... points
    n0 = cut.x + cut.y; // <= N
    parts.emplace_back(pseg.begin() + cut.x, pseg.begin() + (n0==N ? n0: n0+1)); // 2,3,4... points
  }
  parts.emplace_back(pseg.begin() + n0, pseg.end());
  return parts;
}

void
poly_cuts_merge_line(const dMultiLine & parts, dLine & pseg, iLine & cseg){
  // Join parts and update cuts array for new point numbers
  pseg.clear(); // clear current segment
  cseg.clear();
  for (size_t n = 0; n < parts.size(); n++){

    // update cuts
    if (n%2==1 && parts[n].size()>1) cseg.emplace_back(pseg.size(), parts[n].size()-1);

    // merge and remove last point
    pseg.insert(pseg.end(), parts[n].begin(), parts[n].end());
    if (parts[n].size()) pseg.resize(pseg.size()-1);
  }
}


dMultiLine
poly_cuts_get_lines(const dMultiLine & crds, const iMultiLine & cuts){
  dMultiLine ret;
  for (size_t sn = 0; sn < crds.size(); sn++){
    // no cuts for this segment
    if (sn>=cuts.size()){
      ret.push_back(crds[sn]);
      continue;
    }
    // Split segment into lines between cuts.
    size_t N = crds[sn].size();
    if (N<2) continue;
    size_t n0 = 0;
    for (auto & cut: cuts[sn]){
      if (cut.x < n0 || cut.y < 1 || cut.x + cut.y > N) throw Err() << "cuts: overflow: " << cut << " N=" << N;
      if (cut.x>n0) ret.emplace_back(crds[sn].begin() + n0, crds[sn].begin() + cut.x+1); // 2,3,4... points
      n0 = cut.x + cut.y; // <= N
    }
    if (n0<N) ret.emplace_back(crds[sn].begin() + n0, crds[sn].end());
  }
  return ret;
}
