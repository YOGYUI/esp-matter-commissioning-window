#pragma once
#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include <esp_matter.h>
#include <esp_matter_core.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>
#include <iot_button.h>
#include <vector>
#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

class CSystem
{
public:
    CSystem();
    virtual ~CSystem();
    static CSystem* Instance();

public:
    bool initialize();
    void release();

    esp_matter::node_t* get_root_node() { return m_root_node; }

private:
    static CSystem* _instance;
    esp_matter::node_t* m_root_node;

    button_handle_t m_handle_default_btn;
    static bool m_commisioning_session_working;
    
    bool init_default_button();
    bool deinit_default_button();
    static void callback_default_button(void *arg, void *data);
    void print_system_info();

    static void matter_event_callback(const ChipDeviceEvent *event, intptr_t arg);
    static esp_err_t matter_identification_callback(
        esp_matter::identification::callback_type_t type, 
        uint16_t endpoint_id, 
        uint8_t effect_id, 
        uint8_t effect_variant, 
        void *priv_data
    );
    static esp_err_t matter_attribute_update_callback(
        esp_matter::attribute::callback_type_t type, 
        uint16_t endpoint_id, 
        uint32_t cluster_id, 
        uint32_t attribute_id, 
        esp_matter_attr_val_t *val, 
        void *priv_data
    );

    bool matter_open_commissioning_window(uint16_t timeout = 300);
    bool matter_close_commissioning_window();
    bool matter_is_commissioning_window_opened();
};

inline CSystem* GetSystem() {
    return CSystem::Instance();
}

#ifdef __cplusplus
};
#endif
#endif