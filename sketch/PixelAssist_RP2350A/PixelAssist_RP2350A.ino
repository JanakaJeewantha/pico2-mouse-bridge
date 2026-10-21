/*
 * PixelAssist RP2350A firmware  v8.0  (16-bit mouse + full keyboard passthrough + HW key/wheel injection, 240 MHz)
 * Board : Waveshare RP2350-USB-A  (or any RP2350 board)
 *
 *   PHYSICAL MOUSE --(PIO-USB host, GP12/GP13)---> [ RP2350A ] --(native USB-C)--> PC
 *
 * v8.0 changes:
 *   - Device now exposes mouse + command + KEYBOARD HID interfaces (needs CFG_TUD_HID >= 3; the build script
 *     passes -DCFG_TUD_HID=4, previously the keyboard interface silently failed to register).
 *   - Generic HID report-descriptor parser for keyboards: works for boot keyboards, report-ID keyboards, NKRO
 *     bitmap keyboards AND keyboard reports hidden inside the mouse's own interface (Logitech G-keys set to keys).
 *   - 0xAB packets (bhop / tapstrafe key) take Windows VK codes, translated to HID usages, and are merged with
 *     the passthrough keyboard so held physical keys are never dropped.
 *   - 0xAB accepted on the serial path as well.
 *
 * Build: Board "Raspberry Pi Pico 2", 240 MHz (freq=240), USB Stack "Adafruit TinyUSB".
 */

#include "pio_usb.h"
#include "Adafruit_TinyUSB.h"
#include "MouseMap.h"

// ---------------- user configuration ----------------
#define PIN_USB_HOST_DP   12         // D+ ; D- = GP13 (DP+1 automatically)
#define USB_VID           0x2E8A     // PixelAssist finds 0x2E8A / 0x239A / 0x1B4F / 0x16C0
#define USB_PID           0x000A
#define USB_MANUFACTURER  "Raspberry Pi"
#define USB_PRODUCT       "Pico 2 Mouse Bridge"
// ----------------------------------------------------

Adafruit_USBH_Host USBHost;
Adafruit_USBD_HID  usb_mouse;
Adafruit_USBD_HID  usb_cmd;
Adafruit_USBD_HID  usb_kbd;

static const uint8_t desc_kbd[] = { TUD_HID_REPORT_DESC_KEYBOARD() };   // standard 8-byte boot keyboard report
static volatile uint8_t kbdRpt[8] = {0};     // latest passthrough keyboard state (from the physical mouse/keyboard)
static volatile bool    kbdDirty  = false;

// Keyboard injection from PC (0xAB packets)
struct KeyCmd { uint8_t vk; bool down; };
static volatile KeyCmd  keyCmds[16];
static volatile uint8_t keyCmdW = 0, keyCmdR = 0;   // ring buffer indices (mod 16)
static uint8_t injKeys[6] = {0};                    // injected HID usages currently held
static uint8_t injMods    = 0;                      // injected modifier bits currently held
// Wheel injection from PC (0xAC packets): each notch becomes its own mouse report
static volatile int8_t  wheelCmds[16];
static volatile uint8_t wheelW = 0, wheelR = 0;

// Per-interface keyboard report layout (parsed from HID report descriptor)
struct KbdMap {
  bool     valid  = false;
  uint8_t  id     = 0;
  int16_t  modOff = -1;                 // 8 modifier bits
  int16_t  keyOff = -1; uint8_t keyCnt = 0;   // array of 8-bit usages
  int16_t  bmOff  = -1; uint16_t bmCnt = 0; uint16_t bmMin = 0;  // NKRO bitmap
};
static KbdMap kmaps[8][8];

// v6.0: 16-bit X/Y mouse (report: buttons, X lo/hi, Y lo/hi, wheel)
static const uint8_t desc_mouse[] = {
  0x05,0x01, 0x09,0x02, 0xA1,0x01, 0x85,0x01, 0x09,0x01, 0xA1,0x00,
  0x05,0x09, 0x19,0x01, 0x29,0x05, 0x15,0x00, 0x25,0x01, 0x95,0x05, 0x75,0x01, 0x81,0x02,
  0x95,0x01, 0x75,0x03, 0x81,0x03,
  0x05,0x01, 0x09,0x30, 0x09,0x31, 0x16,0x01,0x80, 0x26,0xFF,0x7F, 0x75,0x10, 0x95,0x02, 0x81,0x06,
  0x09,0x38, 0x15,0x81, 0x25,0x7F, 0x75,0x08, 0x95,0x01, 0x81,0x06,
  0xC0, 0xC0
};

