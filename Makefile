CC      := gcc
CFLAGS  := -g -MMD -I./include -Wall -Ofast
LDFLAGS := -g -lallegro -lallegro_main -lallegro_primitives

CFILES  := $(shell find src -name "*.c")
OBJS    := $(CFILES:src/%.c=build/%.o)

build/%.o: src/%.c
	@echo + CC $< "->" $@
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $<
 
litenes: $(OBJS)
	@echo + LD "->" $@
	@$(CC) $(OBJS) $(LDFLAGS) -o litenes

-include $(patsubst %.o, %.d, $(OBJS))

.PHONY: clean

clean:
	rm -rf litenes build/
