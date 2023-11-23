#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include <esp_ota_ops.h>

#define TAG "ota"

typedef enum {
  OTA_STATUS_INACTIVE,
  OTA_STATUS_CONNECTING,
  OTA_STATUS_DOWNLOADING,
  OTA_STATUS_FLASHING,
  OTA_STATUS_WAITING_FOR_REBOOT,
  OTA_STATUS_WAITING_FOR_COMMIT,
  OTA_STATUS_ABORTED,
} ota_status_t;

typedef struct {
  ota_status_t status;
  int image_len_read;
  int image_size;
  esp_app_desc_t *img_desc;
} ota_t;

static ota_t ota_status = {
  .status = OTA_STATUS_INACTIVE,
  .image_len_read = -1,
  .image_size = -1,
  .img_desc = NULL
};

void ota_fetch(const ota_t **state) {
  *state = &ota_status;
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
  if (event_base == ESP_HTTPS_OTA_EVENT) {
    switch (event_id) {
    case ESP_HTTPS_OTA_START:
      ota_status.status = OTA_STATUS_CONNECTING;
      break;
    case ESP_HTTPS_OTA_CONNECTED:
      ota_status.status = OTA_STATUS_DOWNLOADING;
    case ESP_HTTPS_OTA_GET_IMG_DESC:
      ESP_LOGI(TAG, "Reading Image Description");
      break;
    case ESP_HTTPS_OTA_WRITE_FLASH:
      ESP_LOGD(TAG, "Writing to flash: %d written", *(int *)event_data);
      break;
    case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
      ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
      break;
    case ESP_HTTPS_OTA_FINISH:
      ESP_LOGI(TAG, "OTA finish");
      ota_status.status = OTA_STATUS_WAITING_FOR_REBOOT;
      break;
    case ESP_HTTPS_OTA_ABORT:
      ESP_LOGI(TAG, "OTA abort");
      ota_status.status = OTA_STATUS_ABORTED;
      break;
    }

  }
}

static void ota_task(void* _url) {
  const char* url = _url;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
  esp_http_client_config_t config = {
    .url = url,
    .skip_cert_common_name_check = true
  };
  esp_https_ota_config_t ota_config = {
    .http_config = &config
  };
  esp_https_ota_handle_t https_ota_handle = NULL;
  ESP_ERROR_CHECK(esp_https_ota_begin(&ota_config, &https_ota_handle));
  if (https_ota_handle == NULL) {
    ESP_LOGE(TAG, "esp_https_ota_begin failed");
    return;
  }
  esp_app_desc_t app_desc;
  ESP_ERROR_CHECK(esp_https_ota_get_img_desc(https_ota_handle, &app_desc));
  // TODO: think about this pointer to stack
  ota_status.img_desc = &app_desc;
  ota_status.image_size = esp_https_ota_get_image_size(https_ota_handle);
  ota_status.image_len_read = esp_https_ota_get_image_len_read(https_ota_handle);
  esp_err_t err = ESP_OK;
  while (1) {
    err = esp_https_ota_perform(https_ota_handle);
    if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
      break;
    }
  }
  if (err != ESP_OK) {
    esp_https_ota_abort(https_ota_handle);
    ESP_ERROR_CHECK_WITHOUT_ABORT(err);
    return;
  }
  esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);
  ESP_ERROR_CHECK_WITHOUT_ABORT(ota_finish_err);
  vTaskDelete(NULL);
}

void ota_start(const char* url) {
  xTaskCreate(&ota_task, "ota_task", 8192, strdup(url), configMAX_PRIORITIES - 2, NULL);
}

void ota_init() {
  // todo enable non-volatile memory
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_img_state;
  if (esp_ota_get_state_partition(running, &ota_img_state) == ESP_OK) {
    if (ota_img_state == ESP_OTA_IMG_PENDING_VERIFY) {
      ota_status.status = OTA_STATUS_WAITING_FOR_COMMIT;
      ESP_LOGI(TAG, "OTA pending verification, please commit");
    }
  }
}
