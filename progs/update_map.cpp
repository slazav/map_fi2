///\cond HIDDEN (do not show this in Doxyden)

#include "getopt/getopt.h"
#include "getopt/help_printer.h"
#include "geo_data/geo_utils.h"
#include "filename/filename.h"

#include "lib.h"
#include <regex>

using namespace std;

// Update local map from Label/Extra map. Do some object conversions.
// - Use tag ORG to mark original reference points, keep other destination objects untouched.
// - If reference point exists in both places - do nothing.
// - If reference point exists only in the source file - transfer point and labels
// - If reference point exists only in the destination - remove it

GetOptSet options;

void usage(bool pod=false){
  HelpPrinter pr(pod, options, "update_text");
  pr.name("find difference or update objects");
  pr.usage("[<options>] <in_file.vmap2> <out_file.vmap2>");
  pr.head(2, "Options:");
  pr.opts({"HELP","POD", "VMAP2", "A"});
  throw Err();
}

// custom filter for vmaps
void vmap_apply_patch(VMap2 & vmap, const std::string & patch_name){

  std::ifstream ff(patch_name);
  if (!ff) throw Err() << "can't open file: " << patch_name;
  std::cerr << "Reading patch file: " << patch_name << "\n";
  int line_num[2] = {0,0}; // line counter for read_words

  double find_dist = 100; //m

  while (1){
    auto vs = read_words(ff, line_num, false);
    if (vs.size()==0) break;
    try{

      // Include another file.
      if (vs[0] == "include"){
        if (vs.size()!=2) throw Err() << vs[0] << ": filename expected";

        // Add prefix if path is not absolute
       if (vs[1].size()>0 && vs[1][0]!='/')
         vs[1] = file_get_prefix(patch_name) + vs[1];

        vmap_apply_patch(vmap, vs[1]);
        continue;
      }

      // Stop reading file
      if (vs[0] == "exit"){
        if (vs.size()!=1) throw Err() << vs[0] << ": no arguments expected";
        break;
      }

      // Set find distance.
      if (vs[0] == "set_find_dist"){
        if (vs.size()!=2) throw Err() << vs[0] << ": argument expected";
        find_dist = str_to_type<double>(vs[1]);
        continue;
      }

      /***************************************************************/
      // Operations with single objects, selected with vmap.find_nearest. Should be quick

      // Delete single object of a given type and (optionally) given name, closiest to the given point.
      if (vs[0] == "delete"){
        if (vs.size()!=4) throw Err() << vs[0] << ": wrong number of arguments: delete <type> [<lon>,<lat>] <name>|*";
        uint32_t id;
        if (vs[3] == "*") id = vmap.find_nearest(vs[1], dPoint(vs[2]), find_dist);
        else  id = vmap.find_nearest(vs[1], vs[3], dPoint(vs[2]), find_dist);
        if (id!=-1) vmap.del(id);
        else std::cerr << "missing object: " << vs[0] << " " << vs[1] << " " << vs[2] << " " << vs[3] << "\n";
        continue;
      }

      // Move single object of a given type and (optionally) given name, closiest to the given point.
      if (vs[0] == "move"){
        if (vs.size()!=5) throw Err() << vs[0] << ": wrong number of arguments: move <type> [<lon>,<lat>] <name>|* [<new_lon>,<new_lat>]";
        uint32_t id;
        if (vs[3] == "*") id = vmap.find_nearest(vs[1], dPoint(vs[2]), find_dist);
        else  id = vmap.find_nearest(vs[1], vs[3], dPoint(vs[2]), find_dist);
        if (id!=-1){
          auto obj = vmap.get(id);
          obj.clear();
          obj.add_point(dPoint(vs[4]));
          vmap.put(id, obj);
        }
        else std::cerr << "missing object: "
          << vs[0] << " " << vs[1] << " " << vs[2] << " " << vs[3] << " " << vs[4] << "\n";
        continue;
      }

      // Rename single object of a given type and (optionally) given name, closiest to the given point.
      if (vs[0] == "rename"){
        if (vs.size()!=5) throw Err() << vs[0] << ": wrong number of arguments: rename <type> [<lon>,<lat>] <name>|* <new_name>";
        uint32_t id;
        if (vs[3] == "*") id = vmap.find_nearest(vs[1], dPoint(vs[2]), find_dist);
        else  id = vmap.find_nearest(vs[1], vs[3], dPoint(vs[2]), find_dist);
        if (id!=-1){
          auto obj = vmap.get(id);
          obj.name = vs[4];
          vmap.put(id, obj);
        }
        else std::cerr << "missing object: "
          << vs[0] << " " << vs[1] << " " << vs[2] << " " << vs[3] << " " << vs[4] << "\n";
        continue;
      }

      // Change type of a single object of a given type and (optionally) given name, closiest to the given point.
      if (vs[0] == "chtype"){
        if (vs.size()!=5) throw Err() << vs[0] << ": wrong number of arguments: chtype <type> [<lon>,<lat>] <name>|* <new_type>";
        uint32_t id;
        if (vs[3] == "*") id = vmap.find_nearest(vs[1], dPoint(vs[2]), find_dist);
        else  id = vmap.find_nearest(vs[1], vs[3], dPoint(vs[2]), find_dist);
        if (id!=-1){
          auto obj = vmap.get(id);
          obj.set_type(vs[4]);
          vmap.put(id, obj);
        }
        else std::cerr << "missing object: "
          << vs[0] << " " << vs[1] << " " << vs[2] << " " << vs[3] << " " << vs[4] << "\n";
        continue;
      }

      /***************************************************************/
      // Operations with multiple objects, require iterations through the whole map

      // Delete multiple objects selected by (optional) type and (optional) name
      if (vs[0] == "delete_all"){
        if (vs.size()!=3) throw Err() << vs[0] << ": wrong number of arguments: delete_all <type>|* <name>|*";

        vmap.iter_start();
        while (!vmap.iter_end()){
          auto p = vmap.iter_get_next();
          auto id = p.first;
          auto & obj = p.second;
          if (vs[1] != "*" && !obj.is_type(vs[1])) continue; // select type if needed
          if (vs[2] != "*" && obj.name != vs[2]) continue; // select name if needed
          vmap.del(id);
        }
        continue;
      }

      // Delete multiple objects selected by (optional) type and matching name
      if (vs[0] == "delete_re_all"){
        if (vs.size()!=3) throw Err() << vs[0] << ": wrong number of arguments: delete_re_all <type>|* <pattern>";

        vmap.iter_start();
        while (!vmap.iter_end()){
          auto p = vmap.iter_get_next();
          auto id = p.first;
          auto & obj = p.second;
          if (vs[1] != "*" && !obj.is_type(vs[1])) continue; // select type if needed
          if (std::regex_search(obj.name, std::regex(vs[2]))) vmap.del(id);
        }
        continue;
      }

      // Rename multiple objects selected by (optional) type and (optional) name
      if (vs[0] == "rename_all"){
        if (vs.size()!=4) throw Err() << vs[0] << ": wrong number of arguments: rename_all <type>|* <name>|* <new_name>";

        vmap.iter_start();
        while (!vmap.iter_end()){
          auto p = vmap.iter_get_next();
          auto id = p.first;
          auto & obj = p.second;
          if (vs[1] != "*" && !obj.is_type(vs[1])) continue; // select type if needed
          if (vs[2] != "*" && obj.name != vs[2]) continue; // select name if needed
          obj.name = vs[3];
          vmap.put(id, obj);
        }
        continue;
      }

      // Rename multiple objects selected by (optional) type and (optional) name, using regular expressions
      if (vs[0] == "rename_re_all"){
        if (vs.size()!=4) throw Err() << vs[0] << ": wrong number of arguments: rename_re_all <type>|* <pattern> <replacement>";

        vmap.iter_start();
        while (!vmap.iter_end()){
          auto p = vmap.iter_get_next();
          auto id = p.first;
          auto & obj = p.second;
          if (vs[1] != "*" && !obj.is_type(vs[1])) continue; // select type if needed
          auto n = std::regex_replace(obj.name, std::regex(vs[2]), vs[3]);
          if (n!=obj.name){
            obj.name = n;
            vmap.put(id, obj);
          }
        }
        continue;
      }

      /***************************************************************/

      // Add object
      if (vs[0] == "add"){
        if (vs.size()!=4) throw Err() << vs[0] << ": wrong number of arguments: add <type> <coords> <name>";
        uint32_t id;
        VMap2obj obj(vs[1]);
        obj.set_coords(vs[2]);
        obj.name = vs[3];
        vmap.add(obj);
        continue;
      }



      /***************************************************************/

      throw Err() << "Unknown patch command: " << vs[0];
    }
    catch (Err & e) {
      throw Err() << patch_name << ":" << line_num[0] << ": " << e.str();
    }
  }
}

