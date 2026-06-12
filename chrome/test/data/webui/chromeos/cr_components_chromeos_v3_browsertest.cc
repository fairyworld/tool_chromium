// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/webui_url_constants.h"
#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

#define REGISTER_TEST(test_fixture, host, test_path)  \
  class test_fixture : public WebUIMochaBrowserTest { \
   protected:                                         \
    test_fixture() { set_test_loader_host(host); }    \
  };                                                  \
  IN_PROC_BROWSER_TEST_F(test_fixture, All) {         \
    RunTest(test_path, "mocha.run()");                \
  }

// Bluetooth components
REGISTER_TEST(BluetoothBasePageTestV3,
              ash::kChromeUIBluetoothPairingHost,
              "chromeos/bluetooth/bluetooth_base_page_test.js")
REGISTER_TEST(BluetoothBluetoothIconTestV3,
              ash::kChromeUIBluetoothPairingHost,
              "chromeos/bluetooth/bluetooth_icon_test.js")
REGISTER_TEST(BluetoothBatteryIconPercentageTestV3,
              ash::kChromeUIBluetoothPairingHost,
              "chromeos/bluetooth/bluetooth_battery_icon_percentage_test.js")
REGISTER_TEST(BluetoothDeviceBatteryInfoTestV3,
              ash::kChromeUIBluetoothPairingHost,
              "chromeos/bluetooth/bluetooth_device_battery_info_test.js")
REGISTER_TEST(
    BluetoothDeviceSelectionPageTestV3,
    ash::kChromeUIBluetoothPairingHost,
    "chromeos/bluetooth/bluetooth_pairing_device_selection_page_test.js")
REGISTER_TEST(BluetoothPairingConfirmCodeTestV3,
              ash::kChromeUIBluetoothPairingHost,
              "chromeos/bluetooth/bluetooth_pairing_confirm_code_page_test.js")
REGISTER_TEST(BluetoothPairingDeviceItemTestV3,
              ash::kChromeUIBluetoothPairingHost,
              "chromeos/bluetooth/bluetooth_pairing_device_item_test.js")
REGISTER_TEST(BluetoothPairingRequestCodePageTestV3,
              ash::kChromeUIBluetoothPairingHost,
              "chromeos/bluetooth/bluetooth_pairing_request_code_page_test.js")
REGISTER_TEST(BluetoothPairingEnterCodePageTestV3,
              ash::kChromeUIBluetoothPairingHost,
              "chromeos/bluetooth/bluetooth_pairing_enter_code_page_test.js")
REGISTER_TEST(BluetoothPairingUiTestV3,
              ash::kChromeUIBluetoothPairingHost,
              "chromeos/bluetooth/bluetooth_pairing_ui_test.js")
REGISTER_TEST(BluetoothSpinnerPageTestV3,
              ash::kChromeUIBluetoothPairingHost,
              "chromeos/bluetooth/bluetooth_spinner_page_test.js")

