#include <Arduino.h>

#include "BluetoothA2DPSink.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_bt.h"
#include "esp_wifi.h"

/* ============================================================
 * Bluetooth A2DP Sink
 * ============================================================ */
BluetoothA2DPSink a2dp_sink;

/* ============================================================
 * I2S pin map: 3-pin only
 * ============================================================ */
#define I2S_BCLK   21
#define I2S_LRCK   23
#define I2S_DOUT   22
#define I2S_PORT   I2S_NUM_0

/* ============================================================
 * Audio config
 * ============================================================ */
#define I2S_OUTPUT_SAMPLE_RATE  48000U

/*
 * A2DP callback에서 한 블록으로 복사할 최대 입력 frame 수.
 * 1 stereo frame = L int16 + R int16 = 4 bytes
 */
#define AUDIO_BLOCK_FRAMES      1024U
#define AUDIO_BLOCK_SAMPLES     (AUDIO_BLOCK_FRAMES * 2U)
#define AUDIO_BLOCK_BYTES       (AUDIO_BLOCK_SAMPLES * sizeof(int16_t))

/*
 * Pool 개수.
 * 너무 작으면 callback burst에서 drop이 늘고,
 * 너무 크면 RAM을 많이 먹는다.
 */
#define AUDIO_BLOCK_POOL_COUNT  8U

/*
 * I2S에 넣을 무음 블록.
 * ready queue가 비어 있을 때도 I2S clock/DMA 상태를 안정적으로 유지하기 위해 사용.
 */
#define SILENCE_FRAMES          256U
#define SILENCE_SAMPLES         (SILENCE_FRAMES * 2U)

/*
 * Audio task 설정.
 */
#define AUDIO_TASK_STACK_WORDS  8192
#define AUDIO_TASK_PRIORITY     3
#define AUDIO_TASK_CORE         1

/* ============================================================
 * Audio block pool
 * ============================================================ */
typedef struct
{
  uint16_t frames;
  uint32_t sample_rate;
  int16_t pcm[AUDIO_BLOCK_SAMPLES];
} AudioBlock_t;

static AudioBlock_t audioPool[AUDIO_BLOCK_POOL_COUNT];

static QueueHandle_t freeQueue = NULL;
static QueueHandle_t readyQueue = NULL;
static TaskHandle_t audioTaskHandle = NULL;

/* ============================================================
 * I2S / runtime state
 * ============================================================ */
static volatile uint32_t bt_sample_rate = 44100;

static bool i2s_installed = false;

static int16_t silence_buf[SILENCE_SAMPLES];

/* ============================================================
 * Resampler state: audio task only
 * ============================================================ */
#define RESAMPLE_WORK_FRAMES  (AUDIO_BLOCK_FRAMES + 1U)
#define RESAMPLE_OUT_FRAMES   2048U

static int16_t work_buf[RESAMPLE_WORK_FRAMES * 2U];
static int16_t out_buf[RESAMPLE_OUT_FRAMES * 2U];

static bool have_prev_frame = false;
static int16_t prev_l = 0;
static int16_t prev_r = 0;

/*
 * Q16.16 fixed point source position.
 * float보다 재시작/정밀도/속도 면에서 다루기 쉽다.
 */
static uint32_t src_pos_q16 = 0;
static uint32_t src_step_q16 = ((uint32_t)(((uint64_t)44100U << 16) / I2S_OUTPUT_SAMPLE_RATE));

/* ============================================================
 * Debug counters
 * ============================================================ */
volatile uint32_t debugAudioBlocksReceived = 0;
volatile uint32_t debugAudioBlocksDropped = 0;
volatile uint32_t debugAudioBlocksPlayed = 0;
volatile uint32_t debugAudioSilenceWrites = 0;
volatile uint32_t debugSampleRate = 0;
volatile uint32_t debugQueueReadyHighWater = 0;

/* ============================================================
 * Function prototypes
 * ============================================================ */
static void setup_low_noise_mode(void);
static void setup_i2s_48k(void);
static void reduce_i2s_drive_strength(void);
static void setup_audio_queues(void);

static void on_sample_rate_changed(uint16_t rate);
static void audio_data_callback(const uint8_t *data, uint32_t len);

static void audio_task(void *arg);

static void enqueue_pcm_bytes(const uint8_t *data, uint32_t len);
static bool get_free_block(uint8_t *out_index);
static void return_block(uint8_t index);
static void drop_oldest_ready_block(void);
static void flush_ready_queue(void);

