# LIBNAM
NAME = rmqpush

# DEFINITIONS
CC = gcc
CFLAGS = -std=c99 -W -g -fpic -shared
INCLUDE = -Isrc/include -Isrc/proto
RUSTSTREAMLIB = rmqstream
LDLIBS = -lrabbitmq -lprotobuf-c -l$(RUSTSTREAMLIB)
LDFLAGS = -Llib -L$(RUSTSTREAMLIB)/target/debug
PROTODIR = src/proto/hc/opendss
SRCDIR = src
OBJDIR = obj
LIBDIR = lib
OBJS = $(addprefix $(OBJDIR)/, rmqpush.o utils.o EnergyMeter.pb-c.o Diagnostics.pb-c.o OpenDssReport.pb-c.o)

default: $(NAME) 

# TARGETS

$(RUSTSTREAMLIB)/target/debug/lib$(RUSTSTREAMLIB).so: $(RUSTSTREAMLIB)/src/lib.rs
	cargo build --manifest-path=$(RUSTSTREAMLIB)/Cargo.toml

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p obj
	$(CC) $(INCLUDE) $(CFLAGS) $< -c -o $@

$(OBJDIR)/%.o: $(PROTODIR)/%.c
	@mkdir -p obj
	$(CC) $(INCLUDE) $(CFLAGS) $< -c -o $@

$(NAME): $(OBJS) $(RUSTSTREAMLIB)/target/debug/lib$(RUSTSTREAMLIB).so
	@mkdir -p lib
	$(CC) $(INCLUDE) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -o $(LIBDIR)/lib$(NAME).so $? 

clean:
	rm -rf $(OBJDIR) 
	rm -rf $(LIBDIR)
	rm -rf $(RUSTSTREAMLIB)/target
