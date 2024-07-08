#include <driver/i2s.h>

const int waveformButtonPin = 2;  // Pin connected to the waveform selection button
const int modeButtonPin = 4;      // Pin connected to the mono/stereo mode selection button
const int sweepButtonPin = 5;     // Pin connected to the sweep mode button
const int burstButtonPin = 15;    // Pin connected to the burst mode button
const int freqPotPin = 34;        // Pin connected to the frequency potentiometer (ADC1_CH6)
const int ampPotPin = 35;         // Pin connected to the amplitude potentiometer (ADC1_CH7)
const int phasePotPin = 32;       // Pin connected to the phase potentiometer (ADC1_CH4)
const int sweepRatePotPin = 33;   // Pin connected to the sweep rate potentiometer (ADC1_CH5)
const int burstDurationPotPin = 36; // Pin connected to the burst duration potentiometer (ADC1_CH0)

// I2S configuration
#define SAMPLE_RATE     (44100)
#define I2S_NUM         I2S_NUM_0
#define I2S_BCLK_PIN    (26)
#define I2S_WCLK_PIN    (25)
#define I2S_DOUT_PIN    (22)

struct WaveformParams {
  int waveformType;
  bool isStereoMode;
  float frequency;
  int amplitude;
  float phase;
  bool sweepMode;
  float sweepRate;
  bool burstMode;
  int burstDuration;
};

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(waveformButtonPin, INPUT_PULLUP);
  pinMode(modeButtonPin, INPUT_PULLUP);
  pinMode(sweepButtonPin, INPUT_PULLUP);
  pinMode(burstButtonPin, INPUT_PULLUP);

  // Configure I2S
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  // Set I2S pins
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK_PIN,
    .ws_io_num = I2S_WCLK_PIN,
    .data_out_num = I2S_DOUT_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  // Install and start I2S driver
  if (i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("Failed to install I2S driver");
    while (1);
  }

  if (i2s_set_pin(I2S_NUM, &pin_config) != ESP_OK) {
    Serial.println("Failed to set I2S pins");
    while (1);
  }

  Serial.println("I2S initialized successfully.");
}

