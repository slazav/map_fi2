MAPS = r34-101,102 r34-103,104 r34-105,106\
       r34-113,114 r34-115,116 r34-117,118\
       r35-139,140

org: $(patsubst %, %.vmap2.gz, ${MAPS})

all: $(patsubst %, %.vmap2, ${MAPS})


%.org.vmap2.gz:
	../progs/make_nom $*