static const uint8_t desc_cmd[] = {
  0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01,
  0x85, 0x02, 0x09, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x07, 0x91, 0x02,
  0x85, 0x03, 0x09, 0x03, 0x75, 0x08, 0x95, 0x01, 0x81, 0x02, 0xC0
};

static volatile int32_t  physDx = 0, physDy = 0, physWheel = 0;
static volatile uint8_t  physButtons = 0;
static volatile int32_t  aimX = 0, aimY = 0;

static inline int32_t takeAtomic(volatile int32_t* v) { return __atomic_exchange_n(v, 0, __ATOMIC_ACQ_REL); }
static inline void    addAtomic (volatile int32_t* v, int32_t d) { __atomic_fetch_add(v, d, __ATOMIC_ACQ_REL); }

static volatile uint16_t dbgVid = 0, dbgPid = 0, dbgDescLen = 0;
static volatile uint8_t  dbgProto = 0, dbgMapOk = 0, dbgMounted = 0, dbgMouseActive = 0, dbgKbdMap = 0;
static volatile uint32_t dbgReports = 0, dbgKbdReports = 0;

// Windows virtual-key -> HID keyboard usage (0 = unsupported). Modifier VKs return bit via *mod.
static uint8_t vkToHid(uint8_t vk, uint8_t* mod) {
  *mod = 0;
  if (vk >= 0x41 && vk <= 0x5A) return 0x04 + (vk - 0x41);         // A-Z
  if (vk >= 0x31 && vk <= 0x39) return 0x1E + (vk - 0x31);         // 1-9
  if (vk == 0x30) return 0x27;                                      // 0
  if (vk >= 0x70 && vk <= 0x7B) return 0x3A + (vk - 0x70);         // F1-F12
  if (vk >= 0x60 && vk <= 0x69) return (vk == 0x60) ? 0x62 : 0x59 + (vk - 0x61); // numpad 0-9
  switch (vk) {
    case 0x20: return 0x2C;  // Space
    case 0x0D: return 0x28;  // Enter
    case 0x1B: return 0x29;  // Esc
    case 0x08: return 0x2A;  // Backspace
    case 0x09: return 0x2B;  // Tab
    case 0x14: return 0x39;  // CapsLock
    case 0x25: return 0x50;  // Left
    case 0x26: return 0x52;  // Up
    case 0x27: return 0x4F;  // Right
    case 0x28: return 0x51;  // Down
    case 0x2D: return 0x49;  // Insert
    case 0x2E: return 0x4C;  // Delete
    case 0x24: return 0x4A;  // Home
    case 0x23: return 0x4D;  // End
    case 0x21: return 0x4B;  // PgUp
    case 0x22: return 0x4E;  // PgDn
    case 0xBA: return 0x33;  // ;
    case 0xBB: return 0x2E;  // =
    case 0xBC: return 0x36;  // ,
    case 0xBD: return 0x2D;  // -
    case 0xBE: return 0x37;  // .
    case 0xBF: return 0x38;  // /
    case 0xC0: return 0x35;  // `
    case 0xDB: return 0x2F;  // [
    case 0xDC: return 0x31;  // backslash
    case 0xDD: return 0x30;  // ]
    case 0xDE: return 0x34;  // '
    case 0x10: case 0xA0: *mod = 0x02; return 0;   // Shift (left)
    case 0xA1: *mod = 0x20; return 0;
    case 0x11: case 0xA2: *mod = 0x01; return 0;   // Ctrl
    case 0xA3: *mod = 0x10; return 0;
    case 0x12: case 0xA4: *mod = 0x04; return 0;   // Alt
    case 0xA5: *mod = 0x40; return 0;
    case 0x5B: *mod = 0x08; return 0;              // Win
    case 0x5C: *mod = 0x80; return 0;
  }
  return 0;
}

