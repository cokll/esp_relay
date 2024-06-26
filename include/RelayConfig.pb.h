/* Automatically generated nanopb header */
/* Generated by nanopb-0.3.9.4 at Sat Dec 23 20:38:40 2023. */

#ifndef PB_RELAYCONFIG_PB_H_INCLUDED
#define PB_RELAYCONFIG_PB_H_INCLUDED
#include <pb.h>

/* @@protoc_insertion_point(includes) */
#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Struct definitions */
typedef struct _RelayConfigMessage {
    uint8_t led_type;
    uint16_t led_start;
    uint16_t led_end;
    uint8_t power_on_state;
    uint16_t last_state;
    uint32_t study_index[4];
    uint32_t study[40];
    uint8_t led_light;
    uint8_t led_time;
    uint8_t brightness[8];
    uint16_t color_temp[8];
    uint8_t color_index[8];
    uint8_t switch_mode;
    uint8_t power_mode;
    uint8_t module_type;
    uint16_t report_interval;
    uint8_t led;
    uint8_t max_pwm;
    uint8_t led_backlight;
/* @@protoc_insertion_point(struct:RelayConfigMessage) */
} RelayConfigMessage;

/* Default values for struct fields */

/* Initializer values for message structs */
#define RelayConfigMessage_init_default          {0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, 0, 0, 0, 0, 0}
#define RelayConfigMessage_init_zero             {0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, 0, 0, 0, 0, 0}

/* Field tags (for use in manual encoding/decoding) */
#define RelayConfigMessage_led_type_tag          1
#define RelayConfigMessage_led_start_tag         2
#define RelayConfigMessage_led_end_tag           3
#define RelayConfigMessage_power_on_state_tag    4
#define RelayConfigMessage_last_state_tag        5
#define RelayConfigMessage_study_index_tag       6
#define RelayConfigMessage_study_tag             7
#define RelayConfigMessage_led_light_tag         8
#define RelayConfigMessage_led_time_tag          9
#define RelayConfigMessage_brightness_tag        15
#define RelayConfigMessage_color_temp_tag        16
#define RelayConfigMessage_color_index_tag       17
#define RelayConfigMessage_switch_mode_tag       18
#define RelayConfigMessage_power_mode_tag        19
#define RelayConfigMessage_module_type_tag       20
#define RelayConfigMessage_report_interval_tag   21
#define RelayConfigMessage_led_tag               22
#define RelayConfigMessage_max_pwm_tag           23
#define RelayConfigMessage_led_backlight_tag     24

/* Struct field encoding specification for nanopb */
extern const pb_field_t RelayConfigMessage_fields[20];

/* Maximum encoded size of messages (where known) */
#define RelayConfigMessage_size                  515

/* Message IDs (where set with "msgid" option) */
#ifdef PB_MSGID

#define RELAYCONFIG_MESSAGES \


#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
/* @@protoc_insertion_point(eof) */

#endif