void loop() {
  static WaveformParams params = {0, true, 440, 1000, 0, false, 1, false, 100};  // Default values
  static unsigned long lastBurstTime = 0;
  static bool isInBurst = false;
  
  updateWaveformParams(&params);
  
  if (params.burstMode) {
    unsigned long currentTime = millis();
    if (!isInBurst && (currentTime - lastBurstTime >= params.burstDuration)) {
      isInBurst = true;
      lastBurstTime = currentTime;
    } else if (isInBurst && (currentTime - lastBurstTime >= 100)) {  // 100ms burst duration
      isInBurst = false;
    }
  }
  
  if (!params.burstMode || isInBurst) {
    generateWaveform(params);
  } else {
    // Output silence when not in burst
    int16_t silenceBuffer[200] = {0};
    size_t bytes_written;
    i2s_write(I2S_NUM, silenceBuffer, 200 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
  }
}

void updateWaveformParams(WaveformParams* params) {
  static int previousWaveformButtonState = HIGH;
  static int previousModeButtonState = HIGH;
  static int previousSweepButtonState = HIGH;
  static int previousBurstButtonState = HIGH;
  
  int waveformButtonState = digitalRead(waveformButtonPin);
  int modeButtonState = digitalRead(modeButtonPin);
  int sweepButtonState = digitalRead(sweepButtonPin);
  int burstButtonState = digitalRead(burstButtonPin);

  // Check for waveform button press
  if (waveformButtonState == LOW && previousWaveformButtonState == HIGH) {
    params->waveformType = (params->waveformType + 1) % 5;
    Serial.print("Switched to waveform: ");
    Serial.println(params->waveformType);
    delay(200);  // Debounce delay
  }
  previousWaveformButtonState = waveformButtonState;

  // Check for mode button press
  if (modeButtonState == LOW && previousModeButtonState == HIGH) {
    params->isStereoMode = !params->isStereoMode;
    Serial.print("Switched to ");
    Serial.print(params->isStereoMode ? "stereo" : "mono");
    Serial.println(" mode");
    delay(200);  // Debounce delay
  }
  previousModeButtonState = modeButtonState;

  // Check for sweep button press
  if (sweepButtonState == LOW && previousSweepButtonState == HIGH) {
    params->sweepMode = !params->sweepMode;
    Serial.print("Sweep mode ");
    Serial.println(params->sweepMode ? "enabled" : "disabled");
    delay(200);  // Debounce delay
  }
  previousSweepButtonState = sweepButtonState;

  // Check for burst button press
  if (burstButtonState == LOW && previousBurstButtonState == HIGH) {
    params->burstMode = !params->burstMode;
    Serial.print("Burst mode ");
    Serial.println(params->burstMode ? "enabled" : "disabled");
    delay(200);  // Debounce delay
  }
  previousBurstButtonState = burstButtonState;

  // Read the potentiometer values
  int freqPotValue = analogRead(freqPotPin);
  int ampPotValue = analogRead(ampPotPin);
  int phasePotValue = analogRead(phasePotPin);
  int sweepRatePotValue = analogRead(sweepRatePotPin);
  int burstDurationPotValue = analogRead(burstDurationPotPin);

  // Map the potentiometer values
  params->frequency = map(freqPotValue, 0, 4095, 1, 1000);
  params->amplitude = map(ampPotValue, 0, 4095, 0, 1000);
  params->phase = map(phasePotValue, 0, 4095, 0, 628) / 100.0; // 628 / 100 = 2π
  params->sweepRate = map(sweepRatePotValue, 0, 4095, 1, 100) / 10.0; // 0.1 to 10 Hz
  params->burstDuration = map(burstDurationPotValue, 0, 4095, 100, 2000); // 100ms to 2s
}

void generateWaveform(const WaveformParams& params) {
  static float currentFrequency = params.frequency;
  static float sweepPhase = 0;
  
  int16_t sample = 0;
  float period = 1000.0 / currentFrequency;  // Period of the waveform in milliseconds
  int16_t buffer[200];  // Buffer for samples (up to 100 stereo samples)
  int bufferSize = params.isStereoMode ? 200 : 100;  // Adjust buffer size based on mono/stereo mode

  // Update frequency for sweep mode
  if (params.sweepMode) {
    sweepPhase += params.sweepRate / SAMPLE_RATE;
    if (sweepPhase >= 1.0) sweepPhase -= 1.0;
    currentFrequency = params.frequency * pow(2, sweepPhase * 2 - 1); // Sweep one octave up and down
  } else {
    currentFrequency = params.frequency;
  }

  // Generate the selected waveform
  for (int i = 0; i < (params.isStereoMode ? 100 : bufferSize); i++) {
    float t = (float)i / (params.isStereoMode ? 100.0 : bufferSize) + params.phase / (2 * PI);
    switch (params.waveformType) {
      case 0:  // Sine wave
        sample = params.amplitude * sin(2 * PI * currentFrequency * t / SAMPLE_RATE);
        break;
      case 1:  // Square wave
        sample = (fmod(currentFrequency * t / SAMPLE_RATE, 1.0) < 0.5) ? params.amplitude : -params.amplitude;
        break;
      case 2:  // Sawtooth wave
        sample = (2 * params.amplitude * fmod(currentFrequency * t / SAMPLE_RATE, 1.0)) - params.amplitude;
        break;
      case 3:  // Triangle wave
        sample = (2 * params.amplitude * fabs(2 * fmod(currentFrequency * t / SAMPLE_RATE, 1.0) - 1)) - params.amplitude;
        break;
      case 4:  // Pulse wave
        sample = (fmod(currentFrequency * t / SAMPLE_RATE, 1.0) < 0.1) ? params.amplitude : -params.amplitude;  // 10% duty cycle
        break;
    }
    
    if (params.isStereoMode) {
      buffer[i * 2] = sample;     // Left channel
      buffer[i * 2 + 1] = sample; // Right channel
    } else {
      buffer[i] = sample;         // Mono channel
    }
  }

  size_t bytes_written;
  i2s_write(I2S_NUM, buffer, bufferSize * sizeof(int16_t), &bytes_written, portMAX_DELAY);
  delayMicroseconds(period * 1000);  // Adjust delay based on the frequency
}