static void handlePacket(const uint8_t* p) {
  uint8_t hdr = p[0];
  uint8_t chk = p[1]^p[2]^p[3]^p[4]^p[5];
  if (chk != p[6]) return;

  if (hdr == 0xAA) {
    int16_t dx = (int16_t)(p[1] | (p[2] << 8));
    int16_t dy = (int16_t)(p[3] | (p[4] << 8));
    addAtomic(&aimX, dx); addAtomic(&aimY, dy);
    // flags byte: bit0 = tracking, bits[7:1] = signed 7-bit wheel delta
    int8_t wdelta = ((int8_t)p[5]) >> 1;
    if (wdelta != 0) addAtomic(&physWheel, wdelta);
  } else if (hdr == 0xAC) {
    // Wheel notch injection: [AC delta(int8) 0 0 0 0 chk]
    uint8_t wi = (wheelW + 1) & 15;
    if (wi != wheelR) { wheelCmds[wheelW] = (int8_t)p[1]; wheelW = wi; }
  } else if (hdr == 0xAB) {
    // Keyboard injection: [AB vk down 0 0 0 chk]
    uint8_t wi = (keyCmdW + 1) & 15;
    if (wi != keyCmdR) {   // drop if ring buffer full
      keyCmds[keyCmdW].vk   = p[1];
      keyCmds[keyCmdW].down = (p[2] != 0);
      keyCmdW = wi;
    }
  } else if (hdr == 0xFF) {
    // Reboot into BOOTSEL (UF2 mass-storage) for firmware update
    rp2040.rebootToBootloader();
  }
}

static void cmdSetReport(uint8_t report_id, hid_report_type_t type, uint8_t const* buf, uint16_t size) {
  (void)report_id; (void)type;
  const uint8_t* p = buf; uint16_t n = size;
  if (n >= 8 && p[0] == 0x02) { p++; n--; }
  if (n >= 7) handlePacket(p);
}

static void pollSerial() {
  static uint8_t buf[7]; static int len = 0;
  while (Serial.available() > 0) {
    uint8_t b = (uint8_t)Serial.read();
    if (len == 0) { if (b == 0xAA || b == 0xAB || b == 0xAC) { buf[0] = b; len = 1; } continue; }
    buf[len++] = b;
    if (len == 7) { handlePacket(buf); len = 0; }
  }
}

void setup() {
  TinyUSBDevice.setID(USB_VID, USB_PID);
  TinyUSBDevice.setManufacturerDescriptor(USB_MANUFACTURER);
  TinyUSBDevice.setProductDescriptor(USB_PRODUCT);

  usb_mouse.setPollInterval(1);
  usb_mouse.setReportDescriptor(desc_mouse, sizeof(desc_mouse));
  usb_mouse.begin();

  usb_cmd.setPollInterval(1);
  usb_cmd.setReportDescriptor(desc_cmd, sizeof(desc_cmd));
  usb_cmd.enableOutEndpoint(true);
  usb_cmd.setReportCallback(NULL, cmdSetReport);
  usb_cmd.begin();

  usb_kbd.setPollInterval(1);
  usb_kbd.setReportDescriptor(desc_kbd, sizeof(desc_kbd));
  usb_kbd.begin();

  Serial.begin(115200);
  if (TinyUSBDevice.mounted()) { TinyUSBDevice.detach(); delay(10); TinyUSBDevice.attach(); }

  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 3; i++) { digitalWrite(LED_BUILTIN, HIGH); delay(80); digitalWrite(LED_BUILTIN, LOW); delay(80); }
}

static void statusLed() {
  static uint32_t t0 = 0; uint32_t now = millis();
  if (now - t0 < 100) return; t0 = now;
  bool on = dbgMouseActive ? true : (dbgMounted ? (bool)((now/150)&1) : false);
  digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
}

extern "C" volatile uint32_t pio_usb_tx_timeouts;
static volatile uint32_t c1Loops = 0;
static volatile uint8_t  c1Stage = 0;

static void debugPrint() {
  static uint32_t lastR = 0, lastT = 0, lastF = 0, lastL = 0;
  if (!Serial) return;
  uint32_t now = millis(); if (now - lastT < 1000) return; lastT = now;
  uint32_t r = dbgReports, f = pio_usb_host_get_frame_number(), l = c1Loops;
  if (lastT <= 1100) { lastR = r; lastF = f; lastL = l; }
  Serial.printf("[host] mounted=%u vid:pid=%04X:%04X proto=%u map=%u kmap=%u mouse=%u reports/s=%lu kbdReports=%lu sof/s=%lu c1loops/s=%lu stage=%u txTimeouts=%lu\r\n",
                dbgMounted, dbgVid, dbgPid, dbgProto, dbgMapOk, dbgKbdMap, dbgMouseActive,
                (unsigned long)(r-lastR), (unsigned long)dbgKbdReports, (unsigned long)(f-lastF), (unsigned long)(l-lastL),
                c1Stage, (unsigned long)pio_usb_tx_timeouts);
  lastR = r; lastF = f; lastL = l;
}