// NetworkComponents
REGISTER_TEST(NetworkComponentsApnListTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/apn_list_test.js")
REGISTER_TEST(NetworkComponentsApnListItemTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/apn_list_item_test.js")
REGISTER_TEST(NetworkComponentsApnSelectionDialogTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/apn_selection_dialog_test.js")
REGISTER_TEST(NetworkComponentsApnSelectionDialogListItemTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/apn_selection_dialog_list_item_test.js")
REGISTER_TEST(NetworkComponentsCrPolicyNetworkBehaviorMojoTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/cr_policy_network_behavior_mojo_tests.js")
REGISTER_TEST(NetworkComponentsCrPolicyNetworkIndicatorMojoTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/cr_policy_network_indicator_mojo_tests.js")
REGISTER_TEST(NetworkComponentsNetworkApnlistTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_apnlist_test.js")
REGISTER_TEST(NetworkComponentsNetworkChooseMobileTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_choose_mobile_test.js")
REGISTER_TEST(NetworkComponentsNetworkConfigTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_config_test.js")
REGISTER_TEST(NetworkComponentsNetworkConfigElementBehaviorTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_config_element_behavior_test.js")
REGISTER_TEST(NetworkComponentsNetworkConfigInputTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_config_input_test.js")
REGISTER_TEST(NetworkComponentsNetworkConfigSelectTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_config_select_test.js")
REGISTER_TEST(NetworkComponentsNetworkConfigToggleTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_config_toggle_test.js")
REGISTER_TEST(NetworkComponentsNetworkConfigVpnTestTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_config_vpn_test.js")
REGISTER_TEST(NetworkComponentsNetworkConfigWifiTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_config_wifi_test.js")
REGISTER_TEST(NetworkComponentsNetworkIconTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_icon_test.js")
REGISTER_TEST(NetworkComponentsNetworkIpConfigTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_ip_config_test.js")
REGISTER_TEST(NetworkComponentsNetworkListTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_list_test.js")
REGISTER_TEST(NetworkComponentsNetworkListItemTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_list_item_test.js")
REGISTER_TEST(NetworkComponentsNetworkNameserversTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_nameservers_test.js")
REGISTER_TEST(NetworkComponentsNetworkPasswordInputTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_password_input_test.js")
REGISTER_TEST(NetworkComponentsNetworkPropertyListMojoTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_property_list_mojo_test.js")
REGISTER_TEST(NetworkComponentsNetworkProxyExclusionsTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_proxy_exclusions_test.js")
REGISTER_TEST(NetworkComponentsNetworkProxyInputTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_proxy_input_test.js")
REGISTER_TEST(NetworkComponentsNetworkProxyTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_proxy_test.js")
REGISTER_TEST(NetworkComponentsNetworkSelectTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_select_test.js")
REGISTER_TEST(NetworkComponentsNetworkSiminfoTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/network_siminfo_test.js")
REGISTER_TEST(NetworkComponentsSimLockDialogsTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/network/sim_lock_dialogs_test.js")

// NetworkHealth
REGISTER_TEST(NetworkHealthNetworkDiagnosticsTestV3,
              ash::kChromeUIConnectivityDiagnosticsHost,
              "chromeos/network_health/network_diagnostics_test.js")
REGISTER_TEST(NetworkHealthRoutineGroupTestV3,
              ash::kChromeUIConnectivityDiagnosticsHost,
              "chromeos/network_health/routine_group_test.js")

// TrafficCounters
REGISTER_TEST(TrafficCountersTrafficCountersTestV3,
              ash::kChromeUINetworkHost,
              "chromeos/traffic_counters/traffic_counters_test.js")

// MultiDeviceSetup
REGISTER_TEST(MultiDeviceSetupIntegrationTestV3,
              ash::kChromeUIMultiDeviceSetupHost,
              "chromeos/multidevice_setup/integration_test.js")
REGISTER_TEST(MultiDeviceSetupSetupSucceededPageTestV3,
              ash::kChromeUIMultiDeviceSetupHost,
              "chromeos/multidevice_setup/setup_succeeded_page_test.js")
REGISTER_TEST(MultiDeviceSetupStartSetupPageTestV3,
              ash::kChromeUIMultiDeviceSetupHost,
              "chromeos/multidevice_setup/start_setup_page_test.js")

// CellularSetup
REGISTER_TEST(CellularSetupActivationCodePageTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/cellular_setup/activation_code_page_test.js")
REGISTER_TEST(CellularSetupBasePageTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/cellular_setup/base_page_test.js")
REGISTER_TEST(CellularSetupButtonBarTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/cellular_setup/button_bar_test.js")
REGISTER_TEST(CellularSetupCellularSetupTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/cellular_setup/cellular_setup_test.js")
REGISTER_TEST(CellularSetupConfirmationCodePageTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/cellular_setup/confirmation_code_page_test.js")
REGISTER_TEST(CellularSetupProfileDiscoveryListPageTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/cellular_setup/profile_discovery_list_page_test.js")
REGISTER_TEST(CellularSetupEsimFlowUiTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/cellular_setup/esim_flow_ui_test.js")
REGISTER_TEST(CellularSetupFinalPageTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/cellular_setup/final_page_test.js")
REGISTER_TEST(CellularSetupProvisioningPageTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/cellular_setup/provisioning_page_test.js")
REGISTER_TEST(CellularSetupPsimFlowUiTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/cellular_setup/psim_flow_ui_test.js")
REGISTER_TEST(CellularSetupSetupLoadingPageTestV3,
              ash::kChromeUIOSSettingsHost,
              "chromeos/cellular_setup/setup_loading_page_test.js")
