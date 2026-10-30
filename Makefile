ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined)
endif

CFLAGS := -Wall -O2
LDFLAGS := -lkernel

all: bin/sflashdump.elf

bin/sflashdump.elf: sflashdump.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f bin/sflashdump.elf

.PHONY: all clean
