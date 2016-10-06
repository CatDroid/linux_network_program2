CFLAGS = -Iadd -Isub -O2
OBJSDIR = objs
VPATH=add:sub:.
OBJS = add_int.o add_float.o sub_int.o sub_float.o main.o
TARGET = cacu
$(TARGET):$(OBJSDIR) $(OBJS)
	$(CC) -o $(TARGET) $(OBJSDIR)/*.o $(CFLAGS)
$(OBJS):%.o:%.c					
	$(CC) -c $(CFLAGS) $< -o $(OBJSDIR)/$@
$(OBJSDIR):
	mkdir -p ./$@
clean:
	-$(RM) $(TARGET)
	-$(RM) $(OBJSDIR)/*.o
