#include "system.h"
#include "logger.h"
#include "definition.h"
#include <esp_netif.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <esp_app_desc.h>
#include <esp_flash.h>
#include <nvs_flash.h>
#include <app/server/Server.h>
#include <esp_matter_providers.h>

CSystem* CSystem::_instance = nullptr;
bool CSystem::m_commisioning_session_working = false;

CSystem::CSystem() 
{
    m_root_node = nullptr;
    m_handle_default_btn = nullptr;
}

CSystem::~CSystem()
{
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

CSystem* CSystem::Instance()
{
    if (!_instance) {
        _instance = new CSystem();
    }

    return _instance;
}

bool CSystem::initialize()
{
    esp_err_t ret;
    GetLogger(eLogType::Info)->Log("Start Initializing System");

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to initialize nsv flash (%d)", ret);
        return false;
    }

    if (!init_default_button()) {
        GetLogger(eLogType::Warning)->Log("Failed to init default on-board button");
    }

    // create matter root node
    esp_matter::node::config_t node_config;
    snprintf(node_config.root_node.basic_information.node_label, sizeof(node_config.root_node.basic_information.node_label), PRODUCT_NAME);
    m_root_node = esp_matter::node::create(&node_config, matter_attribute_update_callback, matter_identification_callback);
    if (!m_root_node) {
        GetLogger(eLogType::Error)->Log("Failed to create root node");
        return false;
    }
    GetLogger(eLogType::Info)->Log("Root node (endpoint 0) added");

    // start matter
    ret = esp_matter::start(matter_event_callback);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to start matter (ret: %d)", ret);
        return false;
    }
    GetLogger(eLogType::Info)->Log("Matter started");

    GetLogger(eLogType::Info)->Log("System Initialized");
    print_system_info();

    return true;
}

void CSystem::release()
{
    deinit_default_button();
}

void CSystem::callback_default_button(void *arg, void *data)
{
    button_event_t event = iot_button_get_event(arg);
    // GetLogger(eLogType::Info)->Log("button callback event: %d", event);
    switch (event) {
    case BUTTON_SINGLE_CLICK:
        _instance->matter_open_commissioning_window(300);
        break;
    case BUTTON_DOUBLE_CLICK:
        _instance->matter_close_commissioning_window();
        break;
    default:
        break;
    }
}

bool CSystem::init_default_button()
{
    button_config_t cfg = button_config_t();
    cfg.type = BUTTON_TYPE_GPIO;
    cfg.long_press_time = 5000;
    cfg.short_press_time = 180;
    cfg.gpio_button_config.gpio_num = GPIO_PIN_DEFAULT_BTN;
    cfg.gpio_button_config.active_level = 0; // active low (zero level when pressed)

    m_handle_default_btn = iot_button_create(&cfg);
    if (!m_handle_default_btn) {
        GetLogger(eLogType::Error)->Log("Failed to create iot button");
        return false;
    }

    iot_button_register_cb(m_handle_default_btn, BUTTON_SINGLE_CLICK, callback_default_button, nullptr);
    iot_button_register_cb(m_handle_default_btn, BUTTON_DOUBLE_CLICK, callback_default_button, nullptr);
    return true;
}

bool CSystem::deinit_default_button()
{
    if (m_handle_default_btn) {
        iot_button_delete(m_handle_default_btn);
        return true;
    }

    return false;
}