static void process_block_to_i2s(const AudioBlock_t *block);
static void write_i2s_direct(const int16_t *pcm, uint16_t frames);
static void resample_block_to_48k(const int16_t *in, uint16_t in_frames, uint32_t in_rate);
static void reset_resampler(uint32_t in_rate);

/* ============================================================
 * Arduino setup
 * ============================================================ */
void setup()
{
  /*
   * 노이즈 최소화 목적.
   * 디버깅이 필요하면 켜도 되지만, 평소에는 UART TX를 안 쓰는 편이 좋다.
   */
  // Serial.begin(115200);

  setup_low_noise_mode();

  setup_audio_queues();

  setup_i2s_48k();

  xTaskCreatePinnedToCore(
    audio_task,
    "audio_task",
    AUDIO_TASK_STACK_WORDS,
    NULL,
    AUDIO_TASK_PRIORITY,
    &audioTaskHandle,
    AUDIO_TASK_CORE
  );

  a2dp_sink.set_auto_reconnect(false);

  /*
   * AVRCP metadata는 안 써도 되므로 최소화.
   * 라이브러리 버전에 따라 동작하지 않으면 이 줄만 주석 처리.
   */
  a2dp_sink.set_avrc_metadata_attribute_mask(0);

  a2dp_sink.set_sample_rate_callback(on_sample_rate_changed);

  /*
   * false:
   * 라이브러리 내부 I2S 자동 출력 사용 안 함.
   * 여기서는 PCM을 queue에 넣고 audio_task가 I2S 출력 담당.
   */
  a2dp_sink.set_stream_reader(audio_data_callback, false);

  a2dp_sink.start("ESP32_BT_AUDIO_48K");

  /*
   * Classic BT 출력 파워 낮춤.
   * 연결이 불안정하면 N12 → N9 → N6 순서로 올려봐.
   */
  esp_bredr_tx_power_set(ESP_PWR_LVL_N12, ESP_PWR_LVL_N12);

  esp_bt_sleep_enable();
}

void loop()
{
  /*
   * I2S stop/start 없음.
   * audio_task가 queue가 비어 있으면 무음만 계속 출력한다.
   */
  delay(1000);
}

/* ============================================================
 * Low noise / low power setup
 * ============================================================ */
static void setup_low_noise_mode(void)
{
  /*
   * Wi-Fi는 사용하지 않는다.
   */
  esp_wifi_stop();

  /*
   * A2DP는 Classic Bluetooth.
   * BLE는 사용하지 않으므로 release.
   */
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

  /*
   * 160MHz 우선.
   * 업샘플링 중 끊기면 240MHz로 올려라.
   */
  setCpuFrequencyMhz(160);
}

/* ============================================================
 * I2S setup
 * ============================================================ */
static void setup_i2s_48k(void)
{
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = I2S_OUTPUT_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRCK,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_installed = true;

  i2s_set_pin(I2S_PORT, &pin_config);

  reduce_i2s_drive_strength();

  for (uint16_t i = 0; i < SILENCE_SAMPLES; i++)
  {
    silence_buf[i] = 0;
  }

  i2s_zero_dma_buffer(I2S_PORT);

  /*
   * 핵심:
   * I2S는 한 번 시작한 뒤 stop/start 반복하지 않는다.
   */
  i2s_start(I2S_PORT);
}

/* ============================================================
 * GPIO drive strength
 * ============================================================ */
static void reduce_i2s_drive_strength(void)
{
  /*
   * 가장 약한 drive.
   * 신호가 약해서 STM32가 못 받는 느낌이면 BCLK만 CAP_1로 올려라.
   */
  gpio_set_drive_capability((gpio_num_t)I2S_BCLK, GPIO_DRIVE_CAP_2);
  gpio_set_drive_capability((gpio_num_t)I2S_LRCK, GPIO_DRIVE_CAP_2);
  gpio_set_drive_capability((gpio_num_t)I2S_DOUT, GPIO_DRIVE_CAP_2);

  gpio_set_pull_mode((gpio_num_t)I2S_BCLK, GPIO_FLOATING);
  gpio_set_pull_mode((gpio_num_t)I2S_LRCK, GPIO_FLOATING);
  gpio_set_pull_mode((gpio_num_t)I2S_DOUT, GPIO_FLOATING);
}

/* ============================================================
 * Queue setup
 * ============================================================ */
static void setup_audio_queues(void)
{
  freeQueue = xQueueCreate(AUDIO_BLOCK_POOL_COUNT, sizeof(uint8_t));
  readyQueue = xQueueCreate(AUDIO_BLOCK_POOL_COUNT, sizeof(uint8_t));

  if (freeQueue == NULL || readyQueue == NULL)
  {
    while (1)
    {
      delay(1000);
    }
  }

  for (uint8_t i = 0; i < AUDIO_BLOCK_POOL_COUNT; i++)
  {
    xQueueSend(freeQueue, &i, portMAX_DELAY);
  }
}