// Build and send one boot-keyboard report = passthrough keys + injected keys (merged, 6KRO)
static void sendKbdMerged() {
  uint8_t rpt[8] = {0};
  rpt[0] = kbdRpt[0] | injMods;
  int n = 0;
  for (int i = 2; i < 8 && n < 6; i++) if (kbdRpt[i]) rpt[2 + n++] = kbdRpt[i];
  for (int s = 0; s < 6 && n < 6; s++) {
    uint8_t u = injKeys[s]; if (!u) continue;
    bool dup = false; for (int j = 0; j < n; j++) if (rpt[2 + j] == u) { dup = true; break; }
    if (!dup) rpt[2 + n++] = u;
  }
  usb_kbd.sendReport(0, rpt, sizeof(rpt));
}

void loop() {
  static int32_t carryX=0, carryY=0, carryW=0;
  static uint8_t lastBtn=0;

  pollSerial(); statusLed(); debugPrint();

  if (!TinyUSBDevice.mounted()) return;

  // 1) Injected key events (bhop / tapstrafe): one report per event so quick press/release is never merged away
  while (keyCmdR != keyCmdW && usb_kbd.ready()) {
    uint8_t vk = keyCmds[keyCmdR].vk; bool dn = keyCmds[keyCmdR].down;
    keyCmdR = (keyCmdR + 1) & 15;
    uint8_t mod = 0; uint8_t u = vkToHid(vk, &mod);
    if (mod) { if (dn) injMods |= mod; else injMods &= ~mod; }
    if (u) {
      if (dn) { bool have = false; for (int s = 0; s < 6; s++) if (injKeys[s] == u) have = true;
                if (!have) for (int s = 0; s < 6; s++) if (!injKeys[s]) { injKeys[s] = u; break; } }
      else    { for (int s = 0; s < 6; s++) if (injKeys[s] == u) injKeys[s] = 0; }
    }
    sendKbdMerged();
    kbdDirty = false;   // merged report already contains latest passthrough state
  }

  // 2) Passthrough keyboard state changed (mouse G-keys mapped to keys, or a real keyboard)
  if (kbdDirty && usb_kbd.ready()) {
    kbdDirty = false;
    sendKbdMerged();
  }

  if (!usb_mouse.ready()) return;

  // 3) Injected wheel notches (tapstrafe): one dedicated report per notch
  if (wheelR != wheelW) {
    int8_t wd = wheelCmds[wheelR]; wheelR = (wheelR + 1) & 15;
    uint8_t wr[6] = { physButtons, 0, 0, 0, 0, (uint8_t)wd };
    usb_mouse.sendReport(1, wr, sizeof(wr));
    return;
  }

  uint8_t btn = physButtons;
  int32_t tx = carryX + takeAtomic(&physDx) + takeAtomic(&aimX);
  int32_t ty = carryY + takeAtomic(&physDy) + takeAtomic(&aimY);
  int32_t tw = carryW + takeAtomic(&physWheel);

  int16_t cx = (int16_t)constrain(tx,-32767,32767);
  int16_t cy = (int16_t)constrain(ty,-32767,32767);
  int8_t  cw = (int8_t)constrain(tw,-127,127);
  carryX = tx-cx; carryY = ty-cy;
  carryW = constrain(tw-cw,-400,400);

  if (cx || cy || cw || btn != lastBtn) {
    uint8_t rpt[6] = { btn, (uint8_t)(cx & 0xFF), (uint8_t)((cx >> 8) & 0xFF),
                       (uint8_t)(cy & 0xFF), (uint8_t)((cy >> 8) & 0xFF), (uint8_t)cw };
    usb_mouse.sendReport(1, rpt, sizeof(rpt)); lastBtn = btn;
  }
}

