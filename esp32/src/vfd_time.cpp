#include "vfd_time.h"

#include <WiFi.h>
#include <time.h>

#include "app_runtime.h"

#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#endif

#ifndef VFD_TIME_WIFI_SSID
#define VFD_TIME_WIFI_SSID ""
#endif

#ifndef VFD_TIME_WIFI_PASSWORD
#define VFD_TIME_WIFI_PASSWORD ""
#endif

namespace vfd_time {

const char* const WIFI_SSID = VFD_TIME_WIFI_SSID;
const char* const WIFI_PASSWORD = VFD_TIME_WIFI_PASSWORD;

constexpr long GMT_OFFSET_SECONDS = 8 * 60 * 60;
constexpr int DAYLIGHT_OFFSET_SECONDS = 0;
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t TIME_SYNC_RETRY_MS = 30000;
constexpr uint32_t CLOCK_REFRESH_MS = 250;
constexpr uint32_t FOCUS_BLOCK_MS = 25UL * 60UL * 1000UL;
constexpr uint32_t RELAX_BLOCK_MS = 5UL * 60UL * 1000UL;

enum PomodoroMode : uint8_t {
	PomodoroIdle,
	PomodoroSelecting,
	PomodoroFocus,
	PomodoroRelax,
	PomodoroComplete,
};

enum SoundCue : uint8_t {
	SoundCueNone,
	SoundCueStart,
	SoundCueRelax,
	SoundCueComplete,
};

uint32_t lastWifiAttempt = 0;
uint32_t lastTimeSyncAttempt = 0;
uint32_t lastClockRefresh = 0;
bool timeConfigured = false;
bool timeSynced = false;
tm currentTime = {};
PomodoroMode pomodoroMode = PomodoroIdle;
uint8_t selectedTomatoes = 1;
uint8_t completedTomatoes = 0;
uint32_t phaseStartedAt = 0;
uint32_t phaseDurationMs = 0;
SoundCue activeSoundCue = SoundCueNone;
uint8_t soundCueIndex = 0;
uint32_t nextSoundAt = 0;

uint16_t vfdColor(uint8_t r, uint8_t g, uint8_t b) {
	return rgb(r, g, b);
}

void drawGlowText(const char* text, int x, int y, uint16_t color, uint8_t textSize) {
	framebuffer.setTextFont(1);
	framebuffer.setTextSize(textSize);
	framebuffer.setTextColor(vfdColor(3, 58, 56));
	framebuffer.setCursor(x - 1, y);
	framebuffer.print(text);
	framebuffer.setCursor(x + 1, y);
	framebuffer.print(text);
	framebuffer.setCursor(x, y - 1);
	framebuffer.print(text);
	framebuffer.setCursor(x, y + 1);
	framebuffer.print(text);
	framebuffer.setTextColor(color);
	framebuffer.setCursor(x, y);
	framebuffer.print(text);
}

void drawPanelBackground() {
	framebuffer.fillScreen(vfdColor(1, 8, 13));
	framebuffer.drawRect(2, 2, DISPLAY_WIDTH - 4, DISPLAY_HEIGHT - 4, vfdColor(8, 54, 62));
	framebuffer.drawRect(4, 4, DISPLAY_WIDTH - 8, DISPLAY_HEIGHT - 8, vfdColor(1, 22, 31));

	for (int y = 10; y < DISPLAY_HEIGHT - 8; y += 6) {
		framebuffer.drawFastHLine(6, y, DISPLAY_WIDTH - 12, vfdColor(0, 18, 22));
	}

	framebuffer.fillRect(0, 0, DISPLAY_WIDTH, 7, vfdColor(0, 5, 9));
	framebuffer.fillRect(0, DISPLAY_HEIGHT - 7, DISPLAY_WIDTH, 7, vfdColor(0, 5, 9));
}

uint8_t segmentMask(uint8_t digit) {
	constexpr uint8_t A = 1 << 0;
	constexpr uint8_t B = 1 << 1;
	constexpr uint8_t C = 1 << 2;
	constexpr uint8_t D = 1 << 3;
	constexpr uint8_t E = 1 << 4;
	constexpr uint8_t F = 1 << 5;
	constexpr uint8_t G = 1 << 6;
	constexpr uint8_t masks[] = {
		A | B | C | D | E | F,
		B | C,
		A | B | D | E | G,
		A | B | C | D | G,
		B | C | F | G,
		A | C | D | F | G,
		A | C | D | E | F | G,
		A | B | C,
		A | B | C | D | E | F | G,
		A | B | C | D | F | G,
	};
	return digit < 10 ? masks[digit] : 0;
}

constexpr uint8_t MATRIX_COLUMNS = 40;
constexpr uint8_t MATRIX_ROWS = 10;
constexpr uint8_t MATRIX_CELL_WIDTH = DISPLAY_WIDTH / MATRIX_COLUMNS;
constexpr uint8_t MATRIX_CELL_HEIGHT = 8;
constexpr uint8_t MATRIX_TOP = 34;
constexpr uint8_t TIME_MATRIX_COLUMNS = 39;
constexpr uint8_t TIME_MATRIX_ROWS = 7;

void drawMatrixDot(uint8_t col, uint8_t row, bool active) {
	constexpr int dotSize = 4;
	const int x = col * MATRIX_CELL_WIDTH + 1;
	const int y = MATRIX_TOP + row * MATRIX_CELL_HEIGHT;
	if (active) {
		framebuffer.fillRoundRect(x - 1, y - 1, dotSize + 2, dotSize + 2, 2, vfdColor(0, 58, 52));
		framebuffer.fillRoundRect(x, y, dotSize, dotSize, 2, vfdColor(94, 255, 224));
	} else {
		framebuffer.fillRoundRect(x + 1, y + 1, dotSize - 1, dotSize - 1, 1, vfdColor(0, 24, 24));
	}
}

void drawMatrixBackground() {
	for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
		for (uint8_t col = 0; col < MATRIX_COLUMNS; col++) {
			drawMatrixDot(col, row, false);
		}
	}
}