/* ============================================================
 * Bluetooth callbacks
 * ============================================================ */
static void on_sample_rate_changed(uint16_t rate)
{
  if (rate == 0)
  {
    return;
  }

  bt_sample_rate = rate;
  debugSampleRate = rate;

  /*
   * 실제 reset은 audio task 쪽에서 block의 sample_rate를 보고 수행된다.
   * callback에서는 I2S driver를 건드리지 않는다.
   */
}

static void audio_data_callback(const uint8_t *data, uint32_t len)
{
  if (data == NULL || len < 4)
  {
    return;
  }

  /*
   * 16-bit stereo PCM만 받는다.
   */
  if ((len % 4U) != 0U)
  {
    return;
  }

  enqueue_pcm_bytes(data, len);
}

/* ============================================================
 * Queue producer: A2DP callback context
 * ============================================================ */
static void enqueue_pcm_bytes(const uint8_t *data, uint32_t len)
{
  const uint8_t *ptr = data;
  uint32_t bytes_left = len;

  while (bytes_left >= 4U)
  {
    uint16_t frames = (uint16_t)(bytes_left / 4U);

    if (frames > AUDIO_BLOCK_FRAMES)
    {
      frames = AUDIO_BLOCK_FRAMES;
    }

    uint32_t block_bytes = (uint32_t)frames * 4U;

    uint8_t index;

    if (!get_free_block(&index))
    {
      /*
       * ready queue가 가득 찬 경우 지연을 줄이기 위해 가장 오래된 블록을 버림.
       */
      drop_oldest_ready_block();

      if (!get_free_block(&index))
      {
        debugAudioBlocksDropped++;
        return;
      }
    }

    audioPool[index].frames = frames;
    audioPool[index].sample_rate = bt_sample_rate;
    memcpy(audioPool[index].pcm, ptr, block_bytes);

    if (xQueueSend(readyQueue, &index, 0) != pdTRUE)
    {
      return_block(index);
      debugAudioBlocksDropped++;
      return;
    }

    debugAudioBlocksReceived++;

    UBaseType_t ready_count = uxQueueMessagesWaiting(readyQueue);
    if (ready_count > debugQueueReadyHighWater)
    {
      debugQueueReadyHighWater = ready_count;
    }

    ptr += block_bytes;
    bytes_left -= block_bytes;
  }
}

static bool get_free_block(uint8_t *out_index)
{
  if (out_index == NULL)
  {
    return false;
  }

  if (xQueueReceive(freeQueue, out_index, 0) == pdTRUE)
  {
    return true;
  }

  return false;
}

static void return_block(uint8_t index)
{
  xQueueSend(freeQueue, &index, 0);
}

static void drop_oldest_ready_block(void)
{
  uint8_t old_index;

  if (xQueueReceive(readyQueue, &old_index, 0) == pdTRUE)
  {
    return_block(old_index);
    debugAudioBlocksDropped++;
  }
}

static void flush_ready_queue(void)
{
  uint8_t index;

  while (xQueueReceive(readyQueue, &index, 0) == pdTRUE)
  {
    return_block(index);
  }
}

/* ============================================================
 * Audio task: the only place that writes to I2S
 * ============================================================ */
static void audio_task(void *arg)
{
  (void)arg;

  uint32_t active_rate = 0;

  reset_resampler(44100);

  while (1)
  {
    uint8_t index;

    if (xQueueReceive(readyQueue, &index, pdMS_TO_TICKS(20)) == pdTRUE)
    {
      AudioBlock_t *block = &audioPool[index];

      if (block->sample_rate != active_rate)
      {
        active_rate = block->sample_rate;
        reset_resampler(active_rate);

        /*
         * rate 전환 직후 queue에 섞여 있을 수 있는 옛 블록을 제거.
         */
        flush_ready_queue();
      }

      process_block_to_i2s(block);

      return_block(index);
      debugAudioBlocksPlayed++;
    }
    else
    {
      /*
       * 음악이 멈춘 상태에서도 I2S driver는 멈추지 않고 무음을 출력한다.
       * stop/start 재진입 문제를 원천 제거.
       */
      size_t bytes_written = 0;
      i2s_write(I2S_PORT,
                silence_buf,
                sizeof(silence_buf),
                &bytes_written,
                portMAX_DELAY);

      reset_resampler(active_rate == 0 ? 44100 : active_rate);

      debugAudioSilenceWrites++;
    }
  }
}

