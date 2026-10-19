CC := gcc
CFLAGS := -Wall -std=c11 -ggdb

ifeq ($(OS),Windows_NT)
    LIBS := -lopengl32 -lgdi32
    TARGET := demo.exe
    RM_CMD := del /Q $(TARGET) *.o 2>NUL || echo Cleaned.
else
    LIBS := -lX11 -lXrandr -lGL -lm
    TARGET := demo
    RM_CMD := rm -f $(TARGET) *.o 2>/dev/null || echo Cleaned.
endif

SRCS := demo.c microui.c

all: $(TARGET)

$(TARGET): $(SRCS) RGFW.h rglLoad.h
	$(CC) $(CFLAGS) -I. $(SRCS) $(LIBS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM_CMD)

.PHONY: all run clean