void drawDigit(uint8_t digit, uint8_t gridCol, uint8_t gridRow) {
	constexpr uint8_t patterns[10][7] = {
		{ 0b11111, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b11111 },
		{ 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 },
		{ 0b11110, 0b00001, 0b00001, 0b11110, 0b10000, 0b10000, 0b11111 },
		{ 0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110 },
		{ 0b10010, 0b10010, 0b10010, 0b11111, 0b00010, 0b00010, 0b00010 },
		{ 0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110 },
		{ 0b01111, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 },
		{ 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 },
		{ 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 },
		{ 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b11110 },
	};

	for (uint8_t row = 0; row < 7; row++) {
		for (uint8_t col = 0; col < 5; col++) {
			const bool active = (patterns[digit][row] & (1 << (4 - col))) != 0;
			drawMatrixDot(gridCol + col, gridRow + row, active);
		}
	}
}

void drawColon(uint8_t gridCol, uint8_t gridRow) {
	for (uint8_t row = 0; row < 7; row++) {
		drawMatrixDot(gridCol, gridRow + row, row == 2 || row == 4);
	}
}

void drawLargeHms(uint8_t hours, uint8_t minutes, uint8_t seconds) {
	constexpr uint8_t digitWidth = 5;
	constexpr uint8_t colonWidth = 1;
	constexpr uint8_t gap = 1;
	constexpr uint8_t startCol = (MATRIX_COLUMNS - TIME_MATRIX_COLUMNS) / 2;
	constexpr uint8_t startRow = (MATRIX_ROWS - TIME_MATRIX_ROWS) / 2;

	const uint8_t values[] = {
		static_cast<uint8_t>(hours / 10),
		static_cast<uint8_t>(hours % 10),
		static_cast<uint8_t>(minutes / 10),
		static_cast<uint8_t>(minutes % 10),
		static_cast<uint8_t>(seconds / 10),
		static_cast<uint8_t>(seconds % 10),
	};

	drawMatrixBackground();

	uint8_t col = startCol;
	drawDigit(values[0], col, startRow);
	col += digitWidth + gap;
	drawDigit(values[1], col, startRow);
	col += digitWidth + gap;
	drawColon(col, startRow);
	col += colonWidth + gap;
	drawDigit(values[2], col, startRow);
	col += digitWidth + gap;
	drawDigit(values[3], col, startRow);
	col += digitWidth + gap;
	drawColon(col, startRow);
	col += colonWidth + gap;
	drawDigit(values[4], col, startRow);
	col += digitWidth + gap;
	drawDigit(values[5], col, startRow);
}