/* ============================================================
 * Block processing
 * ============================================================ */
static void process_block_to_i2s(const AudioBlock_t *block)
{
  if (block == NULL || block->frames == 0)
  {
    return;
  }

  if (block->sample_rate == I2S_OUTPUT_SAMPLE_RATE)
  {
    write_i2s_direct(block->pcm, block->frames);
  }
  else
  {
    resample_block_to_48k(block->pcm, block->frames, block->sample_rate);
  }
}

static void write_i2s_direct(const int16_t *pcm, uint16_t frames)
{
  if (pcm == NULL || frames == 0)
  {
    return;
  }

  size_t bytes_written = 0;
  i2s_write(I2S_PORT,
            pcm,
            (size_t)frames * 4U,
            &bytes_written,
            portMAX_DELAY);
}

/* ============================================================
 * Fixed-point linear resampler: input rate -> 48kHz
 * ============================================================ */
static void resample_block_to_48k(const int16_t *in, uint16_t in_frames, uint32_t in_rate)
{
  if (in == NULL || in_frames == 0 || in_rate == 0)
  {
    return;
  }

  /*
   * block 크기가 너무 크면 안전하게 잘라 처리.
   */
  if (in_frames > AUDIO_BLOCK_FRAMES)
  {
    in_frames = AUDIO_BLOCK_FRAMES;
  }

  uint16_t work_frames = 0;

  if (have_prev_frame)
  {
    work_buf[0] = prev_l;
    work_buf[1] = prev_r;
    work_frames = 1;
  }

  for (uint16_t i = 0; i < (in_frames * 2U); i++)
  {
    work_buf[(work_frames * 2U) + i] = in[i];
  }

  work_frames += in_frames;

  prev_l = in[(in_frames - 1U) * 2U + 0U];
  prev_r = in[(in_frames - 1U) * 2U + 1U];
  have_prev_frame = true;

  if (work_frames < 2U)
  {
    return;
  }

  uint16_t out_frames = 0;

  while ((((uint16_t)(src_pos_q16 >> 16)) + 1U) < work_frames)
  {
    if (out_frames >= RESAMPLE_OUT_FRAMES)
    {
      size_t bytes_written = 0;
      i2s_write(I2S_PORT,
                out_buf,
                (size_t)out_frames * 4U,
                &bytes_written,
                portMAX_DELAY);

      out_frames = 0;
    }

    uint16_t idx = (uint16_t)(src_pos_q16 >> 16);
    uint32_t frac = src_pos_q16 & 0xFFFFU;

    int16_t l0 = work_buf[idx * 2U + 0U];
    int16_t r0 = work_buf[idx * 2U + 1U];
    int16_t l1 = work_buf[(idx + 1U) * 2U + 0U];
    int16_t r1 = work_buf[(idx + 1U) * 2U + 1U];

    int32_t dl = (int32_t)l1 - (int32_t)l0;
    int32_t dr = (int32_t)r1 - (int32_t)r0;

    int32_t out_l = (int32_t)l0 + ((dl * (int32_t)frac) >> 16);
    int32_t out_r = (int32_t)r0 + ((dr * (int32_t)frac) >> 16);

    if (out_l > 32767)  out_l = 32767;
    if (out_l < -32768) out_l = -32768;
    if (out_r > 32767)  out_r = 32767;
    if (out_r < -32768) out_r = -32768;

    out_buf[out_frames * 2U + 0U] = (int16_t)out_l;
    out_buf[out_frames * 2U + 1U] = (int16_t)out_r;
    out_frames++;

    src_pos_q16 += src_step_q16;
  }

  /*
   * 다음 block 기준 좌표로 이동.
   * 마지막 frame 하나는 prev_frame으로 다음 block 앞에 붙는다.
   */
  src_pos_q16 -= ((uint32_t)(work_frames - 1U) << 16);

  if (out_frames > 0)
  {
    size_t bytes_written = 0;
    i2s_write(I2S_PORT,
              out_buf,
              (size_t)out_frames * 4U,
              &bytes_written,
              portMAX_DELAY);
  }
}

static void reset_resampler(uint32_t in_rate)
{
  if (in_rate == 0)
  {
    in_rate = 44100;
  }

  have_prev_frame = false;
  prev_l = 0;
  prev_r = 0;
  src_pos_q16 = 0;

  src_step_q16 = (uint32_t)(((uint64_t)in_rate << 16) / I2S_OUTPUT_SAMPLE_RATE);
}