BUILD_DIR=build_dir
UBUSD=$(BUILD_DIR)/ubus2d
UBUS=$(BUILD_DIR)/ubus2
SOURCE=\
	src/ubusd_id.c \
	src/ubusd_obj.c \
	src/ubusd_proto.c \
	src/ubusd_event.c \
	src/ubusd_client.c \
	src/ubusd_socket.c \
	src/ubusd_msg.c \
	src/ubusd.c 

OBJECTS=$(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(SOURCE)))

CFLAGS+=$(EXTRA_CFLAGS) -Wall -Werror -std=gnu99 
LDFLAGS+=$(EXTRA_LDFLAGS)

all: $(BUILD_DIR) $(UBUSD) $(UBUS)

$(BUILD_DIR): 
	mkdir -p $(BUILD_DIR)

$(UBUSD): $(OBJECTS) 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^  -lblobpack -ljson-c  -lubus2 -lusys -lutype  -ldl

$(UBUS): $(BUILD_DIR)/src/ubus.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lblobpack -ljson-c -lubus2 -lusys -lutype -ldl

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean: 
	rm -rf build_dir