void drawLargeTime() {
	drawLargeHms(static_cast<uint8_t>(currentTime.tm_hour), static_cast<uint8_t>(currentTime.tm_min),
	             static_cast<uint8_t>(currentTime.tm_sec));
}

uint32_t remainingPhaseMs(uint32_t now) {
	if (pomodoroMode != PomodoroFocus && pomodoroMode != PomodoroRelax) {
		return 0;
	}
	const uint32_t elapsed = now - phaseStartedAt;
	return elapsed >= phaseDurationMs ? 0 : phaseDurationMs - elapsed;
}

void drawLargeDuration(uint32_t milliseconds) {
	const uint32_t totalSeconds = (milliseconds + 999) / 1000;
	const uint8_t hours = static_cast<uint8_t>((totalSeconds / 3600) % 100);
	const uint8_t minutes = static_cast<uint8_t>((totalSeconds / 60) % 60);
	const uint8_t seconds = static_cast<uint8_t>(totalSeconds % 60);
	drawLargeHms(hours, minutes, seconds);
}

const char* pomodoroLabel() {
	switch (pomodoroMode) {
		case PomodoroSelecting:
			return "SELECT";
		case PomodoroFocus:
			return "FOCUS";
		case PomodoroRelax:
			return "RELAX";
		case PomodoroComplete:
			return "COMPLETE";
		case PomodoroIdle:
			break;
	}
	return "TIMER";
}

uint32_t selectedDurationMs() {
	return static_cast<uint32_t>(selectedTomatoes) * FOCUS_BLOCK_MS;
}

void drawTomatoSlots() {
	const uint16_t bright = vfdColor(94, 255, 224);
	const uint16_t dim = vfdColor(39, 138, 128);
	const int centers[] = { 48, 120, 192 };
	const char* const labels[] = { "25", "50", "75" };

	for (uint8_t i = 0; i < 3; i++) {
		const bool selected = selectedTomatoes == i + 1;
		const uint16_t color = selected ? bright : dim;
		framebuffer.drawFastHLine(centers[i] - 22, 105, 44, selected ? bright : vfdColor(2, 48, 48));
		drawGlowText(labels[i], centers[i] - 8, 108, color, 1);
		drawGlowText("MIN", centers[i] + 8, 108, dim, 1);
		if (selected) {
			framebuffer.fillTriangle(centers[i] - 5, 101, centers[i] + 5, 101, centers[i], 96, bright);
		}
	}
}

void drawStatusLabels() {
	const uint16_t bright = vfdColor(94, 255, 224);
	const uint16_t dim = vfdColor(39, 138, 128);

	drawGlowText("STEREO", 10, 13, dim, 1);
	drawGlowText(pomodoroLabel(), 65, 13, pomodoroMode == PomodoroIdle ? dim : bright, 1);
	drawGlowText(timeSynced ? "NTP LOCK" : "NTP SYNC", 119, 13, timeSynced ? bright : dim, 1);

	char dateText[18];
	if (pomodoroMode == PomodoroSelecting) {
		snprintf(dateText, sizeof(dateText), "TILT %u MIN", static_cast<unsigned>(selectedTomatoes * 25));
	} else if (pomodoroMode == PomodoroFocus || pomodoroMode == PomodoroRelax) {
		snprintf(dateText, sizeof(dateText), "%u/%u TOMATO", static_cast<unsigned>(completedTomatoes + 1),
		         static_cast<unsigned>(selectedTomatoes));
	} else if (pomodoroMode == PomodoroComplete) {
		snprintf(dateText, sizeof(dateText), "%u DONE", static_cast<unsigned>(selectedTomatoes));
	} else if (timeSynced) {
		strftime(dateText, sizeof(dateText), "%b %d %a", &currentTime);
	} else {
		snprintf(dateText, sizeof(dateText), "WAITING TIME");
	}
	drawGlowText(dateText, 10, 116, dim, 1);
	if (pomodoroMode == PomodoroIdle) {
		drawGlowText("A:SET", DISPLAY_WIDTH - 43, 116, dim, 1);
	} else if (pomodoroMode == PomodoroSelecting) {
		drawGlowText("A:START", DISPLAY_WIDTH - 56, 116, bright, 1);
	} else {
		drawGlowText("AA:CLR", DISPLAY_WIDTH - 50, 116, dim, 1);
	}
}

