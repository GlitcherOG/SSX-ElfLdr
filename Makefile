EEPREFIX=mips64r5900el-ps2-elf
EECC=$(EEPREFIX)-gcc
EECXX=$(EEPREFIX)-g++
EESTRIP=$(EEPREFIX)-strip

# remove libgcc dependency
EENOLIBGCC = -fno-stack-protector -fno-ident -fno-unwind-tables -fno-asynchronous-unwind-tables
EECCFLAGS = -D_EE -O3 -G0 -Wall $(EENOLIBGCC) -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include
# C++20 (without RTTI/exceptions) or Riot
EECXXFLAGS = -std=c++20 -fno-rtti -fno-exceptions $(EECCFLAGS)
EELDFLAGS := -L$(PS2SDK)/ee/lib -Wl,-zmax-page-size=128

BIN = ssx_elfldr.elf
OBJS = main.o

# set to just `c` to use regular newlib.
# not that you'd really need to, but the option is there!
EELIBC = c_nano
EEPS2SDK_LIBS = -lcdvd  -lkernel -lps2sdkc

# This complicated fun makes it so that I only include the things I exactly need.
LIBS = -nodefaultlibs -Wl,--start-group -l$(EELIBC) $(EEPS2SDK_LIBS) -Wl,--end-group

all: $(BIN)
	$(EESTRIP) --strip-all $<

clean:
	rm -f $(BIN) $(OBJS)

# compile rules	
%.o: %.c
	$(EECC) $(EECFLAGS) -c $< -o $@

%.o: %.cpp
	$(EECXX) $(EECXXFLAGS) -c $< -o $@

# Link rule
$(BIN): $(OBJS)
	$(EECC) -T./linkfile -o $@ $< $(EELDFLAGS) $(LIBS)