void CSystem::print_system_info()
{
    GetLogger(eLogType::Info)->Log("System Info");

    // ESP32 specific
    GetLoggerM(eLogType::Info)->Log("----- ESP32 -----");
    auto desc = esp_app_get_description();
    GetLoggerM(eLogType::Info)->Log("Project Name: %s", desc->project_name);
    GetLoggerM(eLogType::Info)->Log("App Version: %s", desc->version);

    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    GetLoggerM(eLogType::Info)->Log("CPU Core(s): %d", chip_info.cores);
    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    GetLoggerM(eLogType::Info)->Log("Revision: %d.%d", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        float flash_size_mb = (float)flash_size / (1024.f * 1024.f);
        GetLoggerM(eLogType::Info)->Log("Flash Size: %g MB (%s)", flash_size_mb, (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    }
    size_t heap_free_size = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    GetLoggerM(eLogType::Info)->Log("Heap Free Size: %d", heap_free_size);

    // network interface
    GetLoggerM(eLogType::Info)->Log("----- Network -----");
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");   // "WIFI_AP_DEF"
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    GetLoggerM(eLogType::Info)->Log("IPv4 Address: %d.%d.%d.%d", IP2STR(&ip_info.ip));
    GetLoggerM(eLogType::Info)->Log("Gateway: %d.%d.%d.%d", IP2STR(&ip_info.gw));
    unsigned char mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    GetLoggerM(eLogType::Info)->Log("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void CSystem::matter_event_callback(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        if (event->InterfaceIpAddressChanged.Type == chip::DeviceLayer::InterfaceIpChangeType::kIpV6_Assigned) {
            GetLogger(eLogType::Info)->Log("IP Address(v6) assigned");
        } else if (event->InterfaceIpAddressChanged.Type == chip::DeviceLayer::InterfaceIpChangeType::kIpV4_Assigned) {
            // IPv4 주소를 할당받으면 (commision된 AP에서 주소 할당) 웹서버를 (재)시작해준다 
            GetLogger(eLogType::Info)->Log("IP Address(v4) assigned");
        }
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        GetLogger(eLogType::Info)->Log("Commissioning complete");
        m_commisioning_session_working = false;
        break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        GetLogger(eLogType::Error)->Log("Commissioning failed, fail safe timer expired");
        m_commisioning_session_working = false;
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        GetLogger(eLogType::Info)->Log("Commissioning session started");
        m_commisioning_session_working = true;
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        GetLogger(eLogType::Info)->Log("Commissioning session stopped");
        m_commisioning_session_working = false;
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        GetLogger(eLogType::Info)->Log("Commissioning window opened");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        GetLogger(eLogType::Info)->Log("Commissioning window closed");
        break;
    default:
        break;
    }
}

esp_err_t CSystem::matter_identification_callback(esp_matter::identification::callback_type_t type,  uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant, void *priv_data)
{
    return ESP_OK;
}

esp_err_t CSystem::matter_attribute_update_callback(esp_matter::attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    return ESP_OK;
}

static void MatterCommWndWorkHandler(intptr_t context)
{
    chip::CommissioningWindowManager & commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
    if (commissionMgr.IsCommissioningWindowOpen()) {
        commissionMgr.CloseCommissioningWindow();
    }

    if (context > 0) {
        chip::app::FailSafeContext & failSafeContext = chip::Server::GetInstance().GetFailSafeContext();
        auto commissioningTimeout = chip::System::Clock::Seconds16(context);

        GetLogger(eLogType::Info)->Log("Try to open basic commissioning window (timeout: %u)", commissioningTimeout);
        if (commissionMgr.IsCommissioningWindowOpen()) {
            GetLogger(eLogType::Error)->Log("Commissioning window is already opened (busy)");
            return;
        }
        if (!failSafeContext.IsFailSafeFullyDisarmed()) {
            GetLogger(eLogType::Error)->Log("Fail safe is not full disarmed (busy)");
            return;
        }
        if (commissioningTimeout > commissionMgr.MaxCommissioningTimeout()) {
            GetLogger(eLogType::Error)->Log("Timeout max limit exceeded");
            return;
        }
        if (commissioningTimeout < commissionMgr.MinCommissioningTimeout()) {
            GetLogger(eLogType::Error)->Log("Timeout min limit exceeded");
            return;
        }

        chip::ChipError ret = commissionMgr.OpenBasicCommissioningWindow(commissioningTimeout, chip::CommissioningWindowAdvertisement::kDnssdOnly);
        if (ret != CHIP_NO_ERROR) {
            GetLogger(eLogType::Error)->Log("Failed to open basic commissioning window (ret: %d)", ret.Format());
        }
    }
}

bool CSystem::matter_open_commissioning_window(uint16_t timeout/*=300*/)
{
    bool result = true;

    chip::ChipError ret = chip::DeviceLayer::PlatformMgr().ScheduleWork(MatterCommWndWorkHandler, (intptr_t)timeout);
    if (ret != CHIP_NO_ERROR) {
        GetLogger(eLogType::Error)->Log("Failed to schedule work (ret: %d)", ret.Format());
        result = false;
    }

    return result;
}

bool CSystem::matter_close_commissioning_window()
{
    return matter_open_commissioning_window(0);
}

bool CSystem::matter_is_commissioning_window_opened()
{
    chip::CommissioningWindowManager & commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
    return commissionMgr.IsCommissioningWindowOpen();
}