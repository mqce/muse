#include "voicevox_tts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if !CONFIG_MUSE_VOICEVOX_TTS

bool voicevox_tts_start(uint32_t gen, const char *text)
{
    (void)gen;
    (void)text;
    return false;
}

size_t voicevox_tts_read(uint32_t gen, int16_t *pcm, size_t frames)
{
    (void)gen;
    (void)pcm;
    (void)frames;
    return 0;
}

voicevox_state_t voicevox_tts_state(uint32_t gen)
{
    (void)gen;
    return VOICEVOX_FAILED;
}

void voicevox_tts_cancel(void) {}

#else

static const char *TAG = "voicevox";

#define TEXT_CAP 8192
#define QUERY_CAP (48 * 1024)
#define URL_CAP 8192                     /* percent-encoded Japanese text in query */
#define HTTP_RX_BYTES 4096
#define HTTP_TX_BYTES 8192               /* must hold full /audio_query?...&text= URL */
#define PCM_BYTES (256 * 1024)           /* ~8 s at 16 kHz mono; absorbs synth gaps */
#define SENTENCE_CHARS 90
#define CHUNK_CHARS 120                  /* pack several sentences per HTTP round-trip */
#define PREBUFFER_BYTES (16000 * 2 * 1) /* hold playback until ~1 s is queued */
#define HTTP_TIMEOUT_MS 20000

static SemaphoreHandle_t s_lock;
static StreamBufferHandle_t s_pcm;
static TaskHandle_t s_task;
static char *s_text;
static char *s_job;
static char *s_query;
static char *s_url;
static char *s_encoded;
static uint32_t s_seq;
static uint32_t s_gen;
static voicevox_state_t s_state = VOICEVOX_IDLE;
static bool s_primed;

static int utf8_decode(const unsigned char *s, size_t n, uint32_t *cp)
{
    if (!n) {
        return 0;
    }
    if (s[0] < 0x80) {
        *cp = s[0];
        return 1;
    }
    if ((s[0] & 0xe0) == 0xc0 && n >= 2 && (s[1] & 0xc0) == 0x80) {
        *cp = ((uint32_t)(s[0] & 0x1f) << 6) | (s[1] & 0x3f);
        return 2;
    }
    if ((s[0] & 0xf0) == 0xe0 && n >= 3 && (s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80) {
        *cp = ((uint32_t)(s[0] & 0x0f) << 12) | ((uint32_t)(s[1] & 0x3f) << 6) | (s[2] & 0x3f);
        return 3;
    }
    if ((s[0] & 0xf8) == 0xf0 && n >= 4 && (s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80 &&
        (s[3] & 0xc0) == 0x80) {
        *cp = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3f) << 12) |
              ((uint32_t)(s[2] & 0x3f) << 6) | (s[3] & 0x3f);
        return 4;
    }
    *cp = s[0];
    return 1;
}

