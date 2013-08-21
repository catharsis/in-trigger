OUT=in-trigger
OBJS=trigger.o
all: $(OUT)

in-trigger: $(OBJS)
	$(CC) $(CFLAGS) $^ -o  $@

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
