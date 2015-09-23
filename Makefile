BUILD_DIR=build_dir
TARGET=$(BUILD_DIR)/ubus2d
SOURCE=\
	src/ubusd_id.c \
	src/ubusd_obj.c \
	src/ubusd_proto.c \
	src/ubusd_event.c \
	src/ubusd.c 

OBJECTS=$(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(SOURCE)))

CFLAGS+=$(EXTRA_CFLAGS) -Wall -Werror -std=gnu99
LDFLAGS+=$(EXTRA_LDFLAGS)

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR): 
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJECTS) 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lubus2 -lubox -lblobmsg_json -ldl

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean: 
	rm -rf build_dir
