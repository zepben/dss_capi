# LIBNAM
NAME = rmqpush

# DEFINITIONS
CC = gcc
CFLAGS = -std=c99 -W -g -fpic -shared
INCLUDE = -Isrc/include
LDLIBS = -lrabbitmq -lprotobuf-c
LDFLAGS = -Llib
OBJS = rmqpush.o utils.o EnergyMeter.pb-c.o

default: $(NAME) 

# TARGETS

%.o: src/%.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -c

$(NAME): $(OBJS)  
	$(CC) $(INCLUDE) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -o lib/lib$(NAME).so $? 

main: src/main.c $(NAME)
	$(CC) -g $(INCLUDE) $(LDFLAGS) $(LDLIBS) -l$(NAME) -o test $<

listen: src/rmqlisten.c 
	$(CC) -g $(INCLUDE) $(LDFLAGS) $(LDLIBS) -l$(NAME) -o listen $<

clean:
	rm -rf $(OBJS) 
	rm -rf lib/lib$(NAME).so
