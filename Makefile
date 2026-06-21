
OBJ = obj
OBJ_EXT = .obj
SNIPPETS = ../snippets

CFLAGS = -nologo @$(SNIPPETS)/build-release-default.build -I$(SNIPPETS) -Fo$(OBJ)/ -D_CRT_SECURE_NO_WARNINGS -D_UNICODE -DUNICODE $(CFLAGS)
CPPFLAGS = $(CFLAGS) $(CPPFLAGS)
RCFLAGS = -nologo -I$(SNIPPETS) $(RCFLAGS)
LFLAGS = -nologo -opt:ref -subsystem:windows,5.1 $(LFLAGS)
LIBS = user32.lib usp10.lib gdi32.lib ole32.lib comctl32.lib $(LIBS)

all : $(OBJ) emojikbd.exe

clean :
	del $(OBJ)\*.obj
	del $(OBJ)\*.lib

$(OBJ) :
	if not exist $(OBJ)\NUL mkdir $(OBJ)

emojikbd.exe : \
	$(OBJ)/emojikbd.obj \
	$(OBJ)/emojidat.obj \
	$(OBJ)/emojikbd.res \
	$(OBJ)/snippets.lib \
	$(OBJ)/sqlite3.obj
	link $(LFLAGS) -out:$@ $** $(LIBS)

$(OBJ)/snippets.lib : \
	$(OBJ)/bvcstr.obj \
	$(OBJ)/bvfont.obj \
	$(OBJ)/bvwide.obj \
	$(OBJ)/mem.obj \
	$(OBJ)/mem.win32.heap.obj \
	$(OBJ)/w32ex.obj \
	$(OBJ)/w32gdi.obj \
	$(OBJ)/w32ui.obj
	lib $(AFLAGS) -out:$@ $**

*.c : Makefile

*.cpp : Makefile

{}.c{$(OBJ)}$(OBJ_EXT)::
	$(CC) $(CFLAGS) -c $<

{}.cpp{$(OBJ)}$(OBJ_EXT)::
	$(CPP) $(CPPFLAGS) -c $<

{3rd/sqlite3}.c{$(OBJ)}$(OBJ_EXT)::
	$(CC) $(CFLAGS) -c $<

{$(SNIPPETS)}.c{$(OBJ)}$(OBJ_EXT)::
	$(CC) $(CFLAGS) -c $<

{$(SNIPPETS)/_mem}.c{$(OBJ)}$(OBJ_EXT)::
	$(CC) $(CFLAGS) -c $<

{$(SNIPPETS)}.cpp{$(OBJ)}$(OBJ_EXT)::
	$(CPP) $(CPPFLAGS) -c $<

{$(SNIPPETS)/_bvio}.cpp{$(OBJ)}$(OBJ_EXT)::
	$(CPP) $(CPPFLAGS) -c $<

{}.rc{$(OBJ)}.res:
	$(RC) $(RCFLAGS) -r -fo$@ $<