uint16_t soundCueNote(SoundCue cue, uint8_t index) {
	switch (cue) {
		case SoundCueStart: {
			constexpr uint16_t notes[] = { 523, 659, 784, 0 };
			return notes[index];
		}
		case SoundCueRelax: {
			constexpr uint16_t notes[] = { 784, 659, 523, 659, 0 };
			return notes[index];
		}
		case SoundCueComplete: {
			constexpr uint16_t notes[] = { 659, 784, 988, 1175, 0 };
			return notes[index];
		}
		case SoundCueNone:
			break;
	}
	return 0;
}

uint16_t soundCueDuration(SoundCue cue, uint8_t index) {
	if (cue == SoundCueComplete && index == 3) {
		return 180;
	}
	return 90;
}

void startSoundCue(SoundCue cue) {
	activeSoundCue = cue;
	soundCueIndex = 0;
	nextSoundAt = 0;
}

void updateSoundCue(uint32_t now) {
	if (activeSoundCue == SoundCueNone || now < nextSoundAt) {
		return;
	}

	const uint16_t note = soundCueNote(activeSoundCue, soundCueIndex);
	if (note == 0) {
		activeSoundCue = SoundCueNone;
		return;
	}

	const uint16_t duration = soundCueDuration(activeSoundCue, soundCueIndex);
	M5.Speaker.tone(note, duration);
	nextSoundAt = now + duration + 55;
	soundCueIndex++;
}

uint8_t chooseTomatoesFromImu() {
	if (M5.Imu.isEnabled() && M5.Imu.update() != 0) {
		auto imu = M5.Imu.getImuData();
		imuAccelX = imu.accel.x;
		imuAccelY = imu.accel.y;
		imuAccelZ = imu.accel.z;
	}

	const float tilt = fabs(imuAccelX) >= fabs(imuAccelY) ? imuAccelX : -imuAccelY;
	if (tilt > 0.35f) {
		return 1;
	}
	if (tilt < -0.35f) {
		return 3;
	}
	return 2;
}

void startFocusPhase(uint32_t now) {
	pomodoroMode = PomodoroFocus;
	phaseStartedAt = now;
	phaseDurationMs = FOCUS_BLOCK_MS;
	startSoundCue(SoundCueStart);
	LOG_PRINTF("VFD tomato: focus %u/%u started\n", static_cast<unsigned>(completedTomatoes + 1),
	           static_cast<unsigned>(selectedTomatoes));
}

void startRelaxPhase(uint32_t now) {
	pomodoroMode = PomodoroRelax;
	phaseStartedAt = now;
	phaseDurationMs = RELAX_BLOCK_MS;
	startSoundCue(SoundCueRelax);
	LOG_PRINTLN("VFD tomato: relax started");
}

void completePomodoro() {
	pomodoroMode = PomodoroComplete;
	phaseStartedAt = millis();
	phaseDurationMs = 0;
	startSoundCue(SoundCueComplete);
	LOG_PRINTLN("VFD tomato: complete");
}

void updatePomodoroTimer(uint32_t now) {
	if (pomodoroMode != PomodoroFocus && pomodoroMode != PomodoroRelax) {
		return;
	}

	if (now - phaseStartedAt < phaseDurationMs) {
		return;
	}

	if (pomodoroMode == PomodoroFocus) {
		completedTomatoes++;
		startRelaxPhase(now);
		return;
	}

	if (completedTomatoes >= selectedTomatoes) {
		completePomodoro();
	} else {
		startFocusPhase(now);
	}
}

