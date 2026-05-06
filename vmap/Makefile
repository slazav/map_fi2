VMAPS := $(wildcard *.vmap2)

all:

in:
	./xfig_put *.fig


%.vmap2: ../vmap_patches/%.patch ../vmap_labels/%.labels.vmap2
	./update_vmap $@

%.fig: %.vmap2
	./xfig_get $@