static bool utf8_encode(uint32_t cp, char *out, size_t cap, size_t *n)
{
    unsigned char *d = (unsigned char *)out;
    if (cp < 0x80) {
        if (*n + 1 >= cap) return false;
        d[(*n)++] = (unsigned char)cp;
        return true;
    }
    if (cp < 0x800) {
        if (*n + 2 >= cap) return false;
        d[(*n)++] = (unsigned char)(0xc0 | (cp >> 6));
        d[(*n)++] = (unsigned char)(0x80 | (cp & 0x3f));
        return true;
    }
    if (cp < 0x10000) {
        if (*n + 3 >= cap) return false;
        d[(*n)++] = (unsigned char)(0xe0 | (cp >> 12));
        d[(*n)++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
        d[(*n)++] = (unsigned char)(0x80 | (cp & 0x3f));
        return true;
    }
    if (*n + 4 >= cap) return false;
    d[(*n)++] = (unsigned char)(0xf0 | (cp >> 18));
    d[(*n)++] = (unsigned char)(0x80 | ((cp >> 12) & 0x3f));
    d[(*n)++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
    d[(*n)++] = (unsigned char)(0x80 | (cp & 0x3f));
    return true;
}

static bool keep_cp(uint32_t cp)
{
    if (cp == '\n' || cp == '\r' || cp == '\t') return true;
    if (cp >= 0x20 && cp <= 0x7e) {
        return cp != '*' && cp != '#' && cp != '`' && cp != '_' && cp != '~';
    }
    if (cp >= 0x2010 && cp <= 0x2027) return true;
    if (cp >= 0x3000 && cp <= 0x303f) return true;
    if (cp >= 0x3040 && cp <= 0x30ff) return true;
    if (cp >= 0x31f0 && cp <= 0x31ff) return true;
    if (cp >= 0x4e00 && cp <= 0x9fff) return true;
    if (cp >= 0xff00 && cp <= 0xffef) return true;
    return false;
}

static void clean_text(const char *src, char *dst, size_t cap)
{
    size_t n = 0;
    size_t i = 0;
    size_t len = strlen(src);
    bool blank = false;
    while (src[i] && n + 4 < cap) {
        uint32_t cp = 0;
        int k = utf8_decode((const unsigned char *)src + i, len - i, &cp);
        if (k <= 0) break;
        i += (size_t)k;
        if (!keep_cp(cp)) continue;
        if (cp == '\r' || cp == '\t') cp = ' ';
        if (cp == ' ' || cp == '\n') {
            if (blank || n == 0) continue;
            blank = true;
        } else {
            blank = false;
        }
        if (!utf8_encode(cp, dst, cap, &n)) break;
    }
    while (n && (dst[n - 1] == ' ' || dst[n - 1] == '\n')) n--;
    dst[n] = '\0';
}

static bool ends_sentence(uint32_t cp)
{
    return cp == 0x3002 || cp == 0xff01 || cp == 0xff1f || cp == 0xff0e || cp == '!' || cp == '?' ||
           cp == '.' || cp == '\n';
}

static bool soft_break(uint32_t cp)
{
    return cp == 0x3001 || cp == ',' || cp == 0xff0c;
}

static size_t take_sentence(const char *s)
{
    size_t i = 0;
    size_t chars = 0;
    size_t last_soft = 0;
    size_t len = strlen(s);
    while (s[i]) {
        uint32_t cp = 0;
        int k = utf8_decode((const unsigned char *)s + i, len - i, &cp);
        if (k <= 0) break;
        i += (size_t)k;
        chars++;
        if (ends_sentence(cp)) return i;
        if (soft_break(cp)) last_soft = i;
        if (chars >= SENTENCE_CHARS) return last_soft ? last_soft : i;
    }
    return i;
}

/* Several sentences in one synthesis so audio_query gaps don't underrun playback. */
static size_t take_chunk(const char *s)
{
    size_t total = 0;
    size_t chars = 0;
    size_t len = strlen(s);
    while (s[total] && chars < CHUNK_CHARS) {
        size_t n = take_sentence(s + total);
        if (!n) break;
        size_t add = 0;
        size_t i = 0;
        while (i < n) {
            uint32_t cp = 0;
            int k = utf8_decode((const unsigned char *)s + total + i, len - total - i, &cp);
            if (k <= 0) break;
            i += (size_t)k;
            add++;
        }
        if (total && chars + add > CHUNK_CHARS) break;
        total += n;
        chars += add;
        if (chars >= CHUNK_CHARS) break;
    }
    return total;
}

static bool job_current(uint32_t seq)
{
    return s_seq == seq;
}

static bool push_pcm(uint32_t seq, const int16_t *pcm, size_t n)
{
    size_t off = 0;
    while (off < n) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        size_t sent = 0;
        bool current = s_seq == seq;
        if (current) {
            /* Never write a half sample: an odd partial send desyncs int16 PCM
             * into speech-shaped static for the rest of the stream. */
            size_t space = xStreamBufferSpacesAvailable(s_pcm) & ~(sizeof(int16_t) - 1);
            size_t want = (n - off) * sizeof(int16_t);
            if (want > space) {
                want = space;
            }
            if (want) {
                sent = xStreamBufferSend(s_pcm, pcm + off, want, 0);
                /* want is even; an odd return would already have desynced the buffer. */
                if (sent & 1) {
                    ESP_LOGW(TAG, "odd pcm send %u", (unsigned)sent);
                    sent &= ~(sizeof(int16_t) - 1);
                }
            }
        }
        xSemaphoreGive(s_lock);
        if (!current) return false;
        if (!sent) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        off += sent / sizeof(int16_t);
    }
    return true;
}

typedef struct {
    int phase;
    uint8_t acc[16];
    int acc_n;
    uint32_t left;
    int rate;
    int channels;
    bool fmt_ok;
    bool done;
    bool bad;
} wav_t;

enum { W_RIFF = 0, W_CHUNK, W_SKIP, W_FMT, W_PCM, W_DONE };

static uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static int wav_feed(wav_t *w, const uint8_t *in, size_t n, size_t *used, int16_t *out, size_t cap, size_t *out_n)
{
    *used = 0;
    *out_n = 0;
    if (w->bad) return -1;
    while (*used < n && *out_n < cap && !w->bad && !w->done) {
        if (w->phase == W_RIFF || w->phase == W_CHUNK) {
            int need = w->phase == W_RIFF ? 12 : 8;
            while (w->acc_n < need && *used < n) w->acc[w->acc_n++] = in[(*used)++];
            if (w->acc_n < need) break;
            if (w->phase == W_RIFF) {
                if (memcmp(w->acc, "RIFF", 4) != 0 || memcmp(w->acc + 8, "WAVE", 4) != 0) {
                    w->bad = true;
                    break;
                }
                w->phase = W_CHUNK;
            } else {
                uint32_t size = rd_le32(w->acc + 4);
                if (memcmp(w->acc, "fmt ", 4) == 0) {
                    w->phase = W_FMT;
                    w->left = size;
                } else if (memcmp(w->acc, "data", 4) == 0) {
                    if (!w->fmt_ok) {
                        w->bad = true;
                        break;
                    }
                    w->phase = W_PCM;
                    w->left = size;
                } else {
                    w->phase = W_SKIP;
                    w->left = size + (size & 1);
                }
            }
            w->acc_n = 0;
        } else if (w->phase == W_SKIP || w->phase == W_FMT) {
            size_t step = n - *used;
            if (step > w->left) step = w->left;
            if (w->phase == W_FMT && w->acc_n < 16) {
                size_t copy = 16 - (size_t)w->acc_n;
                if (copy > step) copy = step;
                memcpy(w->acc + w->acc_n, in + *used, copy);
                w->acc_n += (int)copy;
            }
            *used += step;
            w->left -= (uint32_t)step;
            if (w->left) break;
            if (w->phase == W_FMT) {
                if (w->acc_n < 16 || rd_le16(w->acc) != 1 || w->acc_n < 16) {
                    w->bad = true;
                    break;
                }
                w->channels = rd_le16(w->acc + 2);
                w->rate = (int)rd_le32(w->acc + 4);
                int bits = rd_le16(w->acc + 14);
                if ((w->channels != 1 && w->channels != 2) || bits != 16 || w->rate < 8000 || w->rate > 48000) {
                    w->bad = true;
                    break;
                }
                w->fmt_ok = true;
            }
            w->acc_n = 0;
            w->phase = W_CHUNK;
        } else if (w->phase == W_PCM) {
            int width = 2 * w->channels;
            while (*used < n && w->left && *out_n < cap) {
                w->acc[w->acc_n++] = in[(*used)++];
                w->left--;
                if (w->acc_n < width) continue;
                int16_t left = (int16_t)rd_le16(w->acc);
                int16_t sample = left;
                if (w->channels == 2) {
                    int16_t right = (int16_t)rd_le16(w->acc + 2);
                    sample = (int16_t)(((int32_t)left + right) / 2);
                }
                out[(*out_n)++] = sample;
                w->acc_n = 0;
            }
            if (!w->left && !w->acc_n) w->done = true;
        }
    }
    return w->bad ? -1 : 0;
}

typedef struct {
    int32_t pos;
    int16_t prev;
    bool primed;
} rs_t;

/* Consumes every input sample. `cap` must hold n * 16000 / rate samples. */
static size_t to_16k(rs_t *rs, int rate, const int16_t *in, size_t n, int16_t *out, size_t cap)
{
    if (!n) return 0;
    if (rate == 16000) {
        if (n > cap) n = cap;
        memcpy(out, in, n * sizeof(int16_t));
        rs->prev = in[n - 1];
        return n;
    }
    int32_t step = (int32_t)(((int64_t)rate << 16) / 16000);
    if (!rs->primed) {
        rs->prev = in[0];
        rs->primed = true;
    }
    size_t o = 0;
    int32_t p = rs->pos;
    while ((p >> 16) < (int32_t)n && o < cap) {
        size_t i = (size_t)(p >> 16);
        int32_t a = i ? in[i - 1] : rs->prev;
        int32_t b = in[i];
        out[o++] = (int16_t)(a + (int32_t)(((int64_t)(b - a) * (p & 0xffff)) >> 16));
        p += step;
    }
    rs->prev = in[n - 1];
    rs->pos = p - ((int32_t)n << 16);
    return o;
}

static bool percent_encode(const char *src, size_t n, char *out, size_t cap)
{
    static const char *hex = "0123456789ABCDEF";
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)src[i];
        bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                     c == '.' || c == '_' || c == '~';
        if (plain) {
            if (o + 1 >= cap) return false;
            out[o++] = (char)c;
        } else {
            if (o + 3 >= cap) return false;
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0xf];
        }
    }
    out[o] = '\0';
    return true;
}

