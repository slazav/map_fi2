#include <map>
#include <string>
#include <fstream>
#include "geom/multiline.h"
#include "geom/line_rectcrop.h"
#include "getopt/getopt.h"
#include "getopt/help_printer.h"
#include "geo_data/conv_geo.h"
#include "geo_data/geo_io.h"
#include "geo_nom/geo_nom_fi.h"
#include "vmap2/vmap2io.h"
#include "gis/gpkg.h"
#include "filename/filename.h"
#include "geom/poly_tools.h"
#include <regex>

#include "lib.h"

using namespace std;

// import Sweedish Topografi 50 Nedladdning vector data (gpkg files)

// text category translation
std::map<std::string, std::string> tr_textcat = {
 {"Anläggningsområde/Byggnadsanläggning",  "construction_area"},
 {"Bebyggelse",                            "building"},
 {"Fjällupplysningstext",                  "mountain_info"},
 {"Hydrografi",                            "hydrography"},
 {"Höjdkurvstext",                         "contour"},
 {"Kulturhistorisk lämning",               "monument"},
 {"Kyrka",                                 "church"},
 {"Skyddad natur",                         "protected_nature"},
 {"Terrängnamn",                           "terrain"},
 {"Tätort",                                "urban_area"},
 {"Upplysningstext",                       "information"},
};

GetOptSet options;

void usage(bool pod=false){
  HelpPrinter pr(pod, options, "import_no");
  pr.name("Read gpkg data from https://geotorget.lantmateriet.se");
  pr.usage("[<options>] <in_file> <out_file> <brd_file>");
  pr.head(2, "Options:");
  pr.opts({"HELP","POD"});
  throw Err();
}

int
main(int argc, char *argv[]){
  try{
    ms2opt_add_std(options, {"HELP","POD"});

    vector<string> args;
    Opt O = parse_options_all(&argc, &argv, options, {}, args);
    if (O.exists("help")) usage();
    if (O.exists("pod"))  usage(true);

    if (args.size() != 1) usage();
    std::string dir  = args[0];
    std::string ofile  = dir + ".vmap2.gz";
    std::string bfile  = dir + ".gpx";

    VMap2 vmap;
    dMultiLine brd;
    dMultiLine brd1, brd2, brd3, brd4, brd5;
    std::set<int> known_types;

    // go through all gpkg files in data_dir
    for (const auto & f: file_glob({dir + "/*.gpkg"})){
      std::cerr << "reading " << f << "\n";
      GPKG db(f);

      for (const auto & tt: db.get_tables()){
        std::cout << ">>> " << tt.first << "\n";

        ConvGeo cnv(tt.second.srs); // se -> wgs
        db.read_start(tt.first);
        while (1){
          auto obj = db.read_next();
          if (obj.type==-1) break;

          /*****************************************/
          // print all object types
          if (1){
            int tnr = obj.opts.get<int>("objekttypnr");
            if (!known_types.count(tnr)) {
              std::cout << obj.print_type() << " "
                        << obj.opts.get("objekttypnr") << " "
                        << obj.opts.get("objekttyp") << "\n";
              known_types.insert(tnr);
            }
          }
          // print all text types
          if (0){
            if (obj.opts.exists("textkategori"))
            std::cout << obj.print_type() << " "
                      << obj.opts.get("textkategori") << "\n";
          }
          // print text objects with different karttext and textstrang
          if (0){
            if (obj.opts.exists("textstrang") &&
                obj.opts.get("textstrang") != obj.opts.get("karttext"))
              std::cout << obj.opts.get("textstrang") << " -> "
                        << obj.opts.get("karttext") << "\n";
          }
          // save border
          //  - 1561 -- national border (sea)
          //  - 1562 -- national border (land)
          //  - 1563 -- regional border (incomplete?!)
          if (obj.opts.get("objekttypnr") == "1561" || obj.opts.get("objekttypnr") == "1562" || obj.opts.get("objekttypnr") == "1563"){
             auto b = cnv.frw_acc(obj);
             brd.insert(brd.end(), b.begin(), b.end());
             continue;
          }

          /*****************************************/

          // remove defaults
          for (auto i=obj.opts.begin(); i!=obj.opts.end();){
            if (i->second == "") i=obj.opts.erase(i);
            else ++i;
          }

          // table name
          obj.opts.put("table", tt.first);

          // remove unneeded fields
          obj.opts.erase("objektidentitet");
          obj.opts.erase("vattendragsid");
          obj.opts.erase("skapad");
          obj.opts.erase("fid");

          // set type according to objekttypnr option
          if (obj.opts.exists("objekttypnr"))
            obj.set_type(obj.get_class(), obj.opts.get<int>("objekttypnr",1));

          // text types
          if (obj.opts.exists("karttext")){
            obj.set_type("text:1");

            if (obj.opts.get<double>("textriktning")!=0)
              obj.angle = obj.opts.get<double>("textriktning");

            // another way to set angle
            if (obj.size()==1 && obj.npts()>1){
            }
          }

          // translate text category
          if (obj.opts.exists("textkategori")){
            auto c=obj.opts.get("textkategori");
            if (tr_textcat.count(c)!=0)
              obj.opts.put("textkategori", tr_textcat[c]);
          }

          // filter points
          if (obj.get_class() == VMAP2_LINE ||
              obj.get_class() == VMAP2_POLYGON){
            line_filter_rdp(obj, 10); // m
            if (obj.npts()==0) continue;
          }
          obj.set_coords(cnv.frw_acc(obj)); // -> WGS84
          vmap.add(obj);
        }
      }
    }
    if (vmap.size()) vmap2_export(vmap, VMap2types(), ofile, Opt());

    {
      join_multiline(brd, 1e-4, true);
      GeoData D;
      D.push_back(GeoTrk(brd));
      write_gpx(bfile, D);
    }

  }
  catch(Err & e){
    if (e.str()!="") cerr << "Error: " << e.str() << "\n";
    return 1;
  }
}