void
vmap_print_diff(VMap2 & vmap1, VMap2 & vmap2){
  // attach labels in the source file - for setting default label positions
  auto refs1 = vmap1.find_refs(100,200);

  double search_dist = 5000; // where we are searching for modified objects [m]
  double match_dist = 10; // which point shift is treated as no change [m].

  std::cout << "DIFF:\n";
  std::set<uint32_t> processed1, processed2;
  std::cout << std::fixed << std::setprecision(5);


  // Pass1: go through vmap1, find objects in vmap2 with same name at close distance
  vmap1.iter_start();
  while (!vmap1.iter_end()){
    auto it1 = vmap1.iter_get_next();
    auto i1 = it1.first;
    auto o1 = it1.second;
    auto pt1 = o1.get_first_pt();
    // work only with point objects
    if (processed1.count(i1)) continue;
    if (!o1.is_class(VMAP2_POINT)) continue;

    // find nearest object with same type, same number of points
    dRect rng = o1.bbox();
    // here we can use approximate degree range
    rng.expand(match_dist * 180/M_PI/6380000);
    uint32_t minid = -1;
    double mindist = +INFINITY;
    for (const auto & i2: vmap2.find(o1.type, rng)){
      auto o2 = vmap2.get(i2);
      if (o2.opts.get("Source")=="")
        continue; // temporary, select objects with any non-empty Source setting
      if (processed2.count(i2)) continue;

      double d = geo_maxdist_2d(o1,o2);
      // (d is +inf for different npts, nsegments)
      if (d<mindist && o1.name==o2.name){ mindist = d; minid = i2;}
    }

    // closiest object with same name within match_dist
    if (mindist < match_dist){
      vmap2.put(minid, o1); // update object
      processed1.insert(i1);
      processed2.insert(minid);
      continue;
    }
  }


  // Pass2: go through vmap1, find objects in vmap2 with same name, moved
  vmap1.iter_start();
  while (!vmap1.iter_end()){
    auto it1 = vmap1.iter_get_next();
    auto i1 = it1.first;
    auto o1 = it1.second;
    auto pt1 = o1.get_first_pt();
    // work only with point objects
    if (processed1.count(i1)) continue;
    if (!o1.is_class(VMAP2_POINT)) continue;

    // find nearest object with same type, same number of points
    dRect rng = o1.bbox();
    // here we can use approximate degree range
    rng.expand(search_dist * 180/M_PI/6380000);
    uint32_t minid = -1;
    double mindist = +INFINITY;
    for (const auto & i2: vmap2.find(o1.type, rng)){
      auto o2 = vmap2.get(i2);
      if (o2.opts.get("Source")=="")
        continue; // temporary, select objects with any non-empty Source setting
      if (processed2.count(i2)) continue;

      double d = geo_maxdist_2d(o1,o2);
      // (d is +inf for different npts, nsegments)
      if (d<mindist && o1.name==o2.name){ mindist = d; minid = i2;}
    }

    // closiest object with same name within search_dist
    if (mindist < search_dist){
      auto o2 = vmap2.get(minid);
      auto pt2 = o2.get_first_pt();
      //move
      if (mindist > match_dist) {
          std::cout << "move   " << o1.print_type()
             << " [" << pt1.x << "," << pt1.y << "]"
             << " \"" << o1.name <<  "\""
             << " [" << pt2.x << "," << pt2.y << "]\n";
      }
      vmap2.put(minid, o1); // update object
      processed1.insert(i1);
      processed2.insert(minid);
      continue;
    }
  }

  // Pass3: go through vmap1, find objects in vmap2 with changed name
  //        transter unattached objects
  vmap1.iter_start();
  while (!vmap1.iter_end()){
    auto it1 = vmap1.iter_get_next();
    auto i1 = it1.first;
    auto o1 = it1.second;


    // select only unprocessed point objects
    if (processed1.count(i1)) continue;
    if (!o1.is_class(VMAP2_POINT)) continue;

    auto pt1 = o1.get_first_pt();

    // find nearest object with same type, same number of points
    dRect rng = o1.bbox();
    // here we can use approximate degree range
    rng.expand(search_dist * 180/M_PI/6380000);
    uint32_t minid = -1;
    double mindist = +INFINITY;
    for (const auto & i2: vmap2.find(o1.type, rng)){
      auto o2 = vmap2.get(i2);
      if (o2.opts.get("Source")=="")
        continue; // temporary, select objects with any non-empty Source setting
      if (processed2.count(i2)) continue;

      double d = geo_maxdist_2d(o1,o2);
      // (d is +inf for different npts, nsegments)
      if (d<mindist){ mindist = d; minid = i2;}
    }

    // closiest object with different name within search_dist
    if (mindist < search_dist){
      auto o2 = vmap2.get(minid);
      auto pt2 = o2.get_first_pt();
      //move
      if (mindist > match_dist) {
        std::cout << "move   " << o1.print_type()
             << " [" << pt1.x << "," << pt1.y << "]"
             << " \"" << o1.name << "\""
             << " [" << pt2.x << "," << pt2.y << "]\n";
        pt1 = pt2; // to print correct rename diff
      }
      // change name
      if (o1.name != o2.name) {
        std::cout << "rename " << o1.print_type()
             << " [" << pt1.x << "," << pt1.y << "]"
             << " \"" << o1.name << "\" \"" << o2.name << "\"\n";
      }

      vmap2.put(minid, o1); // update object
      processed1.insert(i1);
      processed2.insert(minid);
      continue;
    }

    // no object found -- add new
    std::cout << "delete " << o1.print_type()
              << " [" << pt1.x << "," << pt1.y << "]"
              << " \"" << o1.name << "\"\n";

    processed1.insert(i1);
    processed2.insert(vmap2.add(o1));

    // transfer labels
    for (auto i = refs1.find(i1); i!=refs1.end() && i->first == i1; ++i){
      vmap2.add(vmap1.get(i->second));
    }
  }

  // Pass4: go through vmap2, delete unprocessed objects
  vmap2.iter_start();
  while (!vmap2.iter_end()){
    auto it2 = vmap2.iter_get_next();
    auto i2 = it2.first;
    auto o2 = it2.second;

    // select unprocessed point objects with non-empty Source option
    if (processed2.count(i2)) continue;
    if (!o2.is_class(VMAP2_POINT)) continue;
    if (o2.opts.get("Source")=="") continue;

    auto pt2 = o2.get_first_pt();
    std::cout << "add " << o2.print_type()
              << " [" << pt2.x << "," << pt2.y << "]"
              << " \"" << o2.name << "\"\n";

    vmap2.del(i2); // delete object
  }

}

