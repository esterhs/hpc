BINS = newton
CFLAGS = -O2 -g

.PHONY : all
all : $(BINS) 

newton : newton.c
	gcc $(CFLAGS) -o $@ $< -lpthread -lm

.PHONY : clean
clean :
	rm -rf $(BINS)