void setup1() {
  if (F_CPU % 12000000 != 0) Serial.println("[WARN] BAD CLOCK for PIO-USB!");
  delay(20);
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = PIN_USB_HOST_DP;
  // do NOT use the library hardware-alarm timer (dies after 1-2 min on RP2350). We drive frames ourselves.
  pio_cfg.skip_alarm_pool = true;
  USBHost.configure_pio_usb(1, &pio_cfg);
  USBHost.begin(1);
}

void loop1() {
  static uint64_t nextFrame = 0;
  uint64_t now = time_us_64();
  if (nextFrame == 0) nextFrame = now;
  if ((int64_t)(now - nextFrame) >= 0) {
    c1Stage = 1;
    pio_usb_host_frame();
    nextFrame += 1000;
    if ((int64_t)(time_us_64() - nextFrame) > 5000) nextFrame = time_us_64();
  }
  c1Stage = 2;
  USBHost.task();
  c1Stage = 0;
  c1Loops++;
}

// ---- HID descriptor parsers ----
static MouseMap* getMap(uint8_t dev, uint8_t inst) { return (dev<8&&inst<8)?&maps[dev][inst]:nullptr; }
static KbdMap*   getKmap(uint8_t dev, uint8_t inst) { return (dev<8&&inst<8)?&kmaps[dev][inst]:nullptr; }

static bool parseMouse(const uint8_t* d, uint16_t len, MouseMap& out) {
  uint16_t usagePage=0; uint32_t rSize=0,rCount=0; uint8_t rid=0;
  uint32_t usages[24]; uint8_t nUsages=0; uint32_t uMin=0; bool haveMin=false;
  uint16_t offs[16]={0}; MouseMap byId[16]; uint16_t i=0;
  while (i<len) {
    uint8_t b=d[i++];
    if (b==0xFE) { if (i>=len) break; i+=2+d[i]; continue; }
    uint8_t sz=b&3; if (sz==3) sz=4;
    uint8_t type=(b>>2)&3, tag=b>>4;
    if (i+sz>len) break;
    uint32_t v=0; for (uint8_t k=0;k<sz;k++) v|=(uint32_t)d[i+k]<<(8*k); i+=sz;
    if (type==1) { switch(tag){case 0:usagePage=v&0xFFFF;break;case 7:rSize=v;break;case 8:rid=(uint8_t)v;break;case 9:rCount=v;break;} }
    else if (type==2) { switch(tag){case 0:if(nUsages<24)usages[nUsages++]=v&0xFFFF;break;case 1:uMin=v&0xFFFF;haveMin=true;break;} }
    else if (type==0) {
      if (tag==8) {
        MouseMap& m=byId[rid&15]; m.id=rid; uint16_t& off=offs[rid&15];
        if (!(v&1)) {
          for (uint32_t n=0;n<rCount;n++) {
            uint32_t u=nUsages?(n<nUsages?usages[n]:usages[nUsages-1]):(haveMin?uMin+n:0);
            uint16_t bit=off+(uint16_t)(n*rSize);
            if (usagePage==9) { if(m.btnOff<0)m.btnOff=bit; if(m.btnCnt<8)m.btnCnt++; }
            else if (usagePage==1&&rSize<=16) {
              if(u==0x30){m.xOff=bit;m.xSz=(uint8_t)rSize;}
              else if(u==0x31){m.yOff=bit;m.ySz=(uint8_t)rSize;}
              else if(u==0x38){m.wOff=bit;m.wSz=(uint8_t)rSize;}
            }
          }
        }
        off+=(uint16_t)(rSize*rCount);
      }
      nUsages=0; haveMin=false;
    }
  }
  for (int k=0;k<16;k++) if(byId[k].xOff>=0&&byId[k].yOff>=0){out=byId[k];out.valid=true;return true;}
  return false;
}

