#include "getopt/getopt.h"
#include "getopt/help_printer.h"
#include "filename/filename.h"
#include "shape/shape_db.h"
#include "geo_data/conv_geo.h"
#include "tmpdir/tmpdir.h"
#include "geom/poly_tools.h"

#include "geo_nom/geo_nom_fi.h"

#include "lib.h"
#include "poly_cuts.h"

using namespace std;

// Read original zip archive <name>.zip with shape files from MML,
// convert to VMAP with all reasonable data.
// No configuration for specific types involved.
// I want this step to have intermediate "original" files for fast
// experiments with them.

GetOptSet options;

void usage(bool pod=false){
  HelpPrinter pr(pod, options, "import_fi1");
  pr.name("Read shape files from maanmittauslaitos.fi");
  pr.usage("[<options>] pref");
  pr.head(2, "Options:");
  pr.opts({"HELP","POD"});
  throw Err();
}

/************************************************/

// detect object class
std::string detect_class(const std::string & base){
  if (check_suff(base, "KarttanimiPiste")) return "text:";
  if (check_suff(base, "Piste")) return "point:";
  if (check_suff(base, "Viiva")) return "line:";
  if (check_suff(base, "Linja")) return "line:";
  if (check_suff(base, "Reuna")) return "line:";
  if (check_suff(base, "Raja")) return "line:";
  if (check_suff(base, "Alue")) return "area:";
  throw Err() << "unknown type: " << base;
}

// parse <x>;<y> string for [D1] text objects
dPoint
parse_crd(const std::string & s){
  size_t n = s.find(";");
  if (n==string::npos) return dPoint();
  auto a = s.substr(0,n);
  auto b = s.substr(n+1,s.size());
  if (a.size()==0 || b.size()==0) return dPoint();
  return dPoint(str_to_type<double>(a), str_to_type<double>(b));
}

/************************************************/

