TARGET		:= rpga

CC		:= clang
LD		:= clang
CFLAGS		:= -O2
LDFLAGS		:= -O2

.PHONY: clean

$(TARGET): main.o util.o rgssa1.o rgssa3.o wolf.o
	$(LD) -o $(TARGET) $^ $(LDFLAGS)

main.o: main.c util.h rgssa1.h rgssa3.h wolf.h
	$(CC) -c $< -o $@ $(CFLAGS)

util.o: util.c util.h
	$(CC) -c $< -o $@ $(CFLAGS)

rgssa1.o: rgssa1.c rgssa1.h util.h
	$(CC) -c $< -o $@ $(CFLAGS)

rgssa3.o: rgssa3.c rgssa3.h util.h
	$(CC) -c $< -o $@ $(CFLAGS)

wolf.o: wolf.c wolf.h util.h
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(TARGET) *.o
