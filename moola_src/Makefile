CC := clang
CFLAGS := -Wall -Wextra -MD -MP -g -O3
LDFLAGS := -lm -lz

BINARY := ./moola

SRCS := moola.c configure.c reference.c trace_gleipnir.c trace_moola.c trace_pin.c utils.c
OBJS := $(SRCS:%.c=%.o)
DEPS := $(OBJS:%.o=%.d)

all: $(BINARY)

clean:
	$(RM) $(BINARY) $(OBJS) $(DEPS)

$(BINARY): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

-include $(DEPS)
