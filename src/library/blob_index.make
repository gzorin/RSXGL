all: blob_index

run: blob_index
	./blob_index

CPPFLAGS = -I$(HOME)/PSL1GHT-upstream/tools/cgcomp/include -I/opt/salon/include -I$(HOME)/boost-vault/Integer/endian-0.8
CXXFLAGS = -g
CGCOMP = $(PS3DEV)/bin/cgcomp
LDFLAGS = -L/opt/salon/lib -lboost_system -lboost_filesystem

define bin2o
       $(PS3DEV)/bin/bin2s -a 64 $< | $(AS) -o $(@)
       echo "extern const u8" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > `(echo $(<F) | tr . _)`.h
       echo "extern const u8" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> `(echo $(<F) | tr . _)`.h
       echo "extern const u32" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> `(echo $(<F) | tr . _)`.h
endef

blob_index: blob_index.o diffuse_specular_shader.vpo
	$(CXX) -o $@ $< $(LDFLAGS)

%.vpo: %.vcg
	@echo "[CGCOMP] $(notdir $<)"
	$(CGCOMP) -v $^ $@

%.fpo: %.fcg
	@echo "[CGCOMP] $(notdir $<)"
	$(CGCOMP) -f $^ $@

%.vpo.o: %.vpo
	@echo "$(notdir $<)"
	$(bin2o)

%.fpo.o: %.fpo
	@echo "$(notdir $<)"
	$(bin2o)