static void base_url(char *out, size_t cap)
{
    snprintf(out, cap, "%s", CONFIG_MUSE_VOICEVOX_URL);
    size_t n = strlen(out);
    while (n && out[n - 1] == '/') out[--n] = '\0';
}

static bool read_body(esp_http_client_handle_t client, char *dst, size_t cap)
{
    size_t n = 0;
    while (n + 1 < cap) {
        int got = esp_http_client_read(client, dst + n, (int)(cap - 1 - n));
        if (got < 0) return false;
        if (!got) break;
        n += (size_t)got;
    }
    dst[n] = '\0';
    return n > 0 && n + 1 < cap;
}

static bool status_ok(esp_http_client_handle_t client, const char *what)
{
    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status == 200) return true;
    ESP_LOGW(TAG, "%s HTTP %d", what, status);
    return false;
}

static char *audio_query(uint32_t seq, const char *sentence, size_t n)
{
    char root[96];
    if (!percent_encode(sentence, n, s_encoded, URL_CAP)) return NULL;
    base_url(root, sizeof(root));
    int wrote = snprintf(s_url, URL_CAP, "%s/audio_query?speaker=%d&text=%s", root, CONFIG_MUSE_VOICEVOX_SPEAKER,
                         s_encoded);
    if (wrote <= 0 || wrote >= URL_CAP || !job_current(seq)) return NULL;

    esp_http_client_config_t cfg = {
        .url = s_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .buffer_size = HTTP_RX_BYTES,
        .buffer_size_tx = HTTP_TX_BYTES,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client || esp_http_client_open(client, 0) != ESP_OK) {
        if (client) esp_http_client_cleanup(client);
        return NULL;
    }
    bool ok = status_ok(client, "audio_query") && read_body(client, s_query, QUERY_CAP);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (!ok || !job_current(seq)) return NULL;

    cJSON *json = cJSON_Parse(s_query);
    if (!json) return NULL;
    cJSON *rate = cJSON_GetObjectItemCaseSensitive(json, "outputSamplingRate");
    if (cJSON_IsNumber(rate)) cJSON_SetNumberValue(rate, 16000);
    else {
        cJSON_DeleteItemFromObject(json, "outputSamplingRate");
        cJSON_AddNumberToObject(json, "outputSamplingRate", 16000);
    }
    cJSON_DeleteItemFromObject(json, "outputStereo");
    cJSON_AddBoolToObject(json, "outputStereo", 0);
    char *printed = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return printed;
}

