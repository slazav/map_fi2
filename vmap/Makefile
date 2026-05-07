VMAPS := $(wildcard *.vmap2)

all: ${VMAPS}

clean:
	rm -f *.fig *.bak

# update everything from FIG
in:
	./xfig_put *.fig

# update single vmap
%.vmap2: ../vmap_patches/%.patch ../vmap_labels/%.labels.vmap2
	./update_vmap $@

# update single fig file
%.fig: %.vmap2
	./xfig_get $@