int
main(int argc, char *argv[]){
  try{
    ms2opt_add_std(options, {"HELP","POD","VERB"});

    vector<string> args;
    Opt O = parse_options_all(&argc, &argv, options, {}, args);
    if (O.exists("help")) usage();
    if (O.exists("pod"))  usage(true);

    if (args.size() != 1) usage();
    std::string pref = args[0];

    auto box = nom_to_range_fi(pref);

    /********************************************************/

    VMap2 vmap;
    ConvGeo cnv("ETRS-TM35FIN");

    // unzip data to a temporary dir
    TmpDir tmp_dir("shp2vmap_XXXXXX");
    tmp_dir.unzip(pref + ".zip");

    /********************************************************/

    // go through all files
    for (const auto & f: file_glob({tmp_dir.get_dir() + "/*.shp"})){
      std::cerr << "reading file: " << f << "\n";
      auto path = file_get_prefix(f);
      auto base = file_get_basename(f, ".shp");
      if (base.size()<1) throw Err() << "bad basename: " << f;

      // get object class (point, text, line, area) from database name
      std::string cl = detect_class(base);

      // open shape databases
      ShapeDB DB(path + base, 0);
      if (DB.shp_num() != DB.dbf_num())
        throw Err() << "different number of objects in SHP and DBF: " << base;

      // go through all objects
      for (size_t i = 0; i<DB.shp_num(); ++i){
        Opt opts = DB.get_opts(i);
        std::string type = cl + opts.get("Kohdeluokk", "1"); // 1 for text objects
        VMap2obj obj(type);

        auto crds = DB.get(i);
        if (crds.npts()==0) continue;

        //opts.put("file", base);

        // erase options with empty value
        for (auto i=opts.begin(); i!=opts.end();){
          if (i->second == "") i=opts.erase(i);
          else ++i;
        }

        // type number
        if (opts.exists("Kohdeluokk")){
          opts.put("type1", cl + opts.get("Kohdeluokk"));
          opts.put("type2", cl + opts.get("Kohderyhma") + "-" + opts.get("Kohdeluokk"));
          opts.erase("Kohdeluokk"); // type for F1
          opts.erase("Kohderyhma"); // type group for F1
        }

        // convert text position, text direction, reference position (F1)
        if (opts.exists("textdirect")){
          auto p1 = parse_crd(opts.get("textpositi"));
          auto p2 = parse_crd(opts.get("textdirect"));
          if (p1 != dPoint() && p2 != dPoint())
            obj.angle = - 180/M_PI * atan2(p2.y - p1.y, p2.x - p1.x);
          opts.erase("textdirect");
          opts.erase("textpositi"); // equal to coordinates
        }
        if (opts.exists("placelocat")){
          obj.ref_pt = parse_crd(opts.get("placelocat"));
          //cnv.frw(obj.ref_pt);
          opts.erase("placelocat");
        }
        if (opts.exists("textsize")){
          obj.scale = opts.get<int>("textsize", 200)/200.0;
          opts.erase("textsize");
        }

        // remove some font parameters
        opts.erase("textbendin");
        opts.erase("textboundi");
        opts.erase("textcase");
        opts.erase("textcolour");
        opts.erase("textfont");
        opts.erase("textsize");
        opts.erase("textslant");
        opts.erase("textspac_1");
        opts.erase("textspacin");

        // remove some default values
        opt_del_def(opts, "PintaAla", "-2.99990000000e+04");
        opt_del_def(opts, "Korkeus", "-2.99990000000e+04");
        opt_del_def(opts, "Kerrosluku", "-29999");
        opt_del_def(opts, "Tieluokka",  "-29999");
        opt_del_def(opts, "Sahkoistys", "-29999");
        opt_del_def(opts, "Tienumero",  "0");
        opt_del_def(opts, "Vertikaali", "0");
        opt_del_def(opts, "Paallyste",  "0");

        // buildings, towers, etc.: surface area, floors
        opt_mv_dbl(opts, "PintaAla",   "area"); // area
        opt_mv_dbl(opts, "Korkeus",    "height"); // height, also altitude of contours and lakes
        opt_mv_txt(opts, "Kerrosluku", "floors"); // number of floors

        // roads
        opt_mv_txt(opts, "Tienumero",  "road_num");
        opt_mv_txt(opts, "Vertikaali", "level");
        opt_mv_txt(opts, "Paallyste",  "sidewalk");
        opt_mv_txt(opts, "Tieluokka",  "road_class");
        opt_mv_txt(opts, "Sahkoistys", "electrification");

        //opt_mv_txt(opts, "Ajorataluk", "", 0);
        //opt_mv_txt(opts, "Valmiusast", "level", 0);

        if (opts.exists("Suunta")){ // F1, points with directions
          obj.angle = -opts.get<double>("Suunta");
          opts.erase("Suunta");
        }

        /*************************************************/

        obj.opts = opts;

        // find cuts of polygons (accuracy = 1m)
        // polygons are closed! it's important for conversions
        iMultiLine cuts; // (x,y) points where x - point number, z - cut length
        if (obj.get_class() == VMAP2_POLYGON) cuts = poly_cuts_make_box(crds, box, 1);

        // filter and convert points
        // process segments separately
        for (size_t sn = 0; sn < crds.size(); sn++){
          double acc = 10; // m

          // if cuts segment does not exist process the whole line
          if (sn>=cuts.size()){
            if (cl == "line:" || cl == "area:") line_filter_rdp(crds[sn], acc);
            crds[sn] = cnv.frw_acc(crds[sn]);
            continue;
          }

          // Split segment into 2*cuts[n] + 1 parts.
          auto parts = poly_cuts_break_line(crds[sn], cuts[sn]);

          // Process parts (filter + coordinate conversion).
          for (size_t n = 0; n < parts.size(); n++){
            line_filter_rdp(parts[n], acc);
            parts[n] = cnv.frw_acc(parts[n]);
            // special workaroud: if segment contains two same points,
            // frw_acc removes one of them (to be fixed?). We want to avoid 1-point lines
            if (parts[n].size()==1) parts[n].clear();
          }

          // Join parts and update cuts array for new point numbers
          poly_cuts_merge_line(parts, crds[sn], cuts[sn]);

        }

        // skip empty objects
        if (crds.npts()==0) continue;

        // convert ref point
        if (obj.ref_pt != dPoint()) cnv.frw(obj.ref_pt);

        // add object
        if (cuts.size()) obj.opts.put("cuts", cuts);
        obj.dMultiLine::operator=(crds);
        vmap.add(obj);
      }
    }
    if (vmap.size()) vmap2_export(vmap, VMap2types(), pref + ".fi1.vmap2.gz", Opt());

  }
  catch(Err & e){
    if (e.str()!="") cerr << "Error: " << e.str() << "\n";
    return 1;
  }
}