void resetPomodoro() {
	pomodoroMode = PomodoroIdle;
	selectedTomatoes = 1;
	completedTomatoes = 0;
	phaseStartedAt = 0;
	phaseDurationMs = 0;
	activeSoundCue = SoundCueNone;
	soundCueIndex = 0;
	nextSoundAt = 0;
	M5.Speaker.stop();
	LOG_PRINTLN("VFD tomato: cleared");
}

void connectWifiIfNeeded(uint32_t now) {
	if (WIFI_SSID[0] == '\0') {
		return;
	}

	if (WiFi.status() == WL_CONNECTED || now - lastWifiAttempt < WIFI_RETRY_MS) {
		return;
	}

	lastWifiAttempt = now;
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	LOG_PRINTF("VFD time: connecting Wi-Fi SSID=%s\n", WIFI_SSID);
}

void syncTimeIfNeeded(uint32_t now) {
	if (WiFi.status() != WL_CONNECTED || now - lastTimeSyncAttempt < TIME_SYNC_RETRY_MS) {
		return;
	}

	lastTimeSyncAttempt = now;
	if (!timeConfigured) {
		configTime(GMT_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS, "pool.ntp.org", "time.nist.gov");
		timeConfigured = true;
		LOG_PRINTLN("VFD time: NTP configured");
	}

	timeSynced = getLocalTime(&currentTime, 10);
}

}  // namespace vfd_time

void setupVfdTimeApp() {
	vfd_time::lastWifiAttempt = 0;
	vfd_time::lastTimeSyncAttempt = 0;
	vfd_time::lastClockRefresh = 0;
	M5.Speaker.setVolume(80);
}

void updateVfdTimeApp(uint16_t dt) {
	(void)dt;
	const uint32_t now = millis();
	vfd_time::connectWifiIfNeeded(now);
	vfd_time::syncTimeIfNeeded(now);
	if (vfd_time::pomodoroMode == vfd_time::PomodoroSelecting) {
		vfd_time::selectedTomatoes = vfd_time::chooseTomatoesFromImu();
	}
	vfd_time::updatePomodoroTimer(now);
	vfd_time::updateSoundCue(now);

	if (now - vfd_time::lastClockRefresh >= vfd_time::CLOCK_REFRESH_MS) {
		vfd_time::lastClockRefresh = now;
		vfd_time::timeSynced = getLocalTime(&vfd_time::currentTime, 1);
	}
}

void renderVfdTimeApp() {
	vfd_time::drawPanelBackground();
	vfd_time::drawStatusLabels();
	if (vfd_time::pomodoroMode == vfd_time::PomodoroSelecting) {
		vfd_time::drawLargeDuration(vfd_time::selectedDurationMs());
		vfd_time::drawTomatoSlots();
	} else if (vfd_time::pomodoroMode == vfd_time::PomodoroFocus || vfd_time::pomodoroMode == vfd_time::PomodoroRelax) {
		vfd_time::drawLargeDuration(vfd_time::remainingPhaseMs(millis()));
	} else if (vfd_time::pomodoroMode == vfd_time::PomodoroComplete) {
		vfd_time::drawLargeDuration(0);
	} else if (vfd_time::timeSynced) {
		vfd_time::drawLargeTime();
	} else {
		vfd_time::drawGlowText("SYNC", 70, 57, vfd_time::vfdColor(94, 255, 224), 4);
	}
	drawBatteryStatus();
	framebuffer.pushSprite(0, 0);
}

void handleVfdTimeButtonASingleClick() {
	if (vfd_time::pomodoroMode == vfd_time::PomodoroIdle || vfd_time::pomodoroMode == vfd_time::PomodoroComplete) {
		vfd_time::selectedTomatoes = vfd_time::chooseTomatoesFromImu();
		vfd_time::pomodoroMode = vfd_time::PomodoroSelecting;
		return;
	}

	if (vfd_time::pomodoroMode == vfd_time::PomodoroSelecting) {
		vfd_time::completedTomatoes = 0;
		vfd_time::startFocusPhase(millis());
	}
}

void handleVfdTimeButtonADoubleClick() {
	vfd_time::resetPomodoro();
}
