#include "FFT.h"

#define FFT_SIZE        2048
#define SAMPLE_RATE     2000.0f
#define FREQ_RESOLUTION (SAMPLE_RATE / FFT_SIZE)  // ~0.977 Hz per bin
#define MAX_PEAKS       10
#define MIN_PEAK_BIN    2    // ignore DC and bin 1 (~1Hz minimum)
#define MAX_PEAK_BIN    (FFT_SIZE / 4)  // 512 bins = 500Hz at 2000Hz sample rate

// FFT buffers — static so they don't hit task stack
static float fft_input[FFT_SIZE * 2];   // interleaved real/imag for esp-dsp
static float fft_wind[FFT_SIZE];        // Hann window coefficients
static float fft_mag[FFT_SIZE / 2];     // magnitude spectrum
static bool  fft_initialized = false;

static void findPeaks(float *mag, int binCount, FlickerResult &result) {
    result.peakCount = 0;

    // Find noise floor — median of lower 10% of bins
    float sorted[binCount / 10];
    int sortLen = binCount / 10;
    for (int i = 0; i < sortLen; i++) sorted[i] = mag[MIN_PEAK_BIN + i];
    // Simple insertion sort on small array
    for (int i = 1; i < sortLen; i++) {
        float key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > key) { sorted[j+1] = sorted[j]; j--; }
        sorted[j+1] = key;
    }
    float noiseFloor = sorted[sortLen / 2] * 10.0f;  // 10× median as threshold

    // Find local maxima above threshold
    for (int i = MIN_PEAK_BIN + 1; i < MAX_PEAK_BIN - 1; i++) {
        if (mag[i] > noiseFloor &&
            mag[i] > mag[i-1] &&
            mag[i] > mag[i+1]) {

            // Quadratic interpolation for sub-bin frequency accuracy
            float alpha = mag[i-1];
            float beta  = mag[i];
            float gamma = mag[i+1];
            float offset = 0.5f * (alpha - gamma) / (alpha - 2.0f*beta + gamma);
            float freq = (i + offset) * FREQ_RESOLUTION;

            // Store peak — insert in magnitude order
            if (result.peakCount < MAX_PEAKS) {
                result.peaks[result.peakCount]      = freq;
                result.magnitudes[result.peakCount] = beta;
                result.peakCount++;
            } else {
                // Replace smallest if this is larger
                int minIdx = 0;
                for (int k = 1; k < MAX_PEAKS; k++) {
                    if (result.magnitudes[k] < result.magnitudes[minIdx]) minIdx = k;
                }
                if (beta > result.magnitudes[minIdx]) {
                    result.peaks[minIdx]      = freq;
                    result.magnitudes[minIdx] = beta;
                }
            }
        }
    }

    // Normalize magnitudes to 0.0-1.0
    float maxMag = 0.0f;
    for (int i = 0; i < result.peakCount; i++) {
        if (result.magnitudes[i] > maxMag) maxMag = result.magnitudes[i];
    }
    if (maxMag > 0.0f) {
        for (int i = 0; i < result.peakCount; i++) {
            result.magnitudes[i] /= maxMag;
        }
    }


    // After normalization, filter weak peaks
    int validCount = 0;
    for (int i = 0; i < result.peakCount; i++) {
        if (result.magnitudes[i] >= 0.05f) {
            result.peaks[validCount]      = result.peaks[i];
            result.magnitudes[validCount] = result.magnitudes[i];
            validCount++;
        }
    }
    result.peakCount = validCount;

    // Sort peaks by frequency ascending
    for (int i = 0; i < result.peakCount - 1; i++) {
        for (int j = i + 1; j < result.peakCount; j++) {
            if (result.peaks[j] < result.peaks[i]) {
                float tf = result.peaks[i];   result.peaks[i]      = result.peaks[j];   result.peaks[j]      = tf;
                float tm = result.magnitudes[i]; result.magnitudes[i] = result.magnitudes[j]; result.magnitudes[j] = tm;
            }
        }
    }
}

void AS7341_FFT_Task(void *pvParameters) {
    Serial.println("FFTTask: started");

    // Initialize esp-dsp FFT
    esp_err_t ret = dsps_fft2r_init_fc32(nullptr, FFT_SIZE);
    if (ret != ESP_OK) {
        Serial.printf("FFT init failed: %d\n", ret);
        vTaskDelete(nullptr);
        return;
    }

    // Pre-compute Hann window
    dsps_wind_hann_f32(fft_wind, FFT_SIZE);
    fft_initialized = true;

    uint16_t *samples = nullptr;

    for (;;) {
        // Wait for a buffer from FlickerCaptureTask
        if (xQueueReceive(flickerQueue, &samples, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (samples == nullptr) continue;

        uint32_t fftStart = millis();

        // Convert to float, compute mean for DC removal
        float mean = 0.0f;
        for (int i = 0; i < FFT_SIZE; i++) {
            mean += (float)samples[i];
        }
        mean /= FFT_SIZE;

        // Fill interleaved real/imag buffer with DC-removed, windowed samples
        for (int i = 0; i < FFT_SIZE; i++) {
            fft_input[i * 2]     = ((float)samples[i] - mean) * fft_wind[i]; // real
            fft_input[i * 2 + 1] = 0.0f;                                      // imag
        }

        // Run FFT
        dsps_fft2r_fc32(fft_input, FFT_SIZE);

        // Bit-reverse
        dsps_bit_rev_fc32(fft_input, FFT_SIZE);

        // Compute magnitude spectrum (first N/2 bins)
        for (int i = 0; i < FFT_SIZE / 2; i++) {
            float re = fft_input[i * 2];
            float im = fft_input[i * 2 + 1];
            fft_mag[i] = sqrtf(re*re + im*im);
        }

        // Find peaks and store in shared result
        FlickerResult result = {};
        result.timestamp = millis();
        findPeaks(fft_mag, FFT_SIZE / 2, result);
        result.valid = (result.peakCount > 0);

        // Atomic-ish update — Core 1 only writes, Core 0 only reads
        flickerResult = result;

        Serial.printf("FFT: %dms, %d peaks found:\n", millis() - fftStart, result.peakCount);
        for (int i = 0; i < result.peakCount; i++) {
            Serial.printf("  %.1f Hz  mag=%.3f\n", result.peaks[i], result.magnitudes[i]);
        }
    }
}