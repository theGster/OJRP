# The following lines are required because standard make does not
# recognize the Objective-C .m suffix.

# Macros

CC = g++
CFLAGS =
LD = g++
LDFLAGS = -mwindows
PNG = -lpng
OPEGL = -lglut32 -lglu32 -lopengl32
LIBJPEG = -ljpeg
FREETYPE = -lfreetype -lfreetype.dll
OPENAL = -OpenAL32
LIBS = $(PNG) $(FREETYPE)
RM= rm -f
SRC=src/font_create.cpp src/JAf_main.cpp src/wu_io_func.cpp
OBJ=src/font_create.o src/JAf_main.o src/wu_io_func.o
PROG=bin/JAfont.exe

#Link Rule
$(PROG): $(OBJ)
	$(LD) $(LDLAGS) $(OBJ) -o $(PROG) $(LIBS)

# Implicit rules
%.o: %.c
	$(CC) $(CFLAGS) -c $<

run:
	$(PROG)

clean:
	$(RM) $(PROG) $(OBJ)