static bool synthesize(uint32_t seq, const char *query, bool *produced)
{
    char root[96];
    base_url(root, sizeof(root));
    snprintf(s_url, URL_CAP, "%s/synthesis?speaker=%d", root, CONFIG_MUSE_VOICEVOX_SPEAKER);
    if (!job_current(seq)) return false;

    esp_http_client_config_t cfg = {
        .url = s_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .buffer_size = HTTP_RX_BYTES,
        .buffer_size_tx = HTTP_TX_BYTES,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;
    esp_http_client_set_header(client, "Content-Type", "application/json");
    int body_len = (int)strlen(query);
    bool ok = esp_http_client_open(client, body_len) == ESP_OK;
    int written = 0;
    while (ok && written < body_len) {
        int chunk = esp_http_client_write(client, query + written, body_len - written);
        if (chunk <= 0) {
            ok = false;
            break;
        }
        written += chunk;
    }
    ok = ok && status_ok(client, "synthesis");

    wav_t wav;
    memset(&wav, 0, sizeof(wav));
    rs_t rs = {0};
    bool logged = false;
    uint8_t raw[768];
    int16_t decoded[128];
    int16_t play[320];
    while (ok && job_current(seq)) {
        int got = esp_http_client_read(client, (char *)raw, sizeof(raw));
        if (got < 0) {
            ok = false;
            break;
        }
        if (!got) break;
        size_t used = 0;
        while (ok && used < (size_t)got) {
            size_t step = 0;
            size_t samples = 0;
            if (wav_feed(&wav, raw + used, (size_t)got - used, &step, decoded, 128, &samples) < 0) {
                ESP_LOGW(TAG, "unexpected wav");
                ok = false;
                break;
            }
            if (!step && !samples) break;
            used += step;
            if (!samples) continue;
            if (!logged) {
                ESP_LOGI(TAG, "reply audio: %d Hz, %d ch", wav.rate, wav.channels);
                logged = true;
            }
            size_t made = to_16k(&rs, wav.rate, decoded, samples, play, 320);
            if (!made || !push_pcm(seq, play, made)) {
                ok = false;
                break;
            }
            *produced = true;
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ok && job_current(seq);
}

static void speak_job(uint32_t seq, const char *text)
{
    bool produced = false;
    bool ok = true;
    ESP_LOGI(TAG, "synth %u chars", (unsigned)strlen(text));
    const char *cursor = text;
    while (ok && *cursor && job_current(seq)) {
        while (*cursor == ' ' || *cursor == '\n') cursor++;
        if (!*cursor) break;
        size_t n = take_chunk(cursor);
        if (!n) break;
        char *query = audio_query(seq, cursor, n);
        if (!query) {
            ok = false;
            break;
        }
        ok = synthesize(seq, query, &produced);
        cJSON_free(query);
        cursor += n;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_seq == seq) {
        s_state = (ok || produced) ? VOICEVOX_DONE : VOICEVOX_FAILED;
        s_primed = true; /* release any held prebuffer on completion */
    }
    xSemaphoreGive(s_lock);
}

static void worker(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        uint32_t seq = s_seq;
        memcpy(s_job, s_text, TEXT_CAP);
        xSemaphoreGive(s_lock);
        if (s_job[0] && job_current(seq)) speak_job(seq, s_job);
    }
}

static bool ensure_init(void)
{
    if (s_task) return true;
    s_lock = xSemaphoreCreateMutex();
    /* +1: CreateWithCaps keeps one byte back; an odd usable length lets a fill
     * stop halfway through a sample (same fix as hatch s_in). */
    s_pcm = xStreamBufferCreateWithCaps(PCM_BYTES + 1, 2, MALLOC_CAP_SPIRAM);
    s_text = heap_caps_malloc(TEXT_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_job = heap_caps_malloc(TEXT_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_query = heap_caps_malloc(QUERY_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_url = heap_caps_malloc(URL_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_encoded = heap_caps_malloc(URL_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_lock || !s_pcm || !s_text || !s_job || !s_query || !s_url || !s_encoded ||
        xTaskCreate(worker, "voicevox", 16384, NULL, 5, &s_task) != pdPASS) {
        ESP_LOGE(TAG, "init failed");
        return false;
    }
    s_text[0] = '\0';
    return true;
}

bool voicevox_tts_start(uint32_t gen, const char *text)
{
    if (!text || !text[0] || !ensure_init()) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_seq++;
    clean_text(text, s_text, TEXT_CAP);
    if (!s_text[0]) {
        s_state = VOICEVOX_IDLE;
        xSemaphoreGive(s_lock);
        return false;
    }
    s_gen = gen;
    s_state = VOICEVOX_RUNNING;
    s_primed = false;
    xStreamBufferReset(s_pcm);
    xSemaphoreGive(s_lock);
    xTaskNotifyGive(s_task);
    return true;
}

size_t voicevox_tts_read(uint32_t gen, int16_t *pcm, size_t frames)
{
    if (!s_lock || !pcm || !frames) return 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    size_t n = 0;
    if (s_gen == gen) {
        if (!s_primed) {
            size_t avail = xStreamBufferBytesAvailable(s_pcm);
            if (s_state == VOICEVOX_RUNNING && avail < PREBUFFER_BYTES) {
                xSemaphoreGive(s_lock);
                return 0;
            }
            s_primed = true;
        }
        n = xStreamBufferReceive(s_pcm, pcm, frames * sizeof(int16_t), 0) / sizeof(int16_t);
    }
    xSemaphoreGive(s_lock);
    return n;
}

voicevox_state_t voicevox_tts_state(uint32_t gen)
{
    if (!s_lock) return VOICEVOX_FAILED;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    voicevox_state_t state = s_gen == gen ? s_state : VOICEVOX_FAILED;
    xSemaphoreGive(s_lock);
    return state;
}

void voicevox_tts_cancel(void)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_seq++;
    s_gen = 0;
    s_state = VOICEVOX_IDLE;
    s_primed = false;
    xStreamBufferReset(s_pcm);
    xSemaphoreGive(s_lock);
}

#endif