// Finds a keyboard report (Usage Page 7 inputs) in any descriptor, with or without report IDs.
static bool parseKbd(const uint8_t* d, uint16_t len, KbdMap& out) {
  uint16_t usagePage=0; uint32_t rSize=0,rCount=0; uint8_t rid=0;
  uint32_t uMin=0; bool haveMin=false;
  uint16_t offs[16]={0}; KbdMap byId[16]; uint16_t i=0;
  while (i<len) {
    uint8_t b=d[i++];
    if (b==0xFE) { if (i>=len) break; i+=2+d[i]; continue; }
    uint8_t sz=b&3; if (sz==3) sz=4;
    uint8_t type=(b>>2)&3, tag=b>>4;
    if (i+sz>len) break;
    uint32_t v=0; for (uint8_t k=0;k<sz;k++) v|=(uint32_t)d[i+k]<<(8*k); i+=sz;
    if (type==1) { switch(tag){case 0:usagePage=v&0xFFFF;break;case 7:rSize=v;break;case 8:rid=(uint8_t)v;break;case 9:rCount=v;break;} }
    else if (type==2) { if(tag==1){uMin=v&0xFFFF;haveMin=true;} }
    else if (type==0) {
      if (tag==8) {
        KbdMap& k=byId[rid&15]; k.id=rid; uint16_t& off=offs[rid&15];
        if (usagePage==7) {
          if (rSize==1 && rCount==8 && haveMin && uMin==0xE0 && !(v&1)) { k.modOff=off; }
          else if (rSize==8 && !(v&1) && (v&2)==0 && k.keyOff<0) { k.keyOff=off; k.keyCnt=(uint8_t)rCount; }
          else if (rSize==1 && rCount>8 && !(v&1) && k.bmOff<0) { k.bmOff=off; k.bmCnt=(uint16_t)rCount; k.bmMin=haveMin?(uint16_t)uMin:0; }
        }
        off+=(uint16_t)(rSize*rCount);
      }
      haveMin=false;
    }
  }
  for (int k=0;k<16;k++) if(byId[k].keyOff>=0||byId[k].bmOff>=0){out=byId[k];out.valid=true;return true;}
  return false;
}

static int32_t getBits(const uint8_t* r, uint16_t len, uint16_t bitOff, uint8_t size, bool sgn) {
  uint32_t v=0;
  for (uint8_t k=0;k<size;k++) {
    uint16_t bi=bitOff+k; if((bi>>3)>=len) return 0;
    if(r[bi>>3]&(1<<(bi&7))) v|=1u<<k;
  }
  if(sgn&&size<32&&(v&(1u<<(size-1)))) v|=~0u<<size;
  return (int32_t)v;
}

// Convert a parsed (non-boot) keyboard report into the 8-byte boot layout and hand it to core0.
static void kbdFromMap(const KbdMap& k, const uint8_t* r, uint16_t l) {
  uint8_t out[8]={0}; int n=0;
  if (k.modOff>=0) out[0]=(uint8_t)getBits(r,l,k.modOff,8,false);
  if (k.keyOff>=0) {
    for (uint8_t i=0;i<k.keyCnt && n<6;i++) {
      uint8_t u=(uint8_t)getBits(r,l,k.keyOff+i*8,8,false);
      if (u>3) out[2+n++]=u;                      // skip 0 (none) and 1-3 (rollover/error)
    }
  } else if (k.bmOff>=0) {
    for (uint16_t i=0;i<k.bmCnt && n<6;i++) {
      if (getBits(r,l,k.bmOff+i,1,false)) {
        uint16_t u=k.bmMin+i;
        if (u>=0xE0 && u<=0xE7) out[0]|=(uint8_t)(1<<(u-0xE0));
        else if (u>3 && u<0xE0) out[2+n++]=(uint8_t)u;
      }
    }
  }
  for (int i=0;i<8;i++) kbdRpt[i]=out[i];
  kbdDirty=true; dbgKbdReports++;
}

