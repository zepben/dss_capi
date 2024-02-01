# LIBNAM
NAME = rmqpush

# DEFINITIONS
CC = gcc
CFLAGS = -std=c99 -W -g -fpic -shared
INCLUDE = -Isrc/include -Isrc/proto
RUSTSTREAMLIB = rmqstream
LDLIBS = -lrabbitmq -lprotobuf-c -l$(RUSTSTREAMLIB)
LDFLAGS = -L$(LIBDIR)
PROTODIR = src/proto/zepben/protobuf/hc/opendss
SRCDIR = src
OBJDIR = obj
LIBDIR = lib
OBJS = $(addprefix $(OBJDIR)/, rmqpush.o utils.o EnergyMeter.pb-c.o Diagnostics.pb-c.o OpenDssReport.pb-c.o)

default: $(NAME) 

# TARGETS

$(LIBDIR)/lib$(RUSTSTREAMLIB).a: $(RUSTSTREAMLIB)/Cargo.toml
	cargo build --release --manifest-path=$(RUSTSTREAMLIB)/Cargo.toml
	@mkdir -p $(LIBDIR)
	cp $(RUSTSTREAMLIB)/target/release/*.a $(LIBDIR)/

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(INCLUDE) $(CFLAGS) $< -c -o $@

$(OBJDIR)/%.o: $(PROTODIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(INCLUDE) $(CFLAGS) $< -c -o $@

$(NAME): $(OBJS) $(LIBDIR)/lib$(RUSTSTREAMLIB).a
	@mkdir -p $(LIBDIR)
	$(CC) $(INCLUDE) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -o $(LIBDIR)/lib$(NAME).so $? 

clean:
	rm -rf $(OBJDIR) 
	rm -rf $(LIBDIR)