/**********************************************************/

int
main(int argc, char *argv[]){
  try{
    ms2opt_add_std(options, {"HELP","POD"});
    ms2opt_add_vmap2t(options);
    options.add("apply", 0, 'a', "A", "Instead of printing diff, update output file");
    options.add("patch_file",   1, 'p', "A", "File with patch to apply to the input file");

    vector<string> files;
    Opt O = parse_options_all(&argc, &argv, options, {}, files);
    if (O.exists("help")) usage();
    if (O.exists("pod"))  usage(true);

    if (files.size() != 2) usage();

    auto ifile = files[0];
    auto ofile = files[1];
    auto patch_file = O.get("patch_file", "");

    bool apply = O.exists("apply");


    // read file with type information if it's available
    VMap2types types(O);

    // import both files
    VMap2 vmap1, vmap2;
    if (!file_exists(ifile)) throw Err() << "Can't find file: " << ifile;
    vmap2_import(ifile, types, vmap1, Opt());
    std::cerr << "Reading file: " << ifile << ": " << vmap1.size() << " objects\n";

    if (file_exists(ofile)){
      vmap2_import(ofile, types, vmap2, Opt());
      std::cerr << "Reading file: " << ofile << ": " << vmap2.size() << " objects\n";
    }

    // filter objects
    if (patch_file!="") vmap_apply_patch(vmap1, patch_file);
    std::cerr << "Filtering src file: " << vmap1.size() << " objects\n";

    if (apply){
      O.put("keep_labels", 100);
      O.put("update_labels", 1);
      std::cerr << "Saving file: " << ofile << "\n";
      do_update_labels(vmap2, types);
      vmap2_export(vmap1, types, ofile, O);
    }
    else {
      vmap_print_diff(vmap1, vmap2);
    }
  }

  catch(Err & e){
    if (e.str()!="") cerr << "Error: " << e.str() << "\n";
    return 1;
  }
}

///\endcond