extern "C" {

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
  uint16_t vid, pid; tuh_vid_pid_get(dev_addr, &vid, &pid);
  uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);
  MouseMap* m = getMap(dev_addr, instance);
  KbdMap*   k = getKmap(dev_addr, instance);
  bool ok = false, kok = false;
  if (m) { *m = MouseMap(); if (desc_report && desc_len) ok = parseMouse(desc_report, desc_len, *m); }
  if (k) { *k = KbdMap();   if (desc_report && desc_len) kok = parseKbd(desc_report, desc_len, *k); }
  dbgVid=vid; dbgPid=pid; dbgProto=proto; dbgDescLen=desc_len; dbgMapOk=ok; dbgMounted=1; if (kok) dbgKbdMap=1;
  if (ok) {
    if (tuh_hid_get_protocol(dev_addr,instance)!=HID_PROTOCOL_REPORT)
      tuh_hid_set_protocol(dev_addr,instance,HID_PROTOCOL_REPORT);
    dbgMouseActive=1; tuh_hid_receive_report(dev_addr,instance);
  } else if (proto==HID_ITF_PROTOCOL_MOUSE) {
    if (tuh_hid_get_protocol(dev_addr,instance)!=HID_PROTOCOL_BOOT)
      tuh_hid_set_protocol(dev_addr,instance,HID_PROTOCOL_BOOT);
    dbgMouseActive=1; tuh_hid_receive_report(dev_addr,instance);
  } else if (proto==HID_ITF_PROTOCOL_KEYBOARD) {
    // boot keyboard -> fixed 8-byte reports
    if (tuh_hid_get_protocol(dev_addr,instance)!=HID_PROTOCOL_BOOT)
      tuh_hid_set_protocol(dev_addr,instance,HID_PROTOCOL_BOOT);
    tuh_hid_receive_report(dev_addr,instance);
  } else {
    // protocol "none" interface (may still carry keyboard reports, parsed via descriptor map)
    if (kok && tuh_hid_get_protocol(dev_addr,instance)!=HID_PROTOCOL_REPORT)
      tuh_hid_set_protocol(dev_addr,instance,HID_PROTOCOL_REPORT);
    tuh_hid_receive_report(dev_addr,instance);
  }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  MouseMap* m = getMap(dev_addr,instance); if(m) *m=MouseMap();
  KbdMap*   k = getKmap(dev_addr,instance); if(k) *k=KbdMap();
  physButtons=0; dbgMouseActive=0; dbgMounted=0; dbgMapOk=0; dbgKbdMap=0;
  for (int i = 0; i < 8; i++) kbdRpt[i] = 0;
  kbdDirty = true;   // release any keys that were held
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  MouseMap* m = getMap(dev_addr,instance);
  KbdMap*   k = getKmap(dev_addr,instance);
  dbgReports++;

  uint8_t proto = tuh_hid_interface_protocol(dev_addr,instance);
  bool bootNow  = (tuh_hid_get_protocol(dev_addr,instance)==HID_PROTOCOL_BOOT);

  // (a) boot keyboard interface -> hand the 8-byte report to core0 (device stack is not thread-safe)
  if (proto==HID_ITF_PROTOCOL_KEYBOARD && bootNow) {
    if (len >= 8) { for (int i = 0; i < 8; i++) kbdRpt[i] = report[i]; kbdDirty = true; dbgKbdReports++; }
    tuh_hid_receive_report(dev_addr,instance);
    return;
  }

  // (b) keyboard report described by the descriptor (report-ID / NKRO / embedded in a mouse interface)
  if (k && k->valid) {
    if (k->id==0) { kbdFromMap(*k, report, len); tuh_hid_receive_report(dev_addr,instance); return; }
    else if (len>=1 && report[0]==k->id) { kbdFromMap(*k, report+1, len-1); tuh_hid_receive_report(dev_addr,instance); return; }
  } else if (proto==HID_ITF_PROTOCOL_KEYBOARD && !bootNow && len>=8) {
    for (int i = 0; i < 8; i++) kbdRpt[i] = report[i]; kbdDirty = true; dbgKbdReports++;
    tuh_hid_receive_report(dev_addr,instance);
    return;
  }

  // (c) mouse
  bool boot = bootNow && (proto==HID_ITF_PROTOCOL_MOUSE);
  if (boot) {
    if (len>=3) {
      physButtons=report[0]&0x1F;
      addAtomic(&physDx,(int8_t)report[1]); addAtomic(&physDy,(int8_t)report[2]);
      if(len>=4) addAtomic(&physWheel,(int8_t)report[3]);
    }
  } else if (m&&m->valid) {
    const uint8_t* r=report; uint16_t l=len; bool match=true;
    if(m->id!=0){if(len<1||report[0]!=m->id)match=false;else{r=report+1;l=len-1;}}
    if(match){
      if(m->btnOff>=0) physButtons=(uint8_t)getBits(r,l,m->btnOff,m->btnCnt,false)&0x1F;
      addAtomic(&physDx,getBits(r,l,m->xOff,m->xSz,true));
      addAtomic(&physDy,getBits(r,l,m->yOff,m->ySz,true));
      if(m->wOff>=0) addAtomic(&physWheel,getBits(r,l,m->wOff,m->wSz,true));
    }
  }
  tuh_hid_receive_report(dev_addr,instance);
}

} // extern "C"
