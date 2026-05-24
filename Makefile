CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -pedantic -D_POSIX_C_SOURCE=200809L
TARGET  = tarsau
SRC     = tarsau.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.sau
	rm -rf cikti deneme izin_cikti test_cikti test_files